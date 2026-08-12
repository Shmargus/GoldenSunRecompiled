#!/usr/bin/env python3
"""Measure real static-recompilation coverage of the Golden Sun ROM, in bytes.

This answers one question honestly: of the ROM bytes that are actually
executable code (per the ELF's `$t`/`$a`/`$d` mapping symbols), what fraction
has the recompiler emitted a translation for?

It does NOT measure whether translated code has ever executed, and it does
NOT measure whether a translation is correct. "Translated" only means the
recompiler emitted a host function whose guest byte range covers that ROM
byte at least once.

Denominator (code bytes): derived from goldensun.elf's STT_NOTYPE `$t`/`$a`/`$d`
mapping symbols, restricted to the byte ranges that are actually physically
present in the 8 MiB ROM image. A byte is:
  - "code"    if the nearest preceding mapping symbol in its section is $t or $a
  - "data"    if the nearest preceding mapping symbol in its section is $d
  - "unknown" if there is no preceding mapping symbol, two mapping symbols of
              different kinds land on the same address (ambiguous), or the
              byte is not covered by any ELF section that is physically
              present in the ROM file (e.g. inter-section padding).
"unknown" is never folded into "code" or "data".

Numerator (translated bytes): the union of guest [start, end) ranges recorded
in the "/* 0xADDRESS  mode=...  end=0xEND ... */" header comments the
recompiler emits above every generated host function in
<generated-dir>/recompiled_*.cpp. Interior resume aliases
("/* alias 0x... -> ... */") and dispatch-table veneer entries do not carry
their own header comment and are therefore not double-counted: they are
additional entry points into bytes a header comment already covers. Ranges
are deduplicated by ROM byte address using a flat byte-map, so overlapping or
re-emitted ranges count each byte at most once.

Runtime-address handling:
  - Addresses in [0x08000000, 0x08800000) are main-ROM-resident: mapped
    directly to a ROM byte offset.
  - The ELF has exactly one PT_LOAD segment whose p_vaddr differs from its
    p_paddr (the IWRAM shadow copy the ROM ships so it can be memcpy'd to
    IWRAM at startup). Addresses inside that segment's runtime window are
    mapped back to their ROM-resident byte offset via p_paddr; those bytes
    ARE linearly present in the ROM file, just not at their runtime address.
  - Any other runtime address (other IWRAM/EWRAM addresses reached only via
    the transient/relocatable-function machinery) is NOT known to be linearly
    present anywhere in the static 8 MiB ROM image -- those functions may be
    materialized from compressed or dynamically-assembled overlay payloads
    the recompiler does not have a proven ROM byte range for. Those bytes are
    excluded from both the numerator and the denominator and are reported
    separately, with an explicit note that this means the headline
    percentage UNDERSTATES total translated code, not overstates it.

Usage:
    python tools/measure_coverage.py --elf goldensun.elf --generated-dir local/gs011/main
"""

from __future__ import annotations

import argparse
from bisect import bisect_right
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Any

if __package__:
    from .import_main_symbols import STT_FUNC, _mapping_symbols, parse_elf32_arm
    from .verify_rom import EXPECTED_SHA1, EXPECTED_SIZE, sha1_file
else:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from import_main_symbols import STT_FUNC, _mapping_symbols, parse_elf32_arm
    from verify_rom import EXPECTED_SHA1, EXPECTED_SIZE, sha1_file


ROM_BASE = 0x08000000
ROM_SIZE = EXPECTED_SIZE  # 8 MiB
SHF_ALLOC = 0x2
SHT_NOBITS = 8
PT_LOAD = 1

STATE_UNKNOWN = 0
STATE_CODE = 1
STATE_DATA = 2
STATE_AMBIGUOUS_MAPPING = 3  # two different mapping-symbol kinds at one address; folds into "unknown"

NAMED_SECTIONS = (
    "rom_c0",
    "rom_1b70",
    "rom_9000",
    "rom_15000",
    "rom_77000",
    "rom_8a000",
    "rom_a1000",
    "rom_c9000",
)

