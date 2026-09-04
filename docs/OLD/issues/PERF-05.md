# PERF-05 — Prologue boulder scene performance

Status: open; cause unproven.

## Current evidence

The report predates the optimized build and remained slow at guest 4x. The
bottleneck may be present, PPU, or compile work.

## Next action

User retests `gs011_opt` from a pre-scene savestate at 1x and 4x.

## Closure condition

Measured scene timings identify the dominant subsystem and show either an
evidence-backed fix or an accepted performance boundary.
