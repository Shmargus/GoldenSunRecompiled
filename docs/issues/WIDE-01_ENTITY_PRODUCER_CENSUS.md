# WIDE-01 entity-producer census

## Objective and boundary

> **Findings filed.** A first pass across two towns (Bilibin/McCoy Palace and
> a second, different town) is complete; see
> `WIDE-01_ENTITY_PRODUCER_FINDINGS.md` for the record layout, proven X/Y
> offsets, lifecycle, route to screen, cross-validation result, and the full
> remaining TODO-EVIDENCE list. This playbook stays the procedure reference;
> read the findings doc for what is now known.

Find the common lifecycle and pre-OAM submission path for Golden Sun map
NPCs, shadows, and objects, so Expanded mode can eventually cull or mirror
world-space sprites from signed logical coordinates. This is an investigation
playbook, not permission to change culling behavior.

Current milestone: WIDE-01 remains open; faithful Native 240x160 is the
oracle, and Expanded 360x240 vertical NPC culling is unresolved.

Exact hypothesis: multiple field entities feed a common runtime record or
submission ABI before GBA OAM Y truncation. The earliest common seam should
carry signed screen/world X/Y, sprite attributes, destination slot, and enough
identity to correlate body, shadow, and multipart objects.

Non-goals: identify every NPC by name; infer semantics from OAM slots alone;
load the entire world; alter guest movement, AI, collision, scripts, map
loading, animation, OAM limits, Native/288x160 behavior, or generated code;
build a full native renderer; or claim universal coverage from one town.

## Known evidence and gates

- Headless session `20260829_221432` had no recorded input, so it was not a
  reproducible world-map-to-town replay.
- Widescreen diagnostics now automatically record input as
  `logs/session_<id>.input` when no replay is active. Existing recordings are
  never overwritten.
- If running the child directly for a headless replay, first resolve and
  verify that it is exactly the child selected by the root
  `GoldenSunLauncher.exe` (currently `build/gs011_opt/GoldenSunRecomp.exe`).
- Before any ROM-dependent work, verify ROM SHA-1
  `5c4695205413df7db52b9a184815a07783999971`; stop on mismatch.
- The measured `Func_1dc8` relocatable image has verified relative writer
  offsets D4 `+0x58`, EC `+0x70`, and F0 `+0x74`; fixed addresses are not
  identity. Track image base, image key, and generation.
- The final shadow-to-OAM DMA is exactly 1024 bytes from
  `0x0300347C` to `0x07000000` (128 slots). Treat it as the visibility latch,
  not as proof of entity meaning.
- The `0x38`-stride staging records are the record array base `0x03002000`,
  14 physical slots. Semantic identity, per-offset field map, lifecycle, and
  writer/DMA route are now resolved: `+0x04`=Y and `+0x06`=X are proven
  (match counts in the findings doc); `+0x00` is a candidate
  allocator/pool-link pointer, not proven. See
  `WIDE-01_ENTITY_PRODUCER_FINDINGS.md`. Remaining offsets stay
  **TODO-EVIDENCE** as listed there.

Required run settings: original timing; cheats/mods off; canonical 240x160
first; widescreen diagnostics on.

> **Correction (2026-08-30):** this originally said "self-heal off." That is
> backwards and was never actually run that way. Self-heal must be **on**
> (launch via `GoldenSunLauncher.exe` / `scripts/gs-replay.ps1` only) or the
> field NPC/shadow staging DMA (`pc=0x0800C674`) never fires at all --
> see `WIDE-01_ENTITY_PRODUCER_FINDINGS.md`, Reproduction section, for the
> measured 0-vs-504 evidence.

Use the exact verified child only after launcher-resolution verification.
Agents do not launch gameplay unless explicitly authorized; the user
normally performs manual launches.

Keep ROM, BIOS, savestates, input recordings, RAM snapshots, screenshots, and
full/private traces under local/private paths. Never commit, paste, upload, or
expose protected bytes or generated files containing them. Use payload-free
aggregates and synthetic fixtures in public tests.

## Reproducibility gate

Use one savestate immediately before the world-map-to-town transition and its
matching recorded input. Record the exact frame limit and diagnostic build.
Run the same replay twice. Conclusions require matching event anchors (not
just frame numbers), including map-transition completion, VBlank/DMA anchors,
overlay activation, and first entity/render events. If anchors differ, mark
the result non-reproducible and investigate timing/input/restore state first.

Suggested private placeholders:

```powershell
$entityCensusStatePath = "<local savestate path>"
$entityCensusInputPath = "<logs/session_<id>.input>"
$entityCensusChildPath = "<launcher-resolved exact child path>"
$env:GBARECOMP_INPUT_REPLAY = $entityCensusInputPath
& $entityCensusChildPath --no-window `
  --load-state $entityCensusStatePath --frames <N>
