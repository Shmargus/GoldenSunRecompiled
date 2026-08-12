#!/usr/bin/env python3
"""Fail when obvious protected/private artifacts are present in the repository tree."""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_SUFFIXES = {
    ".gba", ".bin", ".rom", ".sav", ".srm", ".state", ".trace", ".dump",
    ".bios", ".wav", ".sf2", ".mid",
}
# These directories are explicitly private or generated workspace boundaries in
# .gitignore. Public CI never receives them, and local audits must not treat
# their user-owned evidence as candidate repository content.
IGNORED_DIRS = {".git", "build", "local", ".venv", "__pycache__"}
MAX_PUBLIC_FILE = 2 * 1024 * 1024


def main() -> int:
    problems: list[str] = []
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(ROOT)
        if any(part in IGNORED_DIRS for part in rel.parts):
            continue
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            problems.append(f"forbidden extension: {rel}")
        if path.stat().st_size > MAX_PUBLIC_FILE:
            problems.append(f"unexpectedly large public file: {rel} ({path.stat().st_size} bytes)")

    if problems:
        print("Public repository audit FAILED:", file=sys.stderr)
        for problem in problems:
            print(f"- {problem}", file=sys.stderr)
        return 1

    print("Public repository audit OK: no obvious protected/private artifacts found.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
