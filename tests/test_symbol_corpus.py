from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.validate_symbol_corpus import REQUIRED_FIELDS, validate_path


FIXTURES = ROOT / "tests" / "fixtures" / "symbols"
SCHEMA = ROOT / "symbols" / "schema-v1.json"
VALIDATOR = ROOT / "tools" / "validate_symbol_corpus.py"


class SymbolCorpusTests(unittest.TestCase):
    def test_schema_required_fields_match_validator(self) -> None:
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        required = set(schema["$defs"]["symbol"]["required"])
        self.assertEqual(required, REQUIRED_FIELDS)

    def test_valid_synthetic_fixtures(self) -> None:
        for path in sorted(FIXTURES.glob("valid_*.json")):
            with self.subTest(path=path.name):
                self.assertEqual(validate_path(path), [])

    def test_overlay_runtime_collision_is_partitioned(self) -> None:
        path = FIXTURES / "valid_overlay_collision.json"
        self.assertEqual(validate_path(path), [])

    def test_duplicate_runtime_address_is_rejected(self) -> None:
        errors = validate_path(FIXTURES / "invalid_duplicate_runtime.json")
        self.assertTrue(any("duplicates" in error for error in errors), errors)
        self.assertTrue(any("overlaps" in error for error in errors), errors)

    def test_unknown_instruction_mode_is_rejected(self) -> None:
        errors = validate_path(FIXTURES / "invalid_unknown_mode.json")
        self.assertTrue(any("expected 'arm' or 'thumb'" in error for error in errors))

    def test_cli_exit_status(self) -> None:
        valid = subprocess.run(
            [sys.executable, str(VALIDATOR), str(FIXTURES / "valid_main.json")],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        invalid = subprocess.run(
            [
                sys.executable,
                str(VALIDATOR),
                str(FIXTURES / "invalid_unknown_mode.json"),
            ],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(valid.returncode, 0, valid.stderr)
        self.assertEqual(invalid.returncode, 1, invalid.stdout)


if __name__ == "__main__":
    unittest.main()
