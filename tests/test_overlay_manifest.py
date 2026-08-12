from __future__ import annotations

import unittest

from test_main_symbol_import import build_synthetic_elf
from tools.import_main_symbols import import_symbols, parse_elf32_arm
from tools.validate_overlay_manifest import validate_overlay_documents
from tools.verify_rom import EXPECTED_SHA1


REVISION = "0" * 40


def overlay_entry(overlay_id: str, rom_offset: int) -> dict:
    return {
        "overlay_id": overlay_id,
        "evidence_source": f"overlays/{overlay_id}/overlay.elf:synthetic",
        "source": {
            "rom_offset": f"0x{rom_offset:08x}",
            "rom_start": f"0x{0x08000000 + rom_offset:08x}",
            "compressed_size": 8,
            "compressed_sha256": "0" * 64,
            "decompressed_size": 12,
            "decompressed_sha256": "1" * 64,
        },
        "runtime": {
            "start": "0x03000000",
            "loaded_size": 12,
            "memory_size": 12,
        },
        "symbols": {
            "count": 2,
            "arm_count": 1,
            "thumb_count": 1,
            "proven_count": 2,
            "unresolved_confidence_count": 0,
            "excluded_count": 0,
        },
    }


def synthetic_documents():
    image = parse_elf32_arm(
        build_synthetic_elf(physical_address=0x03000000)
    )
    overlays = [("rom_000100", 0x100), ("rom_000200", 0x200)]
    symbols = []
    entries = []
    for overlay_id, rom_offset in overlays:
        corpus, report = import_symbols(
            image,
            binary_id="synthetic_overlay",
            overlay_id=overlay_id,
            evidence_source=f"overlays/{overlay_id}/overlay.elf:synthetic",
            evidence_revision=REVISION,
        )
        assert report["unresolved"] == []
        symbols.extend(corpus["symbols"])
        entries.append(overlay_entry(overlay_id, rom_offset))
    symbols.sort(
        key=lambda record: (
            record["binary_id"],
            record["overlay_id"],
            int(record["runtime_address"], 16),
            record["name"],
        )
    )
    manifest = {
        "schema_version": 1,
        "rom_sha1": EXPECTED_SHA1,
        "binary_id": "synthetic_overlay",
        "evidence_revision": REVISION,
        "overlays": entries,
    }
    return manifest, {"schema_version": 1, "symbols": symbols}


class OverlayManifestTests(unittest.TestCase):
    def test_cross_overlay_runtime_collisions_are_partitioned(self) -> None:
        manifest, corpus = synthetic_documents()
        errors, summary = validate_overlay_documents(manifest, corpus)
        self.assertEqual(errors, [])
        self.assertEqual(summary["overlay_count"], 2)
        self.assertEqual(summary["symbol_count"], 4)
        self.assertEqual(summary["collision_addresses"], 2)

    def test_symbol_with_unknown_overlay_is_rejected(self) -> None:
        manifest, corpus = synthetic_documents()
        corpus["symbols"][0]["overlay_id"] = "rom_000300"
        errors, _summary = validate_overlay_documents(manifest, corpus)
        self.assertTrue(any("absent from overlay manifest" in error for error in errors))

    def test_manifest_count_mismatch_is_rejected(self) -> None:
        manifest, corpus = synthetic_documents()
        manifest["overlays"][0]["symbols"]["count"] = 3
        errors, _summary = validate_overlay_documents(manifest, corpus)
        self.assertTrue(any("manifest count does not match corpus" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
