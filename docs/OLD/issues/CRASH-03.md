# CRASH-03 — Wild jump during Bilibin transition

Status: open; dynamic RAM remains **NOT_STATIC**.

## Current evidence

The former failure reached Bilibin after the world-map return at frames
520148–520149. An earlier retry at `0x03006048` bridged 2,716,446 instructions
and ran to `0xDD94D034`; the later trace reached invalid `0xDCEF03D0` with
`r8=0xE`. `0x03006104` is the earliest synchronized bad change:
`LDM SP!,{r5,r9}` loads `r9=0xDCEF0210` from IWRAM; later code derives the
invalid target. The stack overlaps the tail of the stack-hosted `Func_2cf4`
image. Replays `20260825_221446` and `20260825_190416` cross the window cleanly;
no behavior fix is proven. The pool-LDM probe is diagnostic-only.

The 20260826 recurrence confirmed the fixed-slot writer `0x08000716` is
healthy; both live-slot writers were carriers, so the poison origin remains
unidentified. Full evidence chain, ranked hypotheses, safety boundaries, and
the approved bounded provenance diagnostic live in the durable companion
[`CRASH-03_HANDOFF.md`](CRASH-03_HANDOFF.md).

Session `20260828_102032` rebuilt the diagnostic-only first clean-to-poison
latch. It records generated/bus/mirrored writer context and emits it with an
invalid-dispatch dump; no crash behavior fix or completed poison-writer
identity is claimed.

## Next action

The handoff's bounded diagnostic is implemented: depth-8 slot write rings,
`state_epoch` tags, restore/arm-time + auth-epoch CRC over
`0x03007E00..0x03007E40`, plus a non-evicting first clean-to-
`0xDCEF0210` transition latch covering generated, bus, and mirrored writes.
The latch records writer context and is emitted in the invalid-dispatch dump;
synthetic `pool_ldm_probe_tests` passes. This remains diagnostic-only.
Take one root-launcher capture per NEXT_TASK.md. Do not patch guest state
before a non-carrier poisoning writer is proven.

## Closure condition

Completed-writer identity, full RAM CRC/end validation, and resolver coverage
explain the transition, followed by repeated clean root-launcher transitions.