Remove-Item Env:GBARECOMP_INPUT_REPLAY
```

Do not assume option names across builds; inspect the current launcher/runtime
help before use. Do not launch a child until its path has been checked against
the launcher resolution.

## Investigation procedure

1. **Mark map boundaries.** Capture restore, map/room entry, overlay load and
   completion, first stable field frame, room exit, and re-entry. Correlate
   VBlank, DMA, and known function-entry anchors.
2. **Census RAM writes.** During each boundary, aggregate writes by region,
   address, width, writer PC, image base/key/generation, and epoch. Use narrow
   frame windows; never dump all RAM or payload bytes into logs.
3. **Find candidate records.** Search for repeated active records, stable
   stride (including the observed `0x38` candidate), pointer fields, flags,
   coordinates, animation/state words, and map/room ownership. Require
   create-time writes plus later per-frame updates.
4. **Trace pointers end to end.** Follow candidate record pointers from
   creation to update, sprite/animation selection, staging, writer, and final
   OAM DMA. Record call depth/return PC and overlay image identity at every
   edge.
5. **Handle relocation safely.** Resolve `Func_1dc8` writers by verified
   image identity and relative D4/EC/F0 offsets (`+0x58/+0x70/+0x74`), never
   by a fixed RAM PC. Invalidate correlations on image overwrite, generation,
   epoch, or map transition.
6. **Capture pre-cull coordinates.** At the earliest proven common seam,
   record signed logical X/Y before branch rejection and before OAM truncation,
   plus camera/map coordinates, dimensions, flip/priority/shape, and the
   eventual slot/ATTR0/1/2 identity. Mark unknown fields
   **TODO-EVIDENCE**.
7. **Correlate writers and DMA.** Match D4/EC/F0 staging and committed writes
   by exact frame/context, target, slot, and ATTR0/1/2; then match the exact
   1024-byte DMA. Classify consumed, unrelated, context-mismatch,
   identity-unproven, attr-mismatch, expired, and no-handoff outcomes.
8. **Prove grouping.** Body/shadow or multipart ownership requires shared
   creator/owner pointer, stable group ID, explicit relation field, or
   synchronized create/update/despawn evidence. OAM slot, staging address, or
   ATTR alone never proves NPC identity or body/shadow ownership.
9. **Relate camera and map space.** Vary camera movement and edge crossings;
   establish the transform, signed viewport bounds, map/room relation, and
   whether a record is world-space or screen/UI-space.
10. **Trace lifecycle.** Measure activation, hidden/inactive flags, despawn,
    room unload, script-created objects, animation-only changes, and slot
    reuse. Ensure stale provenance cannot survive transitions or restore.
11. **Cross-map validate.** Repeat on at least two towns/fields plus a room
    transition, with NPCs, shadows, static objects, doors, and a multipart or
    scripted object. A common ABI claim fails closed until alternate producers
    are accounted for.

## Evidence standard and output

Every address, range, threshold, mode, stride, field meaning, and overlay
relationship needs a measured write/read/call correlation on the verified ROM.
Otherwise label it **TODO-EVIDENCE**, state the missing observation, and do not
implement against it. Fix the earliest synchronized divergence; prefer a loud
dispatch/identity miss to decoding data as code.

Maintain a payload-free table like this:

| Record/base | Stride | Creator/updater | Render route | Signed X/Y | Owner/group evidence | Overlay identity | Confidence |
|---|---:|---|---|---|---|---|---|
| `<address/base>` | `<bytes or TODO-EVIDENCE>` | `<PC/image/gen>` | `<route>` | `<field/transform>` | `<proof or TODO-EVIDENCE>` | `<key/base/gen>` | `<low/med/high>` |

Also report per replay: build/hash, ROM gate result, state/input/frame limit,
anchor match result, map/overlay epochs, candidate count, record stride,
writer/DMA correlations, unknown routes, dropped samples, and remaining
TODO-EVIDENCE. Filter logs locally (for example, select only
`wide-field-producer`, `wide-obj-y`, writer, DMA, overlay, and map-boundary
tags); keep diagnostics bounded and uncapped totals separate from bounded
samples. Run only one CPU-heavy replay/build at a time and keep total CPU and
memory below the project limit.

## Acceptance and handoff

> **Status (2026-08-30):** record/producer contract, signed X/Y offsets and
> transform rules, creator/update lifecycle, and writer/DMA route are
> established across two towns; see `WIDE-01_ENTITY_PRODUCER_FINDINGS.md`.
> Camera/map transform, full overlay/generation identity for the room-entry
> relocatable writers, and body/shadow/multipart grouping remain unresolved
> -- acceptance below is **not yet met**.

The census proves a common sprite submission ABI only when, across the required
maps and transitions, it identifies a stable record/producer contract; signed
logical coordinates before truncation; camera/map transform; creator/update and
despawn lifecycle; overlay identity; writer and exact-DMA correlation; and
body/shadow/multipart grouping with no unresolved alternate route that can
reach the same seam.

Only then may implementation propose a centralized Expanded cull/mirror seam:
copy immutable per-frame submissions before GBA wrapping, cull by sprite bounds
against the Expanded viewport, preserve the canonical 240x160 center exactly,
and fall back loudly to canonical rendering for unknown/overflow/mismatched
records. Native and 288x160 must remain unchanged. Validate synthetic policy
tests first, then rebuild and request user root-launcher acceptance.

Stop and fall back to canonical behavior if identity, signed coordinates,
writer/DMA matching, scene-space classification, queue capacity, or lifecycle
provenance is incomplete; if anchors diverge; if an overlay is unknown; if a
record is reused; or if the proposed mirror changes guest state. The next
implementation handoff must include the table, anchor-matched replay pair,
route coverage, explicit unresolved TODO-EVIDENCE, bounded log excerpts, and a
small policy/test specification. No behavior change is justified by a single
NPC, screenshot, OAM slot, or unrecorded run.
