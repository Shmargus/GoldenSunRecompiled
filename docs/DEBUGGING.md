# Debugging Loop

The pinned upstream `gbarecomp/DEBUG.md` is authoritative. This file adds the Golden Sun-specific workflow.

## Minimal divergence report

Record:

```text
ROM SHA-1:
BIOS SHA-1:
gbarecomp commit:
GoldenSunRecomp commit:
Oracle build/settings:
Input script/save checkpoint:
Sync event:
Last matching event/state:
First differing event/write:
Native PC/mode/overlay:
Oracle PC/mode/overlay:
Affected memory/register:
Coverage misses this run:
```

## Overlay-aware debugging

Every trace line executing RAM code should make it possible to determine:

- Current runtime PC.
- ARM/THUMB mode.
- Active overlay ID or main/IWRAM code-copy identity.
- ROM source backing address when known.

Without this, two functions sharing a runtime address become indistinguishable.

## Root-cause order for map-transition failures

1. Overlay loader/decompression completion.
2. Destination range and copied bytes.
3. Cache/invalidation/active identity.
4. Function entry and mode.
5. Indirect dispatch target.
6. IRQ interruption/resume inside overlay code.
7. Only then inspect gameplay state symptoms.

## Never fix by

- Hardcoding the expected next map.
- Redirecting one failing PC to a similarly named function.
- Keeping stale overlay entries and selecting whichever does not crash.
- Editing generated C.
- Skipping the map script or event.
