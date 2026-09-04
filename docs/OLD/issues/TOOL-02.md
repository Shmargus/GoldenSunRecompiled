# TOOL-02 — `--load-state` with `--tcp`

Status: open.

## Current evidence

Argument handling enters the blocking TCP loop before processing `--load-state`.
The combination therefore fails silently; TCP `savestate_load` itself works.

## Next action

Load before entering the loop, or reject the combination with a clear error.

## Closure condition

`--load-state --tcp` either loads deterministically before the loop or reports
a clear documented error and has a focused argument test.
