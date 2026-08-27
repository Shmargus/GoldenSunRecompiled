# VFX-LINGER-01 — Sprites over the battle fade

Status: open; cause unknown.

## Current evidence

Sprites stay visible while the battle-exit screen fades black. The current
hypothesis is that OBJ is omitted from a brightness blend target, but this is
not proven.

## Next action

Capture BLDCNT/BLDY and per-layer targets during the fade.

## Closure condition

Register evidence proves the affected target and a rendering change makes OBJ
fade correctly without changing unrelated layers; otherwise document the
hardware-equivalent result.