GENERATED_FUNCTION_RE = re.compile(
    r"^/\* 0x([0-9A-Fa-f]{8})  mode=(arm|thumb)  "
    r"end=0x([0-9A-Fa-f]{8})  branches=(\d+)(?:  indirect)? \*/$"
)


class CoverageError(ValueError):
    pass


@dataclass(frozen=True)
class RomSection:
    index: int
    name: str
    address: int  # runtime vaddr of section start
    size: int
    rom_offset: int  # start offset in the 8 MiB ROM file this section's bytes occupy


@dataclass(frozen=True)
class ShadowWindow:
    """A PT_LOAD segment whose runtime address differs from its ROM residency."""

    vaddr_start: int
    length: int  # bytes physically present (p_filesz)
    rom_offset: int  # start offset in the 8 MiB ROM this window's bytes occupy


# ---------------------------------------------------------------------------
# ELF -> ROM-offset mapping
# ---------------------------------------------------------------------------


def _segment_containing_file_range(image, file_offset: int, size: int):
    for segment in image.program_headers:
        if segment.header_type != PT_LOAD:
            continue
        if (
            segment.offset <= file_offset
            and file_offset + size <= segment.offset + segment.file_size
        ):
            return segment
    return None


def build_rom_sections(image) -> tuple[list[RomSection], list[ShadowWindow]]:
    """Map every physically-present ALLOC section to its 8 MiB ROM byte offset.

    Uses each section's containing PT_LOAD segment's p_paddr (not p_vaddr) so
    that a segment shipped at one runtime address but physically resident at
    a different ROM address (the IWRAM shadow copy) is placed correctly.
    """

    sections: list[RomSection] = []
    unmapped: list[str] = []
    for section in image.sections:
        if not (section.flags & SHF_ALLOC):
            continue
        if section.section_type == SHT_NOBITS:
            continue
        if section.size == 0:
            continue
        segment = _segment_containing_file_range(image, section.offset, section.size)
        if segment is None:
            unmapped.append(section.name)
            continue
        rom_offset = (
            segment.physical_address
            + (section.offset - segment.offset)
            - ROM_BASE
        )
        if rom_offset < 0 or rom_offset + section.size > ROM_SIZE:
            unmapped.append(section.name)
            continue
        sections.append(
            RomSection(
                index=section.index,
                name=section.name,
                address=section.address,
                size=section.size,
                rom_offset=rom_offset,
            )
        )
    if unmapped:
        raise CoverageError(
            "ELF section(s) not mappable into the 8 MiB ROM byte range: "
            + ", ".join(sorted(unmapped))
        )

    shadow_windows: list[ShadowWindow] = []
    for segment in image.program_headers:
        if segment.header_type != PT_LOAD:
            continue
        if segment.virtual_address == segment.physical_address:
            continue
        rom_offset = segment.physical_address - ROM_BASE
        if rom_offset < 0 or rom_offset + segment.file_size > ROM_SIZE:
            continue
        shadow_windows.append(
            ShadowWindow(
                vaddr_start=segment.virtual_address,
                length=segment.file_size,
                rom_offset=rom_offset,
            )
        )
    return sections, shadow_windows


