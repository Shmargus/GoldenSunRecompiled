# CRASH-05 — Mercury Lighthouse decompressor abort

Status: open.

## Current evidence

State5 reproduces an interpreter abort in the shared RAM-slot decompressor.
The copy loop overruns its destination and does not reach the stream sentinel.

## Next action

Verify source pointer/blob alignment, then inspect the unrolled copy routines.
Do not guess an opcode or register a static image.

## Closure condition

Measured source, destination, blob alignment, and loop-bound evidence explains
the abort and a focused replay completes without interpreter abort.
