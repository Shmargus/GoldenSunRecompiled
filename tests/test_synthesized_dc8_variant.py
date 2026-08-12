import hashlib
import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "tools" / "build_synthesized_dc8_variant.py"
SPEC = importlib.util.spec_from_file_location("build_synthesized_dc8_variant", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class SynthesizedDc8VariantTests(unittest.TestCase):
    def test_repeats_six_template_words_without_overwriting_fixed_slots(self):
        rom = bytearray(0x2000)
        source = MODULE.rom_address(0x030008D4) - MODULE.ROM_BASE
        destination = MODULE.rom_address(MODULE.ENTRY) - MODULE.ROM_BASE
        for index in range(6):
            rom[source + index * 4 : source + index * 4 + 4] = bytes([index + 1]) * 4
        rom[destination : destination + 0xA4] = b"\xAA" * 0xA4

        result = MODULE.build_variant(bytes(rom), 0x030008D4)

        for iteration in range(4):
            base = destination + iteration * 0x24
            self.assertEqual(result[base : base + 8], b"\x01" * 4 + b"\x02" * 4)
            self.assertEqual(result[base + 8 : base + 12], b"\xAA" * 4)
            self.assertEqual(
                result[base + 12 : base + 28],
                b"".join(bytes([index]) * 4 for index in range(3, 7)),
            )
            self.assertEqual(result[base + 28 : base + 36], b"\xAA" * 8)

    def test_config_records_identity_and_lifetime(self):
        text = MODULE.render_config("a" * 40, 0x030008D4, "b" * 40)
        self.assertIn('sha1 = "' + "a" * 40 + '"', text)
        self.assertIn("start = 0x03000820", text)
        self.assertIn("end = 0x030008cc", text)
        self.assertIn('mode = "arm"', text)
        self.assertIn("until its next pass or IWRAM reset", text)


if __name__ == "__main__":
    unittest.main()
