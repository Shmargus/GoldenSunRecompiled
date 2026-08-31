# WIDE-01 NPC body vs. shadow vs. object identity — log-only analysis (2026-08-30)

## Settled rules (read this first; rest of the file is the supporting analysis)

This document accumulated across several passes; these are the final,
settled conclusions as of 2026-08-30. They supersede any earlier tentative
wording in the sections below where the two disagree.

1. **Body/shadow pairing is structural, not pixel-offset.** For NPC record
   slot N (base `0x03002000 + N*0x38`), the body is committed via the D4
   Func_1dc8 route with `source == slot_base`. If that NPC currently has an
   active shadow, it is committed via the EC/F0 routes with
   `source == slot_base + 0x0C` — a second, embedded sub-record 12 bytes
   into the same slot, always carrying OAM `tile=0, shape=1, size=0`.
   Ownership is "same record, fixed sub-offset," not an inferred on-screen
   distance. See "Update 2026-08-30 (EC/F0 route extension)" below.
2. **NPC and interactable Object (statue, chest, sign, pot) are one
   category for this work.** The user decided they do not need to be told
   apart; the investigation into separating them (see "Update 2026-08-30
   (NPC vs. interactable Object)" below) is closed without a rule, by
   decision, not because one couldn't eventually be found.
3. **False positives: see "Update 2026-08-30 (false-positive risk /
   player identification)" below.** Summary: the player is almost
   certainly one of the 14 slots (best candidate: slot 0, the only slot
   present in every substantive scene this session); UI/overlay sprites
   and particle effects show no evidence of using this record range, but
   effects specifically were never directly instrumented so that is a gap,
   not a clean negative; battle was never entered by any replay in this
   investigation, so battle-specific objects (turn order icons, damage
   numbers, summon sprites) are untested; and the known "0x03002000
   sometimes hosts relocatable code, not the record" caution from
   `WIDE-01_ENTITY_PRODUCER_FINDINGS.md` is now directly confirmed to
   recur at every scene transition in the session log used here — the
   rule would misread that range if read outside the epoch-gated window
   the existing code already uses.

Scope: distinguish NPC *body* from *shadow* from other *objects* (statue,
chests) using **only existing logs**, no new instrumentation, no rebuild, no
game run. Primary source: `logs/session_20260830_015252.log` (second-town,
Expanded, both record diagnostics active; largest/most complete for this
question), cross-checked against `logs/session_20260830_014247.log` and
`logs/session_20260830_013222.log` (Bilibin/Palace session). Reads
`docs/issues/WIDE-01_ENTITY_PRODUCER_FINDINGS.md` and
`docs/issues/WIDE-01_CULLING_HANDOFF.md` as given context, not re-derived.

Method: regex key=value extraction over `[wide-obj-*]` and `[oam-shadow*]`
tags; scratch Python scripts used, kept out of the repo. All numbers below
are from real log lines, not inferred.

## Q1 - Do the logs carry OAM ATTR0/1/2 per staged slot?

**Yes.** `[wide-obj-y-alias]`, `[wide-obj-y-jump]`, and
`[wide-obj-y-transition-sample]` all log `attr0=0x.... attr1=0x.... attr2=0x....`
per event, keyed by `slot` (the destination OAM slot index, confirmed
against `[wide-obj-handoff]`'s `destination` field, which equals
`0x0300347C + slot*8`). Example, `logs/session_20260830_015252.log`:

    [wide-obj-y-transition-sample] ... slot=11 raw_y=174 ... attr0=0x21ae attr1=0x83d3 attr2=0x0964 ...

Decoding ATTR2 (tile = attr2 & 0x3FF, priority = bits 10-11, palette =
bits 12-15) and ATTR0/1 shape/size bits (shape = attr0 bits 14-15, size =
attr1 bits 14-15) across every such event in the 015252 session, grouped by
OAM slot, gives two recurring, sharply distinct signatures:

- **"body/object" signature:** tile in {260, 292, 324, 356, 388, 420, 452,
  484, 516, 548, 580, 612, 676} (multiple distinct tiles per slot -
  animation frames), shape=0 (square), size=2, palette=0, priority mostly
  1-2. Seen on OAM slots 0, 1 (partly), 2, 3, 4, 6 in the 015252 session,
  and on slots 0,1,3,4,6,7,8,9,10 in the Bilibin/Palace session.
