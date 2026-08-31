# BIOS-FREE-01 — Golden Sun BIOS-dependency census

Status: informational; analysis only. No source, build, or runtime behavior
was changed to produce this document. This is a first-step census toward an
eventual BIOS-free mode; it does not recommend changing the project's BIOS
policy.

## Objective

Determine, statically, everything Golden Sun (U) actually needs the GBA BIOS
for: which SWIs it calls, whether it depends on the BIOS IRQ vector or on
BIOS-region reads, and what would be required to run it without a real BIOS
image, without running the game and without guessing any address or mode.

## Method

- Generated-code root: `GSR_GENERATED_DIR` from
  `build/gs011_opt/CMakeCache.txt` → `local/gs011/main` (32 shards,
  `recompiled_000.cpp`…`recompiled_031.cpp`, plus `dispatch_table.cpp`,
  `recompiled.h`, `symbol_map.cpp`). This is the current statically
  recompiled snapshot of the main-image game code (last regenerated
  2026-08-25).
- Also scanned all 90 `local/gs011/overlay_rom_*` directories (each has its
  own `recompiled_NNN.cpp`, `dispatch_table.cpp`, `symbol_map.cpp`).
- No ROM file or BIOS image was needed for this pass (`verify_rom.py` was not
  run because nothing here reads the ROM directly).
- Traced how the recompiler encodes an ARM/THUMB `SWI` instruction:
  `gbarecomp/src/armv4t/arm_ir.h:91,203-204` (IR op `SWI`, field
  `swi_imm`) → decoded in `arm_decode.cpp:303-308` (ARM: `bits(word,23,0)`,
  unshifted) and `thumb_decode.cpp:423-429` (THUMB: `bits(hw,7,0)`) →
  codegen `arm_codegen.cpp:1408-1415` (`emit_swi`) emits exactly:
  ```
  g_cpu.R[15] = <pc + 2 or 4>;
  runtime_swi(<swi_imm as 0xXXXXXXXXu>);
  ```
  So every SWI call site in generated code is a literal
  `runtime_swi(0x...)` call. Searched all generated `.cpp` for that literal
  call.
- Cross-referenced call sites against `runtime_swi` in
  `gbarecomp/src/armv4t/runtime_arm.cpp:2249-2300` (dispatch: opt-in HLE
  hook first, else recompiled/LLE BIOS via `runtime_dispatch(0x08)`) and
  against `gbarecomp/src/runtime/bios_hle.cpp` (the actual `switch(swi)`,
  lines ~464-498) for HLE coverage.
- Searched generated code for literal reads/writes with a target address in
  the BIOS ROM range (`bus_read_u8/16/32(0x0000####u)` /
  `bus_write_*(0x0000####u)`, i.e. 0x00000000-0x0000FFFF, a superset of the
  0x0-0x3FFF BIOS region).
- Read the IRQ path (`runtime_arm.cpp:2249-2419`, `gba_irq.h`) and the
  boot-skip HLE helper (`bios_hle.cpp:513-535`, gated by
  `runtime.cpp:1005-1009`) to establish IRQ-vector and boot-sequence
  dependence.
- Confirmed ARM vs THUMB mode for every found SWI call site directly from
  the codegen's own PC-advance (`pc+2` = THUMB, `pc+4` = ARM — see
  `arm_codegen.cpp:1410-1411`), not by assumption.

## SWI call-site census (main image)

9 `runtime_swi(...)` call sites found in the entire main-image generated
code (32 shards); 0 found in any of the 90 overlay images.

| SWI # | Standard name | Call sites (main) | Mode (proven) | HLE coverage |
|---|---|---:|---|---|
| 0x02 | Halt | 4 | THUMB (all 4; `pc+2`) | **NOT COVERED** — no `SWI_HALT` case in `bios_hle.cpp` |
| 0x03 | Stop/Sleep | 1 | THUMB (`pc+2`) | **NOT COVERED** — no `SWI_STOP` case |
| 0x0B | CpuSet | 1 | THUMB (`pc+2`) | COVERED — `bios_hle.cpp:484` (`SWI_CPU_SET`) |
| 0x19 | SoundBias | 2 | THUMB (all 2; `pc+2`) | COVERED — `bios_hle.cpp:497` (`SWI_SOUND_BIAS`) |
| 0x2A | SoundDriverGetJumpList (undocumented MP2K sound-driver SWI) | 1 | THUMB (`pc+2`) | **NOT COVERED** — sound-driver SWIs (0x1A-0x2A) have no case in `bios_hle.cpp` |

Exact call sites (guest PC of the `SWI` instruction, its return PC as emitted,
and the immediate literal in generated code):

