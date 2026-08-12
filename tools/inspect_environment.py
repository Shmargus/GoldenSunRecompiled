#!/usr/bin/env python3
"""Report project dependencies without modifying the environment."""

from __future__ import annotations

import platform
import shutil
import subprocess
import sys

TOOLS = [
    ("git", ["git", "--version"]),
    ("cmake", ["cmake", "--version"]),
    ("ninja", ["ninja", "--version"]),
    ("python", [sys.executable, "--version"]),
    ("arm-none-eabi-nm", ["arm-none-eabi-nm", "--version"]),
    ("arm-none-eabi-objdump", ["arm-none-eabi-objdump", "--version"]),
    ("arm-none-eabi-readelf", ["arm-none-eabi-readelf", "--version"]),
    ("make", ["make", "--version"]),
]


def first_line(command: list[str]) -> str:
    try:
        result = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return f"ERROR: {exc}"

    output = (result.stdout or result.stderr).strip().splitlines()
    return output[0] if output else f"exit {result.returncode}, no output"


def main() -> int:
    print(f"OS:      {platform.platform()}")
    print(f"Machine: {platform.machine()}")
    print(f"Python:  {sys.version.split()[0]}")
    print()

    missing_required: list[str] = []
    for name, command in TOOLS:
        executable = command[0]
        found = shutil.which(executable)
        status = first_line(command) if found else "MISSING"
        print(f"{name:24} {status}")
        if name in {"git", "cmake", "python"} and not found:
            missing_required.append(name)

    print()
    print("Notes:")
    print("- ARM binutils and make are needed for the Golden Sun disassembly workflow.")
    print("- A native C++ compiler is selected by CMake and may not appear above.")
    print("- Missing optional tools are blockers only for the phase that requires them.")

    if missing_required:
        print(f"ERROR: missing core tools: {', '.join(missing_required)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