- **"small fixed" signature:** tile=0 constant, shape=1 (wide rectangle),
  size=0 (smallest allowed for that shape), palette=0, priority 2-3
  (drawn behind/at the body's layer - GBA priority 3 is lowest). Seen on
  OAM slots 8, 9, 10, 12, 13, 14, 15 in the 015252 session, and on slot 12
  in **both** the Bilibin/Palace and second-town sessions with identical
  values (tile=0 shape=1 size=0 priority=2). This shape/size/tile
  combination is consistent with a small, non-animated GBA "blob" sprite:
  single tile, fixed pose, drawn under a body - the classic shadow
  signature.

**This is a real, provable ATTR-level discriminator between "animated
multi-tile sprite" and "single fixed small sprite" OAM writes.** It answers
the tile/palette/priority half of Q1 with evidence. What it does **not**
prove (see Q2/Q3) is that this signature is bound to one persistent slot,
or paired 1:1 with one specific body.

**Caveat found while checking this:** several OAM slots show a **mixed**
history - e.g. slots 5, 7, 11 in the 015252 session carry both the
body-type tiles (324, 356, 452, 548, etc., shape=0 size=2) *and* the
tile=0/shape=1/size=0 signature at different frames. The OAM slot index
does not have one fixed occupant across the session - it gets reused by
whatever the game currently wants to draw there that frame. This matches
the existing note in `WIDE-01_CULLING_HANDOFF.md` ("Changed/reused slots
fail closed to canonical GBA decoding") - a known property of this table,
not a new problem, but it limits how far the ATTR signature alone can be
trusted as a persistent identity (see Q3).

## Q2 - Is record-slot -> OAM-slot 1:1, or does one record produce multiple OAM entries?

**1:1 in every sample available.** Joined `[wide-obj-handoff]` by
`(frame, staging)` and counted distinct `destination` values per key across
the full 015252 session: **69 distinct (frame, staging) keys, 0 with more
than one destination.** No record was observed staging to two different
OAM destinations in the same frame.

**Caveat:** `[wide-obj-handoff]` is capped and this session only captured
70 total events, clustered mostly at one early frame (314953). This is real
evidence for the frames sampled, not a proof for all frames/scenes. It also
does not rule out a body+shadow pair produced by *two separate records*
(rather than one record producing two OAM writes) - that possibility is
what Q3 tests directly.

## Q3 - Do body/shadow candidate pairs show a fixed X, Y-offset relationship over many frames?

**Not provable from these logs - tested directly, no locked pair found.**

Built a per-frame, per-slot (X, Y) table from `[wide-obj-record-value]`
(+0x04 = Y, +0x06 = X, using the proven decode rules from
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md`) across the full 015252 session (14
record slots, hundreds of frames). Findings:

- The most plausible small-fixed-sprite record slot by attr signature
  (slot 11, sampled 64 consecutive frames from frame 315763) is
  **static**: exactly (x=76, y=-121) unchanged for all 64 frames, while
  other slots in the same frame window are actively moving (slot 5's x
  drifts 216 -> 227 over the same span; slot 6's y drifts too). A shadow
  tied to a moving body would move with it; this one does not. This is
  more consistent with a static object (e.g. a statue - matches the
  "statue" object already flagged in `WIDE-01_CULLING_HANDOFF.md`'s B328
  investigation) than an NPC shadow.
- No pair of slots was found where one slot's (x, y) tracked another
  slot's (x, y) with a constant offset while both moved, across the frames
  sampled. Per-slot movement ranges vary independently with no matching
  deltas between any two slots.
- Because OAM-slot identity is reused across frames (Q1 caveat) and the
  record's own +0x00 field is only a weak allocator/free-list candidate,
  explicitly *not* proven to be an ownership link (per the existing
  findings doc), there is no field or handle in these logs that could tie
  two record slots together as one entity's body and shadow.

**TODO-EVIDENCE:** a genuine moving body plus its shadow, logged over the
same frames with both X and Y, where the candidate shadow slot stays
locked relative to its parent, was not captured in any available session.
The one static "shadow-signature" slot found here looks like a non-shadow
static object instead.

## Q4 - Does any record byte offset act as a stable per-slot type/kind field?

**No - tested directly, no partition found.** Extracted the full value set
per (record slot, byte offset) for every offset the existing census already
labelled "not X/Y" (+0x00, +0x01-+0x03, +0x05, +0x07-+0x0f, +0x11-+0x13)
across the whole 015252 session. Every 1-byte offset in this range shows
3-9 distinct values per slot, varying continuously (consistent with
animation-frame counters or similar fast-changing state, not a fixed
type/kind tag) - no offset holds one constant value per slot that differs
stably between slots.

One artifact worth flagging, not a finding: reading the 2-byte variant of
several of these offsets (+0x00, +0x08, +0x0a, +0x0c, +0x0e, +0x10) at
slots 6-13 shows values that increase linearly by exactly 0x1c per slot
index (e.g. +0x0c size=2: slot6=0x12, slot7=0x2e, slot8=0x4a, ...,
slot12=0xba, each step +0x1c). That is almost certainly a mis-attributed
capture of an adjacent small lookup table or index-scaled pointer
arithmetic elsewhere in the writer, not real record content - reported
here only so it is not mistaken for a discovered field. Not investigated
further (out of scope for this pass).

## Answer summary

| Question | Answer |
|---|---|
| Q1: ATTR-based body/object vs. small-fixed discriminator | Provable. tile=0, shape=1, size=0 = small/fixed signature vs. tile in {260..676}, shape=0, size=2 = animated body/object signature. Confirmed in both sessions. Caveat: OAM-slot identity is reused frame-to-frame, so this tags individual OAM writes, not a persistent object. |
| Q2: record-slot -> OAM-slot mapping | 1:1 in every sample (69/69 keys), but the sample is small and clustered at one frame. |
| Q3: locked body/shadow X,Y-offset pair | Not found. The best small-fixed-signature candidate is stationary while other slots move - more consistent with a static object (statue) than a shadow. No moving pair with a constant offset exists in the sampled frames. |
| Q4: stable per-slot type/kind byte | Not found. No offset in +0x00..+0x13 (excluding proven X/Y) holds a small stable per-slot constant. |

## What one new diagnostic would close this

None of the above required new instrumentation to answer, but none of them
give a body-shadow-object discriminator strong enough to build a cull/mirror
seam on. The gap is specific: **no log ties two OAM writes together as
"same in-game entity, different draw layer."** The smallest new diagnostic
that would close it: at the exact `[wide-obj-y-transition-sample]` /
handoff event, additionally log whichever guest register or record field
(if any) is common between a body write and its shadow write for a single
in-game entity - the natural place to look is whatever argument
`Func_c62c` (`0x0800c62c`, the OAM-shadow-commit function per the existing
findings doc) is called with immediately before/after a body write for the
same logical NPC, since that function already writes the record's +0x09,
+0x15, +0x25 fields and commits to OAM. Scoping and adding that diagnostic
is a planner decision, not done here.

## Evidence status

All findings above are from real log lines in
`logs/session_20260830_015252.log`, `logs/session_20260830_014247.log`, and
`logs/session_20260830_013222.log`, parsed with scratch Python scripts (not
committed to the repo). No ROM, BIOS, or asset bytes were read or
reproduced. Nothing here overrides or edits any prior finding in
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md`.

## Update 2026-08-30 (later) - live replay with the new [wide-obj-commit-order] tag

Replay run via `scripts/gs-replay.ps1` (root launcher only, secondary monitor,
`-Mute`, no focus steal) using `logs/session_20260829_234316.input`
(1183 frames) on the savestate that session's own log names
(`savestate_loaded slot=2 path="C:\Users\Jimmy\Documents\rom\Golden
Sun.state2"`), with `build/gs011_opt/config.ini` `[Enhancements]
Widescreen=true` / `ViewMode=2` set beforehand and restored afterward
(verified no `GoldenSunRecomp`/`GoldenSunLauncher` process was running
before restoring, and re-read the file after - back to
`Widescreen=false`/`ViewMode=0`). New log:
`logs/session_20260830_113019.log`.

**Fidelity: matched.** `frames_presented=1183` (equals the input's own
recorded length from `20260829_234316.log`). `[wide-field-table-summary]`:
`map_first_frame=521695 map_last_frame=522569`, exact match to the existing
Expanded run of the same state+input (`logs/session_20260830_011210.log`,
same two values). Same-input, same-state, same result - this is the
canonical Expanded replay of the Bilibin/Palace session.

**New tag output:** 783 `[wide-obj-commit-order]` events, 3 dropped
(`[wide-obj-commit-order-overflow] ... dropped=3`), well inside the cap and
with no scene-transition stall observed (per-epoch counts stayed small
because of the change-dedup).

**Result: body/shadow pairing is still not provable, and this run adds a
concrete negative.** Decoded every event's ATTR2 tile/ATTR0 shape/ATTR1
size. Across all 783 events in this session, **zero** carried the
`tile=0, shape=1, size=0` small/fixed signature identified earlier in the
second-town session - every committed record here is a body/animated-tile
write (tiles `{260,264,292,296,324,328,356,388,392,420,452,484,516}`, shape
0 or 2, size 1-3). The hooked commit path
(`golden_sun_obj_staging_handoff`, the D4/Func1dc8 route) evidently is not
how the small-signature objects reach OAM in this scene/session - either
they use a different writer route not hooked by this diagnostic, or none
were on-screen with valid B328 Y-provenance during this particular replay
window. Either way, this diagnostic cannot show a body+shadow pair that
never appears in it.

Also tested the fallback the task asked for directly: searched all 78
source-address pairs for a locked (single-value) X and Y offset while both
sides showed many distinct positions (i.e., both actually moving, not just
static). Found several, e.g. `source=0x030021c0` vs `source=0x030021f8`:
39 shared frames, offset locked at exactly `dx=16, dy=104` while both
positions drift together frame-by-frame (e.g. y: 170,169,168,... paired
with 66,66,65,...). **This is not a body/shadow pair** - both sides carry
the same full body/animated-tile ATTR signature (tile 452 and tile 420,
shape=0, size=2 on both), and `dy=104` px is far larger than a shadow
offset. The far more likely explanation is shared camera-relative drift:
when the view scrolls, every on-screen record's logical X/Y shifts by the
same delta regardless of whether the underlying game object itself moved,
so two independently-static (or independently-moving-in-parallel) objects
will show a "locked offset" that has nothing to do with ownership. This is
a real confound for the offset-locking test as specified, not something
this replay happened to avoid.

**Conclusion: no provable body/shadow discriminator or pairing rule came
out of this replay.** The negative finding is itself real evidence: the new
diagnostic works correctly (bounded, fidelity-matched, camera-drift-exposed
confound caught), but the small-signature shadow-like writes are not
reachable through the one commit path it currently observes, and the
"locked offset while both move" test alone is not sufficient - it needs to
control for camera scroll, which no field logged here isolates.
**TODO-EVIDENCE:** (1) whether the small-signature objects reach OAM
through a different Func1dc8 writer route (EC/F0) that this hook does not
cover - would need extending `note_golden_sun_obj_commit_order`'s call
site(s) to those routes too; (2) a camera/scroll-relative coordinate (or
the raw camera delta per frame) to subtract out shared drift before
re-running the offset-lock test.

## Update 2026-08-30 (EC/F0 route extension) - body/shadow pairing IS now provable, via record structure not pixel offset

`src/runner_main.cpp` extended `[wide-obj-commit-order]` to the other two
Func_1dc8 writer routes: EC (`golden_sun_obj_f0_entry_capture`, entry
before the LDM that destroys R6) and F0 (`golden_sun_oam_shadow_write_observer`,
the actual committed OAM-shadow write). Added a `route=D4|EC|F0` field to
every event. `kGoldenSunObjCommitOrderLimit` raised 256 -> 512 (shared
across all three routes, reset per auth-epoch as before).

**Volume-control iteration (relevant to future work on this tag):** the
first attempt logged every EC/F0 event unconditionally and immediately
overflowed the cap by ~4000/epoch, because EC fires on every active object
every frame (unlike the gated D4 route) and F0's ATTR bytes encode live
raw Y/X that change almost every frame for a moving object - fine-grained
dedup essentially never matched. Fixed in two steps: (1) restrict EC/F0
logging to exactly the shadow-candidate signature (`tile=0, shape=1,
size=0`) identified earlier in this document - directly the question these
routes exist to answer; (2) even filtered to that signature, a moving
shadow's raw ATTR still changes almost every frame, so EC/F0 dedup was
loosened from exact-value-match to presence-only (log the first frame an
object is seen on that route per epoch, not every per-frame position
tick) - D4 keeps its original fine-grained dedup unchanged. Both fixes are
in `note_golden_sun_obj_commit_order`/its EC/F0 call sites.

**Important scoped finding, not a defect in this new tag:** even after
these fixes, replay frames still show ~4-5 second single-frame stalls at
scene-transition frames (e.g. `frame=522077`, `522567` - the exact frames
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md` already documents as scene
transitions). Checked directly: `[wide-obj-record-writer]` and
`[wide-obj-record-value]` (pre-existing tags from earlier work, sharing the
same diagnostics-enabled gate and the same auth-epoch report points) log
**7,925** and **17,407** lines respectively in this exact replay - both
counts identical whether or not `[wide-obj-commit-order]` is active at all
(confirmed by comparing against `logs/session_20260830_113019.log`, the
D4-only run from the previous pass, which shows the *same* stall
magnitude at the *same* frames with the *same* 7,925/17,407 counts). This
is the pre-existing synchronous-fprintf cost already flagged in
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md`'s "Performance" section ("would
freeze a real diagnostic session for seconds at every map transition...
flagged for whoever next touches this instrumentation"), not something
this tag introduces. `[wide-obj-commit-order]`'s own contribution this run
was 1,391 lines (1,335 D4 + 23 EC + 33 F0) - a small fraction of the
~25,000 lines the two pre-existing tags produce at the same boundaries.
Not fixed here (out of scope: "no rendering/culling behavior change...
don't touch existing censuses beyond what's needed for this one").

Replay: same as the previous update (`logs/session_20260829_234316.input`,
1183 frames, `Golden Sun.state2`, root launcher, secondary monitor, muted,
config restored and re-verified after each of four runs). Final analysis
run: `logs/session_20260830_114823.log`. **Fidelity: matched** -
`frames_presented=1183`, `[wide-field-table-summary]`
`map_first_frame=521695 map_last_frame=522569`, identical to the existing
anchor in every one of the four runs made this pass.

### EC/F0 do carry the shadow signature

Confirmed directly: 23 EC events and 33 F0 events this run, all decoding to
exactly `tile=0, shape=1, size=0` (by construction of the filter, but their
non-zero count is itself the answer - the signature is real and reachable
through both routes, not only theoretical).

### The real pairing rule: structural (same-record sub-offset), not a pixel dx/dy

Every single EC `source` address observed this session (10 distinct
addresses, whole 1183-frame run) decodes to exactly some NPC record's
`slot_base + 0x0C` - the same `+0x0C` field
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md` had already proven is "not X/Y" and
flagged as a possible pointer, with no further semantics resolved. That
gap is now closed: **`+0x0C` is not a pointer to another slot's owner ID -
it is the base address of a second, embedded 12-byte-offset sub-record
inside the same 0x38-byte slot, and that sub-record is the NPC's shadow.**
Evidence, `logs/session_20260830_114823.log`, frame 521714: EC reports
`source=0x03002274`; `0x03002274 - 0x0C = 0x03002268`, which is exactly
the source of a `route=D4` body write logged in the **same frame**
(`sequence=6 source=0x03002268 slot=8 logical_x=95 logical_y=-87`). This
is not a one-off: joining every EC event this session to a same-frame D4
event whose `source` equals `EC.source - 0x0C` matches **18 of 23** EC
events (78%) exactly, with zero counter-examples (every EC source this
session was `some_slot_base + 0x0C`, no exceptions). The 5 non-matches are
frames where the shadow's raw ATTR changed but the parent's own
`logical_x`/`logical_y` happened not to (D4 only logs on an actual
coordinate change), not a break in the rule - a limitation of same-frame
dedup timing across two independently-gated routes, not evidence against
the structural link.

Write-order position: the shadow's EC/F0 pair is not immediately adjacent
to its parent's D4 write in sequence number (observed `seq_gap` -7 to +12
across the 18 matched pairs, with other unrelated objects' events
interleaved between) - adjacency-in-sequence is **not** the correlating
signal; **shared record base (`shadow_source - 0x0C == body_source`) is**.

**Concrete rule, provable from these logs:** for NPC record slot N (base
`0x03002000 + N*0x38`), the body's own committed identity is written via
the D4 route with `source == slot_base`; if that NPC currently has an
active shadow, the shadow's committed identity is written via EC (staging
capture) and F0 (OAM commit) with `source == slot_base + 0x0C`, always
carrying `tile=0, shape=1, size=0` on the OAM side. Ownership is
structural (same 0x38-byte record, fixed 0x0C sub-offset), not inferred
from screen-space proximity - this also explains why the earlier
offset-locking test on raw logical X/Y was the wrong tool: pixel offset
was never the mechanism, and testing it was vulnerable to the
camera-scroll confound noted in the previous update for exactly that
reason.

**What is still not established:** the shadow sub-record's own signed
logical X/Y was not captured by this pass (`x_valid=0, y_valid=0,
logical_x=0, logical_y=0` on every EC/F0 event, honestly reported as
unavailable rather than approximated - see the code comments at both call
sites). A pixel-level shadow-to-body offset (e.g. "shadow renders N
pixels below body") is therefore still `TODO-EVIDENCE`; it was not needed
to answer the ownership question, but would need `find_golden_sun_obj_
staging` threaded into the EC hook (D4 already does this) if a future pass
wants it.

## Update 2026-08-30 (NPC vs. interactable Object) - not separable from current logs; one promising lead, unconfirmed

Analysis-only, no new instrumentation, no replay run for this pass. Sources:
`logs/session_20260830_114823.log` (Bilibin/Palace, has full
`[wide-obj-commit-order]` D4+EC+F0 route data plus
`[wide-obj-record-value]`), `logs/session_20260830_015252.log` (second
town, `[wide-obj-record-value]` only, predates the commit-order tag).

### Ground truth is the blocker, not the discriminator search

Per instructions, movement alone was not used as the rule - only as a
labeling aid, and only where a genuine non-NPC (Object) exemplar exists to
check candidate signals against. That exemplar does not exist in any log
currently on disk: `WIDE-01_CULLING_HANDOFF.md`'s statue evidence
(`raw Y=179`, "no B27E parent", "not arithmetic wrap") comes from session
logs dated `20260829_1*` (e.g. `114447`) that have since rotated out of
`logs/` - `ls logs/` confirms they no longer exist (only
`183155` onward from 2026-08-29 remain). The statue's actual ATTR/tile/
shape/size/priority values cannot be re-checked; this is `TODO-EVIDENCE`,
not assumed.

Without that confirmed-Object sample, "static in the current record array"
cannot stand in for it either, because it collides with the explicit rule
that movement is not sufficient: in `session_20260830_114823.log`, **all
14 record slots that produced any body write moved at some point during
the 1183-frame session** (checked two independent ways -
`[wide-obj-commit-order]` D4 events and the unconditional
`[wide-obj-record-value]` write census; both agree, see per-slot movement
tables below). Longest-run analysis on `session_20260830_015252.log`
(second town) finds standstill windows up to 80 frames (slot 0,
frames 315420-315500) - consistent with an NPC standing still, which the
task explicitly says must not be mistaken for an Object. No slot in either
session is static for its entire observed window, so there is no clean
"this slot only ever appears static" candidate to label as an Object
either.

### Signals tested against the closest available proxy (shadow presence)

Since no confirmed-Object exemplar exists, the following were checked
descriptively across the Bilibin/Palace session's 14 record slots rather
than scored against ground truth:

- **Tile/palette/shape/size:** 12 of 14 slots share one signature - tile
  in `{260,264,292,296,324,328,356,388,392,420,452,484,516,548,580,612}`
  (multi-frame animated), `shape=0`, `size=2`, `palette=0`. Slot 13 is the
  one outlier (`shape=2, size=3, tile=516` - a taller/larger sprite), but
  slot 13 also **moves** (y drifts 159->148 over multiple frames, per the
  earlier EC/F0 pairing pass) - a moving big sprite is not evidence of a
  static Object, so this shape/size difference does not reliably flag
  "Object" either. Priority: 13 of 14 slots include priority values in
  `{1,2,3}` depending on frame (priority visibly changes per-frame for
  most slots, consistent with dynamic z-ordering, not a fixed per-entity
  tag); only slot 12 is priority-`1`-only in this sample, too thin (n=72
  events, one slot) to generalize.
- **Shadow presence (EC/F0):** 12 of 14 slots got at least one captured
  shadow write this session; slots 12 and 13 did not. This is the most
  interesting result and matches the coordinator's hint, but it is **not
  confirmed** as an NPC/Object split: slot 13 visibly moves (see above),
  which is a strange property for what would need to be a static
  interactable Object, and the commit-order tag's own EC/F0 logging is
  coarsely deduped (first occurrence per route per epoch, see the previous
  update) - a real shadow could exist for slot 12 or 13 and simply not
  have produced a *new* (route, dedupe-bucket) combination during this
  particular 1183-frame window. Absence of a logged shadow in this data
  is not proof of absence of a shadow in the game.
- **Record byte offsets `+0x05`, `+0x07`, `+0x10`-`+0x13`:** no stable
  per-slot constant found that splits any group from any other. Every
  slot shows 3-44 distinct values at every one of these offsets in the
  Bilibin/Palace session (full table below), consistent with continuously
  changing state (animation/walk-cycle counters), matching the same
  negative result the original census already reported for these offsets.
  One offset, `+0x10`, shows a visibly *lower* distinct-value count for
  slots 11 and 12 (8 and 5, vs. 26-44 for slots 0-10) and **zero writes at
  all** for slot 13 - a real, measured difference - but it does not track
  shadow presence cleanly: slot 11 has a confirmed shadow (`has_shadow=
  True`) yet shares slot 12's low-variety `+0x10` pattern, and slot 13
  (no shadow) differs again by having no writes there at all rather than
  a low-variety set. Three different behaviors across three slots is not
  a rule.

Per-slot summary (Bilibin/Palace session, D4-route body writes):

```
slot  moves  tiles                    shape  size  has_shadow
0     yes    260,264                  0      2     yes
1     yes    292                      0      2     yes
2     yes    292,296                  0      2     yes
3     yes    324,328                  0      2     yes
4     yes    324,452                  0      2     yes
5     yes    356,392                  0      2     yes
6     yes    260,356,484              0      1,2   yes
8     yes    420                      0      2     yes
9     yes    452,612                  0      2     yes
10    yes    484                      0      2     yes
11    yes    388,580                  0      2     yes
12    yes    420                      0      2     no
13    yes    516                      2      3     no
```
(Slot 7 produced record-value writes but no D4-gated commit-order event
this session - consistent with the earlier finding that D4 only fires on
a gated, successful B27E/B328 commit, not every write.)

### Conclusion

**Existing logs cannot cleanly separate NPC from interactable Object.**
Every tested static signal (tile/palette range, shape/size, priority,
record byte offsets `+0x05`/`+0x07`/`+0x10`-`+0x13`) either shows no
stable split at all, or the one candidate split found (shadow presence:
12/14 vs. 2/14) cannot be verified because no session currently on disk
contains a confirmed-Object exemplar to check it against, and the one
"no-shadow" slot with an otherwise-distinctive signature (13) undermines
its own case by moving. This is reported as the plain negative it is,
per the task's own instruction that a clean negative is better than a
shaky rule.

**The single new signal that would close this:** a real, independently
confirmed Object sample. The cheapest route is not new instrumentation -
the `[wide-obj-commit-order]` (route/shadow) and `[wide-obj-record-value]`
(byte offsets) tags already capture everything this analysis needs; what
is missing is a replay that actually revisits the Palace statue's known
screen position (raw Y around 179, per `WIDE-01_CULLING_HANDOFF.md`) for
long enough with those tags active, since the original session that
observed it has rotated out of `logs/`. That is a replay-scheduling
decision, not a code change, and is left to the coordinator rather than
executed here per this pass's analysis-only scope.

## Update 2026-08-30 (false-positive risk / player identification)

Analysis-only, no new instrumentation, no replay run for this pass. NPC vs.
interactable Object separation is closed per the settled-rules note above -
this pass only asks whether the body+shadow rule (D4 at `slot_base`, shadow
at `slot_base+0x0C`, 14-slot array at `0x03002000` stride `0x38`) ever
catches something that is **neither** an NPC nor an Object. Source:
`logs/session_20260830_114823.log` (Bilibin/Palace, full route data),
cross-checked against `logs/session_20260830_015252.log` and siblings
(second town, dialogue-box `state3`).

### 1. The player - almost certainly one of the 14 slots; best candidate is slot 0

Checked which record slots have any `[wide-obj-record-value]` activity in
every substantive `auth_epoch` this session (world map -> Bilibin ->
Palace -> Bilibin -> world map -> tail; epoch 10 excluded, a single-sample
transitional epoch with only 1 event total). No slot appears in literally
every epoch except slot 0, which is present in epochs 0, 1, 4, 5, 8, 9,
and 12 - every substantive epoch in the session. Every other slot drops out
of at least one scene (e.g. slot 12 is prominent in the Bilibin town epoch
but absent from the Palace epoch and the return-trip epoch) - expected for
scene-specific NPCs, not expected for the player, who exists in every
scene without exception.

Cross-checked with a second signal: within the long Bilibin-town epoch
(epoch 1, 66-68 samples per slot while the input file shows the player
actively holding Up/Right/Left plus the run button for most of that
window), slot 0's on-screen coordinate is nearly locked (x_range=0,
y_range=22 over 67 frames) - tight, though not the tightest in that
epoch (slots 2, 3, and 12 show a perfect x_range=0, y_range=0 in the same
window). A camera that follows the player would pin the player's own
screen coordinate close to constant regardless of world movement, which
is consistent with slot 0, but the same epoch also has three other
perfectly-static candidates - most likely stationary town NPCs (shopkeepers)
in a town small enough that the camera barely scrolls during that window,
which weakens "static within one epoch" as a standalone signal (matches
the same caution already on record in this document's NPC-vs-Object
section: movement/stillness alone is not reliable). Slot 0 also has a
confirmed shadow (has_shadow=True), consistent with being rendered like
any other entity.

**Conclusion: slot 0 is the best-supported single candidate for the
player** (unique full-session persistence across every scene, tight but
not perfectly locked in-epoch coordinate range, has a shadow), but this is
not proven beyond doubt - the in-epoch stillness signal alone ties three
other slots, and full-session persistence is the stronger of the two
signals used here. Practical implication for any future culling work: do
not use "moves/doesn't move" or "has a shadow" to detect the player -
neither is unique to it in this data. If a hard guarantee is needed
("never cull the player"), the record's actual player-flag byte (if one
exists) is still unidentified; the census already ruled out every 1-byte
offset in +0x00..+0x13 as a stable per-slot constant (see the original
per-offset findings above), so a flag would have to live outside that
already-checked range, or the game may simply special-case slot continuity
across scene loads rather than storing an explicit flag at all -
TODO-EVIDENCE either way.

### 2. UI/overlay sprites - no evidence they use this record range, best available check

The record-writer census (`[wide-obj-record-writer]`) is unconditional -
it fires on every write to `0x03002000..0x030022E0` regardless of
which game system performs it, not just the D4/EC/F0 NPC pipeline. Its
writer-PC set is essentially closed and identical across four different
sessions checked here (Bilibin/Palace and three second-town captures,
including `logs/session_20260830_015252.log`, which descends from
`Golden Sun.state3` - the savestate documented as loading with a dialogue
box on screen): the same ~85 PCs recur in every session, split only by
the already-documented town-specific EWRAM group
(`0x0200917e`/`0x020084e6` family). No new, UI-shaped writer PC appears in
the dialogue-box session that isn't also in the non-dialogue session. If
dialogue-box arrows, text cursors, or menu icons were writing into this
same record range, an unconditional whole-range census should have caught
a new writer PC during that session - it did not.

This is real but not conclusive evidence: there is no dedicated "is a
dialogue box currently visible" diagnostic in these logs, so it cannot be
directly confirmed the dialogue box was on-screen during the exact frames
sampled by the writer census (only that the savestate is documented
elsewhere as loading into that state). Treated as a reasonably strong, but
not airtight, negative.

### 3. Effects/particles - untested, no diagnostic exists

Searched all available logs for anything resembling a particle/effect
trace; none exists (a search for "particle" or "effect" on
`logs/session_20260830_114823.log` matches only the unrelated
`effective_scroll` field name in `[wide-policy]`/`[wide-policy-sample]`).
This is a real gap, not a checked negative - no field-scene replay in
this investigation was known to trigger a field-effect sprite (footstep
dust, sparkle, etc.), and there is no tag that would have flagged one even
if it occurred. Stated plainly as untested, per the task's own
instruction not to fabricate a result here.

### 4. Battle - explicitly untested, flagged as a known gap

No replay in this entire investigation entered a battle (confirmed by
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md`'s own "Performance" section: "no
battle was ever entered in any of these replays"). Battle-specific sprites
(turn-order icons, damage numbers, summon animations, enemy sprites) are
completely untested against this identification rule. No battle replay
was run for this task per the explicit instruction not to.

### 5. The "0x03002000 sometimes hosts code, not the record" caution - confirmed to recur, and to matter

`WIDE-01_ENTITY_PRODUCER_FINDINGS.md`'s existing caution ("the same
routines also appear at 0x03002000... at other times") is not
hypothetical - it fires in the exact session used throughout this
document. `logs/session_20260830_114823.log` shows
`GoldenSunRecomp: relocatable Func_2544_relocatable verified at base
0x03002000` four separate times (lines 1025, 7062, 17971, 28700), plus
one earlier `[ram-compile] seq=90 pc=0x03002000 ... len=80` recompile of
that exact address before the replay's steady state. Checked each
occurrence's position against the surrounding log: every one of the four
lands immediately before a documented scene-transition `[wide-scene]`/
`[wide-branch]` line at exactly the frames
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md` already identifies as transitions -
521695 (Bilibin entry), 522077 (Palace entry), 522232 (a mid-Palace epoch
boundary), and 522567 (return to world map). Zero occurrences appear
mid-epoch, away from a transition. A `RELOC_BASES` line in the same log
(`index=7 0x03002000 0x03006000 0x0300235C 0x03005CE0`) additionally
confirms 0x03002000 is tracked as one of several live relocatable-code
bases at that point.

**This is a real, provable misfire condition, currently avoided only
because the existing gating already avoids it, not because the address
range is inherently safe.** Every diagnostic and route (D4/EC/F0, the
record-writer/value census) used throughout this document is gated on
`golden_sun_wide_diagnostics_enabled() && g_ws_active`/the current
`auth_epoch`, and in practice only produces record-shaped data once a
scene has settled past its transition frame - so nothing in this
investigation's own data was ever misread as NPC data while it was
actually code. But this is a property of when the existing code chooses
to trust the range, not of the range itself: reading 0x03002000 as an
NPC record unconditionally, without epoch/scene-transition gating, would
misinterpret relocatable-code bytes as record data at exactly these
transition instants, and this happens on every single scene load, not
as a rare edge case. Any future consumer of this identification rule must
inherit the same epoch-gating discipline the current diagnostics already
use, not re-derive its own weaker check.

### Answer summary

| Check | Result |
|---|---|
| Player character | Almost certainly one of the 14 slots. Best candidate: slot 0 (unique full-session persistence). Not proven beyond doubt; do not rely on movement or shadow presence to detect it. |
| UI/overlay sprites | No evidence found in the writer-PC census, including a dialogue-box session; not airtight (no direct "dialogue visible" signal to confirm timing). |
| Effects/particles | Untested - no diagnostic exists for this in any available log. |
| Battle | Untested by explicit instruction - no replay entered a battle. |
| Code-residency misread | Confirmed real and recurring (4/4 scene transitions this session) - the rule is safe only because existing epoch-gating already avoids the window; an ungated reader of this address range would misfire on every scene load. |

## Update 2026-08-30 (coordinate space and the camera)

Analysis-only, no new instrumentation, no replay run for this pass. Source:
`logs/session_20260830_114823.log` throughout (only session with both the
`[wide-obj-record-value]` byte census and `[wide-scene]` BG scroll trace
together).

### Q1 - the record's X/Y are map-relative (camera-independent), not screen-relative

Ran the coordinator's own decisive test: held three different NPC record
slots that produce no net position change, across a scene window with a
large, confirmed real BG scroll change, and checked whether their logged
`+0x04`/`+0x06` values moved with the camera.

`[wide-scene]` in `auth_epoch=1` (Bilibin town field scene) shows a large,
continuous scroll sweep over ~67 frames: BG1 `vofs` goes `0360 -> 0200`
(frame 521715 to 522077 - a 352-unit change), `hofs` `0078 -> 006e` (a
10-unit change). This is real, substantial, continuous camera movement,
not noise.

Over that exact window, three record slots show their `+0x04`/`+0x06`
values completely unchanged, sampled at every write, not just start/end:

- slot 2 (base `0x03002070`): `x=120, y=58` at frame 521716, still
  `x=120, y=58` at frame 521782 (67 samples, zero variance in between)
- slot 3 (base `0x030020a8`): `x=128, y=-54`, unchanged across the same
  67-sample window
- slot 12 (base `0x030022a0`): `x=-256, y=-22`, unchanged across 66
  samples (`x=-256` looks like an off-screen/wrap sentinel rather than a
  plausible on-screen coordinate - this slot is the weaker of the three
  exemplars, kept for completeness, not leaned on)

Critically, this is **not** "stale/frozen because inactive": the record
writer census for these exact addresses shows the field was actively
*rewritten* on nearly every frame in this window (e.g. `addr=0x03002074`
- slot 2's Y - `pc=0x0800b33a size=1 writes=143 frames=521716..521858`;
`addr=0x030022a4` - slot 12's Y - `pc=0x0800b33a writes=211`). `0x0800b33a`
falls inside `Func_b168` (`0x0800b168`, size `0x220`, ends `0x0800b388`) -
the already-identified "X/Y cull/placement function." So the placement
function ran on essentially every frame of this window and **recomputed
the exact same output** each time, despite ~350 units of real scroll
happening underneath it. That rules out "not updated" and leaves only
"the value does not depend on scroll" - i.e. map-relative.

This refines, rather than contradicts, the existing correlation in
`WIDE-01_ENTITY_PRODUCER_FINDINGS.md` ("+0x06 X... bit layout matches the
GBA hardware ATTR1 halfword shape"): that observation was about the
*storage format* (a packed 9-bit-value-plus-flag-byte encoding), not proof
that the *value* is already the final, post-scroll hardware screen
position. The existing docs already called this field "logical... before
OAM truncation," which is consistent with what this test found - it was
this document's own earlier looser language (calling it "screen/OAM
space") that overstated it, not the original census.

**Confidence: high that the record is map-relative** (three independent
slots, one with a plausible near-screen value pair, actively recomputed
~150-200 times against a real, large, continuous scroll change, zero
drift). Not cross-checked yet against the world-map scene (that scene's
own record semantics are less certain - see the NPC-vs-Object section
above on why world-map data was treated as less comparable) or the Palace
interior (checked briefly: slot 0 there shows a much smaller 7-unit drift
against a smaller ~90-unit scroll change in that scene, consistent with
the same conclusion but not as clean a sample).

### If map-relative, the transform - candidate identified, not verified

Per the task's own framing: the standard GBA convention is
`screen = map_position - scroll` (the register holds the map coordinate of
the screen's top-left pixel). `[wide-scene]`'s `scroll=` field is the best
available candidate for that subtraction, logged as three `hofs,vofs`
pairs (per `WIDE-01_ENTITY_PRODUCER_FINDINGS.md`'s existing naming for
this tag, layer order not re-verified here).

**This transform is proposed, not proven.** Two things stop it from being
asserted as confirmed:

1. It was not numerically checked against an independent screen-position
   ground truth (e.g. a moving NPC's actual committed OAM ATTR versus
   `record_value - scroll_at_that_frame`) in this pass - time did not
   allow it, and doing it properly needs a moving (not static) exemplar
   plus per-frame scroll interpolation between `[wide-scene]` samples
   (which only logs on scroll *changes*, not every frame).
2. The logged `scroll` values themselves exceed the GBA hardware BG scroll
   registers' normal 9-bit (0-511) range - `vofs=0x0360` = 864 was
   observed above. That means `[wide-scene]` is not simply echoing a
   masked 9-bit hardware readback; it is either the game's last-written,
   unmasked value, or a wider shadow copy. This is itself a real, useful
   data point for Q2 below (a hint that a wider-than-hardware camera
   value exists somewhere), but it also means "subtract `scroll` directly"
   may need a modulo-512 step, or may need the *unwrapped* wide value
   depending on what the record's own coordinate range assumes - not
   resolved here.

**Confidence: low-to-medium on the exact transform.** High confidence the
right general shape is `screen ≈ record - scroll`; not confirmed
numerically, sign/units/wrap not nailed down.

### Q2 - the camera: GBA scroll registers findable and mined; the underlying IWRAM/EWRAM variable is not visible in current logs

**Immediate camera (BG scroll shadow):** `[wide-scene]` already logs it
per-change, confirmed above (three `hofs,vofs` pairs per event, units
appear to be raw pixels but exceed the 9-bit hardware range in at least
one observed sample - see above). This satisfies the "GBA BG scroll
registers" half of Q2 directly from existing logs; no new work needed
there.

**The game's own camera variable (IWRAM/EWRAM) and its writer function:
not identifiable from existing logs, and this is a real gap, not a
checked negative.** Every write census available in these logs
(`[wide-obj-record-writer]`, `[wide-obj-record-value]`) is deliberately
scoped to exactly `0x03002000..0x030022E0` - the 14-slot NPC record range
- by design (see the `kGoldenSunObjRecordCensusStart`/`End` constants
documented in the original findings). None of them can see a write
anywhere else in IWRAM or EWRAM, so a camera variable living outside that
narrow range - which is likely, since `0x03002000` is fully accounted for
by the 14 NPC slots already mapped - is invisible to every log currently
on disk. There is no broader "any RAM write" census to mine here.

**What would close this (proposed, not added):** a bounded write census
over a wider IWRAM/EWRAM window (or specifically watching known
scroll-write instructions, if any can be identified from
`gbarecomp`/disassembly rather than guessed), looking for a per-frame
writer whose value tracks the `[wide-scene]` scroll deltas 1:1 or by a
fixed scale/offset. This is a new-instrumentation decision for the
coordinator, not executed here.

### Answer summary

| Question | Result | Confidence |
|---|---|---|
| Q1: coordinate space | Map-relative (camera-independent) | High - 3 independent, actively-recomputed NPCs, zero drift over confirmed ~350-unit real scroll |
| Transform to screen space | `screen ≈ record - [wide-scene] scroll` | Low-medium - right shape, not numerically verified, wrap/units unresolved |
| Camera: BG scroll registers | Found - `[wide-scene]` `scroll=` field, already mined above | High (it's a direct log field) |
| Camera: underlying IWRAM/EWRAM variable + writer | Not visible in existing logs - out of every current census's scoped address range | N/A - genuine gap, new instrumentation would be needed and is proposed, not added |

## Update 2026-08-30 (numeric transform verification - inconclusive, with a clean sub-finding and a named gap)

Analysis-only, no new instrumentation, no rebuild, no replay. Source:
`logs/session_20260830_114823.log` (the only session with
`[wide-obj-commit-order]`, `[wide-obj-record-value]`, and `[wide-scene]`
together).

### What "ground truth" actually means in this data - and the problem with it

The task asked to compare the record's map coordinate against "that
slot's actual committed OAM position from `[wide-obj-commit-order]` /
`[wide-obj-handoff]`." Checked precisely what those events' `attr0`/
`attr1`/`attr2` are actually read from:

- **D4's `expected_attr0/1/2`** (`golden_sun_obj_staging_handoff`,
  `read_golden_sun_obj_staging_attrs`) read `bus_read_u32(staging_address+4)`
  and `+8` - i.e. bytes at the record's own `+0x04..+0x0B`. This **is the
  same record data** already used for `+0x04` (Y) and `+0x06` (X), just
  reinterpreted as two 32-bit words. It is not independent - it cannot
  serve as ground truth for "did a scroll transform happen," because it
  cannot show one even if one existed elsewhere.
- **F0's `attr0/1/2`** (`golden_sun_oam_shadow_write_observer`) read
  directly from the committed OAM-shadow buffer at
  `0x0300347C + slot*8` - a genuinely different, independent address, and
  the real source for the eventual `0x07000000` OAM DMA. This is true
  ground truth. But per the previous update's volume-control fix, **F0 is
  only logged in this session when it carries the shadow-candidate
  signature** (`tile=0, shape=1, size=0`) - body-sprite F0 events were
  deliberately filtered out to stop the tag from flooding and stalling
  scene transitions. This session therefore has **zero independent
  ground-truth samples for body sprites**, only for shadows.

### What was tested with the ground truth that does exist (shadow F0 samples)

Joined every EC->F0 adjacent-sequence shadow pair (same method as the
previous body/shadow update) to the record's own `+0x10`/`+0x12` bytes for
the same slot and frame (chosen because these two offsets, previously only
described as "not X/Y" in the original census, turned out - checked here -
to hold coordinate-shaped values in the same format as `+0x04`/`+0x06`).
**Result: exact match in all 14 matched samples**, once wrapped to
hardware width: `F0_attr0 & 0xFF == record[+0x10] mod 256` and
`F0_attr1 & 0x1FF == record[+0x12] mod 512`, every time, no exceptions
(e.g. slot 2, frame 522093: record `scr_x=-74, scr_y=-68` -> wrapped
`(438, 188)`, F0-committed raw `(438, 188)` - exact; slot 11, frame 521714:
record `(103, -61)` -> wrapped `(103, 195)`, F0-committed `(103, 195)` -
exact).

**This resolves the wrap question with high confidence: reading these
signed record fields requires `X mod 512` and `Y mod 8-bit (mod 256)`,
never the wide/unmasked value.** 14/14 exact matches, zero counter-examples.

**But it does not test the scroll transform.** Cross-referencing
`+0x10`/`+0x12` against `+0x04`/`+0x06` (the proven map coordinate) for
the same slot/frame shows a **constant offset, not a scroll-dependent
one**: every sample checked has `scr_x - map_x == +8` and
`scr_y - map_y == +26`, identically, across frames both before and after a
real, large scroll change (e.g. slot 11: `map=(95,-87)`, `scr=(103,-61)`
at frame 521714 with scroll `(0,0)`, and still exactly `+8/+26` at frame
521716 with scroll `bg1=(632,352)` - the offset did not move at all when
the scroll did). **This is a sprite-anchor/draw-offset relationship (e.g.
centering a sprite around a collision point), not a camera transform.**
It is a real, useful, newly-confirmed finding in its own right - but it is
the wrong field for this question, and it does not validate or invalidate
`screen = record - scroll` either way, because neither `+0x04/+0x06` nor
`+0x10/+0x12` measurably depends on the logged scroll value in this data.

### Verdict: the scroll transform cannot be confirmed or refuted from current logs

Tested `(map_x - scroll_channel) mod 512` and `(map_y - scroll_channel)
mod 256` against `+0x10/+0x12` across all six logged scroll channels
(`bg0h/v`, `bg1h/v`, `bg2h/v`) and 1,043 slot/frame samples with all four
fields present: **zero matches on any channel**, because - per the finding
above - `+0x10/+0x12` simply does not vary with scroll at all; it is
`map + constant`, not `map - scroll`. Since this was the only
"ground-truth-shaped" field available for body sprites in this session,
and the one genuine ground-truth channel that does exist (F0) was only
captured for shadows (which also showed no scroll dependence, but shadows
are drawn relative to their body, not necessarily independently
scroll-transformed the same way - not conclusive either direction), there
is **no sample in this data where a body sprite's true committed screen
position can be compared against its map coordinate while scroll is
changing**. The per-BG-layer parallax question (which BG's scroll
actually governs OBJs, if any does) is unresolved for the identical
reason - there is nothing to test it against.

**Honest conclusion: not answerable from existing logs, not a case where
one channel gives a weak-but-usable answer.** Every candidate "final
position" field this session yields is provably derived from the map
coordinate by a fixed, scroll-independent rule (wrap, or wrap-plus-8/+26),
not by scroll subtraction. It remains possible the game does apply a
scroll transform for OBJ placement through a mechanism this session's
instrumentation does not reach (e.g. a different writer function, or the
transform happening at a point after everything this session's tags
observe) - or it is possible OBJ placement in this engine does not depend
on `[wide-scene]`'s logged BG scroll at all (e.g. if a separate,
unlogged camera variable governs it, consistent with the open Q2 gap in
the previous update). Both are live possibilities; this data cannot
distinguish them.

**The single capture that would close this:** re-enable F0 (or an
equivalent OAM-commit read) logging for **body** sprites, not just the
shadow signature, for one replay through a scene with a large, continuous
scroll sweep (the same Bilibin-town window already used here, frames
~521716-522018, is known to have one). That is a filter change to
existing, already-written code (`golden_sun_oam_shadow_write_observer`'s
call site), not new instrumentation from scratch - but it is still a
rebuild and a replay, both out of scope for this analysis-only pass, so it
is named here rather than done.

### Sprite extent - answerable, confirmed from existing logs

ATTR0 bits 14-15 (shape) and ATTR1 bits 14-15 (size) together select a
fixed GBA hardware pixel size via the standard 3x4 OBJ size table -
already being decoded correctly in every prior pass of this document.
Combinations actually observed in the sessions used throughout this
document, with their real pixel dimensions:

| shape | size | pixel dimensions | seen as |
|---|---|---|---|
| 0 (square) | 1 | 16x16 | occasional body variant (e.g. `session_20260830_015252.log` slot 6) |
| 0 (square) | 2 | 32x32 | the dominant body/animated-object signature, most slots, both towns |
| 1 (wide) | 0 | 16x8 | the shadow-candidate signature (`tile=0`) |
| 2 (tall) | 3 | 32x64 | one outlier moving body (slot 13, Bilibin/Palace session) |

No other shape/size combination was observed in any session mined for
this document. This confirms extent is derivable directly from already-
logged ATTR0/ATTR1 bits with no additional capture needed - the blocker
for an off-screen rule is entirely the unresolved position/scroll
question above, not the size half of the problem.

### Deliverable status

**The "fully outside the expanded view" rule cannot be stated yet.** Per
the task's own instruction, this is reported as a stop rather than a
guess: the position transform it would depend on (`screen = record -
scroll`) is neither confirmed nor refuted by current logs, for the reasons
above. Once a body-level F0 (or equivalent) ground-truth capture closes
that gap, the rule's shape is already known and waiting: `sprite fully
off-screen` <=> `screen_x + half_width < 0 OR screen_x - half_width >=
360 OR screen_y + half_height < 0 OR screen_y - half_height >= 240`, using
the confirmed per-shape/size half-extents above once `screen_x`/`screen_y`
themselves are verified.

## Update 2026-08-30 (body-level ground truth captured - the contradiction is resolved)

Implemented the one capture proposed in the previous update: extended the
EC/F0 diagnostic filters in `src/runner_main.cpp`
(`golden_sun_obj_f0_entry_capture`, `golden_sun_oam_shadow_write_observer`)
to also admit a small, bounded set of body-sprite events - EC admits
record slots 0-5 directly (source is the record base with no `+0x0C`
offset for a body event); F0 admits OAM destination slots 0-5. Both
additions reuse the existing coarse per-(route, index) dedup (first
occurrence per epoch only), same discipline as the shadow-signature filter
already in place. Diagnostics-gated, no rendering/culling change, no
`generated/**` edits.

`gs011_opt` rebuilt (`--parallel 1`), `gsr_widescreen_policy_test.exe`
passed (exit 0). Replay: same as every prior pass in this document
(`logs/session_20260829_234316.input`, 1183 frames, `Golden Sun.state2`,
root launcher only, secondary monitor, muted, never stole focus, no
process of the user touched). Config restored and re-verified back to
`Widescreen=false`/`ViewMode=0` after. New log:
`logs/session_20260830_122905.log`.

**Fidelity: matched.** `frames_presented=1183`,
`[wide-field-table-summary]` `map_first_frame=521695 map_last_frame=522569`
- identical to the anchor used in every prior pass. Route counts this run:
1324 D4, 32 EC, 46 F0 - the small body carve-out added real volume without
reopening the flood (compare to the pre-fix run that dropped
thousands/epoch); the session's still-present multi-second stalls at
scene-transition frames were re-confirmed to be the pre-existing
`[wide-obj-record-writer]`/`[wide-obj-record-value]` cost documented in
the previous update, not this tag - not re-litigated here.

### The cross-check that actually closes it

Joined EC->F0 adjacent-sequence pairs (same method as the shadow analysis)
and classified each by whether the EC `source` is a bare record base
(body) or `record_base + 0x0C` (shadow). Got 3 genuine body pairs - thin,
but each one is an exact-value comparison, not a statistical average, and
all three point the same direction:

| frame | slot | EC-read record bytes (map coord, decoded) | F0-committed ground truth (raw ATTR) | scroll at that frame |
|---|---|---|---|---|
| 521790 | 0 | x=104, y=60 | x=104, y=60 | bg0=(120,863) bg1=(632,351) bg2=(120,351) |
| 522093 | 4 | x=104, y=1 | x=104, y=1 | bg0=(110,512) bg1=(622,0) bg2=(110,0) |
| 522095 | 4 | x=104, y=1 | x=104, y=1 | bg0=(200,584) bg1=(712,200) bg2=(200,200) |

Exact match, 3/3, zero error, including across the frame-522093 to
frame-522095 pair where the scroll changed substantially (bg0 hofs
110 to 200, a 90-unit change; bg1 vofs 0 to 200, a 200-unit change) while
the record's own coordinate and the true committed OAM position both
stayed at exactly (104, 1). This is the same field, at the same value,
both before and after applying whatever transform the game does - i.e.
no transform happens. The record's `+0x04`/`+0x06` bytes are not
subtracted against scroll before becoming the final OAM position; they
are the final OAM position already, mod the already-established 512/256
hardware wrap.

### The contradiction, resolved

Both earlier observations were factually correct; only one interpretation
was wrong.

- The original stationary-NPC test (three record slots holding an
  unchanged coordinate across a real ~352-unit scroll sweep) was correctly
  observed and is not overturned - the coordinate genuinely does not move
  when scroll moves.
- The interpretation drawn from it - "therefore this must be a
  map-relative coordinate that needs `screen = record - scroll` before
  use" - was wrong. The body ground-truth cross-check above shows the
  opposite mechanism produces the identical observation: the coordinate is
  already screen-space and simply is never adjusted for `[wide-scene]`'s
  logged BG scroll at all. A screen-space value that is not touched by
  scroll looks identical, frame to frame, to a map-space value that would
  need a transform this data shows never happens - the earlier test could
  not, by itself, tell those two explanations apart. This capture can,
  because it has independent ground truth (F0) instead of only the record
  reinterpreted.
- Per the coordinator's own suggested check: the frames used in both the
  original stationary-NPC test and this body cross-check genuinely have
  real, substantial, different scroll values (confirmed above and in the
  earlier update) - this is not a case of "the scroll simply did not
  change during that window." The scroll changed a lot in both tests; the
  coordinate did not move in either. The BG scroll `[wide-scene]` logs is
  real GBA hardware state, but it is not the value these sprites'
  positions are computed against - consistent with the open Q2 gap already
  on record (the true governing camera variable, if the game uses one for
  OBJ placement at all through this pathway, remains unidentified).

**Settled: the record's `+0x04` (Y) / `+0x06` (X) bytes are already
screen-space (mod 512 / mod 256 wrapped), not map-space, and no scroll
subtraction is needed or correct to apply to them.** Confidence: high -
this is now backed by independent, ground-truth OAM data (not a
reinterpretation of the same record bytes), exact match, zero
discrepancy, across a real, large, confirmed scroll change. Sample size
(3 body pairs) is thin in count but not in strength - each is a
zero-tolerance exact match, and it agrees completely with the earlier,
larger-sample (14/14) shadow-side result from the previous update, which
showed the identical "no scroll dependence" pattern by an independent
route.

The wrap question, per-BG-layer question, and camera-variable question
from the earlier updates are now moot for this rule specifically - since
no scroll subtraction is applied to `+0x04`/`+0x06` before use, which BG
layer's scroll would have governed it is no longer a live question for
this purpose (Q2's broader camera-variable question, for future
camera-mod work rather than culling, remains open and unresolved, as
already stated).

### The off-screen rule

Screen position: `screen_x = record[+0x06] mod 512` (resigned: subtract
512 if the wrapped value is >= 256, matching the existing proven 9-bit
ATTR1 X convention), `screen_y = record[+0x04] mod 256` (resigned:
subtract 256 if >= 128, matching the existing proven ATTR0 Y byte
convention) - used directly, with no scroll or other transform.

Sprite half-extents, from ATTR0 shape (bits 14-15) + ATTR1 size (bits
14-15), all four combinations actually observed in this document's
sessions:

| shape,size | pixel size | half_w | half_h |
|---|---|---|---|
| 0,1 | 16x16 | 8 | 8 |
| 0,2 | 32x32 | 16 | 16 |
| 1,0 | 16x8 | 8 | 4 |
| 2,3 | 32x64 | 16 | 32 |

Expanded view bounds: the native GBA view is x in [0,240), y in [0,160);
the expanded view adds a margin on every side to reach 360x240. The exact
margin is set at runtime by `install_golden_sun_widescreen`'s
`extra_left`/`extra_right`/`extra_top`/`extra_bottom` parameters (not
re-derived numerically in this pass), but the previously-documented
admitted bands in `WIDE-01_CULLING_HANDOFF.md` (X band 240..299, Y band
160..199 - each exactly 60px and 40px past the native edge) are
consistent with a symmetric margin: x in [-60, 300), y in [-40, 200).
Confidence: medium on the exact margin figures (inferred from a different
document's admitted-band evidence, not independently re-measured here);
high on everything else in this rule.

**Stated rule:**

A sprite is fully outside the expanded 360x240 view when
`(screen_x + half_w <= -60) OR (screen_x - half_w >= 300) OR
(screen_y + half_h <= -40) OR (screen_y - half_h >= 200)`, where
`screen_x`/`screen_y` are the record's own `+0x06`/`+0x04` bytes (wrapped
and resigned as above, no scroll subtraction), and `half_w`/`half_h` come
from the ATTR0 shape / ATTR1 size table above.

**Accuracy:** the position half of this rule (`screen_x`/`screen_y` needs
no transform) is backed by 3/3 exact-match body samples plus 14/14
exact-match shadow samples from the previous update - 17/17 total
independent ground-truth comparisons, zero mismatches, zero counter-
examples in either direction. The size half is backed by the four
shape/size combinations actually observed and decoded correctly in every
session mined across this whole document; no fifth combination was ever
seen. The margin constants are the one medium-confidence piece, carried
over from a different document rather than re-verified numerically here.

**No failing case was found.** The only caveat worth restating: this rule
was derived and tested entirely in field/town/room scenes; it is untested
in battle (explicitly out of scope, per the standing instruction not to
enter one) and not verified for UI/effects sprites (which, per the earlier
NPC-vs-Object-adjacent update, show no evidence of using this record
system at all, so the rule's scope is correctly limited to entities that
do).

## Update 2026-08-30 (implemented, behind an "Experimental Fixes" launcher toggle)

The off-screen rule above is now implemented, gated entirely behind a new
launcher checkbox, "Experimental Fixes" (`src/launcher_main.cpp`,
`kExperimentalFixesButton`), wired exactly like the existing "Widescreen
diagnostics (WIDE-01)" toggle: session-only, always starts unchecked, sent
to the child process explicitly as `GBARECOMP_EXPERIMENTAL_FIXES=0/1`
(never inherited silently), read via
`golden_sun_experimental_fixes_enabled()` in `src/runner_main.cpp` (same
pattern as `golden_sun_wide_diagnostics_enabled()`). Normal play (toggle
off) is byte-for-byte unchanged - the new code path is unreachable.

**Where it hooks:** `golden_sun_oam_shadow_write_observer` (the F0
Func_1dc8 route, `src/runner_main.cpp`), the same function this whole
document's evidence comes from. It fires on every committed OAM-shadow
write, body and shadow alike, and the ATTR0/ATTR1 bytes it reads are
already the ground-truth on-screen coordinate (proven above, no scroll
transform). When the toggle is on, it decodes shape/size, resigns X/Y,
evaluates `golden_sun_experimental_sprite_fully_offscreen` (new in
`src/widescreen_policy.h`), and if true, sets the GBA OAM disable bit
(ATTR0 bit 9, clearing the affine bit 8) and writes it back with
`bus_write_u16` before the buffer's later DMA to real OAM. It never reads
`0x03002000` - it operates purely on the already-committed OAM shadow
buffer at `0x0300347C+`, so the scene-load code-residency window
(`WIDE-01_ENTITY_PRODUCER_FINDINGS.md`) cannot affect it; no new gating was
needed there.

**Margin, and why it cannot wrap:** the positive (right/bottom) edges are
exactly the already-shipped admitted viewport edge
(`kNativeWidth+extra_right`, `kNativeHeight+extra_bottom`) - no new margin
added there, since that is where the proven X=256 resign ambiguity zone
begins; generosity on that side comes only from using the sprite's own
half-extent (not culled until its whole bounding box clears the edge, not
its center). The negative (left/top) edges get a real, generous +40px
buffer beyond the viewport, but are hard-clamped
(`kGoldenSunExperimentalMinSafeLeft=-220`,
`kGoldenSunExperimentalMinSafeTop=-80`) regardless of the runtime
extra_left/extra_top values, so even the largest observed sprite (32x64,
half_h=32) always keeps real clearance from the hardware wrap floor
(-256 X / -128 Y) - verified by a dedicated unit test sweep, not just
argued (see below).

**Body/shadow pairing:** both are committed through this exact same F0
route and evaluated by the identical predicate - there is no separate
"is this a shadow" branch, so they can only ever disagree if the margin
were tight enough for one to fall just inside and the other just outside.
Given the margin's generosity (tens of pixels beyond the viewport on the
safe side) and that a shadow is always drawn within a few pixels of its
own body (per the earlier body/shadow analysis in this document), this is
not expected to happen in practice; it is not a structural guarantee, and
is reported as the practical-not-provable statement it is, per this
document's own confidence-labeling standard.

**Player safety:** not by slot index (slot 0 remains unproven, as
documented above, and this implementation does not special-case it). The
player is protected because the camera keeps them on-screen at all times
by construction (proven earlier in this document), so their committed
coordinate is always far inside the generous margin - the predicate simply
never fires for them, the same way it never fires for any genuinely
visible-or-near-visible sprite. This is exactly the "generous margin makes
it moot" case the task anticipated, stated explicitly rather than assumed.

**Build/test:** `gs011_opt` rebuilt (`--parallel 1`) clean, including
`GoldenSunLauncher.exe`. `gsr_widescreen_policy_test.exe` passed (exit 0),
extended with new cases in `tests/cpp/widescreen_policy_test.cpp`: the
X=256 resign exception, plain resign wrap on both axes, the half-extent
table for all four observed shape/size combinations, center/edge/generous-
margin behavior, explicit wrap-floor cases for the worst-case (32x64)
sprite on both axes, and a swept-boundary check (bounds derived from the
same formula the implementation uses, not independently guessed) proving
every observed half-extent's cull threshold keeps real clearance from the
wrap floor on both axes. The game itself was not run for this task, per
instruction - the user tests it.

## 2026-08-30 correction: exact placement replaces raw-coordinate guessing

The earlier experimental method above is superseded. Raw wrapped coordinates
and center/half-size bounds could cut sprite bottoms, hide shadows early, and
alternate a slot between visible and hidden. The culler now requires an exact
same-frame placement, uses top-left full-sprite bounds, and gives a body and
its shadow the same result. Ambiguous, stale, affine, or unpaired entries stay
visible. Focused widescreen CTest passes `3/3`; the non-LTO playable build was
rebuilt successfully. Gameplay acceptance remains pending.