def classify_code_data(
    image, sections: list[RomSection]
) -> tuple[bytearray, list[tuple[str, int]]]:
    """Build an 8 MiB byte-state map: 0 unknown / 1 code / 2 data (3 -> folds to unknown).

    Returns (states, out_of_section_mapping_symbols). The latter lists mapping
    symbols whose value falls outside the [address, address+size) range of the
    section they are indexed under -- observed in this ELF (~1.5% of mapping
    symbols, all with the same 0x10xxxxxx high byte, concentrated in a handful
    of sections). Their meaning is not established by any evidence available to
    this tool, so they are excluded from byte classification entirely rather
    than guessed at, and reported so a human can investigate what they encode
    (a plausible guess is a synthetic address the disassembly toolchain uses to
    disambiguate multiple relocation targets of the same transient function,
    matching this ELF's many `transient-func-*` variants, but that is
    unconfirmed -- TODO-EVIDENCE).
    """

    states = bytearray(ROM_SIZE)  # defaults to STATE_UNKNOWN everywhere
    mapping_by_section = _mapping_symbols(image)
    out_of_range: list[tuple[str, int]] = []
    for section in sections:
        entries = mapping_by_section.get(section.index)
        if not entries:
            continue  # entire section stays "unknown": no mapping symbols at all
        by_address: dict[int, set[str]] = {}
        for address, kind, _symbol_index in entries:
            if not (section.address <= address < section.address + section.size):
                out_of_range.append((section.name, address))
                continue
            by_address.setdefault(address, set()).add(kind)
        if not by_address:
            continue
        addresses = sorted(by_address)
        state_at = []
        for address in addresses:
            kinds = by_address[address]
            if len(kinds) != 1:
                state_at.append(STATE_AMBIGUOUS_MAPPING)
            else:
                kind = next(iter(kinds))
                state_at.append({"a": STATE_CODE, "t": STATE_CODE, "d": STATE_DATA}[kind])
        for i, address in enumerate(addresses):
            run_start = address
            run_end = addresses[i + 1] if i + 1 < len(addresses) else section.address + section.size
            state = state_at[i]
            if state == STATE_AMBIGUOUS_MAPPING:
                continue  # leave as STATE_UNKNOWN (0); ambiguous is not guessed
            rom_start = section.rom_offset + (run_start - section.address)
            rom_end = section.rom_offset + (run_end - section.address)
            if rom_end > rom_start:
                states[rom_start:rom_end] = bytes([state]) * (rom_end - rom_start)
    return states, out_of_range


# ---------------------------------------------------------------------------
# Corpus (translated-bytes) parsing
# ---------------------------------------------------------------------------


def address_to_rom_offset(
    address: int, shadow_windows: list[ShadowWindow]
) -> int | None:
    if ROM_BASE <= address < ROM_BASE + ROM_SIZE:
        return address - ROM_BASE
    for window in shadow_windows:
        if window.vaddr_start <= address < window.vaddr_start + window.length:
            return window.rom_offset + (address - window.vaddr_start)
    return None


def map_translated_intervals(
    intervals: list[tuple[int, int, str]], shadow_windows: list[ShadowWindow]
) -> tuple[bytearray, list[tuple[int, int, str]], list[str], int]:
    """Fold generated-function guest ranges into a deduplicated ROM byte-map.

    Each byte in the returned bitmap is set at most once regardless of how
    many overlapping/aliased intervals cover it (slice assignment simply
    re-writes the same 1). Returns (translated_bitmap, transient_intervals,
    anomalies, rom_mapped_function_count).
    """

    translated = bytearray(ROM_SIZE)
    transient_intervals: list[tuple[int, int, str]] = []
    anomalies: list[str] = []
    rom_mapped_functions = 0
    for address, end, mode in intervals:
        start_off = address_to_rom_offset(address, shadow_windows)
        end_off = address_to_rom_offset(end - 1, shadow_windows)
        if start_off is None:
            transient_intervals.append((address, end, mode))
            continue
        if end_off is None or end_off - start_off != (end - address) - 1:
            anomalies.append(
                f"0x{address:08x}..0x{end:08x} ({mode}) starts in a ROM-mapped window "
                "but does not stay within a single mapped window; excluded"
            )
            continue
        length = end - address
        translated[start_off : start_off + length] = b"\x01" * length
        rom_mapped_functions += 1
    return translated, transient_intervals, anomalies, rom_mapped_functions


@dataclass
class CorpusReadResult:
    intervals: list[tuple[int, int, str]]  # (start_addr, end_addr, mode), raw runtime addrs
    files_read: list[tuple[str, int, float]]  # (name, size, mtime) at read time
    duplicate_keys: int


