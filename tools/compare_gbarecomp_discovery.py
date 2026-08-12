#!/usr/bin/env python3
"""Run and classify the conservative GS-006 gbarecomp discovery baseline."""

from __future__ import annotations

import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tomllib
from typing import Any

if __package__:
    from .import_main_symbols import (
        SHF_EXECINSTR,
        _mapping_symbols,
        parse_elf32_arm,
    )
    from .validate_symbol_corpus import validate_document
    from .verify_rom import EXPECTED_SHA1, EXPECTED_SIZE, sha1_file
else:
    from import_main_symbols import SHF_EXECINSTR, _mapping_symbols, parse_elf32_arm
    from validate_symbol_corpus import validate_document
    from verify_rom import EXPECTED_SHA1, EXPECTED_SIZE, sha1_file


GENERATED_FUNCTION_RE = re.compile(
    r"^/\* 0x([0-9A-Fa-f]{8})  mode=(arm|thumb)  "
    r"end=0x([0-9A-Fa-f]{8})  branches=(\d+)(?:  indirect)? \*/$"
)
DISCOVERY_RE = re.compile(
    r"==> discovered (\d+) functions \(arm=(\d+) thumb=(\d+) "
    r"indirect=(\d+) undefined=(\d+) branch_targets=(\d+)\)"
)
REVISION_RE = re.compile(r"^[0-9a-f]{40}$")
ROM_BASE = 0x08000000


def _sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _parse_key_values(text: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key.strip()] = value.strip()
    return result


def parse_generated_functions(directory: Path) -> list[dict[str, Any]]:
    paths = sorted(directory.glob("recompiled_*.cpp"))
    if not paths:
        single = directory / "recompiled.cpp"
        if single.is_file():
            paths = [single]
    if not paths:
        raise ValueError(f"no generated recompiled C++ bodies found in {directory}")
    records: list[dict[str, Any]] = []
    seen: set[tuple[int, str]] = set()
    for path in paths:
        for line in path.read_text(encoding="utf-8").splitlines():
            match = GENERATED_FUNCTION_RE.fullmatch(line)
            if match is None:
                continue
            address = int(match.group(1), 16)
            mode = match.group(2)
            end = int(match.group(3), 16)
            key = (address, mode)
            if key in seen:
                raise ValueError(
                    f"duplicate generated function 0x{address:08x}/{mode}"
                )
            if end <= address:
                raise ValueError(
                    f"invalid generated extent 0x{address:08x}..0x{end:08x}"
                )
            seen.add(key)
            records.append(
                {
                    "runtime_address": f"0x{address:08x}",
                    "mode": mode,
                    "end_address": f"0x{end:08x}",
                    "branch_count": int(match.group(4)),
                    "has_indirect": line.endswith("  indirect */"),
                }
            )
    records.sort(key=lambda record: (int(record["runtime_address"], 16), record["mode"]))
    return records


def _mapping_lookup(image) -> tuple[list[Any], dict[int, tuple[list[int], list[str]]]]:
    sections = sorted(
        (
            section
            for section in image.sections
            if section.flags & SHF_EXECINSTR and section.size > 0
        ),
        key=lambda section: section.address,
    )
    mapping_data: dict[int, tuple[list[int], list[str]]] = {}
    for section_index, entries in _mapping_symbols(image).items():
        by_address: dict[int, set[str]] = defaultdict(set)
        for address, kind, _symbol_index in entries:
            by_address[address].add(kind)
        addresses = sorted(by_address)
        states = [
            next(iter(by_address[address])) if len(by_address[address]) == 1 else "unknown"
            for address in addresses
        ]
        mapping_data[section_index] = (addresses, states)
    return sections, mapping_data


def _mapping_state(
    address: int,
    sections: list[Any],
    mapping_data: dict[int, tuple[list[int], list[str]]],
) -> str:
    section = next(
        (
            candidate
            for candidate in sections
            if candidate.address <= address < candidate.address + candidate.size
        ),
        None,
    )
    if section is None:
        return "outside_executable_section"
    addresses, states = mapping_data.get(section.index, ([], []))
    index = bisect_right(addresses, address) - 1
    if index < 0:
        return "unknown"
    return {"a": "arm", "t": "thumb", "d": "data"}.get(states[index], "unknown")


