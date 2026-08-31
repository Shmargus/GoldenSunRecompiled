# WIDE-01 NPC culling/wrapping handoff - 2026-08-30

## Current on-disk state (read this first)

The 15:04 `reconcile_golden_sun_obj_slot_commit_identity()` change described
below as regressed has been **removed** from `src/runner_main.cpp` (its two
helper functions and sole call site). The prior top-left/full-size bounds
culler is again the active code. The non-LTO build succeeded after the
revert. **The user has not yet tested the reverted build.** That manual
Expanded-mode/Experimental-Fixes test is the next step before any further
culling change; see "Exact next steps" below, which still applies unless that
test is clean, in which case WIDE-01 moves to the next reported symptom (if
any) rather than another culling change.

## Current result (history: the now-reverted 15:04 build)

The latest build was **regressed**. After the 15:04 slot-reuse change, the user
reports sprites jumping/wrapping to the bottom again. Do not claim this fixed.
No post-15:04 session log accompanied that report.

- Playable: `build/gs011_opt/GoldenSunRecomp.exe`
- Built: `2026-08-30 15:04:51`
- Size: `883148842` bytes
- Launch only repository-root `GoldenSunLauncher.exe`.
- Native 240x160 and 288x160 must remain unchanged.
- Dynamic RAM remains **NOT_STATIC**.

## Current code state

- B27E admits strict Expanded Y `160..199`.
- B328 admits that band only with an exact same-frame B27E parent and matching
  staging address, call depth, and return PC. Parentless routes remain closed.
- Signed placement is tracked from the transient staging record to the OAM
  slot and exact ATTR0/1/2 identity, then published on the exact OAM DMA.
- Experimental culling requires exact same-frame placement, uses the sprite's
  top-left and full dimensions, and keeps ambiguous objects visible.
- Body and shadow culling use the body's measured placement.
- Bounded `[wide-obj-y-parentless-token]` diagnostics record rejected B328
  candidates. They do not change behavior.
- The latest suspect change is
  `reconcile_golden_sun_obj_slot_commit_identity()` in `src/runner_main.cpp`.
  On an experimental F0 OAM commit, it clears pending/visible placement and
  signed/alias state when full ATTR identity differs. The user reports wrapping
  after this change.

Update `2026-08-30` (revert): the 15:04 `reconcile_golden_sun_obj_slot_commit_identity()` change (and its two helper functions and sole call site) was removed from `src/runner_main.cpp`. The top-left/full-size bounds culler is again the active state. Manual user testing is pending.

Relevant files:

- `src/runner_main.cpp`
- `src/widescreen_policy.h`
- `tests/cpp/widescreen_policy_test.cpp`
- `docs/issues/WIDE-01.md`

Never edit `generated/**`.

## Session evidence

### `session_20260830_125122`

- Positive bottom Y is real: raw Y around `160..171` was accepted as positive.
- One slot alternated between accepted positive Y and canonical negative Y
  after an ATTR/raw mismatch. This proved a late identity-timing flicker.
- The old raw-coordinate resign and center/half-size culler was unsafe.

### `session_20260830_134250`

- The top-left/full-size experimental culler improved appearance.
- It made **zero actual cull decisions**; all 164 observed objects were kept.
- Therefore the reported bottom blip was earlier than the final culler.
- Parentless B328 candidates existed, but no matching writer chain was proven.
- A proposed extra 32-pixel producer margin was rejected and reverted because
  it would not affect the proven route.

### `session_20260830_145317`

- Parentless token sequences had no matching EC/F0 entry, target slot, or
  writer identity. They remain unsafe to admit.
- At frame `521826`, slot 0 changed from ATTR `219e/8078/0924` to
  `2150/8d00/05a4`; the observed writer belonged to another staging record.
- At frames `521912..521914`, slot 1 changed from canonical negative output on
  an ATTR mismatch to accepted positive output once ATTR matched again.
- This shows slot/placement identity timing is still the earliest measured
  wrap symptom. It does **not** prove parentless B328 should be widened.

## Changes tried and outcome

1. Raw Y `>=128` resign plus center bounds: caused sliced bottoms, early shadow
   loss, and flicker. Superseded.
2. Exact placement plus top-left/full-size bounds: visibly better and kept
   ambiguous objects visible.
3. Extra 32-pixel cutoff distance: not evidence-backed; reverted before build.
4. Parentless B328 token trace: useful, but proved no safe writer chain.
5. Clear signed state on committed full-ATTR mismatch: built at 15:04; user
   reports wrapping regression. Treat this change as suspect.

## Highest-priority hypothesis

The 15:04 reconciliation may clear valid signed placement at the wrong point
in the writer sequence, or may miss the actual reused-slot route. Full ATTR0
contains Y, so a normal moving sprite changes it. Do not weaken identity masks
without first proving the ordering error.

## Exact next steps

Superseded note: rather than diagnosing the exact divergence via steps 1-4
below, the 15:04 change was directly removed (see "Current on-disk state"
above) without a post-15:04 log ever surfacing. Steps 1-4's diagnostic value
is now moot for that specific change; step 6's instruction to retest still
applies and is the immediate next action. If a *future* culling change
regresses again, steps 1-4 remain the right diagnostic method to apply to it.

1. Find the newest post-15:04 log, if one exists. Otherwise request one repeat
   through the root launcher with Expanded, Experimental Fixes, and WIDE
   diagnostics enabled.
2. Identify the first visible wrap frame and slot. Start at that committed OAM
   write, not a later rendered pixel.
3. Add or use a bounded first/change record around
   `reconcile_golden_sun_obj_slot_commit_identity()` containing slot, writer
   route, old/new ATTR0/1/2, pending/visible match results, and exactly which
   state was cleared.
4. Compare that event with the same frame's staging source, EC/F0 context, DMA
   publication, and final signed/canonical Y decision.
5. If the 15:04 reconciliation is the first divergence, remove or reorder only
   that change while retaining the top-left/full-size culler and diagnostics.
6. Implement a replacement only after the same-frame source-to-slot chain is
   exact. Run focused widescreen tests, rebuild non-LTO with `--parallel 1`,
   then return to user gameplay testing.

## Do not do

- Do not globally reinterpret raw Y `160..255`.
- Do not admit parentless B328 from operand, frame, staging address, or slot
  alone.
- Do not add more distance; the latest logs show no final-culler decision.
- Do not hardcode one NPC, statue, slot, or screen coordinate.
- Do not weaken DMA, epoch, source, or full-identity checks without measured
  replacement evidence.
- Do not change Native/288, edit generated files, or classify RAM as static.

## Acceptance

- NPC bodies and shadows stay paired through all four edges.
- Sprites remain until fully outside the expanded view.
- No sprite appears at the opposite edge.
- Repeated slot reuse does not flicker, pop, or wrap.
- Toggle-off, Native, and 288x160 behavior remain unchanged.
- No new dispatch misses or protected data.
