from __future__ import annotations

import hashlib
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from build_all_overlays import inventory_overlay, render_registry  # noqa: E402
from tests.test_main_symbol_import import build_synthetic_elf  # noqa: E402


class BuildAllOverlaysTests(unittest.TestCase):
    def test_inventory_hashes_only_executable_text_for_runtime_identity(self) -> None:
        loaded = bytes(12) + b"DATA"
        overlay_id = "rom_000100"
        record = {
            "overlay_id": overlay_id,
            "source": {"decompressed_sha256": hashlib.sha256(loaded).hexdigest()},
            "runtime": {"start": "0x03000000", "loaded_size": len(loaded)},
        }
        corpus = {
            "symbols": [{
                "overlay_id": overlay_id,
                "runtime_address": "0x03000000",
                "mode": "arm",
                "size": 4,
                "confidence": "proven",
            }],
        }

        with tempfile.TemporaryDirectory() as temporary:
            overlay = Path(temporary) / overlay_id
            overlay.mkdir()
            (overlay / "orig.bin").write_bytes(loaded)
            (overlay / "overlay.elf").write_bytes(
                build_synthetic_elf(physical_address=0x03000000)
            )

            item = inventory_overlay(record, Path(temporary), corpus)

        self.assertEqual(item["start"], 0x03000000)
        self.assertEqual(item["end"], 0x0300000C)
        self.assertEqual(item["text_sha1"], hashlib.sha1(bytes(12)).hexdigest())
        self.assertNotEqual(item["text_sha1"], hashlib.sha1(loaded).hexdigest())

    def test_inventory_rejects_unverified_decompressed_bytes(self) -> None:
        overlay_id = "rom_000100"
        record = {
            "overlay_id": overlay_id,
            "source": {"decompressed_sha256": "0" * 64},
            "runtime": {"start": "0x03000000", "loaded_size": 12},
        }
        corpus = {"symbols": []}

        with tempfile.TemporaryDirectory() as temporary:
            overlay = Path(temporary) / overlay_id
            overlay.mkdir()
            (overlay / "orig.bin").write_bytes(bytes(12))
            (overlay / "overlay.elf").write_bytes(
                build_synthetic_elf(physical_address=0x03000000)
            )
            with self.assertRaisesRegex(ValueError, "decompressed SHA-256 mismatch"):
                inventory_overlay(record, Path(temporary), corpus)

    def test_registry_render_is_stable_and_records_equivalent_identity(self) -> None:
        inventories = [{
            "id": "rom_000100",
            "start": 0x02008000,
            "end": 0x02008100,
            "text_sha1": "a" * 40,
        }]

        first = render_registry(inventories, {"rom_000100": ["rom_000200"]})
        second = render_registry(inventories, {"rom_000100": ["rom_000200"]})

        self.assertEqual(first, second)
        self.assertIn("Also identifies equivalent code image(s): rom_000200", first)
        self.assertIn(
            'GSR_OVERLAY(rom_000100, 0x02008000u, 0x02008100u, "' + "a" * 40 + '")',
            first,
        )


if __name__ == "__main__":
    unittest.main()
