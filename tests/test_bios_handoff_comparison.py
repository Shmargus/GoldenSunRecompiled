import importlib.util
from pathlib import Path
import tempfile
import unittest


MODULE_PATH = Path(__file__).parents[1] / "tools" / "compare_bios_handoff.py"
SPEC = importlib.util.spec_from_file_location("compare_bios_handoff", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class BiosHandoffComparisonTests(unittest.TestCase):
    def test_oracle_pc_normalization_uses_pipeline_offset(self):
        self.assertEqual(
            MODULE.oracle_executing_pc({"pc": 0x08000004, "thumb": False}),
            0x08000000,
        )
        self.assertEqual(
            MODULE.oracle_executing_pc({"pc": 0x08000002, "thumb": True}),
            0x08000000,
        )

    def test_state_difference_reports_first_class_cpu_fields(self):
        native = {"pc": 0x08000000, "cpsr": 0xD3}
        oracle = {"pc": 0x08000004, "thumb": False, "cpsr": 0xD3}
        for index in range(15):
            native[f"r{index}"] = index
            oracle[f"r{index}"] = index
        oracle["r7"] = 99

        self.assertEqual(
            MODULE.state_differences(native, oracle),
            [{"field": "r7", "native": 7, "oracle": 99}],
        )

    def test_first_region_difference(self):
        self.assertIsNone(MODULE.first_region_difference(b"abc", b"abc"))
        self.assertEqual(MODULE.first_region_difference(b"abc", b"axc"), 1)
        self.assertEqual(MODULE.first_region_difference(b"abc", b"ab"), 2)

    def test_fingerprint_comparison_reports_native_stream_boundary(self):
        regs = tuple(range(16))
        native = [
            (0, MODULE.HANDOFF_PC, 0x1F, regs),
            (1, 0x080003C0, 0x1F, regs),
        ]
        oracle = native + [(2, 0x080003C4, 0x1F, regs)]

        self.assertEqual(
            MODULE.compare_fingerprints(native, oracle),
            {
                "status": "native_stream_ended",
                "identical_prefix": 2,
                "last_native_pc": 0x080003C0,
                "next_oracle_pc": 0x080003C4,
                "native_game_records": 2,
                "oracle_records": 3,
            },
        )

    def test_fingerprint_tail_streams_from_requested_pc(self):
        regs = tuple(range(16))
        records = [
            (10, 0x00000000, 0xD3, regs),
            (20, MODULE.HANDOFF_PC, 0x1F, regs),
            (23, 0x080003C0, 0x1F, regs),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.fp"
            with path.open("wb") as stream:
                stream.write(
                    MODULE.FP_HEADER.pack(
                        MODULE.FP_MAGIC, MODULE.FP_RECORD.size, len(records)
                    )
                )
                for cycles, pc, cpsr, values in records:
                    stream.write(
                        MODULE.FP_RECORD.pack(cycles, pc, cpsr, *values)
                    )

            count, tail = MODULE.load_fingerprint_tail(
                path, MODULE.HANDOFF_PC
            )

        self.assertEqual(count, 3)
        self.assertEqual(tail, records[1:])


if __name__ == "__main__":
    unittest.main()