| Guest PC | Return PC (proves mode) | `swi_imm` literal | File |
|---|---|---|---|
| 0x0800686A | 0x0800686C (+2 → THUMB) | 0x00000019 | `recompiled_001.cpp:7168` |
| 0x080033CE | 0x080033D0 (+2 → THUMB) | 0x00000002 | `recompiled_026.cpp:2523` |
| 0x080033F2 | 0x080033F4 (+2 → THUMB) | 0x00000003 | `recompiled_026.cpp:2870` |
| 0x08006872 | 0x08006874 (+2 → THUMB) | 0x00000019 | `recompiled_026.cpp:6456` |
| 0x0800329E | 0x080032A0 (+2 → THUMB) | 0x00000002 | `recompiled_011.cpp:2067` |
| 0x08003322 | 0x08003324 (+2 → THUMB) | 0x00000002 | `recompiled_024.cpp:2656` |
| 0x0800345C | 0x0800345E (+2 → THUMB) | 0x00000002 | `recompiled_018.cpp:1108` |
| 0x080FA674 | 0x080FA676 (+2 → THUMB) | 0x0000002A | `recompiled_018.cpp:151705` |
| 0x08006864 | 0x08006866 (+2 → THUMB) | 0x0000000B | `recompiled_029.cpp:9323` |

Corroboration: `gbarecomp/bios/gba_bios.toml:588` independently records "Golden
Sun strict-static trace: indirect CpuSet continuation" against the recompiled
BIOS's own SWI 0x0B handler — consistent with the one CpuSet (0x0B) call site
found here.

Caveat (real, not a guess): the game's static coverage is not 100% closed —
`COV-01.md` records open coverage misses, and
`recomp_master_misses_AGSE.toml.frag` shows a handful of IWRAM addresses
bridged by the interpreter this session (none of them decode to a `SWI` op in
what has been regenerated so far). This census is therefore a **lower bound**
on the distinct SWI set: it covers everything currently statically
recompiled, not code that is undiscovered or still interpreter-bridged. No
undiscovered-code SWI call has been found or is claimed.

## Non-SWI BIOS dependencies

1. **IRQ vector (0x18).** `runtime_irq()` (`runtime_arm.cpp:2326` onward)
   unconditionally calls `runtime_dispatch(0x18)` into the recompiled BIOS
   IRQ handler — there is no HLE path for IRQ delivery at all (matches the
   stated "NOT implemented in HLE" list). Every interrupt Golden Sun takes
   (VBlank, timers, DMA, keypad, etc.) is delivered through this recompiled
   BIOS code, not bypassed. This is an architectural fact of the runtime, not
   something specific to Golden Sun's own code.
2. **User IRQ handler pointer at 0x03007FFC.** The BIOS's IRQ dispatcher
   reads a game-installed function pointer from IWRAM `0x03007FFC`
   (confirmed from `bios_hle_boot_skip()`, `bios_hle.cpp:531-534`, which
   deliberately zeroes this exact address when skipping the BIOS intro,
   "the recompiled BIOS dispatcher (vector 0x18) reads"). Golden Sun's own
   write of its handler address to this location was **not** found via a
   literal-address grep (see next point on why), but the mechanism itself
   is proven from the runtime side.
3. **Boot sequence.** The BIOS-file gate (`runtime.cpp:1005-1009`) runs
   before boot-skip is even considered, so the opt-in `bios_hle_boot_skip()`
   path still **requires a valid, hash-matched real BIOS file to be loaded**
   — it only skips *executing* the reset/intro code afterward
   (`bios_hle.cpp:513-535`), synthesizing the post-boot CPU/bank state GBATEK
   documents for GBA reset. It does not remove the BIOS-file requirement by
   itself.
4. **Literal BIOS-region (0x0-0x3FFF) reads/writes.** Searched every
   generated `.cpp` (main + all 90 overlays) for a literal
   `bus_read_u8/16/32(0x0000####u)` / `bus_write_*(0x0000####u)` target.
   **Zero matches.** No deliberate open-bus/protected-BIOS-region literal
   access was found in the recompiled code.
   - Method limitation (not a finding of "no dependency"): the codegen only
     folds a memory target into a compile-time literal when the ARM/THUMB
     addressing mode is PC-relative (i.e., the instruction's own address,
     always in cart/IWRAM/EWRAM space, never BIOS space for game code).
     A register-indirect access whose base register was loaded from a
     ROM literal-pool constant equal to a BIOS address would appear in
     generated code as a **dynamic** `bus_read_u32(<runtime register
     expression>)` call, not a literal, and is invisible to this grep. So
     "no literal BIOS-region access found" is proven; "no BIOS-region
     access of any kind" is **not** provable this way and is left
     **TODO-EVIDENCE** (would need either full constant-propagation over
     the generated IR, or a runtime read-range probe against real hardware
     timing/behavior).
   - Generic open-bus semantics for unmapped/protected reads already exist
     at the bus level (`gbarecomp/src/gba/gba_bus.cpp:163-353`), and are
     GBA-hardware-generic, not Golden-Sun-specific.