def compare_discovery(
    scanner: list[dict[str, Any]],
    imported: dict[str, Any],
    elf_image,
) -> dict[str, Any]:
    errors = validate_document(imported)
    if errors:
        raise ValueError("imported corpus is invalid: " + "; ".join(errors))
    imported_symbols = imported["symbols"]
    scanner_by_key = {
        (int(record["runtime_address"], 16), record["mode"]): record
        for record in scanner
    }
    imported_by_key = {
        (int(record["runtime_address"], 16), record["mode"]): record
        for record in imported_symbols
    }
    scanner_by_address: dict[int, set[str]] = defaultdict(set)
    imported_by_address: dict[int, set[str]] = defaultdict(set)
    for address, mode in scanner_by_key:
        scanner_by_address[address].add(mode)
    for address, mode in imported_by_key:
        imported_by_address[address].add(mode)

    exact_keys = sorted(set(scanner_by_key) & set(imported_by_key))
    mode_conflict_addresses = sorted(
        address
        for address in set(scanner_by_address) & set(imported_by_address)
        if not (scanner_by_address[address] & imported_by_address[address])
    )
    mode_conflicts = [
        {
            "runtime_address": f"0x{address:08x}",
            "scanner_modes": sorted(scanner_by_address[address]),
            "imported_modes": sorted(imported_by_address[address]),
        }
        for address in mode_conflict_addresses
    ]

    extent_counts: Counter[str] = Counter()
    extent_mismatches: list[dict[str, Any]] = []
    for key in exact_keys:
        scan = scanner_by_key[key]
        symbol = imported_by_key[key]
        scan_end = int(scan["end_address"], 16)
        import_end = key[0] + symbol["size"]
        if scan_end == import_end:
            extent_class = "equal"
        elif scan_end < import_end:
            extent_class = "scanner_shorter"
        else:
            extent_class = "scanner_longer"
        extent_counts[extent_class] += 1
        if extent_class != "equal":
            extent_mismatches.append(
                {
                    "runtime_address": f"0x{key[0]:08x}",
                    "mode": key[1],
                    "name": symbol["name"],
                    "confidence": symbol["confidence"],
                    "scanner_end": scan["end_address"],
                    "imported_end": f"0x{import_end:08x}",
                    "classification": extent_class,
                }
            )

    interval_symbols = sorted(
        imported_symbols,
        key=lambda symbol: int(symbol["runtime_address"], 16),
    )
    interval_starts = [int(symbol["runtime_address"], 16) for symbol in interval_symbols]

    def containing_import(address: int) -> dict[str, Any] | None:
        index = bisect_right(interval_starts, address) - 1
        if index < 0:
            return None
        symbol = interval_symbols[index]
        start = interval_starts[index]
        return symbol if address < start + symbol["size"] else None

    sections, mapping_data = _mapping_lookup(elf_image)
    conflict_set = set(mode_conflict_addresses)
    scanner_only: list[dict[str, Any]] = []
    scanner_class_counts: Counter[str] = Counter()
    for key in sorted(set(scanner_by_key) - set(imported_by_key)):
        address, mode = key
        if address in conflict_set:
            continue
        state = _mapping_state(address, sections, mapping_data)
        container = containing_import(address)
        if state == "data":
            classification = "scanner_entry_in_elf_data"
        elif state in {"arm", "thumb"} and state != mode:
            classification = "scanner_mode_conflicts_elf_mapping"
        elif state == mode and container is not None:
            classification = "scanner_interior_entry_in_imported_extent"
        elif state == mode:
            classification = "scanner_code_without_stt_func"
        elif state == "outside_executable_section":
            classification = "scanner_outside_elf_executable_section"
        else:
            classification = "scanner_mapping_unknown"
        scanner_class_counts[classification] += 1
        detail: dict[str, Any] = {
            **scanner_by_key[key],
            "elf_mapping": state,
            "classification": classification,
        }
        if container is not None:
            detail["containing_import"] = {
                "name": container["name"],
                "runtime_address": container["runtime_address"],
                "size": container["size"],
                "confidence": container["confidence"],
            }
        scanner_only.append(detail)

    imported_only: list[dict[str, Any]] = []
    imported_class_counts: Counter[str] = Counter()
    for key in sorted(set(imported_by_key) - set(scanner_by_key)):
        address, _mode = key
        if address in conflict_set:
            continue
        symbol = imported_by_key[key]
        if not (ROM_BASE <= address < ROM_BASE + EXPECTED_SIZE):
            classification = "runtime_code_copy_not_scanned"
        elif symbol["confidence"] == "unresolved":
            classification = "unresolved_extent_not_reached"
        else:
            classification = "proven_function_not_reached"
        imported_class_counts[classification] += 1
        imported_only.append({**symbol, "classification": classification})

    return {
        "exact_entry_mode_matches": len(exact_keys),
        "extent_counts": dict(sorted(extent_counts.items())),
        "extent_mismatches": extent_mismatches,
        "mode_conflicts": mode_conflicts,
        "scanner_only_counts": dict(sorted(scanner_class_counts.items())),
        "scanner_only": scanner_only,
        "imported_only_counts": dict(sorted(imported_class_counts.items())),
        "imported_only": imported_only,
    }


