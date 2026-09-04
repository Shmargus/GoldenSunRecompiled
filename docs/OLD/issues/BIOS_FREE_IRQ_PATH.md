# BIOS-FREE-02 — IRQ path analysis (deciding-feasibility step)

Status: informational; analysis only. No source, build, or runtime behavior
was changed to produce this document. This follows `BIOS_FREE_CENSUS.md` and
is scoped to the one gap that census flagged as largest/hardest: **IRQ vector
0x18 delivery**. It records facts to decide feasibility; it does not
recommend changing the project's BIOS policy (`docs/PARKED.md` "BIOS-01" is
explicitly parked at the user's request).

## Objective

Trace the current IRQ path end-to-end with citations, state precisely what a
BIOS-free replacement would have to reproduce, determine what Golden Sun
itself depends on, record existing scaffolding, and give a feasibility
verdict plus an ordered implementation plan — without guessing any address,
mode, or byte sequence that isn't proven.

## 1. The current path (traced, cited)

1. **Eligibility check — generic runtime, not BIOS bytes.**
   `runtime_tick_common()` (`gbarecomp/src/runtime/runtime_bus_bridge.cpp:1140`)
   checks `bus->io().irq_pending() && (g_cpu.cpsr & CPSR_I_BIT) == 0` once per
   flush boundary (per instruction / per event-budget tick). This is the
   ARM7TDMI "IRQ becomes eligible at the next instruction boundary" rule —
   generic ARMv4T semantics, independent of any BIOS bytes.
2. **HALT-wake latency.** If the CPU is halted
   (`runtime_bus_bridge.cpp:1143-1171`), the code clears halt, ticks devices
   for `kIrqWakeDelayCycles = 7` (`gbarecomp/src/gba/gba_irq.h:28`) to model
   the ARM7TDMI's fixed wake + pipeline-refill latency, advances
   `g_runtime_cycles`, then re-arms the event horizon. This models
   documented ARM7TDMI/GBA hardware timing (comment cites the prior
   MC-HP-002 regression from omitting it), not BIOS code — it happens
   *before* `runtime_irq()` is called.
3. **`runtime_irq(return_address)`** (`gbarecomp/src/armv4t/runtime_arm.cpp:2326`)
   performs the ARM ARM A2.6 exception-entry sequence in the generic runtime
   (not BIOS bytes): computes `new_cpsr` = IRQ mode (0x12), ARM state
   (T clear), IRQ masked (I=1) (`:2359-2362`); banks in/out registers via
   `mode_to_bank`/`bank_out`/`bank_in` (`:2364-2369`); sets
   `g_cpu.banked_spsr[new_bank] = saved_cpsr` — **SPSR_irq ← old CPSR**
   (`:2372`); sets `LR_irq = return_address + 4` (`:2373`, the documented
   GBA/ARM IRQ return-address adjustment); sets `PC = 0x00000018`
   (`:2374`, the hardware IRQ vector).