def read_generated_intervals(directory: Path) -> CorpusReadResult:
    paths = sorted(directory.glob("recompiled_*.cpp"))
    if not paths:
        single = directory / "recompiled.cpp"
        if single.is_file():
            paths = [single]
    if not paths:
        raise CoverageError(f"no generated recompiled C++ bodies found in {directory}")

    intervals: list[tuple[int, int, str]] = []
    seen: set[tuple[int, str]] = set()
    duplicate_keys = 0
    files_read: list[tuple[str, int, float]] = []
    for path in paths:
        stat_before = path.stat()
        with path.open("r", encoding="utf-8") as handle:
            for line in handle:
                if not line.startswith("/* 0x"):
                    continue
                match = GENERATED_FUNCTION_RE.match(line.rstrip("\n"))
                if match is None:
                    continue
                address = int(match.group(1), 16)
                mode = match.group(2)
                end = int(match.group(3), 16)
                key = (address, mode)
                if key in seen:
                    duplicate_keys += 1
                    continue
                if end <= address:
                    continue
                seen.add(key)
                intervals.append((address, end, mode))
        files_read.append((path.name, stat_before.st_size, stat_before.st_mtime))
    return CorpusReadResult(intervals=intervals, files_read=files_read, duplicate_keys=duplicate_keys)


def corpus_is_stable(directory: Path, snapshot: list[tuple[str, int, float]]) -> bool:
    paths = sorted(directory.glob("recompiled_*.cpp"))
    current = [(p.name, p.stat().st_size, p.stat().st_mtime) for p in paths]
    return current == snapshot


# ---------------------------------------------------------------------------
# Reporting helpers
# ---------------------------------------------------------------------------


def find_runs(flags: bytes, value: int) -> list[tuple[int, int]]:
    """Return (start, end) exclusive ranges of contiguous bytes equal to `value`."""

    target = bytes([value])
    runs: list[tuple[int, int]] = []
    pattern = re.compile(re.escape(target) + b"+")
    for match in pattern.finditer(flags):
        runs.append((match.start(), match.end()))
    return runs


def section_for_rom_offset(offset: int, sections: list[RomSection]) -> RomSection | None:
    for section in sections:
        if section.rom_offset <= offset < section.rom_offset + section.size:
            return section
    return None


def build_symbol_index(image) -> tuple[list[int], list[tuple[int, str]]]:
    entries = sorted(
        (symbol.value & ~1, symbol.size, symbol.name)
        for symbol in image.symbols
        if symbol.symbol_type == STT_FUNC and symbol.size > 0
    )
    starts = [e[0] for e in entries]
    rest = [(e[1], e[2]) for e in entries]
    return starts, rest


