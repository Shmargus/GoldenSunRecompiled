# AUD-03 — Native MP2K timeline and fidelity fail closed

Status: open; native path remains experimental and fail-closed.

## Current evidence

The target is an accurate native MP2K replacement on an audio wall clock;
canonical PSG/FIFO remains the oracle. The latest probation evidence failed at
correlation `0.68`, ratio `0.19`, with host gaps/resets/underruns/late
`33/52/36418/79` and candidate missing `101277/262144`. Producer ownership,
fidelity, and independent clock completion remain open. Full evidence is in
[`features/MP2K.md`](../features/MP2K.md).

## Next action

Follow the ordered MP2K work in the feature file: reproduce the saved-state
run, prove complete C70 `FullProducerSeed` ownership, then collect fidelity and
fallback evidence. Do not lower gates.

## Closure condition

Configured tests, producer ownership, fidelity/probation, independent timing,
Turbo behavior, fallback, strict-static behavior, and root-launcher manual
acceptance all pass with canonical output preserved as oracle.
