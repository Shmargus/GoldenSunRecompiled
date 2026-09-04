# Instructions for Codex — Sol and Luna

This file tells a Codex-based team how to work in this repository.
**Sol** orchestrates and decides. **Luna** executes.

Read in this order before doing anything:

1. `AGENTS.md` — the binding rulebook. It overrides everything, including this file.
2. `docs/history/SESSION_2026-08-13.md` — the current state of the investigation, and
   which leads are dead.
3. `TECHNICAL_HANDOFF.md` — historical context. **Its "NEXT STEP" section is
   wrong**; `docs/history/SESSION_2026-08-13.md` explains why. Do not act on it.

---

## Division of labor

### Sol decides (do not delegate these)

- Whether a fix belongs in this repo or upstream in `gbarecomp`.
- Whether the evidence for an address, a CPU mode, or a data range is actually
  sufficient.
- Diagnosing a divergence that is not yet synchronized to a first differing event.
- Deciding what a first divergence *means*.
- Scope, architecture, milestone sequencing.
- Anything where two readings of the task lead to materially different work.

### Luna executes

Hand off once the *what* and the *why* are settled and only the *doing* remains:

- Implementing a change Sol has already specified.
- Running builds, the Python suites, CTest, linters.
- Running importers, validators, comparison tools.
- Reproducing a documented baseline or a known repro.
- Locating code, symbols, or evidence across the tree.
- Mechanical refactors, docs and config updates.
- Verifying that a fix holds.

Every handoff to Luna must carry four things: the milestone/task ID, the
hypothesis, the concrete change, and the acceptance criteria. Vague handoffs
produce vague work.

Independent, well-scoped tasks can go to several Lunas in parallel. Dependent
work must be sequenced.

### Sol reviews Luna critically

Luna reports what it changed, real command output, evidence gaps, and what it
deliberately did not fix. **A clean report is not the same as a correct change.**
Verify claims that matter before building on them — especially anything touching
evidence status or `generated/**`.

This session produced three concrete examples of why:

- A prior handoff's confidently-worded "next step" was based on a misreading of
  the code. Building it would have won ~1%. Reading the actual source killed it
  in ten minutes.
- A plausible hypothesis (the ISR is secretly interpreting) was measured and
  came back exactly zero. Stated plainly, it redirected the whole investigation.
- A worker reported a build was "still compiling" for over an hour. It had
  actually failed at link and produced a 0-byte binary. Checking the filesystem
  directly settled it.

---

## Rules that survive delegation

Delegation relaxes nothing in `AGENTS.md`. In particular:

- `generated/**` is **never** hand-edited.
- No address and no ARM/THUMB mode is ever guessed. Cite evidence or mark
  `TODO-EVIDENCE`.
- Fixtures are synthetic.
- **No ROM, BIOS, or asset bytes enter the repository — including inside a
  worker's report.** PCs, CRCs, sizes, symbol names, and counters are fine;
  code bytes are not.

## Verification defaults

The Python suites need no ROM and are the cheapest honest check. Run before
every report:

```powershell
python -m unittest discover -s tests -p "test_*.py"
```

ROM-dependent acceptance runs are local-only and hash-gated. Public CI must
never require ROM or BIOS data.

## Measurement discipline

Performance work here has repeatedly gone wrong in the same ways. Guard against
all four:

- **Instrumentation distorts.** A `steady_clock` read on a path called 65M times
  per run inflated wall time 2.24x. Always report probe-on versus probe-off wall,
  and treat the bracketed interval — not the wall delta — as the real number.
- **Re-entrant timers double-count.** `runtime_tick` re-enters itself through
  nested `runtime_dispatch`; a naive timer there reported more time than the
  process had been alive. Only wrap single-level, non-re-entrant containers.
- **Containers overlap.** `tick_devices` and `irq_handler` both include their own
  share of the subsystem rows. Buckets do not sum to 100%, and that is expected —
  say so rather than reconciling it away.
- **Benchmarks under contention are noise.** Check whether
  `GoldenSunRecomp.exe` or another agent's build is running before each measured
  run, and disclose any contended run.
- **`fp_pair_diff.py` checks less than it claims.** Its docstring says each
  record is `(cycles, pc, cpsr, r[0..15])`, but `main()` only compares
  `a[i][0]` (cycles) and `a[i][1]` (pc) — cpsr and all 16 registers are loaded
  but never diffed. Do not trust it alone for a bit-exactness claim; write or
  use a full 20-field comparator.
