# WIDE-01 technical handoff — session 20260828_163525

This is the starting point for the next Sol session. Do not use older handoff
conclusions as current guidance.

Update: the OBJ-Y diagnostic plan below was superseded by the implemented
simplified object path. Strict Expanded mode now carries signed logical X/Y
from the widened guest cull into the PPU per OAM slot. Manual acceptance is
pending. The Palace leakage investigation in this handoff remains the next
engineering task.

## Scope and user result

The user repeated the same Bilibin/McCoy Palace walk with diagnostics enabled.
Behavior is unchanged:

- horizontal NPC/object culling appears correct;
- vertical culling remains premature at the top and bottom expanded edges;
- McCoy Palace still shows detached texture fragments outside map edges.

Visual references are local in `Culling and leak issues/`. Do not publish or
commit them. Dynamic RAM remains **NOT_STATIC**.

## Build and capture

- Log: `logs/session_20260828_163525.log`
- Playable build: `build/gs011_opt/GoldenSunRecomp.exe`
- Built: `2026-08-28 16:17:28`
- Size: `879867294` bytes
- Launch only repository-root `GoldenSunLauncher.exe`.

## Palace leakage evidence

The latest diagnostics ran in authenticated Palace epoch 3. Provider exit:

- BG1: 21,419,780 replacements; 3,156,220 region rejects.
- BG2: 21,270,140 replacements; 3,305,860 region rejects.
- BG3: 21,755,170 replacements; 404,660 region rejects and 2,416,170 lookup
  misses.

Leakage is provider-backed, not wrapped fallback. The bounded mask still
accepts millions of `accepted-connected` margin samples. Representative first
samples include BG3 left `map=(8,12) id=0x029`, BG3 top
`map=(8,10) id=0x0fc`, and BG3 bottom `map=(8,22) id=0x20c`.

Important limitation: `[wide-palace-margin-sample]` currently retains only the
first sample for each BG/edge/outcome combination. It recorded 41 samples with
zero drops, but cannot identify which later coordinates correspond to the
visible fragments. Do not blacklist these representative IDs.

### Next Palace step

1. Keep rendering behavior unchanged.
2. Replace first-sample-only attribution with bounded per-map-cell aggregation
   for `accepted-connected`, keyed by BG, edge, `map_x`, `map_y`, and `map_id`.
3. Report top cells/counts or a compact 128x128 occupancy summary; retain no
   tile bytes.
4. Repeat the same Palace walk/screenshots and correlate visible fragments to
   exact accepted cells.
5. Only then derive an actual room-edge/ownership rule. Distance and `0x026`
   heuristics are already disproven.

## Vertical culling evidence

All reviewed final branches execute and widen in epoch 3: B27E 108 bypasses,
B328 239, C702 162, and C708 234. This places the remaining visible loss after
or outside the already-widened branch policy.

Bilibin epoch 1 has active slots with raw Y `200..255` recorded only as
`coordinate-rejected`, notably slots 4–20. Palace epoch 3 similarly records
raw `200..255` rejections in slots 6, 7, 9, 10, 14, and 16. These values include
hardware-wrapped negative top coordinates, but provider rejection should fall
back to normal signed OBJ-Y decoding. The capture does not prove whether that
fallback produced the correct expanded output coordinate.

Important limitation: `[wide-obj-y-slot]` counts provider invocations, not one
event per slot/frame. Hidden Y=192 dominates: epoch 3 reports 14,604,000 calls
and 14,423,760 exact-192 observations. Counts are multiplied by renderer work
and cannot identify the exact frame/slot of the visible cull.

### Next OBJ-Y step

1. Do not globally reinterpret Y=192 or widen another branch speculatively.
2. Deduplicate diagnostics by `(frame, OAM slot, raw_y, outcome)`.
3. For non-192 slots, record the final renderer decision and final decoded
   output Y after provider fallback, plus fresh OAM writer generation/PC.
4. Add bounded transition records when a slot crosses the top or bottom
   expanded edge; exclude unchanged hidden slots.
5. Repeat the Bilibin and Palace edge walk, identify the exact actor slot and
   earliest stage that drops it, then implement only that proven fix.

## Safety and acceptance

- No behavior change is justified by this capture alone.
- Preserve Native and 288x160 behavior.
- Do not edit `generated/**` or classify dynamic RAM as static.
- Do not suppress map IDs from the representative samples.
- The user performs gameplay tests through `GoldenSunLauncher.exe`.
- WIDE-01 remains open until visual retest confirms objects remain visible
  while any pixels intersect the expanded view and Palace fragments are gone.
