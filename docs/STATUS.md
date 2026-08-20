# Current status

Last updated 2026-08-20. This is the short answer to "where is the project?".
Open work lives in `docs/ACTIVE_ISSUES.md`. Closed records live in
`docs/history/`.

## Right now (2026-08-20)

A battle/menu stutter regression is fixed in the working tree (uncommitted):
`gbarecomp`'s 128-deep dispatch recursion cap was reporting resident code as
missing, causing the same body to be recompiled hundreds of times per battle.
RAM compiles per battle dropped 2581 to ~195, user-confirmed. Two permanent
stderr diagnostics (`[ram-compile]`, `[dispatch-miss]`) are deliberately left on
in normal play. The tail-dispatch refactor (**CORE-01**) is parked and its
stashes must not be dropped. Remaining small hitch tracked as **PERF-08**,
blocked on a longer play-session log. See `docs/HANDOFF_2026-08-20.md`.

## Where the project is

Golden Sun runs. The prologue, Vale, the overworld, battles, menus, saves and
the early game are all playable end to end on a local hash-gated build. The
faithful 240x160 output at original timing is still the default, and every
enhancement is opt-in.

This is no longer a research scaffold. It is a playable port with known rough
edges, tracked in `docs/ACTIVE_ISSUES.md`.

## The build you actually run

`build/gs011_opt` — RelWithDebInfo (`-O2 -g -DNDEBUG`). It is the only build
configured with SDL2, so it is the only one with a host window.
`src/launcher_main.cpp:365-374` prefers it and falls back to `build/gs011`.

- `build/gs011` is an older unoptimized Debug build, kept only because earlier
  measurements are keyed to it. It is roughly 2.9x slower. Do not benchmark
  against it.
- `build/gs011_rel` is headless and has no SDL2.
- LTO is off deliberately. Two attempts produced 0-byte binaries and nearly
  exhausted this machine's RAM.

Public builds keep `GSR_BUILD_LOCAL_RUNNER=OFF` and require no ROM or BIOS.
Local runs supply the user's own ROM and BIOS by path; nothing protected is in
the repository.

## What is on by default, and what is not

Default (faithful):

- 240x160 output, original 59.7275 Hz timing, raw color.

Opt-in, all default-off:

- Enhanced Timing (exact 60/120 Hz host pacing).
- Guest CPU overclock, 1x/2x/4x/8x, live-toggleable. Scales per-instruction
  execution cost only, never halt/idle, DMA or IRQ cycles. 1x is verified
  bit-exact.
- 2x scene interpolation, native supersampling, temporal blend ("LCD
  ghosting"), soft-filter shimmer reduction, color profiles, integer scaling.
- Turbo, in both Held and Toggle forms, bindable to keyboard or controller.

Configuration is the F1 menu; bindings and choices persist.

## Static coverage

Strict-static acceptance runs (no interpreter, no self-heal, no cache load)
complete FULLY_STATIC on bounded recorded tracks. That claim is always
frame- and route-bounded: a route nobody has run will find its own gaps, and
new gaps are ordinary work, not regressions.

Golden Sun copies code into RAM and swaps map overlays through the same
addresses, so dispatch cannot key on PC alone. The mechanism is documented in
`docs/GS011_TRANSIENT_IMAGES.md` (the mechanism is current; its frame counts
are historical) and `docs/OVERLAYS.md`.

## What is being worked on now

A tail-dispatch refactor in `gbarecomp`. Guest jumps are currently emitted as
host function calls, so a guest loop that never returns grows the host stack
without bound — the cause of the Bilibin stack overflow. The refactor makes
those dispatch as tail transfers instead. `d36b253` is the working checkpoint
taken before it started.

The refactor introduced its own crash: `overlay_resolve()` sets
`g_runtime_image_base` for a relocatable RAM-heal entry but nothing restored
it after the call returned, so a later relocatable body reached through the
one dispatch tier that never sets the base (the fixed static table) could
read a stale value — this aborted the Mercury Lighthouse cutscene. Fixed at
the emitter level (`arm_codegen.cpp`/`emit_function.cpp` resync the base
right before every dispatch/call transfer and again after a `BL` returns),
which keeps the tail-jump property the refactor exists for, unlike a
caller-side wrapper. User-confirmed clear on `build/gs011_test`; full details
in `docs/ACTIVE_ISSUES.md` under CRASH-04. **Folded into `build/gs011_opt`
2026-08-16** (rebuilt 12:14); 86-test Python suite and `tail_dispatch_tests`
pass.

A new crash surfaced in that same build on 2026-08-16 — a genuine interpreter
opcode gap hit while self-healing `0x03002000`, unrelated to the fix above.
See `docs/ACTIVE_ISSUES.md` CRASH-05. Also see SESSION-2026-08-16 there for
why the world map can measure as slow (interpreter-bridge-bound without
`GBARECOMP_SELFHEAL_RAM=1` and a warm cache) — not a regression.

### Session 2026-08-19

**Mercury Lighthouse crash (CRASH-06) — cause narrowed to one thing.** The guest
LZ decompressor `Func_2808` never terminates: it runs its output past the
destination, over its own code at `0x03006000`, and on into the stack. Every
earlier theory (wrong destination, slot reuse, stack corruption) is fallout from
that and is now retired. Our translation of the terminator check, bit-refill and
flag handling was re-checked and looks faithful, so the fault is either the
compressed blob/pointer going in, or one of ~35 unverified jump-table copy
routines. Full detail, evidence and next steps in `docs/ACTIVE_ISSUES.md`
CRASH-06. Temporary `[c06-*]` diagnostics are in the tree and must be removed
when it closes; `[c06-term]` is broken and needs a different approach.

**Walk speed QoL (QOL-01) — parked.** The hooked write site never fires in the
real windowed build and the accumulator never moves while walking; all the
original measurements came from headless/`gs011_rel` and did not transfer. The
F1 → Enhancements → QoL → Walk Speed control is now greyed out and pinned to 1x.

**Crash tracing now follows the launcher.** The launcher checkbox decides the
state at every launch and rewrites `AdditionalDebugLogging` in `config.ini` to
match, so an F1 toggle left on can no longer survive as a hidden override.

**Roadmap:** custom items/weapons and custom Psynergy added as future modding
work, both gated on finding the tables and the width of the ID fields.

## Reference

- `AGENTS.md` — binding rules. Read first.
- `docs/ACTIVE_ISSUES.md` — what is open.
- `ARCHITECTURE.md`, `TESTING.md`, `LEGAL.md` — boundaries.
- `docs/DEBUGGING.md` — the divergence workflow.
- `docs/history/` — closed milestones and past sessions. Do not act on them.
