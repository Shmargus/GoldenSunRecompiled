from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

from test_main_symbol_import import build_synthetic_elf
from tools.compare_gbarecomp_discovery import (
    compare_discovery,
    parse_generated_functions,
)
from tools.import_main_symbols import import_symbols, parse_elf32_arm


class DiscoveryComparisonTests(unittest.TestCase):
    def test_parses_sharded_generated_function_comments(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "recompiled_000.cpp").write_text(
                "/* 0x03000000  mode=arm  end=0x03000004  branches=1 */\n",
                encoding="utf-8",
            )
            (root / "recompiled_001.cpp").write_text(
                "/* 0x03000004  mode=thumb  end=0x03000008  branches=0  indirect */\n",
                encoding="utf-8",
            )
            records = parse_generated_functions(root)
        self.assertEqual(len(records), 2)
        self.assertFalse(records[0]["has_indirect"])
        self.assertTrue(records[1]["has_indirect"])

    def test_classifies_exact_interior_and_data_entries(self) -> None:
        image = parse_elf32_arm(build_synthetic_elf())
        imported, _report = import_symbols(
            image,
            binary_id="synthetic_main",
            evidence_source="synthetic.elf",
            evidence_revision="0" * 40,
        )
        scanner = [
            {
                "runtime_address": "0x03000000",
                "mode": "arm",
                "end_address": "0x03000004",
                "branch_count": 0,
                "has_indirect": False,
            },
            {
                "runtime_address": "0x03000004",
                "mode": "thumb",
                "end_address": "0x03000008",
                "branch_count": 0,
                "has_indirect": False,
            },
            {
                "runtime_address": "0x03000006",
                "mode": "thumb",
                "end_address": "0x03000008",
                "branch_count": 0,
                "has_indirect": False,
            },
            {
                "runtime_address": "0x03000008",
                "mode": "thumb",
                "end_address": "0x0300000a",
                "branch_count": 0,
                "has_indirect": False,
            },
        ]
        result = compare_discovery(scanner, imported, image)
        self.assertEqual(result["exact_entry_mode_matches"], 2)
        self.assertEqual(
            result["scanner_only_counts"],
            {
                "scanner_entry_in_elf_data": 1,
                "scanner_interior_entry_in_imported_extent": 1,
            },
        )
        self.assertEqual(result["imported_only"], [])


if __name__ == "__main__":
    unittest.main()