def _run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--gba-scan", required=True, type=Path)
    parser.add_argument("--gba-recompile", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--generated-dir", required=True, type=Path)
    parser.add_argument("--main-corpus", required=True, type=Path)
    parser.add_argument("--main-report", required=True, type=Path)
    parser.add_argument("--main-elf", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--gbarecomp-revision", required=True)
    parser.add_argument("--disasm-revision", required=True)
    parser.add_argument("--max-functions", type=int, default=4096)
    args = parser.parse_args()
    if REVISION_RE.fullmatch(args.gbarecomp_revision) is None or REVISION_RE.fullmatch(
        args.disasm_revision
    ) is None:
        print("ERROR: revisions must be lowercase 40-hex commits", file=sys.stderr)
        return 2

    rom_path = args.rom.expanduser().resolve()
    if (
        not rom_path.is_file()
        or rom_path.stat().st_size != EXPECTED_SIZE
        or sha1_file(rom_path) != EXPECTED_SHA1
    ):
        print("ERROR: unsupported ROM identity", file=sys.stderr)
        return 1
    config_path = args.config.expanduser().resolve()
    config = tomllib.loads(config_path.read_text(encoding="utf-8"))
    if config.get("identity", {}).get("sha1") != EXPECTED_SHA1:
        print("ERROR: scan config is not bound to the supported ROM", file=sys.stderr)
        return 1

    scan_tool = args.gba_scan.expanduser().resolve()
    recompile_tool = args.gba_recompile.expanduser().resolve()
    scan_process = _run([str(scan_tool), str(rom_path)])
    if scan_process.returncode != 0:
        print(scan_process.stdout, file=sys.stderr)
        print(scan_process.stderr, file=sys.stderr)
        return 1
    header = _parse_key_values(scan_process.stdout)
    entry_target = int(header["entry_target"], 16)
    program = config.get("program", {})
    if program.get("entry_pc") != entry_target:
        print("ERROR: config entry_pc disagrees with gba_scan", file=sys.stderr)
        return 1
    if program.get("load_address") != ROM_BASE or program.get("size") != EXPECTED_SIZE:
        print("ERROR: config program layout disagrees with supported ROM", file=sys.stderr)
        return 1

    generated_dir = args.generated_dir.expanduser().resolve()
    recompile_process = _run(
        [
            str(recompile_tool),
            "--rom",
            str(rom_path),
            "--config",
            str(config_path),
            "--out",
            str(generated_dir),
            "--max-functions",
            str(args.max_functions),
        ]
    )
    if recompile_process.returncode != 0:
        print(recompile_process.stdout, file=sys.stderr)
        print(recompile_process.stderr, file=sys.stderr)
        return 1
    discovery_match = DISCOVERY_RE.search(recompile_process.stdout)
    if discovery_match is None:
        print("ERROR: unable to parse gbarecomp discovery summary", file=sys.stderr)
        return 1
    discovery_summary = {
        "function_count": int(discovery_match.group(1)),
        "arm_count": int(discovery_match.group(2)),
        "thumb_count": int(discovery_match.group(3)),
        "indirect_count": int(discovery_match.group(4)),
        "undefined_count": int(discovery_match.group(5)),
        "branch_target_count": int(discovery_match.group(6)),
    }
    scanner = parse_generated_functions(generated_dir)
    if len(scanner) != discovery_summary["function_count"]:
        print("ERROR: generated function count disagrees with discovery summary", file=sys.stderr)
        return 1

    imported = json.loads(args.main_corpus.read_text(encoding="utf-8"))
    import_report = json.loads(args.main_report.read_text(encoding="utf-8"))
    elf_image = parse_elf32_arm(args.main_elf.read_bytes())
    comparison = compare_discovery(scanner, imported, elf_image)
    report = {
        "schema_version": 1,
        "rom_sha1": EXPECTED_SHA1,
        "gbarecomp_revision": args.gbarecomp_revision,
        "disassembly_revision": args.disasm_revision,
        "tools": {
            "gba_scan_sha256": _sha256_path(scan_tool),
            "gba_recompile_sha256": _sha256_path(recompile_tool),
            "config_sha256": _sha256_path(config_path),
        },
        "header": {
            key: header[key]
            for key in (
                "rom_size",
                "entry_branch_word",
                "entry_is_branch",
                "entry_target",
                "game_title",
                "game_code",
                "maker_code",
                "complement_valid",
                "logo_present",
                "save_type",
                "save_signature",
                "save_signature_offset",
                "ok",
            )
        },
        "scanner": discovery_summary,
        "imported": {
            "function_count": len(imported["symbols"]),
            "excluded_count": len(import_report.get("unresolved", [])),
        },
        "comparison": comparison,
    }
    payload = (json.dumps(report, indent=2, ensure_ascii=True) + "\n").encode("utf-8")
    output = args.output.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    print(f"Scanner functions: {discovery_summary['function_count']}")
    print(f"Imported functions: {len(imported['symbols'])}")
    print(f"Exact entry/mode matches: {comparison['exact_entry_mode_matches']}")
    print(f"Mode conflicts: {len(comparison['mode_conflicts'])}")
    print(f"Scanner-only: {len(comparison['scanner_only'])}")
    for name, count in comparison["scanner_only_counts"].items():
        print(f"  {name}: {count}")
    print(f"Imported-only: {len(comparison['imported_only'])}")
    for name, count in comparison["imported_only_counts"].items():
        print(f"  {name}: {count}")
    print(f"Report SHA-256: {hashlib.sha256(payload).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