- **The runtime has a pre-existing, unexplained 1-cycle nondeterminism.**
  Fingerprint-ring index 7253085 (pc 0x08079090) differed by exactly 1 cycle
  between two runs of the SAME unmodified binary; registers and PC were
  identical at that index. Cause unidentified. Always run an A/A control (same
  binary, same inputs, twice) before calling any observed divergence a
  regression.

Methodology for the standard benchmark: `build/gs011/cost_probe.ps1`,
`--frames 1800 --no-window --quiet`, one discarded warmup plus at least five
measured runs, report the spread.

**A negative result is a real result.** Report it plainly. Do not spin it, and
do not keep digging for a confirmation the data does not support.

## Working on this user's machine

The user plays and works on the same machine while agents run.

- **Never pop a console window.** Launch build and script processes with
  `-WindowStyle Hidden` or `-NoNewWindow`.
- The **game window is fine** — run windowed whenever visual inspection helps.
- Do not rebuild or relink `build/gs011` while the user is playing; the exe will
  be locked and their session breaks.

The user is not a programmer. Explain findings in plain cause-and-effect terms —
what it means for the game, not for the codebase. They supply the ROM, the play
sessions, and the logs. The technical judgment is Sol's to carry.

## Environment facts you will need

The user plays via `GoldenSunLauncher.exe` at the repo root, which launches
`build/gs011/GoldenSunRecomp.exe` with **exactly** these five environment
variables and working directory = repo root:

```
GBARECOMP_PRESENT_IN_PLACE=0
GBARECOMP_SELFHEAL_RAM=1
GBARECOMP_AUDIO_NATIVE=0
GBARECOMP_AUDIO_STEREO=1
GBARECOMP_HEAL_CACHE=<repo root>\recomp_cache
```

Reproducing a user-visible symptom means reproducing this environment. Omitting
`GBARECOMP_HEAL_CACHE` silently resolves `recomp_cache` against the process CWD
and warm-loads nothing — this wasted a play session already. Omitting
`GBARECOMP_SELFHEAL_RAM` leaves mutable-RAM overlays interpreter-only, which
disables the entire tier most of this work concerns.