4. **Dispatch into recompiled BIOS.** `runtime_dispatch(0x18)` (`:2407`)
   enters the recompiled BIOS code at the vector. Per
   `gbarecomp/bios/gba_bios.toml:39-43`, address `0x18` is named
   `bios_irq_vector` ("B irq_vector — IRQ entry dispatches here"), branching
   to `irq_vector` at `0x128` (`gba_bios.toml:81-84`), which falls through to
   `after_interrupt` at `0x138` (`gba_bios.toml:86-89`) before the real
   `swi_vector` at `0x140` (`gba_bios.toml:91-94`). These are the disassembly
   project's own symbol names (`third_party/gba_bios_disasm`, not vendored
   in this checkout — see Evidence status), imported via
   `tools/import_bios_symbols/import_bios_symbols.py`; they corroborate the
   well-known public GBA BIOS layout (GBATEK "BIOS RAM Usage" / "Interrupt
   and Return") without this analysis reading or citing BIOS ROM bytes.
   **What that BIOS code does internally (register push order, exact
   instruction sequence) was not read from ROM bytes for this document and
   is not reproduced here — see §2 for what's provable vs. TODO-EVIDENCE.**
5. **Runtime drives the handler to completion.** `runtime_irq()`'s own
   dispatch loop (`runtime_arm.cpp:2396-2420`) keeps re-dispatching
   `g_cpu.R[15]` until the IRQ-mode `runtime_exception_return` at this
   nesting depth fires (`g_irq_iret_depth == my_depth`), because a SWI
   executed *inside* the handler would otherwise unwind the host C stack via
   the call-cancel cascade and abandon the handler mid-flight (documented
   FireRed regression in the comment at `:2380-2389`).
6. **Exception return (`SUBS PC, LR, #4` equivalent).**
   `runtime_exception_return(new_pc)` (`runtime_arm.cpp:2187-2212`) restores
   `CPSR ← SPSR_irq` (bank-out/bank-in), sets `PC = new_pc`, and — because
   `old_mode == 0x12` (IRQ) — records `g_irq_iret_depth = g_irq_nest_depth`
   (`:2211`) so the driving loop in step 5 knows this IRQ's handler has
   fully returned. This function is generic ARMv4T exception-return
   semantics (used for SWI/FIQ/undef returns too), not BIOS-specific.
7. **User handler dispatch at IWRAM `0x03007FFC` and IF-acknowledge at
   `0x03007FF8`.** These two addresses are proven from the *runtime* side,
   not from reading BIOS bytes:
   - `bios_hle_boot_skip()` (`gbarecomp/src/runtime/bios_hle.cpp:531-534`)
     deliberately zeroes `0x03007FFC` because "the recompiled BIOS
     dispatcher (vector 0x18) reads" it — i.e., the real BIOS IRQ handler
     reads a user-installed function pointer from `0x03007FFC` and calls it.
   - `runtime_swi_log_record(...)` is called with
     `bus_read_u32(0x03007FF8u)` (`runtime_arm.cpp:2254`) labeled "BIOS
     IntrWait flags" (`runtime_arm.h:849-850`, `runtime_arm.cpp:1481`
     "flags at 0x03007FF8, so a recomp-vs-interp SWI-5 sequence diff shows
     ..."), and `tools/bios_smoke/main.cpp:982-992` independently documents
     "BIOS_IF flag is at IWRAM offset 0x7FF8" read alongside
     "user_handler_ptr (IWRAM[0x7FFC])". This is the real BIOS's own
     IntrWait/VBlankIntrWait bookkeeping location (GBATEK "BIOS RAM Usage"),
     confirmed here from two independent runtime call sites, not assumed.
   - The exact order of operations inside the real handler (does it ack IF
     before or after calling the user handler; does it OR into
     `0x03007FF8` unconditionally or only if a user handler ran) was **not**
     read from BIOS bytes for this document and is **TODO-EVIDENCE** (see
     §2).
8. **HLE has no IRQ path at all, by design.** `bios_hle.h:44-49` states
   explicitly: "The recompiled BIOS stays linked and still services IRQs
   (vector 0x18) ... only the boot ANIMATION is skipped." `runtime_swi()`
   (`runtime_arm.cpp:2249-2300`) has the `g_bios_hle_hook` seam for SWIs;
   `runtime_irq()` has no equivalent hook anywhere in its body
   (`:2326-2430`) — confirmed by reading the whole function, not inferred.

## 2. What the real BIOS does that a replacement would have to reproduce

Provable from the runtime side (§1) without reading BIOS bytes:

- Exact CPU/mode state on IRQ entry: IRQ mode (0x12), ARM state, IRQ masked,
  `SPSR_irq` = pre-IRQ CPSR, `LR_irq` = return_address + 4, `PC` = 0x18.
  (Already generic runtime code, not something a BIOS-free path needs to
  add — it happens before BIOS code runs at all.)
- A user-installed handler pointer is read from IWRAM `0x03007FFC` and
  called (mechanism proven; calling convention — ARM vs. THUMB entry, which
  register holds the return address, BLX vs. MOV LR+BX — is
  **TODO-EVIDENCE**, would need either disassembly of the real BIOS bytes
  against a primary reference (GBATEK/mGBA `src/gba/bios.c` IRQ handler) or
  an oracle trace showing the mode/registers at the instant the user handler
  starts executing).
- `0x03007FF8` accumulates BIOS-side IntrWait/VBlankIntrWait flags, read by
  those two SWIs to decide when to return from their wait loop. The
  precise ack semantics (is it `OR`'d with the IE&IF mask taken this IRQ,
  written before or after the user handler runs, cleared by SWI 4/5 on
  consumption) is **TODO-EVIDENCE** for the same reason as above.
- **Stack usage during the handler**: which banked stack (`SP_irq`, per
  `gbarecomp/src/runtime/bios_hle.cpp:523` set to `0x03007FA0`) is used, how
  many words the entry sequence pushes/pops, and whether the handler
  temporarily switches to System mode to call the user handler (a common,
  GBATEK-documented BIOS convention that lets the user handler take a
  nested IRQ) — **TODO-EVIDENCE**, not read from ROM bytes for this
  analysis and not independently corroborated elsewhere in this repository
  (`gbarecomp/src/runtime/generated_bios/*` contains the actual recompiled
  BIOS logic derived from ROM bytes; per project rules this document does
  not quote or paraphrase that derived content as a substitute for
  ROM-byte-level evidence).
- **HALT-wake latency**: `kIrqWakeDelayCycles = 7` (`gba_irq.h:18-28`) is
  already modeled at the runtime level (§1.2), applies once per frame (the
  game `VBlankIntrWait`s every frame), and is explicitly called out as the
  cause of a real prior regression (MC-HP-002, a sequencer/audio hang) when
  omitted. Any BIOS-free IRQ path must preserve this exact latency value
  and its placement (before vectoring, not folded into the vector-entry
  cost) — this is a runtime-level requirement already met by the current
  architecture (the latency is applied before `runtime_irq()` regardless of
  what runs at 0x18), not a new thing a replacement would introduce.
- **Register/mode state left on return to mainline code**: proven — CPSR
  restored from `SPSR_irq`, PC = the interrupted return address
  (`runtime_exception_return`, §1.6). What R0-R3/R12/LR look like at that
  point depends on whether the real BIOS handler preserves them
  (documented GBATEK convention: it does, via push/pop around the user-
  handler call) — **TODO-EVIDENCE** for the exact register set preserved
  vs. clobbered, same reason as above.

## 3. What Golden Sun depends on

- **Does it install its own handler at `0x03007FFC`?** Not found by literal-
  address grep of the generated main-image code
  (`local/gs011/main/*.cpp`, `Select-String -Pattern "0x03007FFC"` /
  `"0x03007FF8"` → zero matches, checked directly for this document). This
  is the same limitation `BIOS_FREE_CENSUS.md` already documents: the
  codegen only folds a memory target into a compile-time literal for
  PC-relative addressing; a register-indirect store (e.g. `LDR r0,
  =0x03007FFC` from a literal pool then `STR r1, [r0]`) is invisible to this
  grep and would appear as a dynamic `bus_write_u32(<expr>, ...)` call.
  **The mechanism (some GBA game must write this pointer or no interrupt
  ever calls into game code) is certain; Golden-Sun-code-side confirmation
  of the write is TODO-EVIDENCE**, would need dataflow/constant-propagation
  tracing of the register that holds `0x03007FFC`, not found here. No
  symbol name for an interrupt-handler installer exists in
  `symbols/` (grepped for `Intr`/`Irq`/`InterruptHandler`/`SetVBlank...` —
  no matches), consistent with Golden Sun's symbol corpus being
  address-based rather than named for this area.
- **Does it use IntrWait/VBlankIntrWait?** `BIOS_FREE_CENSUS.md`'s SWI
  census found **zero** call sites for SWI 0x04 (IntrWait) or SWI 0x05
  (VBlankIntrWait) in the entire main-image generated code (9 SWI sites
  total: Halt x4, Stop x1, CpuSet x1, SoundBias x2, sound-driver 0x2A x1 —
  see that document's table). Re-confirmed here: neither `0x04` nor `0x05`
  swi_imm appears among the 9 recorded call sites. Golden Sun's own wait
  primitive is **Halt (SWI 0x02)**, not IntrWait/VBlankIntrWait — so it does
  **not** directly depend on the `0x03007FF8` BIOS-IF-flags wait-loop
  semantics that IntrWait/VBlankIntrWait read. It still depends on Halt
  correctly waking on *any* enabled, pending interrupt via the standard
  IE/IF/IME path, and it depends on whatever handler it installed at
  `0x03007FFC` actually running once per interrupt (VBlank at minimum, to
  drive its main loop / Halt wake).
  - Caveat carried over from the census: this is a **lower bound** — code
    that is not yet statically recompiled (interpreter-bridged / healed at
    runtime, per `COV-01.md`) would not appear in this SWI census. No
    IntrWait/VBlankIntrWait call has been found; none is claimed to be
    impossible.

## 4. Existing scaffolding

- **`gbarecomp/ENHANCEMENTS.md` §2, "No-BIOS HLE tier — DEFERRED (documented,
  not planned)"** (`ENHANCEMENTS.md:59-96`) already names this exact gap as
  item 2 of 3: *"HLE the IRQ entry — replace the recompiled BIOS IRQ
  dispatcher at `0x18` (register save/restore, read user handler at
  `0x03007FFC`, call it, `IF` ack) with an in-runtime equivalent, so no BIOS
  code executes on an interrupt."* (`:75-77`). The document's own decision,
  dated 2026-07-02: *"not worth building now... Revisit only if shipping to
  users without a BIOS dump becomes a real requirement."* No code exists for
  it — it is a decision record, not a partial implementation.
- **`docs/PARKED.md` "BIOS-01 — BIOS-free opt-in mode"** (`PARKED.md:15-23`,
  this repository, parked 2026-08-15 at the user's request) lists resume
  criteria including *"Halt/IRQ replacement is implemented in this
  repository"* — i.e., this exact gap is a named blocking condition for
  resuming that effort, already tracked, not newly discovered here.
- **`gbarecomp/docs/ARCHITECTURE.md:39-40`** explicitly lists "fake IRQ
  dispatch" under "Things we must NOT borrow as truth" / must not build as a
  convenience shortcut — a standing architectural constraint any IRQ-HLE
  design must satisfy (real hardware basis required, not a game-specific
  workaround).
- **No partial IRQ-HLE code or test exists.** `bios_hle.cpp`/`bios_hle.h`
  were read in full (§1.8); there is no IRQ hook, no `SWI_INTR_WAIT`/
  `SWI_VBLANK_INTR_WAIT` case (both fall through to LLE by the `default:`
  case, `bios_hle.cpp:499-500`), and no test file under
  `gbarecomp/tests` or this repo's `tests/` referencing an IRQ-HLE path
  (searched for "IRQ.*HLE"/"HLE.*IRQ" project-wide; only the deferred-plan
  prose above matched).

## 5. Risk / verifiability — is there an oracle to diff against?

Yes, at the `gbarecomp` toolchain level (generic, not Golden-Sun-specific):

- **`gbarecomp/oracle/diff_irq_nba.py`** — an existing "Axis 3 (Interrupt /
  event timing)" comparator against a NanoBoyAdvance oracle. It aligns IRQ
  TAKE events by `(source, Nth-of-source)` via the recomp's always-on
  `irq_cap` TCP ring (`g_runtime_irq_log_*` machinery, populated by
  `runtime_irq_log_record` in `runtime_arm.cpp`) and diffs take-to-take
  cycle intervals, reporting missing/extra IRQs per source and sequence
  mismatches. This is exactly the kind of tool that could diff a BIOS-free
  IRQ path against the current (BIOS-executing) path, source-of-truth-wise
  it diffs recomp-vs-NBA today, not recomp-vs-recomp(HLE), but the same
  `irq_cap` ring and comparator shape would work for an internal
  LLE-vs-HLE diff.
- **`gbarecomp/COSIM_ORACLE.md`** documents a broader, only partially-built
  plan (checklist items marked `[ ]`, not shipped) for a full state-hash
  co-simulation covering "Interrupt controller (`gba_irq.h`): IE / IF / IME
  + pending/latched state, the HALT-wake latency (`kIrqWakeDelayCycles`),
  the BIOS IRQ handshake (LR_irq/SPSR_irq/mode/PC=0x18 entry, `0x03007FFC`
  handler pointer)" (`COSIM_ORACLE.md:109-111`) explicitly as one of the
  axes to hash and compare. This is a **documented plan, not an existing
  working tool** — the checklist items (`§2 State-hash module`, etc.) are
  unchecked.
- **`tools/compare_bios_handoff.py`** (this repo) syncs GoldenSunRecomp and
  an mGBA oracle over the TCP debug protocol at BIOS handoff
  (`HANDOFF_PC = 0x08000000`) and diffs CPU register state — a working,
  ROM/BIOS-hash-gated tool, but scoped to the boot handoff instant, not
  ongoing IRQ delivery.
- **Verdict on verifiability:** a BIOS-free IRQ path *could* be validated
  by (a) reusing the existing `irq_cap` ring + `diff_irq_nba.py` shape to
  compare take-sequence/timing between the current LLE path and a
  candidate HLE path run side-by-side, and (b) the `COSIM_ORACLE.md`
  IRQ-controller state-hash axis once (if) it is built. Neither tool
  currently does an **LLE-vs-HLE IRQ diff for this game** out of the box —
  both would need to be pointed at Golden Sun and, for (b), the state-hash
  module built first. This is a real, identified gap in tooling, not a
  blocker on the analysis itself.

## Verdict on feasibility

A BIOS-free IRQ path is **not close to feasible today**, for reasons that
are architectural facts, not estimates:

1. **No HLE hook point exists for IRQ delivery at all** (§1.8) — unlike SWI,
   which has `g_bios_hle_hook` as a seam, `runtime_irq()` has zero branching
   for an alternative implementation. This would need new runtime code, not
   a config flip.
2. **The exact behavior to reproduce is only partially provable without BIOS
   bytes.** The exception-entry mechanics (mode switch, banking, SPSR, LR
   adjustment, vector address) are already generic runtime code and provable
   (§1). But the *handler's own* behavior — user-handler calling convention,
   the precise `0x03007FF8` ack semantics, stack depth/register preservation
   — is TODO-EVIDENCE in this document because reproducing it faithfully
   requires either (a) reading and disassembling the real BIOS bytes against
   GBATEK/mGBA as a primary reference (a real, boundable task — the disasm
   project `third_party/gba_bios_disasm` this repo's own `gba_bios.toml`
   symbol names come from is exactly that reference, just not currently
   vendored in this checkout), or (b) an oracle trace capturing user-handler
   entry/exit state. Neither was done here since this is an analysis-only
   pass; §2's TODO-EVIDENCE items are exactly the remaining unknowns.
3. **Golden Sun's own dependency is narrower than the general case**: it
   uses Halt, not IntrWait/VBlankIntrWait (§3), so the `0x03007FF8`
   wait-flag semantics specifically may not need to be bit-exact for this
   game's own SWI usage — but Halt still needs *some* handler to run per
   VBlank via `0x03007FFC`, so the core "call the user handler, ack IF,
   return" sequence is still required regardless.
4. **This is already a named, deliberately deferred/parked effort** with
   explicit resume criteria (§4) that name this exact gap
   ("Halt/IRQ replacement is implemented in this repository" —
   `docs/PARKED.md:21`). This document's finding does not change that
   status; it sharpens what "implemented" would require.
5. **Verifiability tooling is partially there but not turnkey** (§5): the
   `irq_cap` ring and `diff_irq_nba.py` shape are reusable; the
   `COSIM_ORACLE.md` IRQ-controller state-hash axis is a documented plan,
   not a built tool.

## Ordered implementation steps (if this were undertaken)

1. Obtain/vendor a primary reference for the real BIOS IRQ handler's exact
   instruction sequence — either the disassembly project already named in
   `gba_bios.toml`'s provenance comment (`third_party/gba_bios_disasm`,
   not currently checked out here) or an oracle (mGBA/NanoBoyAdvance) trace
   of register/memory state at user-handler entry and at IF-ack. Close
   every §2 TODO-EVIDENCE item this way — do not guess any of them.
