#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 --rom PATH --gbarecomp PATH --disasm PATH" >&2
}

ROM=""
GBARECOMP=""
DISASM=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --rom) ROM="$2"; shift 2 ;;
    --gbarecomp) GBARECOMP="$2"; shift 2 ;;
    --disasm) DISASM="$2"; shift 2 ;;
    *) usage; exit 2 ;;
  esac
done

[[ -n "$ROM" && -n "$GBARECOMP" && -n "$DISASM" ]] || { usage; exit 2; }
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 "$ROOT/tools/inspect_environment.py"
python3 "$ROOT/tools/verify_rom.py" "$ROM"
[[ -f "$GBARECOMP/CMakeLists.txt" ]] || { echo "Missing gbarecomp checkout" >&2; exit 1; }
[[ -f "$DISASM/Makefile" ]] || { echo "Missing Golden Sun disassembly checkout" >&2; exit 1; }

echo "OK: paths and exact ROM are valid."
echo "Next: pin upstream commits in UPSTREAM.md and complete GS-001/GS-002."
