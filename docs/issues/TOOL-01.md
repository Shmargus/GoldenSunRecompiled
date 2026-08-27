# TOOL-01 — Overlay trace symbolization

Status: open.

## Current evidence

Crash traces use the primary symbol table even when another same-PC overlay was
selected, which can mislead RAM debugging.

## Next action

Print the selected candidate identity or symbolize against the selected
overlay.

## Closure condition

Every overlay trace identifies the selected candidate used for symbolization,
with a focused same-PC overlay test.
