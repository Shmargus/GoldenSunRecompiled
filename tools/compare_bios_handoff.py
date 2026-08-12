#!/usr/bin/env python3
"""Synchronize GoldenSunRecomp and the mGBA oracle at BIOS handoff.

The native side is advanced only through the upstream TCP debug protocol. A
per-instruction breakpoint lets a recompiled dispatch unwind before the next
guest instruction, so post-instruction CPU state can be compared with mGBA
without adding logging to guest execution.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import time
from typing import Any


ROM_SHA1 = "5c4695205413df7db52b9a184815a07783999971"
BIOS_SHA1 = "300c20df6731a33952ded8c436f7f186d25d3492"
HANDOFF_PC = 0x08000000
REG_FIELDS = tuple(f"r{i}" for i in range(15)) + ("cpsr",)
FP_HEADER = struct.Struct("<IIQ")
FP_RECORD = struct.Struct("<QII16I")
FP_MAGIC = 0x31504647


def sha1_file(path: Path) -> str:
    digest = hashlib.sha1()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_asset(path: Path, expected_sha1: str, label: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"{label} is not a file: {path}")
    actual = sha1_file(path)
    if actual != expected_sha1:
        raise RuntimeError(
            f"{label} SHA-1 mismatch: expected {expected_sha1}, got {actual}"
        )


def oracle_executing_pc(state: dict[str, Any]) -> int:
    if "executing_pc" in state:
        return int(state["executing_pc"])
    raw_pc = int(state["pc"])
    return (raw_pc - (2 if state.get("thumb", False) else 4)) & 0xFFFFFFFF


def state_differences(
    native: dict[str, Any], oracle: dict[str, Any]
) -> list[dict[str, Any]]:
    differences: list[dict[str, Any]] = []
    native_pc = int(native["pc"])
    oracle_pc = oracle_executing_pc(oracle)
    if native_pc != oracle_pc:
        differences.append(
            {"field": "pc", "native": native_pc, "oracle": oracle_pc}
        )
    for field in REG_FIELDS:
        native_value = int(native[field])
        oracle_value = int(oracle[field])
        if native_value != oracle_value:
            differences.append(
                {
                    "field": field,
                    "native": native_value,
                    "oracle": oracle_value,
                }
            )
    return differences


class JsonClient:
    def __init__(self, port: int, timeout: float = 15.0):
        deadline = time.monotonic() + timeout
        last_error: OSError | None = None
        self.socket: socket.socket | None = None
        while time.monotonic() < deadline:
            try:
                self.socket = socket.create_connection(
                    ("127.0.0.1", port), timeout=1.0
                )
                break
            except OSError as error:
                last_error = error
                time.sleep(0.05)
        if self.socket is None:
            raise RuntimeError(f"cannot reach TCP port {port}: {last_error}")
        self.socket.settimeout(timeout)
        self.buffer = b""

    def call(self, **request: Any) -> dict[str, Any]:
        assert self.socket is not None
        self.socket.sendall(json.dumps(request).encode("utf-8") + b"\n")
        while b"\n" not in self.buffer:
            chunk = self.socket.recv(65536)
            if not chunk:
                raise RuntimeError("TCP peer closed the connection")
            self.buffer += chunk
        line, _, self.buffer = self.buffer.partition(b"\n")
        response = json.loads(line.decode("utf-8"))
        if not isinstance(response, dict):
            raise RuntimeError(f"unexpected TCP response: {response!r}")
        return response

    def close(self) -> None:
        if self.socket is None:
            return
        try:
            self.call(cmd="quit")
        except (OSError, RuntimeError, json.JSONDecodeError):
            pass
        self.socket.close()
        self.socket = None


def drive_native_to_handoff(
    native: JsonClient, max_dispatches: int
) -> tuple[dict[str, Any], int]:
    response = native.call(cmd="set_break_pc", value=HANDOFF_PC)
    if not response.get("ok"):
        raise RuntimeError(f"native rejected BIOS-handoff breakpoint: {response}")
    for dispatches in range(1, max_dispatches + 1):
        response = native.call(cmd="step_inst")
        if not response.get("ok"):
            raise RuntimeError(f"native stopped before BIOS handoff: {response}")
        if int(response["pc"]) == HANDOFF_PC:
            return response, dispatches
    raise RuntimeError(
        f"native did not reach 0x{HANDOFF_PC:08x} in {max_dispatches} dispatches"
    )


def drive_native_to_process_stop(
    native: JsonClient,
    max_dispatches: int,
    fingerprint_path: Path,
    capture_break_pc: int = 0,
) -> tuple[int, str]:
    for dispatches in range(1, max_dispatches + 1):
        try:
            response = native.call(cmd="step_inst")
        except (OSError, RuntimeError, socket.timeout) as error:
            return dispatches, str(error)
        if not response.get("ok"):
            return dispatches, f"native step returned {response}"
        saved = native.call(cmd="fp_save", path=str(fingerprint_path))
        if not saved.get("ok") or int(saved.get("count", 0)) == 0:
            raise RuntimeError(f"native fingerprint save failed: {saved}")
        if capture_break_pc and int(response.get("pc", 0)) == capture_break_pc:
            cleared = native.call(cmd="set_break_pc", value=0)
            if not cleared.get("ok"):
                raise RuntimeError(
                    f"native rejected capture-breakpoint clear: {cleared}"
                )
            capture_break_pc = 0
    raise RuntimeError(
        f"native remained runnable for {max_dispatches} post-handoff dispatches"
    )


def drive_oracle_to_handoff(
    oracle: JsonClient, max_instructions: int
) -> dict[str, Any]:
    response = oracle.call(
        cmd="emu_run_to_pc", target=HANDOFF_PC, max_steps=max_instructions
    )
    if not response.get("ok"):
        raise RuntimeError(f"oracle did not reach BIOS handoff: {response}")
    if oracle_executing_pc(response) != HANDOFF_PC:
        raise RuntimeError(f"oracle stopped at the wrong PC: {response}")
    return response


def first_region_difference(left: bytes, right: bytes) -> int | None:
    for index, (left_byte, right_byte) in enumerate(zip(left, right)):
        if left_byte != right_byte:
            return index
    if len(left) != len(right):
        return min(len(left), len(right))
    return None


def read_region(
    client: JsonClient, command: str, base: int, size: int, chunk_size: int = 4096
) -> bytes:
    result = bytearray()
    while len(result) < size:
        address = base + len(result)
        length = min(chunk_size, size - len(result))
        response = client.call(cmd=command, addr=address, len=length)
        if not response.get("ok"):
            raise RuntimeError(f"{command} failed at 0x{address:08x}: {response}")
        data = bytes.fromhex(str(response["data"]))
        if len(data) != length:
            raise RuntimeError(
                f"{command} returned {len(data)} bytes, expected {length}"
            )
        result.extend(data)
    return bytes(result)


def compare_handoff_regions(
    native: JsonClient, oracle: JsonClient
) -> list[dict[str, Any]]:
    regions = (
        ("ewram", 0x02000000, 0x40000, "read_ewram", "read_emu_ewram"),
        ("iwram", 0x03000000, 0x8000, "read_iwram", "read_emu_iwram"),
        ("io", 0x04000000, 0x400, "read_io", "read_emu_io"),
        ("palette", 0x05000000, 0x400, "read_pal", "read_emu_pal"),
        ("vram", 0x06000000, 0x18000, "read_vram", "read_emu_vram"),
        ("oam", 0x07000000, 0x400, "read_oam", "read_emu_oam"),
    )
    comparisons: list[dict[str, Any]] = []
    for name, base, size, native_command, oracle_command in regions:
        native_data = read_region(native, native_command, base, size)
        oracle_data = read_region(oracle, oracle_command, base, size)
        difference = first_region_difference(native_data, oracle_data)
        item: dict[str, Any] = {
            "region": name,
            "base": base,
            "size": size,
            "equal": difference is None,
            "native_sha256": hashlib.sha256(native_data).hexdigest(),
            "oracle_sha256": hashlib.sha256(oracle_data).hexdigest(),
        }
        if difference is not None:
            item.update(
                {
                    "first_offset": difference,
                    "address": base + difference,
                    "native_byte": native_data[difference],
                    "oracle_byte": oracle_data[difference],
                }
            )
        comparisons.append(item)
    return comparisons


def load_fingerprints(path: Path) -> list[tuple[int, int, int, tuple[int, ...]]]:
    data = path.read_bytes()
    if len(data) < FP_HEADER.size:
        raise RuntimeError(f"fingerprint file is truncated: {path}")
    magic, entry_size, count = FP_HEADER.unpack_from(data, 0)
    if magic != FP_MAGIC or entry_size != FP_RECORD.size:
        raise RuntimeError(
            f"invalid fingerprint header in {path}: "
            f"magic=0x{magic:08x}, entry_size={entry_size}"
        )
    expected_size = FP_HEADER.size + count * entry_size
    if len(data) != expected_size:
        raise RuntimeError(
            f"fingerprint size mismatch in {path}: {len(data)} != {expected_size}"
        )
    records = []
    offset = FP_HEADER.size
    for _ in range(count):
        fields = FP_RECORD.unpack_from(data, offset)
        records.append((fields[0], fields[1], fields[2], tuple(fields[3:])))
        offset += entry_size
    return records


def load_fingerprint_tail(
    path: Path, start_pc: int
) -> tuple[int, list[tuple[int, int, int, tuple[int, ...]]]]:
    """Stream a GFP1 file and retain records from the first matching PC.

    Native startup rings contain millions of BIOS records. Keeping only the
    cartridge tail avoids expanding a roughly 560 MiB binary into several
    GiB of Python integer/tuple objects during every comparison.
    """
    file_size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(FP_HEADER.size)
        if len(header) != FP_HEADER.size:
            raise RuntimeError(f"fingerprint file is truncated: {path}")
        magic, entry_size, count = FP_HEADER.unpack(header)
        if magic != FP_MAGIC or entry_size != FP_RECORD.size:
            raise RuntimeError(
                f"invalid fingerprint header in {path}: "
                f"magic=0x{magic:08x}, entry_size={entry_size}"
            )
        expected_size = FP_HEADER.size + count * entry_size
        if file_size != expected_size:
            raise RuntimeError(
                f"fingerprint size mismatch in {path}: "
                f"{file_size} != {expected_size}"
            )

        found = False
        records = []
        for _ in range(count):
            payload = stream.read(entry_size)
            fields = FP_RECORD.unpack(payload)
            if not found and fields[1] == start_pc:
                found = True
            if found:
                records.append(
                    (fields[0], fields[1], fields[2], tuple(fields[3:]))
                )
    if not found:
        raise RuntimeError(
            f"fingerprint stream does not contain start PC 0x{start_pc:08x}: "
            f"{path}"
        )
    return count, records


def fingerprint_architecture(
    record: tuple[int, int, int, tuple[int, ...]]
) -> tuple[int, int, tuple[int, ...]]:
    return record[1], record[2], record[3]


def fingerprint_state(
    record: tuple[int, int, int, tuple[int, ...]]
) -> dict[str, Any]:
    return {
        "cycles": record[0],
        "pc": record[1],
        "cpsr": record[2],
        "registers": {f"r{index}": value for index, value in enumerate(record[3])},
    }


def compare_fingerprints(
    native_records: list[tuple[int, int, int, tuple[int, ...]]],
    oracle_records: list[tuple[int, int, int, tuple[int, ...]]],
) -> dict[str, Any]:
    handoff_index = next(
        (index for index, record in enumerate(native_records) if record[1] == HANDOFF_PC),
        None,
    )
    if handoff_index is None:
        raise RuntimeError("native fingerprint ring does not contain BIOS handoff")
    native_game = native_records[handoff_index:]
    overlap = min(len(native_game), len(oracle_records))
    for index in range(overlap):
        native = native_game[index]
        oracle = oracle_records[index]
        if fingerprint_architecture(native) != fingerprint_architecture(oracle):
            fields = ("pc", "cpsr") + tuple(f"r{i}" for i in range(16))
            native_values = (native[1], native[2]) + native[3]
            oracle_values = (oracle[1], oracle[2]) + oracle[3]
            differences = [
                {"field": field, "native": left, "oracle": right}
                for field, left, right in zip(fields, native_values, oracle_values)
                if left != right
            ]
            return {
                "status": "architectural_divergence",
                "index": index,
                "source_pc": native_game[index - 1][1] if index else None,
                "native_pc": native[1],
                "oracle_pc": oracle[1],
                "differences": differences,
                "identical_prefix": index,
                "native_before": (
                    fingerprint_state(native_game[index - 1]) if index else None
                ),
                "oracle_before": (
                    fingerprint_state(oracle_records[index - 1]) if index else None
                ),
                "native_after": fingerprint_state(native),
                "oracle_after": fingerprint_state(oracle),
            }
    if len(native_game) < len(oracle_records):
        return {
            "status": "native_stream_ended",
            "identical_prefix": overlap,
            "last_native_pc": native_game[-1][1] if native_game else None,
            "next_oracle_pc": oracle_records[overlap][1],
            "native_game_records": len(native_game),
            "oracle_records": len(oracle_records),
        }
    if len(oracle_records) < len(native_game):
        return {
            "status": "oracle_stream_ended",
            "identical_prefix": overlap,
            "last_oracle_pc": oracle_records[-1][1] if oracle_records else None,
            "next_native_pc": native_game[overlap][1],
            "native_game_records": len(native_game),
            "oracle_records": len(oracle_records),
        }
    return {
        "status": "identical",
        "identical_prefix": overlap,
        "native_game_records": len(native_game),
        "oracle_records": len(oracle_records),
    }


def extract_strict_miss(log_path: Path) -> dict[str, Any] | None:
    text = log_path.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"STRICT_STATIC dispatch miss for pc=0x([0-9A-Fa-f]+) \((arm|thumb)\)",
        text,
    )
    if not match:
        return None
    return {"pc": int(match.group(1), 16), "mode": match.group(2)}


def lockstep_from_handoff(
    native: JsonClient, oracle: JsonClient, max_steps: int
) -> dict[str, Any]:
    current_pc = HANDOFF_PC
    recent: list[dict[str, int]] = []
    for step in range(1, max_steps + 1):
        oracle_state = oracle.call(cmd="emu_step_inst")
        if not oracle_state.get("ok"):
            return {
                "status": "oracle_stop",
                "step": step,
                "source_pc": current_pc,
                "response": oracle_state,
                "recent": recent,
            }
        expected_pc = oracle_executing_pc(oracle_state)
        if expected_pc == current_pc:
            return {
                "status": "unsupported_self_edge",
                "step": step,
                "source_pc": current_pc,
                "expected_pc": expected_pc,
                "recent": recent,
            }
        response = native.call(cmd="set_break_pc", value=expected_pc)
        if not response.get("ok"):
            return {
                "status": "native_breakpoint_rejected",
                "step": step,
                "source_pc": current_pc,
                "expected_pc": expected_pc,
                "response": response,
                "recent": recent,
            }
        try:
            native_state = native.call(cmd="step_inst")
        except (OSError, RuntimeError, socket.timeout) as error:
            return {
                "status": "native_process_stop",
                "step": step,
                "source_pc": current_pc,
                "expected_pc": expected_pc,
                "error": str(error),
                "recent": recent,
            }
        if not native_state.get("ok"):
            return {
                "status": "native_stop",
                "step": step,
                "source_pc": current_pc,
                "expected_pc": expected_pc,
                "response": native_state,
                "recent": recent,
            }
        differences = state_differences(native_state, oracle_state)
        recent.append(
            {"step": step, "source_pc": current_pc, "next_pc": expected_pc}
        )
        recent = recent[-16:]
        if differences:
            return {
                "status": "state_divergence",
                "step": step,
                "source_pc": current_pc,
                "expected_pc": expected_pc,
                "native_pc": int(native_state["pc"]),
                "differences": differences,
                "native": {key: native_state[key] for key in REG_FIELDS},
                "oracle": {key: oracle_state[key] for key in REG_FIELDS},
                "recent": recent,
            }
        current_pc = expected_pc
    return {
        "status": "no_divergence",
        "steps": max_steps,
        "last_pc": current_pc,
        "recent": recent,
    }


def process_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment["GBARECOMP_STRICT_STATIC"] = "1"
    environment["GBARECOMP_SELF_HEAL"] = "0"
    environment["GBARECOMP_CACHE"] = "0"
    environment["GBARECOMP_INTERP_BRIDGE"] = "0"
    if os.name == "nt":
        mingw_bin = Path(r"C:\msys64\mingw64\bin")
        if mingw_bin.is_dir():
            environment["PATH"] = str(mingw_bin) + os.pathsep + environment["PATH"]
    return environment


def stop_process(process: subprocess.Popen[bytes]) -> None:
    try:
        process.wait(timeout=3.0)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3.0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--bios", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-port", type=int, default=19842)
    parser.add_argument("--oracle-port", type=int, default=19843)
    parser.add_argument("--max-native-dispatches", type=int, default=10000)
    parser.add_argument("--max-oracle-instructions", type=int, default=50000000)
    parser.add_argument("--max-lockstep-steps", type=int, default=100000)
    parser.add_argument("--expected-boundary", type=lambda value: int(value, 0),
                        default=0x080047AE)
    parser.add_argument(
        "--capture-break-pc",
        type=lambda value: int(value, 0),
        default=0,
        help=(
            "optional executing PC at which native should cooperatively stop "
            "once to save its trace before a later strict abort"
        ),
    )
    args = parser.parse_args()

    native_path = args.native.resolve()
    oracle_path = args.oracle.resolve()
    bios_path = args.bios.resolve()
    rom_path = args.rom.resolve()
    output_path = args.output.resolve()
    require_asset(rom_path, ROM_SHA1, "ROM")
    require_asset(bios_path, BIOS_SHA1, "BIOS")
    if not native_path.is_file() or not oracle_path.is_file():
        raise RuntimeError("native and oracle executables must exist")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    native_log = output_path.with_name("native.log")
    oracle_log = output_path.with_name("oracle.log")
    native_fp = output_path.with_name("native.fp")
    oracle_fp = output_path.with_name("oracle.fp")
    for stale_path in (native_fp, oracle_fp):
        stale_path.unlink(missing_ok=True)
    environment = process_environment()
    environment["GBARECOMP_INSN_TRACE"] = "1"
    environment["GBARECOMP_FP_SAVE"] = str(native_fp)
    processes: list[subprocess.Popen[bytes]] = []
    clients: list[JsonClient] = []
    report: dict[str, Any] = {
        "schema": 1,
        "rom_sha1": ROM_SHA1,
        "bios_sha1": BIOS_SHA1,
        "handoff_pc": HANDOFF_PC,
        "strict_static": True,
    }
    try:
        with native_log.open("wb") as native_stream, oracle_log.open("wb") as oracle_stream:
            processes.append(
                subprocess.Popen(
                    [
                        str(native_path),
                        "--bios",
                        str(bios_path),
                        "--rom",
                        str(rom_path),
                        "--tcp",
                        str(args.native_port),
                        "--no-window",
                    ],
                    cwd=str(native_path.parent),
                    env=environment,
                    stdout=native_stream,
                    stderr=subprocess.STDOUT,
                )
            )
            processes.append(
                subprocess.Popen(
                    [
                        str(oracle_path),
                        "--bios",
                        str(bios_path),
                        "--rom",
                        str(rom_path),
                        "--port",
                        str(args.oracle_port),
                    ],
                    cwd=str(oracle_path.parent),
                    env=environment,
                    stdout=oracle_stream,
                    stderr=subprocess.STDOUT,
                )
            )
            native = JsonClient(args.native_port)
            oracle = JsonClient(args.oracle_port)
            clients.extend((native, oracle))
            for label, process in zip(("native", "oracle"), processes):
                if process.poll() is not None:
                    raise RuntimeError(
                        f"{label} process exited before TCP ownership was verified"
                    )
            report["native_ping"] = native.call(cmd="ping")
            report["oracle_ping"] = oracle.call(cmd="ping")

            native_state, dispatches = drive_native_to_handoff(
                native, args.max_native_dispatches
            )
            oracle_state = drive_oracle_to_handoff(
                oracle, args.max_oracle_instructions
            )
            report["handoff"] = {
                "native_dispatches": dispatches,
                "oracle_instructions": int(oracle_state["steps"]),
                "cpu_differences": state_differences(native_state, oracle_state),
                "native": {"pc": native_state["pc"]}
                | {key: native_state[key] for key in REG_FIELDS},
                "oracle": {"pc": oracle_executing_pc(oracle_state)}
                | {key: oracle_state[key] for key in REG_FIELDS},
            }
            report["handoff_regions"] = compare_handoff_regions(native, oracle)
            if report["handoff"]["cpu_differences"]:
                report["first_divergence"] = {
                    "status": "bios_handoff_cpu_state",
                    "differences": report["handoff"]["cpu_differences"],
                }
            else:
                next_break_pc = args.capture_break_pc
                armed = native.call(cmd="set_break_pc", value=next_break_pc)
                if not armed.get("ok"):
                    raise RuntimeError(
                        f"native rejected post-handoff breakpoint: {armed}"
                    )
                post_handoff_dispatches, stop_reason = drive_native_to_process_stop(
                    native,
                    args.max_native_dispatches,
                    native_fp,
                    capture_break_pc=next_break_pc,
                )
                try:
                    processes[0].wait(timeout=15.0)
                except subprocess.TimeoutExpired as error:
                    raise RuntimeError("native did not stop at its strict boundary") from error

                strict_miss = extract_strict_miss(native_log)
                oracle_target = (
                    strict_miss["pc"] if strict_miss is not None
                    else args.expected_boundary
                )
                oracle_trace = oracle.call(
                    cmd="emu_trace_to_pc",
                    target=oracle_target,
                    max_steps=args.max_lockstep_steps,
                )
                if not oracle_trace.get("ok"):
                    raise RuntimeError(f"oracle trace failed: {oracle_trace}")
                oracle_saved = oracle.call(cmd="emu_fp_save", path=str(oracle_fp))
                if not oracle_saved.get("ok"):
                    raise RuntimeError(f"oracle fingerprint save failed: {oracle_saved}")
                native_count, native_records = load_fingerprint_tail(
                    native_fp, HANDOFF_PC
                )
                oracle_records = load_fingerprints(oracle_fp)
                fingerprints = compare_fingerprints(
                    native_records, oracle_records
                )
                report["fingerprints"] = {
                    "native_post_handoff_dispatches": post_handoff_dispatches,
                    "native_stop_reason": stop_reason,
                    "native_count": native_count,
                    "oracle_count": len(oracle_records),
                    "comparison": fingerprints,
                    "strict_miss": strict_miss,
                }
                if (
                    fingerprints["status"] == "native_stream_ended"
                    and strict_miss is not None
                    and fingerprints["next_oracle_pc"] == strict_miss["pc"]
                ):
                    report["first_divergence"] = {
                        "status": "static_coverage_boundary",
                        "pc": strict_miss["pc"],
                        "mode": strict_miss["mode"],
                        "previous_pc": fingerprints["last_native_pc"],
                        "identical_instruction_states": fingerprints[
                            "identical_prefix"
                        ],
                    }
                else:
                    report["first_divergence"] = fingerprints
    finally:
        for client in reversed(clients):
            client.close()
        for process in processes:
            stop_process(process)
        report["native_exit_code"] = processes[0].returncode if processes else None
        report["oracle_exit_code"] = processes[1].returncode if len(processes) > 1 else None
        output_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    handoff_differences = report["handoff"]["cpu_differences"]
    print(
        f"handoff: native dispatches={report['handoff']['native_dispatches']} "
        f"oracle instructions={report['handoff']['oracle_instructions']} "
        f"cpu differences={len(handoff_differences)}"
    )
    divergence = report["first_divergence"]
    print(f"first divergence: {divergence['status']}")
    if "step" in divergence:
        print(
            f"step={divergence['step']} "
            f"source=0x{divergence['source_pc']:08x} "
            f"next=0x{divergence['expected_pc']:08x}"
        )
    elif "pc" in divergence:
        print(
            f"pc=0x{divergence['pc']:08x} "
            f"previous=0x{divergence['previous_pc']:08x} "
            f"identical states={divergence['identical_instruction_states']}"
        )
    elif divergence.get("status") == "architectural_divergence":
        fields = ",".join(item["field"] for item in divergence["differences"])
        print(
            f"source=0x{divergence['source_pc']:08x} "
            f"comparison=0x{divergence['native_pc']:08x} fields={fields} "
            f"identical states={divergence['identical_prefix']}"
        )
    print(f"report: {output_path}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(2)
