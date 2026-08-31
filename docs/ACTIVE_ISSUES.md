# Active issues

Updated 2026-08-30. This is an index only. Read `STATUS.md` and
`NEXT_TASK.md`, then open exactly the linked detail file for the task. Closed
evidence belongs in [`history/`](history/), which is not current guidance.

## Performance and correctness

- [LAUNCH-01](issues/LAUNCH-01.md) — Launcher freezes after ROM selection; SDL2 rebuild done, freeze persists.
- [PERF-08](issues/PERF-08.md) — Residual stutter and dynamic-RAM churn.
- [CRASH-03](issues/CRASH-03.md) — Wild jump during Bilibin transition.
- [CRASH-02](issues/CRASH-02.md) — Interpreter bridge call-return depth.
- [CRASH-05](issues/CRASH-05.md) — Mercury Lighthouse decompressor abort.
- [CRASH-06](issues/CRASH-06.md) — Mercury Lighthouse interpreter-gap symptom.
- [CRASH-07](issues/CRASH-07.md) — Town re-entry RAM dispatch recursion.
- [TOOL-02](issues/TOOL-02.md) — `--load-state` with `--tcp`.
- [BIN-01](issues/BIN-01.md) — RelWithDebInfo binary size.
- [PERF-05](issues/PERF-05.md) — Prologue boulder scene performance.
- [PERF-02](issues/PERF-02.md) — Uneven scene interpolation coverage.
- [COV-01](issues/COV-01.md) — New progression coverage misses.

## Audio

- [PERF-03](issues/PERF-03.md) — Unconfirmed audio events.
- [AUD-01](issues/AUD-01.md) — Unconfirmed audio/item/shop behavior.
- [AUD-03](issues/AUD-03.md) — Native MP2K timeline and fidelity fail closed.

## Graphics and presentation

- [WIDE-01](issues/WIDE-01.md) — Widescreen/culling/scene acceptance umbrella;
  2026-08-30 culling regression reverted, user test pending.
- [VFX-LINGER-01](issues/VFX-LINGER-01.md) — Sprites over battle fade.
- [PRES-01](issues/PRES-01.md) — Enhanced Timing A/V drift gate.
- [PRES-02](issues/PRES-02.md) — Reported screen tearing.
- [PRES-03](issues/PRES-03.md) — Ivan/world-map shimmer.
- [UI-01](issues/UI-01.md) — Turbo ceiling.
- [UI-03](issues/UI-03.md) — F1 menu sizing reconfirmation.

## Tooling and cleanup

- [TOOL-01](issues/TOOL-01.md) — Overlay trace symbolization.
- [CLEANUP-02](issues/CLEANUP-02.md) — Cost-probe IRQ instrumentation.

## BIOS-free investigation (informational, parked)

- [BIOS-FREE-01](issues/BIOS_FREE_CENSUS.md) — Golden Sun BIOS-dependency
  census. Informational analysis only; see `BIOS-01` in `PARKED.md`.
- [BIOS-FREE-02](issues/BIOS_FREE_IRQ_PATH.md) — IRQ path analysis and
  feasibility verdict. Informational analysis only; see `BIOS-01` in
  `PARKED.md`.

Resolved: CHEAT-01 is documented in [`features/CHEATS.md`](features/CHEATS.md).
Deferred: CORE-01 and other parked work are documented in
[`PARKED.md`](PARKED.md). Wishlist items are in [`BACKLOG.md`](BACKLOG.md).