2. Add an IRQ-HLE hook point in `runtime_irq()` mirroring the existing
   `g_bios_hle_hook` seam in `runtime_swi()` (`runtime_arm.cpp:2266-2270`),
   default-off, LLE remaining the oracle — matching the project's only
   permitted HLE shape ("opt-in, LLE stays the oracle, falls through to
   LLE").
3. Implement the handler-call sequence proven in step 1: read
   `0x03007FFC`, call it with the proven calling convention, ack
   `0x03007FF8`/IF per the proven semantics, restore the proven
   register/mode state, return.
4. Preserve `kIrqWakeDelayCycles` exactly as already modeled at the runtime
   level (§1.2/§2) — this is already correct in the current architecture and
   must not regress.
5. Build the LLE-vs-HLE IRQ diff tooling named as a gap in §5: extend
   `diff_irq_nba.py`'s comparator shape (or the `irq_cap` ring it reads) to
   compare the current LLE path against the new HLE path directly, and/or
   build the `COSIM_ORACLE.md` interrupt-controller state-hash axis.
6. Validate against Golden Sun specifically: confirm (closing this
   document's §3 TODO-EVIDENCE) that Golden Sun's own `0x03007FFC` handler
   write is found and its behavior matches bit-for-bit between LLE and the
   new HLE path across representative play (battles, menus, map
   transitions, per `docs/PARKED.md:20-21`'s resume criteria).
7. Address the remaining BIOS-free gaps `BIOS_FREE_CENSUS.md` already lists
   (Halt, Stop, SWI 0x2A) — IRQ delivery is a prerequisite for all of them
   (Halt/Stop both wait on an interrupt taken through this exact path), so
   this gap must close first regardless of order among the others.

## Evidence status summary

- Generic exception-entry/return mechanics (mode switch, banking, SPSR,
  LR adjustment, vector address, HALT-wake latency, IRQ-nesting drive-to-
  completion loop): **proven** from `runtime_arm.cpp` /
  `runtime_bus_bridge.cpp` / `gba_irq.h`, cited by file:line above.
- `0x03007FFC` (user handler pointer) and `0x03007FF8` (BIOS IntrWait/IF
  flags) addresses and their general purpose: **proven** from the runtime
  side (`bios_hle.cpp`, `runtime_arm.h`/`.cpp`, `tools/bios_smoke`), cross-
  confirmed at two independent call sites.
- Real BIOS handler's exact instruction sequence (push/pop order, stack
  depth, calling convention into the user handler, precise `0x03007FF8`
  ack semantics, which registers survive the round trip): **TODO-EVIDENCE**
  — would need disassembly of the real BIOS against a primary reference
  (`third_party/gba_bios_disasm`, not vendored in this checkout) or an
  oracle trace; deliberately not derived from reading
  `gbarecomp/src/runtime/generated_bios/*` (recompiled-from-ROM-bytes
  content) for this document.
- Golden Sun writing its own handler to `0x03007FFC`: **TODO-EVIDENCE** —
  not found by literal-address grep of `local/gs011/main/*.cpp` (checked
  directly for this document); would need dataflow/constant-propagation
  tracing, same limitation `BIOS_FREE_CENSUS.md` already documents.
- Golden Sun not calling IntrWait/VBlankIntrWait (SWI 0x04/0x05): **proven**
  as a lower bound from `BIOS_FREE_CENSUS.md`'s exhaustive 9-site SWI
  census of the current static-recompilation snapshot; not provable against
  any code not yet statically recompiled (`COV-01.md`).
- Existing scaffolding (deferred plan, parked status, architectural
  constraint against fake IRQ dispatch): **proven** from
  `ENHANCEMENTS.md:59-96`, `docs/PARKED.md:15-23`,
  `ARCHITECTURE.md:39-40`, quoted/cited directly.
- Verifiability tooling (`irq_cap` ring, `diff_irq_nba.py`,
  `COSIM_ORACLE.md` plan, `compare_bios_handoff.py`): **proven to exist**;
  their applicability to an LLE-vs-HLE diff *for this specific gap* is
  **not yet built** (COSIM_ORACLE.md's IRQ axis is an unchecked plan item).

## 6. Observed behaviour (black-box capture, 2026-08-30)

This section records what was actually captured by running the real,
hash-verified ROM/BIOS through the existing per-game runtime
(`build/gs011_opt/GoldenSunRecomp.exe`, the same binary
`gbarecomp/src/runtime/runtime.cpp` builds) using **existing, already-shipped
capture hooks only** — no new instrumentation was written. No BIOS
instruction byte was read, quoted, or disassembled to produce this section;
everything below is memory/register *state*, observed from the outside.

### Capture mechanism used (all pre-existing)

- The always-on `irq_cap` ring (`runtime_irq_log_record`,
  `gbarecomp/src/armv4t/runtime_arm.cpp:1426-1444`), dumped to CSV on process
  exit via the existing `GBARECOMP_IRQ_LOG=<path>` env var
  (`runtime.cpp:1617-1619`). One row per IRQ vectoring:
  `seq,cycles,src,ret,cpsr,from_halt`. `cpsr` is `saved_cpsr` — the pre-IRQ
  CPSR, i.e. exactly the value written to `SPSR_irq`
  (`runtime_arm.cpp:2349,2372`), confirmed by reading the call site, not
  assumed.
- The always-armable SWI log (`runtime_swi_log_record`,
  `runtime_arm.cpp:1494-1511`), armed by the existing
  `GBARECOMP_SWI_LOG=<path>` env var, dumped on exit
  (`runtime.cpp:1621-1623`). One row per SWI:
  `seq,cycles,imm,ret,r0,r1,r2,lr,iwflags`, where `iwflags` is
  `bus_read_u32(0x03007FF8)` sampled at that exact SWI instant
  (`runtime_arm.cpp:2254`, `runtime_arm.h:849-850`).
- A raw 32KB IWRAM snapshot at process exit via the existing
  `GBARECOMP_IWRAM_DUMP=<path>` env var (`runtime.cpp:1629-1640`), inspected
  locally (scratchpad, not committed) at offsets `0x7FF8`/`0x7FFC` only —
  this is RAM *content* (numeric words), not BIOS code.

No new hook, flag, or subsystem was added to reproduce any of this — all
three env vars and the `irq_cap` ring already existed in the repo before
this session.

### Exact commands run

ROM verified first: `python tools/verify_rom.py "<rom path>"` →
`SHA-1: 5c4695205413df7db52b9a184815a07783999971` — `OK: exact supported
Golden Sun ROM verified.`

Capture A (2 s / 120 frames, cold boot):

```
$env:GBARECOMP_IRQ_LOG   = "<scratch>\irq_log.csv"
$env:GBARECOMP_SWI_LOG   = "<scratch>\swi_log.csv"
$env:GBARECOMP_IWRAM_DUMP = "<scratch>\iwram_dump.bin"
build\gs011_opt\GoldenSunRecomp.exe --bios <bios> --rom <rom> --frames 120
```

Capture B (10 s / 600 frames, cold boot, fresh output files):

```
$env:GBARECOMP_IRQ_LOG   = "<scratch>\irq_log2.csv"
$env:GBARECOMP_SWI_LOG   = "<scratch>\swi_log2.csv"
$env:GBARECOMP_IWRAM_DUMP = "<scratch>\iwram_dump2.bin"
build\gs011_opt\GoldenSunRecomp.exe --bios <bios> --rom <rom> --frames 600
```

Both run fully headless: `--frames N` without `--window` and without
`--tcp` runs the emulated console for exactly N PPU frames with no SDL
window and no TCP server, then dumps the three files and exits (exit code
0 both times) — confirmed by reading `runtime.cpp:984-997` before running,
then by the actual `EXIT CODE: 0` and `ppu_frames=120` / `ppu_frames=600`
banners. This *is* the non-interactive/headless path the task asked to
find; no human-at-a-window step was required for this capture.

### What was recorded

**Capture A (120 frames, entirely inside the BIOS logo/intro — `final_pc`
at exit was `0x00000348`, a BIOS address, never reaching the ROM at
`0x08000000`):**
- `irq_cap`: 115 IRQ vectorings, **all** `src=0x0001` (VBlank only).
  `from_halt` alternates 0 then 1 for the remainder — the CPU enters HALT
  and every subsequent VBlank wakes it. Sample rows:
  `0,1887175,src=0x0001,ret=0x00002d62,cpsr=0x0000007f,from_halt=0` then
  `1,2163399,src=0x0001,ret=0x00000348,cpsr=0x2000005f,from_halt=1`, then a
  steady ~280896-cycle take-to-take interval (matches the known GBA
  ~280896-cycle/frame VBlank period) for the rest.
- `swi_cap`: **0 records.** No SWI instruction executed in the first 120
  frames even though the CPU repeatedly entered/left HALT
  (`from_halt=1` in 114/115 entries) — i.e. whatever puts the CPU in HALT
  during the BIOS intro is **not** SWI 0x02 (Halt) as observed here; it did
  not go through the SWI trap this build hooks. **TODO-EVIDENCE**: the
  mechanism (direct `HALTCNT` MMIO write vs. some other path) was not
  identified — would need an `mmio_cap`-style write trace on
  `0x04000301` during this exact window, not captured in this pass.
- IWRAM at exit: `0x03007FF8 = 0x00000001`, `0x03007FFC = 0x00000300`.
  Neither looks like a resolved user-handler pointer at this point (BIOS
  intro hasn't handed off to game code yet).

**Capture B (600 frames / 10 s, reaches the ROM — game SWI calls observed,
`final_pc` at exit `0x000001b4`, a BIOS address, i.e. sampled while
halted):**
- `irq_cap`: 594 vectorings, **all** `src=0x0001` (VBlank only — no
  Timer/DMA/Serial IRQ observed in this window). `from_halt=1` for
  552/594, `from_halt=0` for 42. `ret` (interrupted PC) is a BIOS address
  (`0x000001b4` x290, `0x00000348` x262) for the vast majority — consistent
  with the CPU being frozen inside the BIOS's own Halt-SWI implementation
  while halted, per generic ARM/GBA Halt semantics, not something inferred
  from BIOS bytes here. A minority (22 entries: `0x080068f8`,
  `0x080068b6`, `0x08006900`) show `ret` inside the ROM (`0x08xxxxxx`), and
  2 show `ret=0x03002294` (an IWRAM-copied function) — IRQs that landed
  while the CPU was *not* halted (`from_halt=0`), i.e. early in this window
  before the Halt-driven main loop took over.
- `swi_cap`: 294 records, only two distinct `imm` values —
  `imm=11` (0x0B, `CpuSet`) x4, all with `ret=0x08006866` (one constant ROM
  call site) early in the window; `imm=2` (`Halt`) x290, **all** with
  `ret=0x08003324` (one constant ROM call site — this is Golden Sun's own
  main-loop Halt call, address proven from this trace, not assumed) and
  **all** with `iwflags=0x00000000`. Interval between consecutive Halt
  calls is ~280893-280896 cycles — one call per GBA frame. This is direct,
  numeric confirmation of the census claim in §3: Golden Sun's own Halt
  usage never observes a nonzero `0x03007FF8` value at the SWI instant, in
  290/290 samples.
- IWRAM at exit: `0x03007FF8 = 0x00000000`, `0x03007FFC = 0x03000000`.
  `0x03007FFC` changed value between Capture A and B (`0x300` →
  `0x03000000`) but neither sampled value is confirmed to be a resolved,
  stable user-handler function pointer — **TODO-EVIDENCE**: whether/when
  Golden Sun writes its final handler pointer to `0x03007FFC`, and what
  that pointer's value is, was not pinned down by this pass (would need
  either a write-watchpoint on that exact address across a longer capture,
  or the dataflow tracing already named as TODO-EVIDENCE in §3).
- Build-honesty note (unrelated to the IRQ question but must be reported
  per this session's own build output): this 600-frame run's coverage
  banner read `self_heal_coverage=NOT_STATIC dispatch_misses=67
  interpreted_insns=2493083 ... bridge_native_handoffs=519`, with console
  lines like `bridged 0x03000658 (thumb) x2624 near gf_tfunc_03000658` —
  IWRAM-copied game functions (RAM-resident code the game copies at
  startup, e.g. its sound driver) were self-healed via the interpreter
  bridge this run, honestly logged, not silently interpreted forever. This
  does not affect the IRQ observations above (all IRQ/SWI log entries came
  from the generic runtime paths, not from the bridged functions) but is
  reported here for coverage-honesty per project rules; it is a pre-
  existing, already-known characteristic of this build (IWRAM-resident
  code discovery), not something this capture session changed.

### What is still unproven after this capture

Everything §2 already marked TODO-EVIDENCE remains TODO-EVIDENCE — this
was an entry/exit-boundary and IWRAM-snapshot capture, not an instruction-
level trace of the BIOS handler's interior:
- The user-handler calling convention (ARM vs. THUMB entry, which register
  holds the return address) — not observed; would need register state at
  the instant execution reaches whatever address is finally stored at
  `0x03007FFC`, which this pass did not resolve to a stable value.
- The exact `0x03007FF8` ack semantics (OR'd unconditionally vs.
  conditionally, before/after the user handler runs) — this pass only
  proved Golden Sun's Halt call site never observes it nonzero; it does not
  show what the BIOS handler itself does to that word on the way in/out.
- Stack usage/depth inside the handler, and which registers the real BIOS
  handler preserves across the round trip — not observed by this capture
  shape (IRQ-log entries are entry/exit snapshots, not a push/pop trace).
- Whether/when Golden Sun's own code writes a resolved function pointer to
  `0x03007FFC` — two samples were taken (10 s apart) and neither looked
  like a settled pointer; a longer capture or a write-watchpoint would be
  needed to catch the actual write.

No BIOS HLE was enabled, no game behavior was changed, and no code was
added under `gbarecomp/src` or elsewhere to produce this section — it is
an observation-only pass using the three pre-existing env-var dumps
(`GBARECOMP_IRQ_LOG`, `GBARECOMP_SWI_LOG`, `GBARECOMP_IWRAM_DUMP`) and the
existing `irq_cap` ring's on-disk CSV format.