Savestates (`<private-rom-dir>\`):

- **slot 6** — overworld, reproduces the world-map streaking artifact.
- **slot 7** — battle, for the sprite-blit investigation.

Every session is logged to `logs/session_YYYYMMDD_HHMMSS.log` with `[OUT]`/`[ERR]`
tags. The newest path is always in `logs/latest.txt`. Logs survive a force-kill.

PowerShell gotcha that has bitten twice: `Start-Process -ArgumentList` with an
**array** does not quote elements, and every ROM path here contains a space.
Build one pre-quoted argument string instead.

---

## The three open work items

### 1. World-map visual artifact — closest to resolution

Horizontal streaking in the top band (the sea) on the overworld. Reproduces
headlessly from slot 6 and is deterministic across runs. **It alternates on an
exact 2-frame period** — frames 1/3/5 clean, 2/4/6 streaked.

Already refuted, do not re-investigate: normal lazy device catch-up cannot span
a scanline boundary, and HBlank-**DMA**-driven effects are provably correctly
ordered. See `docs/history/SESSION_2026-08-13.md` for the code citations.

The `drain_dma_steal` theory is now **killed by measurement**. Across six
post-load frames, 1,155 steals were observed. The global maximum was 1,026
cycles; the active-display maximum was 514; none reached the 1,232-cycle
scanline length. Clean and streaked frames had no qualifying steal.

The previously documented absolute cycle bracket was stale. Headless savestate
loading resets `g_runtime_cycles` to zero (`runtime.cpp:1613-1624`); the valid
six-frame probe window was `0..1685376`.

**Resolved upstream:** the MMIO/IRQ rings proved DMA0 HBlank repeat alternates
two affine tables. The artifact table has nonzero vertical terms in the damaged
top rows. The old PPU used each DMA-written per-line reference and then applied
the absolute scanline's vertical term again. `gbarecomp` now tracks the hidden
BG2/BG3 affine references and reload/advance behavior. Synthetic HBlank and
save-state tests pass; old slot files remain compatible through an optional
`AFRF` section. Slot 6 frames 1-6 are clean and deterministic.

### 2. Battle lag — RESOLVED (was two separate problems)

Golden Sun generates sprite-blit code into IWRAM at runtime; 44 distinct PCs in
0x03006000..0x0300645C report "DYNAMIC RAM CODE ... no registered identity
matches the live bytes". Self-heal *is* attempted for these. What looked like
one symptom ("battle lag") was actually two independent causes. Both are now
fixed. See `docs/history/SESSION_2026-08-13B.md` for the full session record.

**2a. Our compile storms — fixed in `gbarecomp/src/runtime/overlay_loader.cpp`.**
Two defects compounded:

1. The compile-dedup key (`request_identity`) hashed up to a 16KB surrounding-
   RAM snapshot instead of the actual function extent. Golden Sun's sound
   driver constantly mutates bytes in that window, so byte-identical function
   content produced different identities and dedup failed — the same blit
   variant got recompiled repeatedly.
2. A warm-load race: `warm_load_background` only publishes results once per
   frame via `overlay_drain_ready`, so a dispatch miss could trigger a live
   gcc compile for a routine whose `.dll`/`.pic` already sat on disk waiting
   to be published.

Fixes: a game-thread-only `s_ram_extent_seen` map (`overlay_loader.cpp:134`)
narrows the identity window to the largest real compiled extent seen for that
key, floored at `kMinRamIdentityWindow = 64` bytes (`overlay_loader.cpp:142`),
with a full-snapshot fallback on first sighting. Second, a targeted synchronous
warm-lookup for just the missed key while the warm load is in flight, so a miss
no longer forces a live compile when the cached body already exists on disk.

Verified: 19/19 CTest, 86/86 Python. Measured before/after across play sessions
`session_20260813_195708` → `session_20260813_212941`:

| metric | before | after |
|---|---|---|
| interpreted_insns | 870,336 | 34,221 |
| ram_crc_mismatches | 1,587 | 82 |
| relocatable_alias hits/misses | 0 / 41 | 241 / 6 |
| dominant miss PC 0x03006320 | 393 misses | 0 |
| worst frame | 136.8ms (6-frame compile storm) | 49.8ms one-time first-touch, no storm |

**2b. The real GBA's own slowdown — see section "Guest CPU overclock" below.**
Even with the compile storms gone, the *real hardware* is legitimately
CPU-bound during heavy battle VFX; that part of the lag is not a bug in this
port, it is Golden Sun running at 1x GBA speed. The overclock feature gives the
player a way to buy headroom past that ceiling.

**Superseded:** the earlier "pre-warm or ship the cache plus content-keyed
lookup" proposal from this section is no longer the plan — the identity-window
and warm-lookup fixes above address the same symptom at the actual root cause.
Cache bytes remain protected local material: never commit, package, log, or
upload `.pic`/`.dll` files from `recomp_cache`.

### Guest CPU overclock — SHIPPED, user-confirmed

What it does for the player: an optional multiplier (1x/2x/4x/8x) that lets the
emulated GBA CPU do more work per real-world frame, so scenes that are
genuinely CPU-bound on real hardware (heavy battle VFX) run smoother. Video and
audio timing are untouched — only how much guest CPU work fits inside the same
real 1/60s slice changes.

**Where it lives:** `gbarecomp/src/runtime/runtime_bus_bridge.cpp:969-1172`.
The old single `runtime_tick` was split into three functions:

- `runtime_tick_common` — the original shared body, moved verbatim.
- `runtime_tick` — the scaling door. Every generated-code, bridge, and
  interpreter call site uses this one.
- `runtime_tick_realtime` — unscaled, called ONLY by `pump_idle`
  (`gbarecomp/src/runtime/runtime.cpp:1463-1466`).

**The halt-vs-work split is the whole correctness argument.** Halt/idle time is
real elapsed hardware time and must never be scaled — a "halt until the next
device event" wait is a wait, not CPU work, and scaling it would desync PPU,
audio, timers, and IRQ cadence from the master clock. Executed-instruction cost
is CPU work, and that is the only thing the overclock is allowed to scale.
`drain_dma_steal` and `kIrqWakeDelayCycles` add cycles to `g_runtime_cycles`
directly rather than through `runtime_tick`, so DMA-stolen time and IRQ wake
latency are unscaled too, by construction.

**Mechanics:** scaling uses an exact integer carry accumulator, so charged
cycles sum to exactly `floor(total_unscaled_cycles / factor)` with no drift.
The factor is a relaxed `std::atomic<unsigned>`
(`runtime_set_overclock_factor` / `runtime_get_overclock_factor`), seeded once
at startup from env `GBARECOMP_CPU_OVERCLOCK`, and overridable live from the
"CPU Overclock" combo on the Enhancements tab
(`gbarecomp/src/runtime/host_config_ui.cpp:309-312`). The choice persists as
`CpuOverclock=` in the config file's `[Enhancements]` section
(`gbarecomp/src/runtime/host_window.cpp:1311-1318,1785-1797`). Changing the
factor resets the carry to 0 — a deliberate, bounded, one-time loss of at most
a few unscaled cycles of buffered charge, accepted to keep the hot path free of
extra logic.

**Why it works, plainly:** the PPU, audio, timers, DMA, and IRQ system all
consume the same master clock, which the overclock never touches. Charging
less simulated cost per guest instruction just lets more instructions run
before that same fixed 280,896-cycle real GBA frame elapses — nothing about
when video renders or audio plays changes. It is self-limiting: when the guest
isn't CPU-bound, the extra headroom just means it reaches the next halt sooner
in real time.

**Correctness rule for future edits:** default (1x) must stay a bit-exact
no-op. This was verified across 8,388,608 fingerprint records on all 20 fields
(cycles, pc, cpsr, r0-r15) — 1x produces byte-identical execution to the
pre-overclock build. **Any future edit to `runtime_tick` or its call sites must
re-run that full 20-field fingerprint comparison at 1x before being trusted**
(see the `fp_pair_diff.py` gotcha below — the shipped comparator does not check
all 20 fields itself).

**User-confirmed, not instrumented:** the user play-tested 2x and 4x live and
reports battle VFX is smooth at both, with no music or animation problems. 8x
was not tried by choice. The throughput gain itself has not been measured with
a stopwatch or probe — see `docs/history/SESSION_2026-08-13B.md` for why (no available
savestate captures a CPU-bound moment).

### 3. Per-instruction bookkeeping — measured, hypothesis unproven

Recompiled code runs at roughly **167ns per guest instruction** — interpreter-
grade, not native-grade. Measured call volumes per 1800 frames:
`runtime_tick` = 64,733,186; `runtime_mem_cycles` = 112,831,449. The calls are
genuinely out-of-line (7,893 `IMAGE_REL_AMD64_REL32 runtime_tick` relocations in
`recompiled_000.cpp.obj` alone), and `build/gs011` has
`CMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF`.

**The A/B experiment never completed. Do not treat the hypothesis as
established.** The inline-fast-path variant was judged too risky to attempt
casually: it needs `g_active_bus` / `g_active_ppu` / `g_pending_cycles` /
`g_event_budget` exposed out of `runtime_bus_bridge.cpp`, and any partial
duplication of the `drain_dma_steal` / IRQ-pending check risks altering
IRQ-delivery timing.

**Hard constraint on any attempt here:** semantics must be bit-identical. When
devices materialize, when IRQs are delivered, and the wake-from-HALT latency path
must not change. The repo has a per-instruction fingerprint ring
(`GBARECOMP_FP_SAVE`); two builds' dumps must diff bit-for-bit. **If the
fingerprints differ, the variant is wrong no matter how fast it is.**

Also open: whether LTO is viable on this machine at all. Both
`build/gs011_lto_probe` and `build/gs011_tickprobe` produced 0-byte binaries.
Mid-link showed GCC `-flto=auto` WPA `lto1` near 3.2GB RSS plus ~16 LTRANS
workers at ~450-480MB each, with free memory dropping to ~1.2GB on a 16.7GB
machine. **No captured stderr proves the cause** — get the actual link error
before concluding anything about LTO.

*Cleanup owed:* `gbarecomp/src/runtime/runtime_bus_bridge.cpp` carries an
uncommitted throwaway counter (`g_cost_mem_cycles_total_calls`); revert it or
keep it deliberately before any commit. `build/gs011_tickprobe` holds stale
0-byte link artifacts.

---

## The user's goals, for prioritization

A Golden Sun PC port that runs fast and opens the door to modding. 1x real-time
is the floor, not the target — they want uncapped turbo headroom with game logic
still at 60Hz. Audio should be bit-faithful *and* efficient. Battle smoothness is
the symptom they actually feel, so weigh it above headless benchmark numbers.

## Visual enhancement follow-up — 2026-08-13

See `docs/history/VISUAL_ENHANCEMENTS_2026-08-13.md`.

- F1 Video has live color profiles; Raw remains faithful default.
- Integer scaling is verified at 3440x1440 (uniform 9x).
- Native supersampling verifies the original pixel lattice and falls back per
  frame. Battle verified 108/120 frames; world map rejected 120/120 safely.
- 2x interpolation now uses per-scanline IO/affine state and stable short sprite
  motion. Slot-7 battle improved from 0/120 to 65/120 midpoint frames; slot-6
  world map remains 0/120 with exact safe fallback.
- Widescreen remains hidden and clamped to 240. World map is the best first
  research target; unknown scenes must pillarbox.