## What would be required — gap list (easiest to hardest)

1. **SoundBias (0x19) and CpuSet (0x0B)** — already HLE-covered; no new work
   needed for these two to run BIOS-free.
2. **Halt (0x02)** — not HLE-covered. Reimplementing it faithfully requires
   modeling exactly when a pending IRQ resumes execution, including the
   documented `kIrqWakeDelayCycles = 7` wake latency the runtime already
   tracks as "single source of truth" for LLE/interpreter parity
   (`gba_irq.h:18-28`). An HLE Halt that doesn't reproduce this latency risks
   the same class of drift that previously caused a sequencer/audio hang
   (documented in that file as the cause of a prior regression). This makes
   Halt harder than the already-covered math/decompression SWIs but is a
   known, bounded piece of behavior.
3. **Stop/Sleep (0x03)** — not HLE-covered, only 1 call site found. Stop
   additionally powers down more of the system (sound, most clocks) than
   Halt before waking on a specific subset of IRQ sources; faithful
   reimplementation needs the same IRQ/timing care as Halt, plus correctly
   modeling which peripherals keep running.
4. **SWI 0x2A (sound-driver "GetJumpList")** — not HLE-covered; it is part
   of the MP2K/m4a sound-driver SWI family (0x1A-0x2A), which the stated
   background already flags as unimplemented as a group. Only one call site
   was found for Golden Sun specifically, but the surrounding driver
   SWIs (Init/Mode/Main/VSync/ChannelClear, 0x1A-0x1E) are used by the
   sound engine as a whole even where this game's code doesn't call every
   one of them directly — see `gbarecomp/src/gba/mp2k_shadow.cpp` and
   `mp2k_wall_mixer.cpp`, which already model MP2K behavior at the runtime
   level outside the SWI path. Faithfully HLE'ing 0x2A in isolation without
   the rest of the sound-driver family is the kind of partial reimplementation
   that risks a silent behavior mismatch; this needs a scoped decision, not
   a guess.
5. **IRQ vector (0x18) delivery.** No HLE path exists for this at all today.
   Any BIOS-free mode has to either (a) keep dispatching interrupts through
   a recompiled/emulated BIOS IRQ handler (i.e., not truly "BIOS-free" for
   IRQs), or (b) implement the entire ARM7TDMI exception-entry sequence and
   the specific ack/return convention the game's installed handler at
   `0x03007FFC` expects, independently of any real BIOS bytes. This is the
   largest and hardest-to-verify gap, since it touches every interrupt the
   game takes every frame, and the exact latency/ack semantics are what
   previously caused a real divergence (`kIrqWakeDelayCycles`) between
   engines.
6. **Boot sequence.** `bios_hle_boot_skip()` already exists and is exercised
   as an opt-in path, but it still requires a real, hash-verified BIOS file
   to be present at startup (the gate runs first, unconditionally). Making
   the *loader* itself BIOS-free (no BIOS file required at all) is a
   separate, larger change from anything analyzed here and was out of scope
   for this census.
7. **Possible undiscovered SWIs.** Per `COV-01.md`, coverage is not fully
   closed. Any SWI inside a not-yet-recompiled/interpreter-bridged function
   would not appear in this census. Closing `COV-01` (or otherwise proving
   100% static coverage) would be needed before this SWI list can be called
   complete rather than a lower bound.

## Evidence status summary

- SWI call-site set, counts, and ARM/THUMB mode: **proven** from generated
  code and codegen source, not guessed.
- HLE coverage per SWI: **proven** from `bios_hle.cpp`'s `switch` statement.
- IRQ-vector-always-LLE fact: **proven** from `runtime_arm.cpp`.
- User IRQ handler pointer address (`0x03007FFC`): **proven** from the
  runtime side (`bios_hle_boot_skip`); Golden-Sun-code-side confirmation of
  the write to that address is **TODO-EVIDENCE** (not found by literal-address
  grep; would need dataflow tracing of the register that holds it).
- "No BIOS-region access of any kind": **not proven**, only "no literal
  BIOS-region access found" — the gap between those two is explicitly
  **TODO-EVIDENCE** and explained above (dynamic/register-indirect access is
  invisible to a literal-address search).
- Completeness of the SWI list against the whole game: **lower bound only**,
  gated on `COV-01` closing. Stated as such, not claimed as final.
