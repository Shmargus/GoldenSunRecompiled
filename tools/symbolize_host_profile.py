#!/usr/bin/env python3
"""Turn a GBARECOMP_HOST_PROF sample dump into a ranked host-function profile.

The runtime samples the emulation thread's instruction pointer ~1 kHz and
writes "0x<rip> <count>" lines plus the module base it was loaded at. This
maps those runtime addresses back to link-time addresses and attributes each
to the enclosing symbol, using `nm` on the executable.

Why this exists: the MinGW build emits DWARF, and Windows Performance Analyzer
wants PDB, so external profilers show a 29,000-function binary as raw
addresses. The executable itself keeps ~83k symbols, so `nm` is all we need.

Usage:
  python tools/symbolize_host_profile.py <profile.txt> <GoldenSunRecomp.exe>
                                         [--top N] [--group]

  --top N   how many rows to print (default 40)
  --group   collapse the generated corpus (gf_*) into one row, so runtime
            overhead stands out from translated game code
"""
from __future__ import annotations

import argparse
import bisect
import re
import subprocess
import sys
from pathlib import Path


def load_samples(path: Path) -> tuple[int, int, list[tuple[int, int]]]:
    base = 0
    total = 0
    samples: list[tuple[int, int]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("# module_base"):
            base = int(line.split()[-1], 16)
            continue
        if line.startswith("# total_samples"):
            total = int(line.split()[-1])
            continue
        if not line or line.startswith("#"):
            continue
        rip_s, count_s = line.split()
        samples.append((int(rip_s, 16), int(count_s)))
    if not samples:
        raise SystemExit(f"no samples in {path}")
    return base, total, samples


def load_symbols(exe: Path) -> tuple[list[int], list[str]]:
    """Sorted (address, name) from nm. Addresses are LINK-TIME."""
    out = subprocess.run(
        ["nm", "--numeric-sort", "--defined-only", str(exe)],
        capture_output=True, text=True, errors="replace",
    )
    if out.returncode != 0:
        raise SystemExit(f"nm failed on {exe}:\n{out.stderr[:400]}")
    addrs: list[int] = []
    names: list[str] = []
    pat = re.compile(r"^([0-9a-fA-F]+)\s+[tTwW]\s+(\S+)")
    for line in out.stdout.splitlines():
        m = pat.match(line)
        if not m:
            continue
        addrs.append(int(m.group(1), 16))
        names.append(m.group(2))
    if not addrs:
        raise SystemExit("nm returned no text symbols — is this the right exe?")
    return addrs, names


def preferred_image_base(exe: Path) -> int:
    """The base the linker assumed, so runtime RIPs can be de-relocated."""
    out = subprocess.run(["objdump", "-p", str(exe)],
                         capture_output=True, text=True, errors="replace")
    for line in out.stdout.splitlines():
        if "ImageBase" in line:
            return int(line.split()[-1], 16)
    return 0x140000000  # typical mingw x86-64 default; only a fallback


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("profile", type=Path)
    ap.add_argument("exe", type=Path)
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--group", action="store_true")
    args = ap.parse_args()

    run_base, total, samples = load_samples(args.profile)
    addrs, names = load_symbols(args.exe)
    link_base = preferred_image_base(args.exe)
    slide = run_base - link_base

    if total == 0:
        total = sum(c for _, c in samples)

    by_symbol: dict[str, int] = {}
    unresolved = 0
    for rip, count in samples:
        link_addr = rip - slide
        i = bisect.bisect_right(addrs, link_addr) - 1
        if i < 0:
            unresolved += count
            continue
        by_symbol[names[i]] = by_symbol.get(names[i], 0) + count

    if args.group:
        grouped: dict[str, int] = {}
        for name, count in by_symbol.items():
            key = "<generated game code (gf_*)>" if name.startswith(
                ("gf_", "gsr_")) else name
            grouped[key] = grouped.get(key, 0) + count
        by_symbol = grouped

    ranked = sorted(by_symbol.items(), key=lambda kv: -kv[1])
    print(f"host profile: {total} samples, module_base=0x{run_base:x}, "
          f"link_base=0x{link_base:x}, slide=0x{slide:x}")
    if unresolved:
        print(f"  WARNING {unresolved} samples ({100.0*unresolved/total:.1f}%) "
              f"below the lowest symbol — slide may be wrong")
    print()
    print(f"{'share':>7}  {'samples':>9}  symbol")
    for name, count in ranked[: args.top]:
        print(f"{100.0*count/total:6.2f}%  {count:9d}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
