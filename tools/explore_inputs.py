#!/usr/bin/env python3
"""explore_inputs.py — coverage-guided input explorer for the Golden Sun runner.

Part 2 of the func-coverage work (see GBARECOMP_FUNC_COVERAGE, Part 1). Only
~11% of translated code has ever executed under the existing regression
tracks (4,108 / 36,181 functions at campaign-5400). This tool searches for
input sequences that reach code the fixed campaign/no-input tracks never
touch, by fuzzing button macros from checkpointed game states and keeping
only sequences that grow the cumulative "functions ever reached" set.

Core loop, one iteration:
  1. pick a parent checkpoint (a savestate) from the frontier
  2. generate or mutate a short macro of held button-presses
  3. spawn the runner in --tcp mode, load the parent state, drive the macro
     over the debug TCP protocol (see gbarecomp/TCP.md and oracle/gba_tcp.py),
     savestate_save the end state, send `quit` (flushes
     GBARECOMP_FUNC_COVERAGE to disk on exit)
  4. parse the coverage file; if it names any (addr, mode) never seen in any
     prior iteration this run, KEEP the macro (record it + its end-state
     checkpoint as a new frontier node); otherwise DISCARD (delete the
     checkpoint, keep only a one-line log entry)
  5. repeat, extending kept nodes and occasionally mutating siblings

Design decisions (see the module docstrings on each piece for the "why"):
  - Cost per iteration: checkpointed via debug::save_state/load_state over
    the TCP debug server (savestate_save / savestate_load), NOT CLI
    --load-state — the CLI flag is unreachable in --tcp mode (see
    runtime.cpp: the --tcp branch returns before the --load-state apply
    site). A bootstrap macro (replayed from one of the recorded
    local/play-sessions/*.input tracks) produces the first checkpoint from a
    cold boot; every later iteration extends or re-branches from a
    checkpoint instead of re-driving the intro.
  - Mutation: RPG-biased action units (advance-text A-taps, held direction
    changes, occasional B/START, idle waits), never single-frame presses —
    see generate_burst_ops.
  - Determinism: the corpus is worthless if a kept macro cannot be
    reproduced. --verify-determinism re-runs a kept macro from its parent
    checkpoint a second time and compares both the coverage file (must be
    byte-identical, per runtime_coverage_save_file's own determinism
    contract) and the TCP `state_hash` FNV-1a-64 digest of IWRAM/EWRAM/
    VRAM/PAL/OAM + cycle count.
  - Parallelism: NONE. Iterations run strictly sequentially in one Python
    loop; each iteration's runner process is spawned, driven, and joined
    (process exit observed) before the next is spawned. A pre-flight check
    refuses to start if a GoldenSunRecomp.exe is already running, matching
    scripts/gs.ps1's own guard.

Usage:
    python tools/explore_inputs.py run \\
        --runner build/gs011/GoldenSunRecomp.exe \\
        --bios "<path to gba_bios.bin>" --rom "<path to Golden Sun.gba>" \\
        --corpus-dir local/explore/session1 \\
        --iterations 200 --burst-frames 240 --seed 1

    python tools/explore_inputs.py report --corpus-dir local/explore/session1 \\
        --elf "<path to goldensun.elf>"
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import random
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable

if __package__:
    from . import measure_coverage
else:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import measure_coverage  # type: ignore[no-redef]


# ---------------------------------------------------------------------------
# GBA KEYINPUT bits (active-low). Same convention as oracle/gba_tcp.py and
# the recorded local/play-sessions/*.input tracks.
# ---------------------------------------------------------------------------

KEY_A, KEY_B, KEY_SELECT, KEY_START, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_R, KEY_L = (
    1 << i for i in range(10)
)
NONE_KEYINPUT = 0x03FF
DIRECTIONS = (KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN)


def keyinput_with(*bits: int) -> int:
    """Active-low KEYINPUT value with the given bits pressed."""
    v = NONE_KEYINPUT
    for b in bits:
        v &= ~b
    return v & NONE_KEYINPUT


# ---------------------------------------------------------------------------
# Macro representation
# ---------------------------------------------------------------------------


@dataclasses.dataclass(frozen=True)
class MacroOp:
    """Hold `keyinput` (active-low) for `hold` frames. One TCP run_frames call."""

    keyinput: int
    hold: int

    def to_json(self) -> dict:
        return {"keyinput": self.keyinput, "hold": self.hold}

    @staticmethod
    def from_json(d: dict) -> "MacroOp":
        return MacroOp(keyinput=int(d["keyinput"]), hold=int(d["hold"]))


def macro_ops_total_frames(ops: Iterable[MacroOp]) -> int:
    return sum(op.hold for op in ops)


# ---------------------------------------------------------------------------
# Reuse the existing frame-indexed keyinput file format (GBARECOMP_INPUT_RECORD
# / GBARECOMP_INPUT_REPLAY / local/play-sessions/*.input): "frame,0xNNNN" rows,
# emitted only on value change, non-decreasing frame numbers, '#' comments.
# ---------------------------------------------------------------------------


@dataclasses.dataclass(frozen=True)
class ReplayEvent:
    frame: int
    keyinput: int


def parse_replay_file(text: str) -> list[ReplayEvent]:
    events: list[ReplayEvent] = []
    previous_frame = None
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        frame_str, _, key_str = line.partition(",")
        frame = int(frame_str)
        if not key_str.lower().startswith("0x"):
            raise ValueError(f"invalid replay line (expected 0x keyinput): {line!r}")
        keyinput = int(key_str, 16)
        if keyinput > NONE_KEYINPUT:
            raise ValueError(f"keyinput out of range in line: {line!r}")
        if previous_frame is not None and frame < previous_frame:
            raise ValueError(f"non-increasing frame in replay file: {line!r}")
        events.append(ReplayEvent(frame=frame, keyinput=keyinput))
        previous_frame = frame
    return events


def replay_events_to_macro_ops(
    events: list[ReplayEvent], tail_frames: int = 0
) -> list[MacroOp]:
    """Convert frame-indexed events into held-value ops for TCP run_frames driving.

    Mirrors apply_input_replay in runtime.cpp exactly: value[i] holds from
    events[i].frame until events[i+1].frame (exclusive); the last event holds
    for `tail_frames` more frames. Zero-length holds are dropped.
    """

    ops: list[MacroOp] = []
    for i, ev in enumerate(events):
        if i + 1 < len(events):
            hold = events[i + 1].frame - ev.frame
        else:
            hold = tail_frames
        if hold > 0:
            ops.append(MacroOp(keyinput=ev.keyinput, hold=hold))
    return ops


# ---------------------------------------------------------------------------
# RPG-biased mutation. Pure-random single-frame mashing gets stuck at menus
# and dialogue (a 1-frame press is frequently missed entirely by an
# edge-triggered menu handler); this generator only ever emits actions an
# RPG needs, each held for several frames.
# ---------------------------------------------------------------------------

_ACTIONS = ("advance_text", "move", "menu_b", "start", "idle")
_ACTION_WEIGHTS = (0.45, 0.25, 0.10, 0.10, 0.10)


def _one_action_ops(rng: random.Random, action: str) -> list[MacroOp]:
    if action == "advance_text":
        # Edge-triggered dialogue/menu advance: press then release, matching
        # the reviewed demo/inspection tracks' tap() shape (hold=8, gap=14 in
        # tools/make_inspection_track.py; the same shape the campaign demo
        # driver uses in runtime.cpp's demo_keyinput_for_frame).
        return [MacroOp(keyinput_with(KEY_A), 8), MacroOp(NONE_KEYINPUT, 14)]
    if action == "move":
        direction = rng.choice(DIRECTIONS)
        hold = rng.randint(20, 40)
        return [MacroOp(keyinput_with(direction), hold)]
    if action == "menu_b":
        return [MacroOp(keyinput_with(KEY_B), 6), MacroOp(NONE_KEYINPUT, 10)]
    if action == "start":
        return [MacroOp(keyinput_with(KEY_START), 6), MacroOp(NONE_KEYINPUT, 12)]
    if action == "idle":
        return [MacroOp(NONE_KEYINPUT, rng.randint(10, 30))]
    raise ValueError(f"unknown action {action!r}")


def generate_burst_ops(rng: random.Random, target_frames: int, max_ops: int = 200) -> list[MacroOp]:
    """Deterministic (given `rng`'s state) RPG-biased macro of ~target_frames."""

    ops: list[MacroOp] = []
    total = 0
    while total < target_frames and len(ops) < max_ops:
        action = rng.choices(_ACTIONS, weights=_ACTION_WEIGHTS, k=1)[0]
        new_ops = _one_action_ops(rng, action)
        ops.extend(new_ops)
        total += macro_ops_total_frames(new_ops)
    return ops


def mutate_ops(rng: random.Random, ops: list[MacroOp], rate: float = 0.3) -> list[MacroOp]:
    """Perturb a kept macro: replace ~`rate` fraction of action-units, and a
    small chance to grow or shrink the tail. Deterministic given `rng`.
    """

    out: list[MacroOp] = []
    for op in ops:
        if rng.random() < rate:
            action = rng.choices(_ACTIONS, weights=_ACTION_WEIGHTS, k=1)[0]
            out.extend(_one_action_ops(rng, action))
        else:
            out.append(op)
    if rng.random() < 0.2:
        action = rng.choices(_ACTIONS, weights=_ACTION_WEIGHTS, k=1)[0]
        out.extend(_one_action_ops(rng, action))
    if rng.random() < 0.15 and len(out) > 2:
        out = out[: -rng.randint(1, 2)]
    return out


# ---------------------------------------------------------------------------
# Coverage file parsing + set arithmetic. The runner writes
# "0xADDRESS MODE\n" lines, address-sorted, deterministic (runtime_arm.cpp
# runtime_coverage_save_file). One line = one (address, mode) function.
# ---------------------------------------------------------------------------

CoverageSet = frozenset


def parse_coverage_text(text: str) -> "CoverageSet[tuple[int, str]]":
    entries = set()
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        addr_str, _, mode = line.partition(" ")
        entries.add((int(addr_str, 16), mode.strip()))
    return frozenset(entries)


def coverage_delta(
    cumulative: "CoverageSet[tuple[int, str]]", observed: "CoverageSet[tuple[int, str]]"
) -> "CoverageSet[tuple[int, str]]":
    """Functions in `observed` never seen in `cumulative`."""
    return frozenset(observed - cumulative)


def merge_coverage(
    cumulative: "CoverageSet[tuple[int, str]]", observed: "CoverageSet[tuple[int, str]]"
) -> "CoverageSet[tuple[int, str]]":
    return frozenset(cumulative | observed)


def decide_keep(new_functions: "CoverageSet[tuple[int, str]]") -> bool:
    """Keep iff this attempt reached at least one function never seen before."""
    return len(new_functions) > 0


# ---------------------------------------------------------------------------
# Section attribution for the report — reuses measure_coverage's ELF ->
# ROM-section mapping rather than duplicating it.
# ---------------------------------------------------------------------------


def attribute_sections(
    addrs: Iterable[int],
    sections: list["measure_coverage.RomSection"],
    shadow_windows: list["measure_coverage.ShadowWindow"],
) -> dict[str, int]:
    counts: dict[str, int] = {}
    for addr in addrs:
        offset = measure_coverage.address_to_rom_offset(addr, shadow_windows)
        name = "(unmapped/transient)"
        if offset is not None:
            section = measure_coverage.section_for_rom_offset(offset, sections)
            if section is not None:
                name = section.name
        counts[name] = counts.get(name, 0) + 1
    return counts


# ---------------------------------------------------------------------------
# TCP debug client — minimal, adapted from gbarecomp/oracle/gba_tcp.py's
# JsonClient (that script lives in the pinned gbarecomp checkout, not this
# repo, so it is not importable here; the wire protocol it implements is
# reused verbatim: newline-delimited JSON request/response, one connection).
# ---------------------------------------------------------------------------


class RunnerCrashed(RuntimeError):
    pass


class JsonTcpClient:
    def __init__(self, host: str, port: int, connect_timeout: float = 15.0):
        deadline = time.time() + connect_timeout
        self.sock = None
        last_err = None
        while time.time() < deadline:
            try:
                self.sock = socket.create_connection((host, port), timeout=2.0)
                break
            except OSError as e:
                last_err = e
                time.sleep(0.1)
        if self.sock is None:
            raise RunnerCrashed(f"can't reach {host}:{port}: {last_err}")
        self.buf = b""

    def call(self, timeout: float | None = 20.0, **kw) -> dict:
        assert self.sock is not None
        self.sock.sendall(json.dumps(kw).encode() + b"\n")
        self.sock.settimeout(timeout)
        try:
            while b"\n" not in self.buf:
                chunk = self.sock.recv(1 << 20)
                if not chunk:
                    raise RunnerCrashed("runner closed the TCP connection unexpectedly")
                self.buf += chunk
        except socket.timeout as e:
            # A hung guest loop (e.g. a nonsensical mutated input parked the
            # game in a real busy-wait it never exits) parks `step` inside
            # runtime.cpp's game thread forever; the TCP server thread stays
            # alive but never answers. Surfaced as RunnerCrashed so callers
            # can discard this one candidate (kill + move on) instead of the
            # whole exploration run dying on one bad mutation.
            raise RunnerCrashed(f"no response within {timeout}s (runner likely wedged)") from e
        finally:
            self.sock.settimeout(None)
        line, _, self.buf = self.buf.partition(b"\n")
        return json.loads(line.decode())

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None


# ---------------------------------------------------------------------------
# Process orchestration (I/O, needs the real runner/ROM/BIOS — not unit
# tested; the Python suite covers the pure logic above with synthetic data).
# ---------------------------------------------------------------------------


@dataclasses.dataclass(frozen=True)
class RunnerConfig:
    runner_exe: Path
    bios: Path
    rom: Path
    port: int = 19845
    host: str = "127.0.0.1"


def assert_no_other_runner_running(exe_name: str = "GoldenSunRecomp.exe") -> None:
    """Refuse to proceed if another runner is already up.

    A concurrent run has silently destroyed another run's entire output
    before in this project (see AGENTS.md / scripts/gs.ps1's own guard).
    This tool never spawns more than one runner at a time itself, but this
    guards against a stray leftover process (e.g. a previous hung session)
    before we add a second.
    """
    try:
        out = subprocess.run(
            ["tasklist", "/FI", f"IMAGENAME eq {exe_name}", "/FO", "CSV"],
            capture_output=True, text=True, timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return  # tasklist unavailable (non-Windows); nothing we can check
    if exe_name.lower() in out.stdout.lower():
        raise RuntimeError(
            f"a {exe_name} process is already running; refusing to start a "
            "second one against the same build/corpus. Close it first."
        )


class RunnerSession:
    """One runner process, driven over its TCP debug server, for one iteration.

    Always ends the process (via `quit`, which flushes
    GBARECOMP_FUNC_COVERAGE, then a wait/kill fallback) before returning
    control, so iterations are strictly serial — see module docstring.
    """

    def __init__(self, config: RunnerConfig, coverage_path: Path, extra_env: dict | None = None):
        self.config = config
        self.coverage_path = coverage_path
        self.proc: subprocess.Popen | None = None
        self.client: JsonTcpClient | None = None
        self._extra_env = extra_env or {}

    def __enter__(self) -> "RunnerSession":
        import os

        env = dict(os.environ)
        env["GBARECOMP_FUNC_COVERAGE"] = str(self.coverage_path)
        env.pop("GBARECOMP_DEMO_INPUT", None)
        env.pop("GBARECOMP_INPUT_REPLAY", None)
        env.pop("GBARECOMP_STRICT_STATIC", None)
        env.update(self._extra_env)
        args = [
            str(self.config.runner_exe),
            "--bios", str(self.config.bios),
            "--rom", str(self.config.rom),
            "--tcp", str(self.config.port),
        ]
        self.proc = subprocess.Popen(
            args, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        try:
            self.client = JsonTcpClient(self.config.host, self.config.port)
        except RunnerCrashed:
            self._kill()
            raise
        return self

    def call(self, timeout: float | None = 20.0, **kw) -> dict:
        assert self.client is not None
        return self.client.call(timeout=timeout, **kw)

    def quit_and_wait(self, timeout: float = 30.0) -> tuple[int, str]:
        stdout_text = ""
        try:
            if self.client is not None:
                self.client.call(timeout=10.0, cmd="quit")
        except (RunnerCrashed, OSError, socket.timeout, json.JSONDecodeError):
            pass
        finally:
            if self.client is not None:
                self.client.close()
        assert self.proc is not None
        try:
            stdout_text, _ = self.proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            self._kill()
            stdout_text = "(killed after quit timeout)"
        return self.proc.returncode if self.proc.returncode is not None else -1, stdout_text or ""

    def _kill(self) -> None:
        if self.proc is not None and self.proc.poll() is None:
            self.proc.kill()
            try:
                self.proc.communicate(timeout=10.0)
            except subprocess.TimeoutExpired:
                pass

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.proc is not None and self.proc.poll() is None:
            self._kill()
        if self.client is not None:
            self.client.close()


def run_macro_session(
    config: RunnerConfig,
    coverage_path: Path,
    ops: list[MacroOp],
    *,
    load_state: Path | None,
    save_state_to: Path | None,
) -> tuple["CoverageSet[tuple[int, str]]", str | None, str]:
    """Drive one macro over TCP; return (coverage_set, state_hash_or_None, stdout_tail).

    Raises RunnerCrashed if the runner process or its TCP server misbehaves.
    """

    assert_no_other_runner_running()
    with RunnerSession(config, coverage_path) as sess:
        if load_state is not None:
            r = sess.call(cmd="savestate_load", path=str(load_state))
            if not r.get("ok"):
                raise RunnerCrashed(f"savestate_load failed: {r}")
        for op in ops:
            # Generous per-call timeout: self_heal_recompile is ON (matching
            # how the 4,108/4,912-function baselines were themselves measured
            # -- see Step 0), so a burst that reaches code no prior run ever
            # touched can block on a synchronous gcc invocation, not just
            # frame-stepping cost. A cold miss has been observed to take well
            # over 60s; 300s gives real headroom while still surfacing a
            # genuinely wedged runner as a loud failure rather than hanging
            # forever.
            r = sess.call(cmd="run_frames", n=op.hold, keyinput=op.keyinput, timeout=300.0)
            if not r.get("ok"):
                raise RunnerCrashed(f"run_frames failed mid-macro: {r}")
        state_hash = None
        if save_state_to is not None:
            r = sess.call(cmd="savestate_save", path=str(save_state_to))
            if not r.get("ok"):
                raise RunnerCrashed(f"savestate_save failed: {r}")
            h = sess.call(cmd="state_hash")
            if h.get("ok"):
                state_hash = h.get("hash")
        returncode, stdout_text = sess.quit_and_wait()
    if returncode != 0:
        raise RunnerCrashed(f"runner exited {returncode}:\n{stdout_text[-2000:]}")
    if not coverage_path.is_file():
        raise RunnerCrashed("runner exited cleanly but wrote no coverage file")
    observed = parse_coverage_text(coverage_path.read_text())
    return observed, state_hash, stdout_text[-2000:]


# ---------------------------------------------------------------------------
# Corpus persistence
# ---------------------------------------------------------------------------


@dataclasses.dataclass
class SequenceRecord:
    id: str
    parent_id: str | None
    ops: list[MacroOp]
    checkpoint: str  # path, relative to corpus dir
    new_function_count: int
    total_functions_at_keep: int
    frames: int

    def to_json(self) -> dict:
        d = dataclasses.asdict(self)
        d["ops"] = [op.to_json() for op in self.ops]
        return d

    @staticmethod
    def from_json(d: dict) -> "SequenceRecord":
        return SequenceRecord(
            id=d["id"], parent_id=d.get("parent_id"),
            ops=[MacroOp.from_json(o) for o in d["ops"]],
            checkpoint=d["checkpoint"], new_function_count=d["new_function_count"],
            total_functions_at_keep=d["total_functions_at_keep"], frames=d["frames"],
        )


class Corpus:
    """On-disk explorer state under a corpus directory (gitignored local/ tree)."""

    def __init__(self, root: Path):
        self.root = root
        self.checkpoints_dir = root / "checkpoints"
        self.sequences_dir = root / "sequences"
        self.cumulative_path = root / "cumulative.json"
        self.growth_path = root / "growth.jsonl"
        self.checkpoints_dir.mkdir(parents=True, exist_ok=True)
        self.sequences_dir.mkdir(parents=True, exist_ok=True)
        self.cumulative: "CoverageSet[tuple[int, str]]" = frozenset()
        self.sequences: dict[str, SequenceRecord] = {}
        self._load()

    def _load(self) -> None:
        if self.cumulative_path.is_file():
            data = json.loads(self.cumulative_path.read_text())
            self.cumulative = frozenset((e[0], e[1]) for e in data)
        for p in sorted(self.sequences_dir.glob("*.json")):
            rec = SequenceRecord.from_json(json.loads(p.read_text()))
            self.sequences[rec.id] = rec

    def save_cumulative(self) -> None:
        data = sorted([[a, m] for a, m in self.cumulative])
        self.cumulative_path.write_text(json.dumps(data))

    def frontier(self) -> list[SequenceRecord]:
        """Kept sequences, in keep order (insertion order of sequences_dir scan
        is filename order, which we make chronological via zero-padded ids)."""
        return [self.sequences[k] for k in sorted(self.sequences.keys())]

    def add_sequence(self, rec: SequenceRecord) -> None:
        self.sequences[rec.id] = rec
        (self.sequences_dir / f"{rec.id}.json").write_text(json.dumps(rec.to_json(), indent=0))

    def log_growth(self, entry: dict) -> None:
        with self.growth_path.open("a") as f:
            f.write(json.dumps(entry) + "\n")


# ---------------------------------------------------------------------------
# Explorer loop
# ---------------------------------------------------------------------------


def bootstrap_root(
    config: RunnerConfig, corpus: Corpus, bootstrap_input: Path, tail_frames: int
) -> SequenceRecord:
    events = parse_replay_file(bootstrap_input.read_text())
    ops = replay_events_to_macro_ops(events, tail_frames=tail_frames)
    checkpoint = corpus.checkpoints_dir / "root.state"
    coverage_path = corpus.root / "attempts" / "root.cov"
    coverage_path.parent.mkdir(parents=True, exist_ok=True)
    observed, state_hash, _tail = run_macro_session(
        config, coverage_path, ops, load_state=None, save_state_to=checkpoint,
    )
    corpus.cumulative = merge_coverage(corpus.cumulative, observed)
    rec = SequenceRecord(
        id="0000_root", parent_id=None, ops=ops,
        checkpoint=str(checkpoint.relative_to(corpus.root)),
        new_function_count=len(observed), total_functions_at_keep=len(corpus.cumulative),
        frames=macro_ops_total_frames(ops),
    )
    corpus.add_sequence(rec)
    corpus.save_cumulative()
    corpus.log_growth({
        "iteration": 0, "kind": "bootstrap", "id": rec.id, "kept": True,
        "new_functions": len(observed), "cumulative_total": len(corpus.cumulative),
        "state_hash": state_hash,
    })
    return rec


def run_iteration(
    config: RunnerConfig, corpus: Corpus, rng: random.Random, iteration: int, burst_frames: int,
) -> dict:
    frontier = corpus.frontier()
    parent = frontier[-1] if rng.random() < 0.7 else rng.choice(frontier)
    # The root node's `ops` is the ENTIRE bootstrap replay (thousands of
    # frames reaching from cold boot into gameplay), not a short burst —
    # mutating it means re-driving nearly the whole intro+opening with a few
    # values swapped, which is both far slower than every other iteration
    # (observed: ~530s vs ~9s) and can land the mutated macro in a menu/text
    # state the swapped inputs never escape (observed: a hang requiring the
    # 300s per-call timeout to surface, tolerated but pure waste). Root is a
    # checkpoint to extend from, never a macro to mutate.
    if parent.parent_id is None or rng.random() < 0.5 or not parent.ops:
        ops = generate_burst_ops(rng, burst_frames)
        kind = "extend"
    else:
        ops = mutate_ops(rng, parent.ops)
        kind = "mutate"
    # Short id: iteration counter + kind only. NOT parent.id + "_from_" +
    # parent.id's own name — parent ids can themselves already carry a
    # "_from_<parent>" suffix, and chaining that in every generation grows
    # the filename by a constant amount per generation, eventually exceeding
    # Windows MAX_PATH over a long deep-extension run. Lineage is preserved
    # in the JSON record's parent_id field instead.
    seq_id = f"{iteration:04d}_{kind}"
    candidate_checkpoint = corpus.checkpoints_dir / f"{seq_id}.state"
    coverage_path = corpus.root / "attempts" / f"{seq_id}.cov"
    coverage_path.parent.mkdir(parents=True, exist_ok=True)
    parent_state = corpus.root / parent.checkpoint

    try:
        observed, state_hash, tail = run_macro_session(
            config, coverage_path, ops, load_state=parent_state, save_state_to=candidate_checkpoint,
        )
    except RunnerCrashed as e:
        # One candidate hanging (a mutated macro can genuinely wedge the
        # guest in a real busy-wait) must not take down the whole
        # exploration run. RunnerSession.__exit__ already killed the
        # process; discard this candidate and let the caller move on to a
        # fresh process for the next iteration.
        candidate_checkpoint.unlink(missing_ok=True)
        coverage_path.unlink(missing_ok=True)
        entry = {
            "iteration": iteration, "kind": kind, "id": seq_id, "parent": parent.id,
            "kept": False, "new_functions": 0, "observed_functions": 0,
            "cumulative_total": len(corpus.cumulative),
            "frames": macro_ops_total_frames(ops), "state_hash": None,
            "error": str(e),
        }
        corpus.log_growth(entry)
        return entry
    new_functions = coverage_delta(corpus.cumulative, observed)
    kept = decide_keep(new_functions)
    entry = {
        "iteration": iteration, "kind": kind, "id": seq_id, "parent": parent.id,
        "kept": kept, "new_functions": len(new_functions),
        "observed_functions": len(observed),
        "cumulative_total": len(corpus.cumulative) + (len(new_functions) if kept else 0),
        "frames": macro_ops_total_frames(ops), "state_hash": state_hash,
    }
    if kept:
        corpus.cumulative = merge_coverage(corpus.cumulative, observed)
        rec = SequenceRecord(
            id=seq_id, parent_id=parent.id, ops=ops,
            checkpoint=str(candidate_checkpoint.relative_to(corpus.root)),
            new_function_count=len(new_functions),
            total_functions_at_keep=len(corpus.cumulative),
            frames=macro_ops_total_frames(ops),
        )
        corpus.add_sequence(rec)
        corpus.save_cumulative()
    else:
        candidate_checkpoint.unlink(missing_ok=True)
        coverage_path.unlink(missing_ok=True)
    corpus.log_growth(entry)
    return entry


def cmd_run(args: argparse.Namespace) -> int:
    config = RunnerConfig(
        runner_exe=Path(args.runner).resolve(), bios=Path(args.bios).resolve(),
        rom=Path(args.rom).resolve(), port=args.port,
    )
    corpus_root = Path(args.corpus_dir).resolve()
    corpus_root.mkdir(parents=True, exist_ok=True)
    corpus = Corpus(corpus_root)
    rng = random.Random(args.seed)

    if not corpus.sequences:
        bootstrap_input = Path(args.bootstrap_input).resolve()
        print(f"[bootstrap] replaying {bootstrap_input} to build the root checkpoint...", flush=True)
        t0 = time.time()
        root = bootstrap_root(config, corpus, bootstrap_input, args.bootstrap_tail_frames)
        print(f"[bootstrap] done in {time.time() - t0:.1f}s: "
              f"{root.new_function_count} functions reached, "
              f"checkpoint={root.checkpoint}", flush=True)
    else:
        print(f"[resume] corpus already has {len(corpus.sequences)} kept sequence(s), "
              f"{len(corpus.cumulative)} functions in cumulative set", flush=True)

    start_total = len(corpus.cumulative)
    t0 = time.time()
    kept_count = 0
    for i in range(1, args.iterations + 1):
        it_t0 = time.time()
        entry = run_iteration(config, corpus, rng, i, args.burst_frames)
        dt = time.time() - it_t0
        if entry["kept"]:
            kept_count += 1
        suffix = f" ERROR={entry['error']}" if entry.get("error") else ""
        print(
            f"[{i:4d}/{args.iterations}] {entry['kind']:7s} from {entry['parent']:24s} "
            f"new={entry['new_functions']:4d} total={entry['cumulative_total']:5d} "
            f"kept={'Y' if entry['kept'] else 'n'} ({dt:.1f}s){suffix}",
            flush=True,
        )

    elapsed = time.time() - t0
    print()
    print(f"=== summary: {args.iterations} iterations in {elapsed:.1f}s "
          f"({elapsed / max(args.iterations, 1):.2f}s/iter), {kept_count} kept")
    print(f"    functions: {start_total} -> {len(corpus.cumulative)} "
          f"(+{len(corpus.cumulative) - start_total})", flush=True)
    return 0


def cmd_report(args: argparse.Namespace) -> int:
    corpus = Corpus(Path(args.corpus_dir).resolve())
    print(f"corpus: {corpus.root}")
    print(f"kept sequences: {len(corpus.sequences)}")
    print(f"cumulative functions reached: {len(corpus.cumulative)}")
    print()
    growth = []
    if corpus.growth_path.is_file():
        for line in corpus.growth_path.read_text().splitlines():
            if line.strip():
                growth.append(json.loads(line))
    print(f"attempts logged: {len(growth)}")
    kept = [g for g in growth if g.get("kept")]
    print(f"kept: {len(kept)}  discarded: {len(growth) - len(kept)}")
    print()
    print("growth curve (cumulative_total at each kept iteration):")
    for g in kept:
        print(f"  iter {g['iteration']:5d}  +{g['new_functions']:4d}  -> {g['cumulative_total']:5d}"
              f"  ({g['kind']})")
    print()
    best = sorted(kept, key=lambda g: -g["new_functions"])[:10]
    print("best sequences (most new functions in one attempt):")
    for g in best:
        print(f"  {g['id']:32s} +{g['new_functions']:4d} new")

    if args.elf:
        elf_path = Path(args.elf).resolve()
        elf_bytes = elf_path.read_bytes()
        image = measure_coverage.parse_elf32_arm(elf_bytes)
        sections, shadow_windows = measure_coverage.build_rom_sections(image)
        addrs = [addr for addr, _mode in corpus.cumulative]
        counts = attribute_sections(addrs, sections, shadow_windows)
        print()
        print("cumulative functions by ROM section:")
        for name, count in sorted(counts.items(), key=lambda kv: -kv[1]):
            print(f"  {name:16s} {count:5d}")
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="command", required=True)

    r = sub.add_parser("run", help="run exploration iterations")
    r.add_argument("--runner", required=True, help="path to GoldenSunRecomp.exe")
    r.add_argument("--bios", required=True)
    r.add_argument("--rom", required=True)
    r.add_argument("--corpus-dir", required=True)
    r.add_argument("--iterations", type=int, default=50)
    r.add_argument("--burst-frames", type=int, default=240)
    r.add_argument("--seed", type=int, default=1)
    r.add_argument("--port", type=int, default=19845)
    r.add_argument("--bootstrap-input", default="local/play-sessions/20260807-002319.input")
    r.add_argument("--bootstrap-tail-frames", type=int, default=60)
    r.set_defaults(func=cmd_run)

    rp = sub.add_parser("report", help="summarize a corpus")
    rp.add_argument("--corpus-dir", required=True)
    rp.add_argument("--elf", default=None, help="optional: goldensun.elf, for per-section attribution")
    rp.set_defaults(func=cmd_report)

    return p


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
