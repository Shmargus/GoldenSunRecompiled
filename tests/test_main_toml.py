from __future__ import annotations

import sys
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from build_main_toml import (  # noqa: E402
    REVIEWED_JUMP_TABLES,
    REVIEWED_RESUME_FUNCTIONS,
    ROM_BASE,
    apply_data_exceptions,
    decode_cartridge_entry,
    derive_code_copies,
    derive_data_ranges,
    derive_resume_ranges,
    render_toml,
    select_proven_seeds,
    subtract_jump_table_ranges,
    verify_reviewed_jump_tables,
)
from import_main_symbols import (  # noqa: E402
    ElfImage,
    ProgramHeader,
    Section,
    Symbol,
)


class MainTomlTests(unittest.TestCase):
    def synthetic_image(self) -> ElfImage:
        sections = (
            Section(0, "", 0, 0, 0, 0, 0, 0, 0),
            Section(1, "rom_code", 1, 4, ROM_BASE, 0, 0x10, 0, 0),
            Section(2, "iwram_code", 1, 4, 0x03000000, 0, 0x10, 0, 0),
        )
        symbols = (
            Symbol(0, "", 0, 0, 0, 0, 0),
            Symbol(1, "$a", ROM_BASE, 0, 0, 0, 1),
            Symbol(2, "$d", ROM_BASE + 8, 0, 0, 0, 1),
            Symbol(3, "$a", 0x03000000, 0, 0, 0, 2),
            Symbol(4, "$d", 0x03000004, 0, 0, 0, 2),
            Symbol(5, "$t", 0x03000008, 0, 0, 0, 2),
            # A linker artifact with the section index but an out-of-range value.
            Symbol(6, "$d.artifact", 0x12345678, 0, 0, 0, 2),
        )
        headers = (
            ProgramHeader(1, 0, ROM_BASE, ROM_BASE, 0x10, 0x10, 5),
            ProgramHeader(1, 0, 0x03000000, ROM_BASE + 0x10, 0x10, 0x10, 5),
        )
        return ElfImage(sections, headers, symbols)

    def test_resume_ranges_split_around_a_functions_literal_pool(self) -> None:
        """A resume range must never offer `$d` bytes as an entry point."""

        import build_main_toml

        image = ElfImage(
            (Section(0, "", 0, 0, 0, 0, 0, 0, 0),),
            (),
            (
                Symbol(0, "", 0, 0, 0, 0, 0),
                Symbol(1, "Func_test", ROM_BASE | 1, 0x40, 2, 1, 1),
            ),
        )
        reviewed = (
            {"addr": ROM_BASE, "mode": "thumb", "note": "synthetic"},
        )
        original = build_main_toml.REVIEWED_RESUME_FUNCTIONS
        build_main_toml.REVIEWED_RESUME_FUNCTIONS = reviewed
        try:
            ranges = derive_resume_ranges(
                image, [(ROM_BASE + 0x10, ROM_BASE + 0x18)]
            )
        finally:
            build_main_toml.REVIEWED_RESUME_FUNCTIONS = original
        self.assertEqual(
            [(entry["start"], entry["end"]) for entry in ranges],
            [(ROM_BASE, ROM_BASE + 0x10), (ROM_BASE + 0x18, ROM_BASE + 0x40)],
        )
        self.assertTrue(all(entry["mode"] == "thumb" for entry in ranges))

    def test_resume_ranges_respect_the_schema_size_cap(self) -> None:
        import build_main_toml

        size = build_main_toml.RESUME_RANGE_MAX_BYTES + 0x10
        image = ElfImage(
            (Section(0, "", 0, 0, 0, 0, 0, 0, 0),),
            (),
            (
                Symbol(0, "", 0, 0, 0, 0, 0),
                Symbol(1, "Func_big", ROM_BASE, size, 2, 1, 1),
            ),
        )
        original = build_main_toml.REVIEWED_RESUME_FUNCTIONS
        build_main_toml.REVIEWED_RESUME_FUNCTIONS = (
            {"addr": ROM_BASE, "mode": "arm", "note": "synthetic"},
        )
        try:
            ranges = derive_resume_ranges(image, [])
        finally:
            build_main_toml.REVIEWED_RESUME_FUNCTIONS = original
        self.assertEqual(len(ranges), 2)
        self.assertTrue(
            all(
                entry["end"] - entry["start"]
                <= build_main_toml.RESUME_RANGE_MAX_BYTES
                for entry in ranges
            )
        )

    def test_decodes_reset_branch(self) -> None:
        self.assertEqual(decode_cartridge_entry(bytes.fromhex("ee0000ea")), 0x080003C0)

    def test_derives_data_and_copy_ranges(self) -> None:
        image = self.synthetic_image()
        self.assertEqual(
            derive_data_ranges(image),
            [
                (0x03000004, 0x03000008),
                (ROM_BASE + 8, ROM_BASE + 0x00800000),
            ],
        )
        self.assertEqual(
            derive_code_copies(image),
            [
                {
                    "runtime_start": 0x03000000,
                    "source_start": ROM_BASE + 0x10,
                    "size": 0x10,
                }
            ],
        )

    def test_reviewed_data_exception_splits_a_range(self) -> None:
        self.assertEqual(
            apply_data_exceptions(
                [(0x100, 0x110)],
                [
                    {
                        "start": "0x00000104",
                        "end": "0x00000106",
                        "mode": "thumb",
                        "evidence": "synthetic control flow",
                    }
                ],
            ),
            [(0x100, 0x104), (0x106, 0x110)],
        )

    def test_reviewed_jump_table_is_verified_and_owns_its_data_range(self) -> None:
        rom = bytearray(0x800000)
        for table in REVIEWED_JUMP_TABLES:
            offset = table["addr"] - ROM_BASE
            for index, target in enumerate(table["targets"]):
                start = offset + index * table["stride"]
                rom[start:start + 4] = target.to_bytes(4, "little")
        verify_reviewed_jump_tables(bytes(rom))
        self.assertEqual(
            subtract_jump_table_ranges(
                [(0x080779C0, 0x080779E0)],
                ({"addr": 0x080779C4, "stride": 4, "count": 6},),
            ),
            [(0x080779C0, 0x080779C4), (0x080779DC, 0x080779E0)],
        )

    def test_only_proven_entries_are_rendered(self) -> None:
        base = {
            "binary_id": "golden_sun_main",
            "overlay_id": "",
            "size": 4,
            "section": "code",
            "evidence_source": "synthetic",
            "evidence_revision": "a" * 40,
        }
        corpus = {
            "schema_version": 1,
            "symbols": [
                {
                    **base,
                    "name": "copied",
                    "runtime_address": "0x03000000",
                    "source_address": "0x08000010",
                    "mode": "arm",
                    "confidence": "proven",
                },
                {
                    **base,
                    "name": "uncertain",
                    "runtime_address": "0x08000100",
                    "source_address": "0x08000100",
                    "mode": "thumb",
                    "confidence": "unresolved",
                },
            ],
        }
        seeds = select_proven_seeds(corpus, 0x080003C0)
        self.assertEqual([seed["name"] for seed in seeds], ["copied"])
        text = render_toml(
            reset_target=0x080003C0,
            disassembly_revision="a" * 40,
            corpus_sha256="b" * 64,
            seeds=seeds,
            pointer_seeds=[],
            section_seeds=[],
            interwork_resumes=[],
            veneer_resumes=[],
            resume_ranges=[
                {
                    "start": 0x08001000,
                    "end": 0x08001040,
                    "mode": "thumb",
                    "note": "synthetic resume run",
                }
            ],
            data_ranges=[(0x03000004, 0x03000008)],
            code_copies=[
                {
                    "runtime_start": 0x03000000,
                    "source_start": 0x08000010,
                    "size": 0x10,
                }
            ],
            data_exception_sha256="c" * 64,
        )
        self.assertIn("[[resume_range]]", text)
        self.assertIn("start = 0x08001000", text)
        self.assertIn("end = 0x08001040", text)
        self.assertIn("source_addr = 0x08000010", text)
        self.assertIn("entry_pc = 0x08000000", text)
        self.assertIn("reset vector: 0x08000000 ARM branch to 0x080003c0", text)
        self.assertIn('name = "copied"', text)
        self.assertIn("[[runtime_code_entry]]", text)
        self.assertIn("addr = 0x03000828", text)
        self.assertIn('mode = "arm"', text)
        for addr in range(0x03000198, 0x030001B8, 4):
            self.assertIn(f"addr = 0x{addr:08x}", text)
            self.assertIn(
                f'name = "Func_8d8_computed_entry_{addr:08x}"', text
            )
        self.assertIn("source_addr = 0x08000924", text)
        self.assertIn("independently reached by mGBA with CPSR.T clear", text)
        self.assertIn("addr = 0x03000164", text)
        self.assertIn('name = "Func_8d4"', text)
        self.assertIn("addr = 0x03002000", text)
        self.assertIn('name = "copied_Func_15430_03002000"', text)
        self.assertIn("addr = 0x03002140", text)
        self.assertIn('name = "copied_Func_15570_03002140"', text)
        self.assertIn("addr = 0x0300387c", text)
        self.assertIn('name = "copied_Func_1dc8_0300387c"', text)
        self.assertIn("addr = 0x0300347c", text)
        self.assertIn("addr = 0x03000954", text)
        self.assertIn("source_addr = 0x080010c4", text)
        self.assertIn('name = "resume_03000954"', text)
        self.assertIn("addr = 0x080103a6", text)
        self.assertIn('name = "resume_080103a6"', text)
        self.assertIn("GS-011 strict-static IRQ resume at Func_10230+0x176", text)
        self.assertIn("addr = 0x0300139c", text)
        self.assertIn("source_addr = 0x08001b0c", text)
        self.assertIn('name = "resume_0300139c"', text)
        self.assertIn("GS-011 strict-static IRQ resume at Func_1af8+0x14", text)
        self.assertIn('name = "copied_Func_2544_0300347c"', text)
        # The three PCs that were once listed individually inside the
        # Func_2544 copy (+0x50, +0x144, +0x298) are now covered by the
        # word-by-word alias over its whole ARM extent, so each must still be
        # emitted with the same runtime/ROM pairing it had before.
        for runtime, source in (
            (0x030034CC, 0x08002594),
            (0x030035C0, 0x08002688),
            (0x03003714, 0x080027DC),
        ):
            self.assertIn(f"addr = 0x{runtime:08x}", text)
            self.assertIn(f"source_addr = 0x{source:08x}", text)
            self.assertIn(f'name = "resume_{runtime:08x}"', text)
        self.assertIn(
            "GS-011 IRQ/VBlank resume inside the Func_2544 flash driver", text
        )
        # The copy's own bounds must never be aliased past: the last entry maps
        # to the final ARM word of the ELF extent, not beyond it.
        self.assertIn("source_addr = 0x08002804", text)
        self.assertNotIn("source_addr = 0x08002808", text)
        self.assertIn("addr = 0x080f2c06", text)
        self.assertIn('name = "resume_080f2c06"', text)
        self.assertIn("GS-011 strict-static IRQ resume at Func_f2b70+0x96", text)
        self.assertIn("addr = 0x080068fe", text)
        self.assertIn('name = "resume_080068fe"', text)
        self.assertIn("GS-011 strict-static IRQ resume at Func_6878+0x86", text)

        self.assertTrue(
            any(
                entry["addr"] == 0x080F26EC and entry["mode"] == "thumb"
                for entry in REVIEWED_RESUME_FUNCTIONS
            )
        )
        self.assertTrue(
            any(
                entry["addr"] == 0x080F2EBC and entry["mode"] == "thumb"
                for entry in REVIEWED_RESUME_FUNCTIONS
            )
        )
        self.assertIn("addr = 0x08006900", text)
        self.assertIn('name = "resume_08006900"', text)
        self.assertIn("GS-011 strict-static IRQ resume at Func_6878+0x88", text)
        self.assertIn("addr = 0x080fae58", text)
        self.assertIn("addr = 0x080908e0", text)
        self.assertIn('name = "Func_908e0"', text)
        self.assertIn("addr = 0x0800cacc", text)
        self.assertIn('name = "Func_cacc"', text)
        self.assertIn("function-pointer target through _call_via_r0", text)
        self.assertIn("addr = 0x0808a360", text)
        self.assertIn('name = "_Func_91dc8"', text)
        self.assertIn("__Func_91dc8 veneer exchanges to 0x0808a361", text)
        self.assertIn("addr = 0x080000d0", text)
        self.assertIn('name = "_Func_41d8"', text)
        self.assertIn("__Func_41d8 veneer exchanges to 0x080000d1", text)
        self.assertIn("addr = 0x0808a080", text)
        self.assertIn('name = "_Func_92054"', text)
        self.assertIn("__Func_92054 veneer exchanges to 0x0808a081", text)
        self.assertIn("addr = 0x08000290", text)
        self.assertIn("addr = 0x080001a8", text)
        self.assertIn('name = "_Func_5340"', text)
        self.assertIn('name = "_Func_2f40"', text)
        self.assertIn("strict-static verified-overlay callback target", text)
        self.assertIn('name = "Func_fae58"', text)
        self.assertIn("function contains ELF-marked literal pools", text)
        observed_miss_addresses = (
            0x08000298,
            0x080002A8,
            0x080002D0,
            0x0801011C,
            0x08010128,
            0x080102C8,
            0x080102D8,
            0x08011CE0,
            0x08015318,
            0x080160FC,
            0x0808A010,
            0x0808B28C,
            0x080F9A50,
            0x080F2018,
            0x080F2020,
            0x080F9A6E,
            0x080F9AE0,
            0x080F9B60,
            0x080F9B66,
            0x080F9B74,
            0x080F9B8E,
            0x080F9B96,
            0x080F9B9E,
            0x080F9BA4,
            0x080F9BAA,
            0x080F9BF4,
            0x080F9BFA,
            0x080F9F6C,
            0x080F9FB0,
            0x080F9FB2,
            0x080FA0B2,
            0x080FA0C0,
            0x080FA0DA,
            0x080FA100,
            0x080FA10C,
            0x080FA144,
            0x080FA1D4,
            0x080FA1E8,
            0x080FACF8,
        )
        for addr in observed_miss_addresses:
            self.assertIn(f"addr = 0x{addr:08x}", text)
            self.assertIn(f'name = "observed_thumb_entry_{addr:08x}"', text)
        self.assertEqual(
            text.count("GS-011 observed THUMB dispatch miss; on-demand finder"),
            len(observed_miss_addresses),
        )
        self.assertIn("addr = 0x080047ae", text)
        self.assertIn('name = "resume_080047ae"', text)
        for addr in (0x0801CC64, 0x0801CC74, 0x0801CC84):
            self.assertIn(f"addr = 0x{addr:08x}", text)
            self.assertIn(f'name = "resume_{addr:08x}"', text)
        self.assertIn("resume = true", text)
        self.assertNotIn("uncertain", text)
        self.assertIn("[[jump_table]]", text)
        self.assertIn("addr = 0x080779c4", text)
        self.assertIn('name = "Func_77320_state_dispatch"', text)


if __name__ == "__main__":
    unittest.main()
