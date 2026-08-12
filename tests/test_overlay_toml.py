from __future__ import annotations

import contextlib
import hashlib
import io
import json
import sys
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import build_overlay_toml  # noqa: E402
from build_overlay_toml import (  # noqa: E402
    derive_overlay_data_ranges,
    render_overlay_toml,
    select_overlay_seeds,
)
from import_main_symbols import ElfImage, Section, Symbol  # noqa: E402
from tests.test_main_symbol_import import build_synthetic_elf  # noqa: E402


class OverlayTomlTests(unittest.TestCase):
    def test_derives_mapping_data_and_loaded_tail(self) -> None:
        image = ElfImage(
            sections=(
                Section(0, "", 0, 0, 0, 0, 0, 0, 0),
                Section(1, ".text", 1, 4, 0x02008000, 0, 0x10, 0, 0),
            ),
            program_headers=(),
            symbols=(
                Symbol(0, "", 0, 0, 0, 0, 0),
                Symbol(1, "$t", 0x02008000, 0, 0, 0, 1),
                Symbol(2, "$d", 0x02008008, 0, 0, 0, 1),
                Symbol(3, "$t", 0x0200800C, 0, 0, 0, 1),
            ),
        )
        self.assertEqual(
            derive_overlay_data_ranges(image, 0x02008000, 0x18),
            [(0x02008008, 0x0200800C), (0x02008010, 0x02008018)],
        )

    def test_reviewed_executed_alignment_is_not_declared_data(self) -> None:
        image = ElfImage(
            sections=(
                Section(0, "", 0, 0, 0, 0, 0, 0, 0),
                Section(1, ".text", 1, 4, 0x02008FF0, 0, 0x10, 0, 0),
            ),
            program_headers=(),
            symbols=(
                Symbol(0, "", 0, 0, 0, 0, 0),
                Symbol(1, "$t", 0x02008FF0, 0, 0, 0, 1),
                Symbol(2, "$d", 0x02008FF6, 0, 0, 0, 1),
                Symbol(3, "$t", 0x02008FF8, 0, 0, 0, 1),
            ),
        )
        self.assertEqual(
            derive_overlay_data_ranges(
                image, 0x02008FF0, 0x10, "rom_791794"
            ),
            [],
        )

    def test_observed_entry_and_resume_require_symbol_evidence(self) -> None:
        base = {
            "binary_id": "golden_sun_overlay",
            "overlay_id": "rom_test",
            "source_address": "0x02008000",
            "section": ".text",
            "evidence_source": "synthetic",
            "evidence_revision": "a" * 40,
            "mode": "thumb",
            "confidence": "unresolved",
        }
        corpus = {
            "schema_version": 1,
            "symbols": [{
                **base,
                "name": "func",
                "runtime_address": "0x02008000",
                "size": 0x10,
            }],
        }
        observed = {"entries": [
            {"addr": "0x02008000", "mode": "thumb", "evidence": "entry"},
            {"addr": "0x02008008", "mode": "thumb", "resume": True,
             "evidence": "resume"},
        ]}
        seeds = select_overlay_seeds(corpus, observed, "rom_test")
        self.assertEqual([seed["addr"] for seed in seeds], [0x02008000, 0x02008008])
        self.assertTrue(seeds[1]["resume"])

    def test_empty_observation_still_selects_complete_elf_entry_set(self) -> None:
        base = {
            "binary_id": "golden_sun_overlay",
            "overlay_id": "rom_test",
            "source_address": "0x02008000",
            "section": ".text",
            "evidence_source": "synthetic",
            "evidence_revision": "a" * 40,
            "mode": "thumb",
        }
        corpus = {
            "schema_version": 1,
            "symbols": [
                {
                    **base,
                    "name": "proven_func",
                    "runtime_address": "0x02008000",
                    "size": 8,
                    "confidence": "proven",
                },
                {
                    **base,
                    "name": "mapped_func",
                    "runtime_address": "0x02008008",
                    "source_address": "0x02008008",
                    "size": 8,
                    "confidence": "unresolved",
                },
            ],
        }

        seeds = select_overlay_seeds(corpus, {"entries": []}, "rom_test")

        self.assertEqual(
            [(seed["addr"], seed["owner_end"]) for seed in seeds],
            [(0x02008000, 0x02008008), (0x02008008, 0x02008010)],
        )
        self.assertTrue(all(not seed["resume"] for seed in seeds))

    def test_proactive_render_covers_each_elf_routine_but_not_data(self) -> None:
        seeds = [
            {
                "addr": 0x02008000,
                "mode": "thumb",
                "name": "first",
                "resume": False,
                "owner_start": 0x02008000,
                "owner_end": 0x02008010,
                "note": "synthetic entry",
            },
            {
                "addr": 0x02008010,
                "mode": "thumb",
                "name": "second",
                "resume": False,
                "owner_start": 0x02008010,
                "owner_end": 0x02008018,
                "note": "synthetic entry",
            },
        ]

        text = render_overlay_toml(
            overlay_id="rom_test",
            start=0x02008000,
            size=0x18,
            sha1="1" * 40,
            sha256="2" * 64,
            seeds=seeds,
            data_ranges=[(0x02008008, 0x0200800C)],
        )

        self.assertIn("start = 0x02008000\nend = 0x02008008", text)
        self.assertIn("start = 0x0200800c\nend = 0x02008010", text)
        self.assertIn("start = 0x02008010\nend = 0x02008018", text)
        self.assertNotIn("start = 0x02008008\nend = 0x0200800c\nmode", text)

    def test_proactive_cli_needs_no_observation_file(self) -> None:
        binary = bytes(12)
        binary_sha256 = hashlib.sha256(binary).hexdigest()
        base = {
            "binary_id": "synthetic_overlay",
            "overlay_id": "rom_000100",
            "section": ".text",
            "evidence_source": "synthetic",
            "evidence_revision": "a" * 40,
            "confidence": "proven",
        }
        corpus = {
            "schema_version": 1,
            "symbols": [
                {
                    **base,
                    "name": "synthetic_arm",
                    "runtime_address": "0x03000000",
                    "source_address": "0x03000000",
                    "size": 4,
                    "mode": "arm",
                },
                {
                    **base,
                    "name": "synthetic_thumb",
                    "runtime_address": "0x03000004",
                    "source_address": "0x03000004",
                    "size": 4,
                    "mode": "thumb",
                },
            ],
        }
        manifest = {
            "overlays": [{
                "overlay_id": "rom_000100",
                "source": {"decompressed_sha256": binary_sha256},
                "runtime": {"start": "0x03000000", "loaded_size": len(binary)},
            }],
        }

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary_path = root / "orig.bin"
            elf_path = root / "overlay.elf"
            manifest_path = root / "manifest.json"
            corpus_path = root / "corpus.json"
            output_path = root / "overlay.toml"
            binary_path.write_bytes(binary)
            elf_path.write_bytes(build_synthetic_elf(physical_address=0x03000000))
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            corpus_path.write_text(json.dumps(corpus), encoding="utf-8")
            argv = [
                "build_overlay_toml.py",
                "--overlay-id", "rom_000100",
                "--binary", str(binary_path),
                "--elf", str(elf_path),
                "--manifest", str(manifest_path),
                "--corpus", str(corpus_path),
                "--proactive",
                "--output", str(output_path),
            ]
            with mock.patch.object(sys, "argv", argv):
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(build_overlay_toml.main(), 0)

            text = output_path.read_text(encoding="utf-8")
            self.assertIn("name = \"synthetic_arm\"", text)
            self.assertIn("name = \"synthetic_thumb\"", text)
            self.assertNotIn("resume = true", text)


if __name__ == "__main__":
    unittest.main()
