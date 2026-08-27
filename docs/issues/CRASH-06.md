# CRASH-06 — Mercury Lighthouse interpreter-gap symptom

Status: open; tracked with the CRASH-05 decompressor path.

## Current evidence

The interpreter-gap report occurs on the same shared RAM-slot decompressor
path as CRASH-05. It is a symptom, not evidence for a guessed missing opcode.

## Next action

Use CRASH-05's source/blob alignment and unrolled-copy measurements to identify
the earliest divergence.

## Closure condition

The shared path has measured copy bounds and reaches its stream sentinel with
no interpreter gap or abort in the reproducible state5 replay.
