# CRASH-02 — Interpreter bridge call-return depth asymmetry

Status: open; no current gameplay failure is reproduced.

## Current evidence

The interpreter bridge tracks local call nesting and current tests cover native
handoff and zero-depth cleanup. The runtime IRQ path also raises a call-return
floor while it drives the handler. A mixed static → interpreter → static path
has no documented gameplay acceptance result, so leaked or corrupted guest
return depth is not closed.

## Next action

Run the focused bridge/stack tests and a bounded mixed-path replay alongside
CORE-01. Record depth before/after the bridge and any non-local return.

## Closure condition

The mixed path has measured balanced depth and correct return control flow in
focused tests and a representative replay, or a targeted fix has equivalent
evidence.
