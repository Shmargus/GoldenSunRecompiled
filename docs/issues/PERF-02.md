# PERF-02 — Uneven scene interpolation coverage

Status: open.

## Current evidence

Only 31.6% of measured midpoint opportunities succeeded. Safe duplicate
fallback handles the rest, and measured PPU cost is small.

## Next action

Measure battle and field cadence/coverage. Keep world-map fallback and endpoint
checks strict.

## Closure condition

Coverage and cadence are measured for representative battle and field scenes,
with every failed midpoint using a verified safe fallback.