def containing_symbol(
    vaddr: int, starts: list[int], rest: list[tuple[int, str]]
) -> str | None:
    index = bisect_right(starts, vaddr) - 1
    if index < 0:
        return None
    size, name = rest[index]
    start = starts[index]
    if start <= vaddr < start + size:
        return name
    return None


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Measure real static-recompilation coverage of the Golden Sun ROM, "
            "in bytes of code (translated vs. total code bytes per the ELF's "
            "mapping symbols)."
        )
    )
    parser.add_argument("--elf", required=True, type=Path, help="Path to goldensun.elf")
    parser.add_argument(
        "--generated-dir",
        required=True,
        type=Path,
        help="Directory containing recompiled_*.cpp (the emitted corpus)",
    )
    parser.add_argument(
        "--rom",
        type=Path,
        default=None,
        help="Optional path to the Golden Sun ROM, verified by hash only (no ROM bytes are read for coverage math)",
    )
    parser.add_argument("--top-n", type=int, default=20, help="Number of largest untranslated runs to report")
    parser.add_argument("--output", type=Path, default=None, help="Optional path to write the full JSON report")
    args = parser.parse_args()

    if args.rom is not None:
        rom_path = args.rom.expanduser().resolve()
        if not rom_path.is_file():
            print(f"ERROR: ROM file not found: {rom_path}", file=sys.stderr)
            return 2
        size = rom_path.stat().st_size
        digest = sha1_file(rom_path)
        if size != EXPECTED_SIZE or digest.lower() != EXPECTED_SHA1:
            print("ERROR: unsupported ROM revision; refusing to proceed", file=sys.stderr)
            return 1
        print(f"ROM verified: {rom_path} sha1={digest}")

    elf_path = args.elf.expanduser().resolve()
    if not elf_path.is_file():
        print(f"ERROR: ELF not found: {elf_path}", file=sys.stderr)
        return 2
    elf_bytes = elf_path.read_bytes()
    elf_sha256 = hashlib.sha256(elf_bytes).hexdigest()
    image = parse_elf32_arm(elf_bytes)

    sections, shadow_windows = build_rom_sections(image)
    sections_by_name = {s.name: s for s in sections}
    missing_named = [name for name in NAMED_SECTIONS if name not in sections_by_name]
    if missing_named:
        print(
            "ERROR: expected ELF section(s) not present: " + ", ".join(missing_named),
            file=sys.stderr,
        )
        return 1

    states, out_of_range_mapping_symbols = classify_code_data(image, sections)
    assert len(states) == ROM_SIZE

    code_total = states.count(STATE_CODE)
    data_total = states.count(STATE_DATA)
    unknown_total = ROM_SIZE - code_total - data_total
    assert code_total + data_total + unknown_total == ROM_SIZE

    # --- Corpus read, with a stability check against the concurrently-building corpus. ---
    generated_dir = args.generated_dir.expanduser().resolve()
    first = read_generated_intervals(generated_dir)
    stable = corpus_is_stable(generated_dir, first.files_read)
    corpus_result = first
    reread_note = None
    if not stable:
        second = read_generated_intervals(generated_dir)
        if corpus_is_stable(generated_dir, second.files_read):
            corpus_result = second
            reread_note = (
                "corpus changed under us during the first read (concurrent build); "
                "re-read once and it was stable on the second pass"
            )
        else:
            reread_note = (
                "corpus changed under us during BOTH read attempts (concurrent build "
                "still in progress); using the second read's contents anyway -- "
                "treat the translated-byte figures below as approximate / racy, not final"
            )
            corpus_result = second

    translated, transient_intervals, anomalies, rom_mapped_functions = map_translated_intervals(
        corpus_result.intervals, shadow_windows
    )

    translated_total = translated.count(1)
    code_bytes = bytes(states)

    # code_only[i] == 1 iff byte i is classified as code; use a translate table
    # (C-level) rather than a Python-level loop over 8 MiB.
    translate_table_code = bytes(1 if i == STATE_CODE else 0 for i in range(256))
    code_only = code_bytes.translate(translate_table_code)

    # Overlap (code AND translated) via big-integer AND, again to avoid an
    # 8-million-iteration Python loop.
    code_int = int.from_bytes(code_only, "big")
    translated_int = int.from_bytes(bytes(translated), "big")
    overlap_int = code_int & translated_int
    overlap_bytes = overlap_int.to_bytes(ROM_SIZE, "big")
    code_and_translated = sum(overlap_bytes)  # each byte is 0 or 1

    # --- Untranslated code = code bytes with no translation. ---
    untranslated_code_int = code_int & ~translated_int & ((1 << (ROM_SIZE * 8)) - 1)
    untranslated_code_bytes = untranslated_code_int.to_bytes(ROM_SIZE, "big")

    runs = find_runs(untranslated_code_bytes, 1)
    starts, rest = build_symbol_index(image)
    run_records = []
    for start, end in runs:
        section = section_for_rom_offset(start, sections)
        if section is not None:
            vaddr = section.address + (start - section.rom_offset)
        else:
            vaddr = ROM_BASE + start
        sym = containing_symbol(vaddr, starts, rest)
        run_records.append(
            {
                "rom_offset": f"0x{start:06x}",
                "runtime_address": f"0x{vaddr:08x}",
                "size": end - start,
                "symbol": sym,
            }
        )
    run_records.sort(key=lambda r: (-r["size"], r["rom_offset"]))
    top_runs = run_records[: args.top_n]

    # --- Per-section breakdown. ---
    translated_bytes_flat = bytes(translated)

    def range_stats(rom_start: int, rom_end: int) -> dict[str, int]:
        seg_code_bytes = code_only[rom_start:rom_end]
        seg_translated_bytes = translated_bytes_flat[rom_start:rom_end]
        c = sum(seg_code_bytes)
        seg_overlap = int.from_bytes(seg_code_bytes, "big") & int.from_bytes(seg_translated_bytes, "big")
        t = sum(seg_overlap.to_bytes(rom_end - rom_start, "big")) if rom_end > rom_start else 0
        return {"code_bytes": c, "translated_bytes": t}

    section_report = {}
    named_offsets_covered = 0
    for name in NAMED_SECTIONS:
        section = sections_by_name[name]
        stats = range_stats(section.rom_offset, section.rom_offset + section.size)
        pct = (100.0 * stats["translated_bytes"] / stats["code_bytes"]) if stats["code_bytes"] else None
        section_report[name] = {
            "runtime_address": f"0x{section.address:08x}",
            "size": section.size,
            "code_bytes": stats["code_bytes"],
            "translated_bytes": stats["translated_bytes"],
            "percent_of_code_translated": pct,
        }
        named_offsets_covered += section.size

    # "the rest": every ROM byte not in one of the named sections above.
    named_ranges = sorted(
        (sections_by_name[n].rom_offset, sections_by_name[n].rom_offset + sections_by_name[n].size)
        for n in NAMED_SECTIONS
    )
    rest_code = 0
    rest_translated = 0
    cursor = 0
    for rs, re_ in named_ranges + [(ROM_SIZE, ROM_SIZE)]:
        if cursor < rs:
            stats = range_stats(cursor, rs)
            rest_code += stats["code_bytes"]
            rest_translated += stats["translated_bytes"]
        cursor = max(cursor, re_)
    rest_pct = (100.0 * rest_translated / rest_code) if rest_code else None
    section_report["__rest__"] = {
        "size": ROM_SIZE - named_offsets_covered,
        "code_bytes": rest_code,
        "translated_bytes": rest_translated,
        "percent_of_code_translated": rest_pct,
    }

    overall_pct = (100.0 * code_and_translated / code_total) if code_total else None

    # --- Transient (non-ROM-resident) translated functions: informational only. ---
    transient_addr_set = sorted({(a, m) for a, _e, m in transient_intervals})
    transient_bytes_sum = sum(e - a for a, e, _m in transient_intervals)

    report: dict[str, Any] = {
        "schema_version": 1,
        "elf_path": str(elf_path),
        "elf_sha256": elf_sha256,
        "generated_dir": str(generated_dir),
        "corpus_files_read": len(corpus_result.files_read),
        "corpus_reread_note": reread_note,
        "corpus_duplicate_entry_point_keys_skipped": corpus_result.duplicate_keys,
        "rom_size": ROM_SIZE,
        "denominator": {
            "code_bytes": code_total,
            "data_bytes": data_total,
            "unknown_bytes": unknown_total,
        },
        "out_of_section_mapping_symbols": {
            "count": len(out_of_range_mapping_symbols),
            "note": (
                "Mapping symbols ($t/$a/$d) whose value lies outside the "
                "[address, address+size) range of the ELF section they are "
                "indexed under. Excluded from classification entirely (not "
                "guessed into code/data/unknown at any address) -- see "
                "classify_code_data() docstring. TODO-EVIDENCE: their meaning "
                "is not established."
            ),
            "by_section": sorted(
                {name for name, _addr in out_of_range_mapping_symbols}
            ),
        },
        "numerator": {
            "rom_mapped_translated_function_entries": rom_mapped_functions,
            "translated_bytes_total_rom_mapped": translated_total,
            "code_and_translated_bytes": code_and_translated,
        },
        "overall_percent_of_code_translated": overall_pct,
        "sections": section_report,
        "top_untranslated_runs": top_runs,
        "transient_non_rom_resident": {
            "distinct_entries": len(transient_addr_set),
            "raw_byte_sum_not_deduplicated_against_rom": transient_bytes_sum,
            "note": (
                "These are translated functions whose runtime address is not "
                "linearly present anywhere in the static 8 MiB ROM image (only "
                "the single proven IWRAM shadow window is treated as ROM-resident). "
                "They are excluded from both numerator and denominator above. "
                "This means the headline percentage UNDERSTATES total translated "
                "code: this many additional bytes of guest code are translated "
                "but cannot be attributed to a proven ROM byte range."
            ),
        },
        "anomalies": anomalies,
        "caveats": [
            "This measures TRANSLATED coverage only: whether the recompiler emitted "
            "a host function covering a byte. It does NOT mean that byte's code has "
            "ever executed (EXERCISED), and it does NOT mean the translation is "
            "correct (VERIFIED).",
            "Compressed/dynamically-assembled overlay payloads are not linearly "
            "present in the ROM and are excluded from this measurement entirely "
            "(not counted as code, not counted as data, not counted as translated).",
            "'unknown' ROM bytes (no controlling ELF mapping symbol, or two "
            "different mapping-symbol kinds at the same address) are never folded "
            "into 'code' or 'data'.",
        ],
    }

    print(f"ELF: {elf_path}  sha256={elf_sha256}")
    print(f"Generated corpus: {generated_dir}  ({len(corpus_result.files_read)} files)")
    if reread_note:
        print(f"NOTE: {reread_note}")
    print()
    print(f"ROM size:            {ROM_SIZE} bytes (8 MiB)")
    print(f"Code bytes (denom):  {code_total}")
    print(f"Data bytes:          {data_total}")
    print(f"Unknown bytes:       {unknown_total}")
    if out_of_range_mapping_symbols:
        print(
            f"NOTE: {len(out_of_range_mapping_symbols)} mapping symbols excluded "
            f"(value outside their own section's range; sections: "
            f"{sorted({n for n, _a in out_of_range_mapping_symbols})}) -- see report for detail"
        )
    print()
    print(f"Translated function entries mapped into ROM: {rom_mapped_functions}")
    print(f"Code bytes translated (numerator):  {code_and_translated}")
    if overall_pct is not None:
        print(f"OVERALL: {code_and_translated} / {code_total} code bytes translated = {overall_pct:.2f}%")
    print()
    print("Per-section breakdown:")
    for name in NAMED_SECTIONS:
        s = section_report[name]
        pct_str = f"{s['percent_of_code_translated']:.2f}%" if s["percent_of_code_translated"] is not None else "n/a"
        print(f"  {name:12s} code={s['code_bytes']:7d} translated={s['translated_bytes']:7d} ({pct_str})")
    rest = section_report["__rest__"]
    rest_pct_str = f"{rest['percent_of_code_translated']:.2f}%" if rest["percent_of_code_translated"] is not None else "n/a"
    print(f"  {'(rest)':12s} code={rest['code_bytes']:7d} translated={rest['translated_bytes']:7d} ({rest_pct_str})")
    print()
    print(f"Transient (non-ROM-resident) translated entries: {len(transient_addr_set)} "
          f"(~{transient_bytes_sum} guest bytes, NOT in the numerator or denominator above)")
    if anomalies:
        print(f"\nAnomalies ({len(anomalies)}):")
        for a in anomalies[:20]:
            print(f"  {a}")
    print(f"\nTop {len(top_runs)} largest untranslated contiguous code runs:")
    for r in top_runs:
        sym = r["symbol"] or "(no containing STT_FUNC)"
        print(f"  {r['runtime_address']}  size={r['size']:6d}  {sym}")

    if args.output is not None:
        payload = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
        print(f"\nFull report written to {args.output}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
