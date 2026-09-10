# Roadmap

## Resume here -- inspect the in-game options and dialogue renderers, 2026-09-10

The boulder placement investigation is shelved at the user's request. The
latest capture (`session_20260910_141441` / `objrec_20260910_141449`) verified
the new `0x08094980` post-call trace: all `3,272` Region B result rows stored
zero at source `+0x10`, while exact joins still showed changing final OAM
positions. No placement fix is justified. When resumed, capture the earlier
source that builds F0's packed attributes/coordinates; the camera-versus-object
question remains open.

Current work is limited to the game-owned options screen and dialogue/text
rendering. Establish their existing drawing paths and any measured text-speed
control before considering changes. Keep the boulder trace and its evidence
intact.

## Shelved -- capture the cutscene source before filtering, 2026-09-09

Garet and the chest are confirmed working; the four pushers and boulder remain
unresolved. Source review found that the previous `context=none` census cannot
prove EC was never entered: its contexts are populated only after an IWRAM-only
attribute read and actor-identity admission (or recorder-only rejection capture).
An unreadable/non-IWRAM source leaves no context even when EC ran. No context is
also distinct from no B324/B328 coordinate capture; those are separate hooks.

`Record sprite placement` now captures EC's actual R6 source and R0 destination
before those filters, then consumes the observation at the corresponding F0
write. New `entry_*` columns in `lifetime.csv` distinguish missing entry,
invocation mismatch, unreadable RAM, changed attributes, and a matched source.
Only physical EWRAM/IWRAM attributes are read; no asset payload is recorded.
Existing captured coordinates for that source (or its known paired body) are
included when present. Rendering and all placement checks are unchanged.

That capture path has since been run and extended with the bounded producer
trace recorded above; no further boulder run is planned while this line is
shelved.

## Previous -- second sprite source is unfound, 2026-09-09

Garet and the chest render fully in the expanded view; the boulder cutscene
does not, and the reason is now measured rather than suspected.

`session_20260909_213843` ran the `[wide-obj-nearmiss]` census. Of `5,073`
sprites left without a position, `3,432` had no captured record for their call
at all -- the staging seam never runs for them -- and the largest single group,
`2,264`, is one call site (`0x030038f0` returning to `0x030038b4`) and one
sprite class (tall, semi-transparent, `ATTR2` `0xd524`) that never appears among
the successes. Everything that works is square 256-colour sprites out of the
`0x03002000` actor array.

So the remaining work is not tuning the placement path. It is finding the second
structure these sprites are staged in and the instructions that write it, the
way the `0x03002000` array and `B324`/`B328` were found. Do not loosen the
identity, frame or provenance checks further to chase it -- they already reject
nothing that reaches them (`f0_placed == f0_identified` in every run).

Full session write-up for the user: [WIDE_SPRITE_CLIPPING.md](WIDE_SPRITE_CLIPPING.md).


## Previous -- context-key mismatch on one call site, 2026-09-09

`session_20260909_211709` / `logs/objrec_20260909_211716`: the one-frame window
worked. Coverage `19.82%` -> `33.42%`, `context-frame` failures `3,479` -> `219`.
The user still sees the boulder and the four pushers cropped.

The failure moved: `3,988` commits now fail with `context-no-match`, meaning the
frame's context table exists but holds no entry for those sprites. They are one
call site (writer `0x030038f0` returning to `0x030038b4`: `2,707` failures,
`80` successes) and one sprite class (256-colour tall sprites, `ATTR2` `0xd524`)
that never appears among the successes. Since EC entry and the F0 store sit in
one invocation of the same routine, depth and return_pc cannot disagree for a
sprite -- only the attributes can. Either the EC seam does not run for that
call, or it runs against a different structure and reads the wrong attributes.

`[wide-obj-nearmiss]` was added to separate those two. On a failed lookup it
searches by `(frame, depth, return_pc)` alone and prints, deduped and bounded,
whether any context existed for that call and what pointer it held.

Needs a rebuild and a rerun of the boulder cutscene. The `[wide-obj-nearmiss]`
lines at the end of the log are the whole result; `context=none` on
`return_pc=0x030038b4` means the EC seam never runs for that call, while
`context=yes` with a staging pointer names the structure those sprites come
from.


## Previous -- per-frame context stamp widened to one frame, 2026-09-09

Garet and the chest render fully. The boulder cutscene does not: the four
pushers and the boulder are still cropped. The capture
`logs/objrec_20260909_203451` says why, and it is not the identity predicate --
that now rejects nothing.

All `3,479` failures out of `4,339` committed sprites have reason
`context-frame` and `checks=0`, with zero `context-no-match`. In `647` of the
`788` frames both successes and failures occur, and in every one of those `647`
frames all failures come before all successes. The context table is wiped at
the frame's first staging entry, so everything committed earlier in the frame
is compared against the previous frame's stamp and refused while the matching
record is still sitting in the table.

`golden_sun_obj_record_frame_current` now accepts a record up to one frame old
in the context lookup and the body provenance check. One frame is the measured
bound -- the table is cleared once per frame -- and the exact `ATTR0/1/2` match
and OAM-byte truncation check still decide trust. `[wide-obj-hooks]` gained
`f0_placed_prev_frame`.

Needs a rebuild and a rerun of the boulder cutscene. Expect `f0_placed` to rise
well past `860/4,339` with most of the gain in `f0_placed_prev_frame`. If
`f0_placed` rises but sprites land in the wrong place, the one-frame window is
too generous and the next question is the staging record's own lifetime.


## Previous -- actor-record allowlist removed, 2026-09-09

The rerun after the production-gate fix, `session_20260909_190746`, confirms
both seams are live: `[wide-obj-hooks]` reports `fast_iwram=armed
oam_commit=armed`, `169,186` observer calls, and all `2,702` F0 commits
reaching the placement resolver. Only `882` of them (`32.6%`) were traced to an
actor record. The remaining `1,820` were refused by the identity predicate's
hardcoded address range before any provenance check ran -- that is the ceiling
that made "authenticate one more pointer" unable to generalise past Garet.

`golden_sun_obj_record_identity` now checks the shape of the actor array
(base `0x03002000`, stride `0x38`, body `+0x00`, shadow `+0x0C`, record inside
IWRAM) instead of an audited address range plus a per-actor exception. The
authentication that actually decides trust is unchanged and still downstream:
staged `ATTR0/1/2` must equal the committed OAM attributes, staged coordinates
must truncate to the committed OAM bytes, and the staging entry must carry the
same frame and auth epoch. `[wide-obj-hooks]` gained `f0_considered` and
`f0_placed` so one Enhanced run reports how many committed sprites ended with a
trusted full-precision position.

Needs a rebuild and a rerun of the prologue chest scene. Expect `f0_identified`
to approach `f0_placements` and `f0_placed` to rise from near zero; if
`f0_identified` climbs but `f0_placed` does not, the next question is the
provenance gates, not identity.


## Previous -- production F0 seam armed for Enhanced, 2026-09-09

The first rerun after the measured `0x03002348` identity fix was
`session_20260909_174751`; the user still saw Garet clipped. Its log confirms
the same savestate at frame `8531`, Enhanced timing and the expanded
room-buffer view, but no F0 writer, OAM-shadow, object-recorder or cull rows.
The child was the rebuilt `build/gs011_opt/GoldenSunRecomp.exe` (written
`17:00:53`, before the `17:47:51` run), and the root launcher was
`GoldenSunLauncher.exe` (written `13:32:47`).

The missing rows alone do not identify the cause. Source and executable
inspection found the production gate: generated fast-IWRAM writes were armed
only for VRAM diagnostics, and the reusable committed OAM callback was also
suppressed when diagnostic print tracing was off. The measured F0 writer at
`0x030038F0` therefore bypassed the placement resolver in a normal Enhanced
run, so the `0x03002348` identity correction could not affect rendering. The
source now arms the fast seam for Enhanced/recording, gates committed
callbacks with the existing primary/alternate OAM-table predicate, and invokes
the payload-free callback independently of print tracing. `run_game()` now
clears both vram-trace observer slots and the range gate before each run. It
also emits one bounded `[wide-obj-hooks]` exit line in Enhanced runs with the
observer, F0, placement and identity counts. No gameplay success is claimed
until the user rebuilds and reruns the marked scene.

## Previous -- Garet body provenance source measured, 2026-09-09

The targeted capture `session_20260909_154121` closes the source gap behind
the marked `garet_partial.png` window. Across frames `8531..9209`, every
`record-identity` commit row carries the same authenticated EC/F0 source
`0x03002348` (body `N=15` at the measured `0x38` stride), with the source
attributes matching the committed OAM attributes in the same frame. At the
window's end, frame `9082` slot `10` has the candidate body's attributes but
stale provenance; its matching source commit is frame `9080`, slot `10`, and
the intervening DMA is frame `9081`.

The identity predicate preserves the original half-open range
`[0x03002000,0x030022E0)` and admits only the additionally authenticated body
pointer `0x03002348` (`N=15`). The `N=13` shadow `0x030022E4`, `N=14` body and
shadow `0x03002310`/`0x0300231C`, and `N=15` shadow `0x03002354` remain rejected.
Existing frame, epoch, complete-attribute and hardware-truncation checks still
gate the source handoff. This is a source fix only; no build or gameplay
verification was run here. The user must manually rebuild and rerun the marked
scene to verify that the full body appears in the expanded top margin.

## Resume here -- normal shadow path verified; D8 bypass remains, 2026-09-08

Latest user test: `session_20260908_210056`; matching capture:
`logs/objrec_20260908_210104`. The user reports that the shadow problem looks
fixed. The capture verifies the normal D4 path: all `3,815` shadow entry tokens
have a matching `store-token-found` event, with only `6` `handoff-body-stale`
events and no paired-body-check rejection. It also retains a bounded D8 gap:
the same shadow-shaped attributes as the prior representative occur at slot 8
in frame 373104 with `store-token-missing`; the DMA publishes them with
provenance from frame 373086 and the renderer records `stale-frame`. Attributes
do not prove actor identity, so this is evidence of a remaining writer path,
not a global sprite count. The paired-body correction is therefore confirmed
for normal D4 entry, while a true D8 resume/bypass remains unclassified. See
"Latest writer capture" below.

The measured failure is stale or missing shadow provenance at rendering:
without a trusted full-precision position, the renderer confines the shadow
to the native rectangle. Detailed counts and event identities are in FACTS.md,
"Sprites, NPCs and shadows".

What is ruled out or cannot be inferred:

- The alternate table does not explain the representative failing primary
  slot or the dominant recorded failures. Alternate-table gaps still exist.
- Upload does not always discard correct provenance: DMA 176 preserves the
  later matching commit. This does not establish that every upload is sound.
- That later commit has different attributes from the failing shadow.
  Reusing slot 14 does not identify the same actor or prove its placement
  was available before the failed render.
- Checks 31 includes commit resolution; it is not evidence of an unresolved
  commit. Aggregate placement coverage is not renderer acceptance coverage.
- RAM PCs 0x030001A4/0x030001A8 identify a clear-fill routine, not a proven
  shadow producer. Do not hook them or fixed OAM slots as a shadow fix.

Next: if another visible shadow failure is found, isolate the D8 bypass before
considering an instruction-level resume seam. Keep the writer recorder enabled
and do not relax identity/frame checks or undo the working body/no-wrap
corrections.

## Planned next architectural task -- BIOS removal, 2026-09-08

The shadow status above is unchanged. BIOS removal is planned, not implemented;
the source-grounded work sequence is in
[BIOS_REMOVAL_PLAN.md](BIOS_REMOVAL_PLAN.md).

## Current recording setup

`lifetime.csv` orders commits, full OAM upload snapshots and renderer
observations. It includes context rejection reasons, body staging frame and
epoch, attributes and provenance. Renderer failures outside raw Y159..199
are now recorded too. The DMA counter is diagnostic only. Buffered output
continues through the session rather than expiring at an initial sample cap.
The follow-up `shadow_writes.csv` tracks every affected primary or alternate
table slot and contains no guest payload bytes.

The successful capture used these launcher toggles: Self-heal RAM, Record
sprite placement, VRAM map write trace, and Draw field from room buffer.
Other test variables were off. VRAM trace is required to observe generated
fast IWRAM stores; sprite recording alone arms only the normal store path.
The `session_20260908_173410` and `session_20260908_210056` writer captures
used these settings after the manual rebuild. Launch root
`GoldenSunLauncher.exe`.

## Latest writer capture -- D8 paired-body handoff corrected, 2026-09-08

The supplied `session_20260908_173410` matches
`logs/objrec_20260908_173418`. It has `2,303` primary commits (`1,925`
full precision), including `1,511` shadows (`1,505` full precision). The
writer trace contains `1,332` shadow-shaped D4 stores at
`0x030038D8` with no same-frame lifetime commit; every one is present in a
same-identity DMA in the same or one of the next two frames. By comparison,
the `0x030038F0` F0 writer has `1,418` shadow-shaped stores and `1,190`
same-frame commits. This keeps the table/upload path ruled out for the D4
loss.

The representative new row is slot `5`, frame `373104`, with
`ATTR0/1/2=0x6104/0x07C0/0x0800` at `0x030038D8`. The identity is published
by DMA, but a later renderer row is `no-provenance`; the authenticated D4
handoff log covers slots `0` through `4` and `11`, not slot `5`. Source audit
now explains that gap: D4 captures the shadow source before the lookup, then
the old lookup searched for body+0x0C in a table keyed by the body base. The
source now maps that source through `golden_sun_obj_record_identity` before
the handoff, reusing the paired-body resolver. No gameplay result is claimed
until the user reruns the matching capture.

The generated resume path consumes `g_runtime_resume_pc` and jumps to the
D8 label before the function-entry hook. That remains a separate, unmeasured
possibility; the session has no alias/resume event. The current correction
addresses the proven normal-entry shadow lookup mismatch by reusing the body
record's staging provenance. The user must rerun before any resume seam is
considered. No gameplay success is claimed.

The prior `session_20260908_152234` matches
`logs/objrec_20260908_152249`. Its writer trace follows the failing identity
through a real table write and the next full upload, so the table itself is
not losing the shadow. The current representative is slot 14: a shadow
identity written at frame 373209 by `0x030038D8` is uploaded next and rendered
at frame 373211 with stale frame provenance.

`0x030038D8` is the second instruction of the verified D4 writer, immediately
after entry `0x030038D4`; the accepted `0x030038F0` PC is a separate F0 writer.
The source now admits the D4 STM only when its preceding entry is authenticated.
The D4 entry hook captures the actual R6 staging pointer immediately before the
no-writeback LDM and keeps it for a same-frame store. For shadows, that pointer
is body+0x0C while the coordinate table is keyed by the paired body base; the
source now maps that record before publishing provenance. OAM attributes are
not used to guess source identity. The existing frame, epoch, table-target,
attribute and truncation gates remain in force. The manual rerun must establish
whether any remaining D8 store bypasses the normal entry path before a resume
seam is considered.

## Latest writer capture -- normal D4 path verified, D8 bypass remains, 2026-09-08

The supplied `session_20260908_210056` matches
`logs/objrec_20260908_210104`. It has `5,096` primary commits (`4,453`
full precision), including `3,568` shadows (`3,553` full precision). The D4
trace has `3,815` shadow `entry-token-found` rows and the same `3,815` shadow
`store-token-found` rows. There are `6` `handoff-body-stale` rows and no
`handoff-body-checks` rows. The `2,414` `store-token-missing` rows have no
source address and cannot be assigned to an actor from this capture.

In the overlapping frames `373079..373493`, deduplicated shadow-signature
renderer observations (`ATTR2=0x0800`, active `ATTR0`) changed from `133`
`no-provenance`, `149` `accepted`, and `1,440` `stale-frame` in the previous
capture to `8`, `449`, and `417`. These are renderer observations, not sprite
counts. The prior representative's `ATTR0/1/2=0x6104/0x07C0/0x0800` appears
at slot `8`, frame `373104`, in the new capture: its D8 event is
`store-token-missing`, its DMA row is published with provenance frame `373086`,
and its render row is `stale-frame`. This confirms the normal D4 paired-body
correction and leaves a specific D8 bypass/resume path unclassified.

## Manual builds -- user only

Never start a build unless the user explicitly changes that instruction.
When a source change needs one, tell the user. Root `build_lto.bat` calls
`scripts/build_lto.ps1` for the existing `build/gs011_opt` LTO build, setting
MAKE with forward slashes. Default ceilings: 90% CPU and job-wide committed
memory equal to 90% of installed RAM, not usage targets. Optional overrides:
`build_lto.bat -CpuPercent 80 -RamPercent 75`. Resource-limit enforcement has
not been independently measured. Do not present the older September 6
executable timestamp as the latest build: September 8 captures exercised
newer recorder code, without a newly audited executable timestamp here.

## The goal

Run Golden Sun on native C++, with the ROM used only for assets, and be able to
change how it works — PC-native widescreen, turbo, walk speeds, potentially
replacement item and Psynergy tables.

The test for "done", in the user's words: widescreen becomes **"render more of
the world at once"** by changing a camera or render-resolution value, instead of
a per-pixel reconstruction problem.

Readable generated code is explicitly not a goal. A faithful but ugly native
implementation counts as success.

## Where the project actually is

**The code half of the goal is already substantially met.** The recompiler
translated 26,935 functions plus 95 overlay banks into C++ ahead of time, and
that generated C++ is what executes. The interpreter is only a fallback for
dispatch misses — one distinct miss against 186,079 native calls in a
2026-09-03 snapshot. The ROM is already just a data source.

What is still emulation is the **hardware** layer: PPU, DMA, timers, IRQ,
sound. That is hand-written native C++ modelling a GBA, never derived from the
ROM.

So the remaining distance is in the hardware layer, and in the ability to
modify behaviour — not in translating game code.

## Milestone 1 — understand the map data (current)

**Work out exactly how Golden Sun's map data is laid out, so a room can be
reconstructed ahead of time instead of resolved per pixel while rendering.**

This is the foundation for everything else. Nothing else starts until it is
understood.

Known starting points, from the widescreen work:

- Metatile table at `0x02010000`
- Tile-entry table at `0x02020000`
- Room bounds struct — the pointer at `0x03001E70` is always `0x02030CCC` and
  is useless as an identity, but the four bounds values beside it do change per
  room (23 distinct combinations measured in one session)
- Camera clamp function `Func_10230`

Scope broadened 2026-09-04 to all five scene classes: towns, dungeons, indoors,
world map, battle. The field table layout was recovered rather than re-derived —
see `FACTS.md`, "Map and room data" and "Scene classes".

What needs answering:

1. ~~**Character-data residency.**~~ **Answered** 2026-09-05: it does not
   stream. Within a room the tileset is fixed apart from one 46-tile animation
   band, so the buffer snapshots character data once per room load. This was
   the stated main risk to milestone 2 and it is retired. See `FACTS.md`.
2. ~~**Is the overworld 512x512 the whole world or a streaming ring?**~~
   **Answered.** A streaming ring, and its source is the same pair of EWRAM
   tables the field uses — measured 2026-09-05, see `FACTS.md`, "The overworld
   is built from the same two EWRAM tables as the field." The world map needs
   a source buffer, and one implementation covers both scene classes — but it
   must select the table by scene class, because the two 64 KB regions swap
   roles between Mode 0 and Mode 2 (measured 2026-09-05, table in `FACTS.md`).
3. ~~**The room-bounds field offsets**, lost with the deleted recorder.~~
   **Answered** 2026-09-05: four `u16` at `0x02030DC0`, cross-checked on three
   sessions. See `FACTS.md`, "The bounds field offsets are recovered."
4. ~~**The bounds-rect to grid coordinate mapping**~~ **Answered** 2026-09-05,
   both halves: the camera shares the bounds rect's pixel space (camera at
   `0x02030DB0`), and the room is anchored at the grid origin at 16 px per
   cell, 49 of 49 snapshots. What the culling attempts assumed here was
   correct, so their failure is still unexplained.
5. ~~**Per-layer sourcing**~~ **Answered 2026-09-05.** One grid, one atlas,
   six writer functions, all three layers written into one contiguous 6 KB
   destination from a single base. Each layer reads a different *region* of
   the same grid; the region offset is what its own scroll register holds
   beyond the hardware's 9 bits. Confirmed two ways -- from the writers'
   arguments and from the registers, which agree -- and in play, where the
   field now draws from the reconstruction. See `FACTS.md`.
6. ~~**Are towns, dungeons and indoors one mechanism?**~~ **Answered
   2026-09-05: yes.** Human-labelled dungeon captures against town and indoor
   ones agree on the register signature, the bounds struct, the sourcing rule
   and the room-load write footprint. They collapse into one milestone-2
   implementation. See `FACTS.md`.
7. **Table write churn**, and **how often scroll changes mid-frame**.

These are measurements, not opinions. See rule 1 in `AGENTS.md`.

### Tooling — the scene recorder

Built 2026-09-04. Launcher toggle "Record map + scene data", off by default,
independent of the function tracer and usable alongside it. Per frame it logs
changed-page CRCs across EWRAM/IWRAM/VRAM/palette/OAM plus full display IO; on
mode/layer change, every 300 frames, and on a manual labelled button it writes a
complete memory image (`GSRSNAP1`) plus a screenshot. Raw unmodified frame
numbers throughout, so captures correlate with tracer windows.

Decoder `tools/decode_snap.py` (stdlib only): decodes IO by name, renders each
enabled BG layer and the Mode 0 metatile grid, hexdumps the room-bounds region,
and `--compare` prints one grouped table per register signature.

Complete memory images are deliberate: the overworld source is unlocated, the
bounds offsets are lost, and indoor sourcing is unconfirmed. Selective dumping
would guess all three, and each wrong guess costs a rebuild.

## Milestone 2 — the room buffer

Build the room's tilemap once into a plain buffer and render from it, instead of
calling back into game-specific host code per pixel.

### Decided with the user, 2026-09-05

- **Step 1 changes nothing on screen.** Build the tilemap from the EWRAM tables
  and compare it against what the game actually put in VRAM. An exact match is
  the proof; only then does anything get rendered from the buffer. The previous
  widescreen attempts never checked their per-pixel answers against anything,
  and that is a large part of why three of them failed the same way.
- **Field first** — towns, dungeons, indoors. The world map reuses the same
  code afterwards; it streams as the camera moves, so its ring holds rows built
  from earlier table contents and needs a different comparison.
- **Room edges, when the view is eventually widened:** keep the camera inside
  the room and let the character sit off-centre; fill with black only when the
  room is too small to fill the widened view even then.

**Step 1 is done, 2026-09-05.** `src/room_buffer.{h,cpp}`, launcher toggle
"Room buffer self-check", off by default. Per guest frame it rebuilds the
visible cells of each field layer from the grid and atlas — using the measured
room rect, camera, and the per-layer grid regions taken from the writers'
arguments — and compares byte-for-byte against live VRAM.

Result: **98.77% / 98.40% / 99.18%** across 15.1 million cell comparisons,
**100% whenever the camera is still**. The shortfall is the game's streaming
lag, not reconstruction error (see `FACTS.md`). The field map format is
confirmed; a room can be reconstructed ahead of time.

**Step 2 draws from the buffer, 2026-09-05.** PPU hook
`g_ws_field_tilemap_source` (nullable, off by default) asks the game for a
regular-BG map entry by logical screen coordinate, for every sample rather
than only beyond the native area; `gsr_field_tilemap_source` answers from the
room. Launcher toggle "Draw field from room buffer".

At the native 240x160 the picture is correct in towns, houses, dungeons,
battles and across the world-map transition. Four defects were found by
playing and all four are fixed: per-layer offsets rounded to a multiple of the
ring (broke every indoor room), the callback answering outside Mode 0 (broke
battles), a camera cached a frame behind the renderer, and a Mode 0 frame
carrying the world map's BGCNT during the transition. A residual "shimmer" on
fine detail while walking was **confirmed native** — it happens with every
toggle off.

Each layer's grid region now comes from its own scroll register, live, rather
than from the writers' arguments: measured equivalent (91.5/94.3/95.3% against
90.7/93.9/95.3%) and immune to drift from the registers the renderer is using.

### Expanded View 360x240, 2026-09-05

The native 240x160 grown 50% in both axes. Replaces the old 288x160 widescreen
and its aspect cycle, which predate the room buffer. Launcher and F1 both call
it "Expanded View"; needs "Draw field from room buffer" on, since that is what
sources the extra area.

Confirmed working: the margin shows genuinely reconstructed map, and a room
smaller than the view sits on black rather than repeated garbage.

Three things had to be fixed in the PPU, and none of them were about the
buffer -- worth remembering, because the buffer only substitutes background
tile identities. Everything else in the picture is still the emulator's:

- **Parked sprites.** Games stow unused OAM entries off-screen; widening put
  them on screen. Objects lying entirely outside the native 240x160 are now
  skipped when a game supplies margin content.
- **Window regions.** Margin columns lie outside every WIN0/WIN1 rectangle, so
  querying the window registers there let WINOUT enable layers the game hides
  on screen -- text and menu tiles appearing beside the world. Margin columns
  now take the nearest native column's answer.
- **No invented margin content.** Where the room has no answer, the sample is
  left blank instead of drawing the wrapped ring.

### State as of 2026-09-05, end of session

Three overlapping builds collided during this session (see the memory note on
never running two at once). The tree recovered: the final link completed and
`build/gs011_opt/GoldenSunRecomp.exe` at 23:26 is valid and current, carrying
the window-clamp revert and the 64 px object slack. It has **not been played
yet** -- everything below is compiled state, not tested state.

Source state (all compiled, most untested together):

- `src/room_buffer.{h,cpp}` — the resolver, the self-check, refusal counters,
  a room-rect report. Rendering no longer depends on the self-check toggle
  being on (it silently did, which wasted several test rounds).
- `gbarecomp/src/gba/gba_ppu.cpp` — `g_ws_field_tilemap_source` hook consulted
  for every regular-BG sample; margin samples blanked where the game has no
  answer; object culling judged against the expanded view with 64 px slack;
  the top/bottom band renderer given the real view geometry.
- `src/runner_main.cpp` — installs a policy that pillarboxes nothing while the
  buffer is drawing. Must be a real callback returning 0, **not** nullptr: the
  PPU reads a null policy as "use the legacy pillarbox globals", which blanks
  everything.

Verified working at some point, and worth re-confirming first:

- Expanded View 360x240 with genuinely reconstructed map in the margin, and
  black where a room actually ends (town, ~18:28 build).
- NPCs and statues standing correctly in the margin rather than vanishing or
  wrapping (23:10 build) — though pop-in was still late, which the 64 px slack
  addresses.

Known open, in the order they should be taken:

1. **Re-confirm the margin fills** after the window-clamp revert. That clamp
   (margin columns taking the screen-edge column's window control) blanked the
   background across the whole margin and was mistaken for a Goma Cave-specific
   fault for several rounds. It is reverted; the stray text/menu tiles it was
   meant to suppress are still open and need a different approach.
2. **Goma Cave Entrance** — status genuinely unknown. Its failures were
   conflated with the clamp regression and with a build that contained none of
   the fixes. Re-test before theorising.
3. **Sprite pop-in** with 64 px slack — untested.
4. **Performance** — the room-buffer source is consulted per sample, while
   OBJ placement providers run once per OAM slot per rendered scanline. The
   remaining cost is not attributed yet; use `GBARECOMP_COST_PROBE=1`.
5. **Remove the temporary diagnostics** once the above settle:
   `g_ws_expanded_diag` in the PPU, and the refusal/rect reports in
   `room_buffer.cpp`. The room-rect report also has a display bug (it prints
   one line per frame instead of deduplicating).
6. **Name-entry screen** — user-reported graphical corruption with Expanded
   View enabled (2026-09-09; screenshot). Troubleshooting is deferred.

Method note, because it kept paying off: every real cause this session was
found by a counter, not by reasoning about the code. Three separate wrong
diagnoses were stated confidently before the numbers arrived. Instrument
first.

Still open:
- **Cost.** The room-buffer path repeats camera, room-rect and layer-scroll
  reads for each supplied sample. The expanded OBJ X and Y providers also
  resolve the same slot provenance independently. Neither impact is measured;
  use `GBARECOMP_COST_PROBE=1` for a controlled baseline before caching either
  result.
- **The margins themselves.** Drawing beyond 240x160 is the point of the
  milestone and has not been attempted.
- The check's residual few percent is presumed streaming lag; the
  edge-vs-interior counter that would confirm it has not yet been read.

Why: the old widescreen implementation called
`golden_sun_wide_tilemap_provider` per pixel from the PPU's inner texel loop.
Each margin pixel meant a host callback that read emulated memory, branched on
mutable globals and consulted a stateful bitmap. That caused both problems at
once — the cost (a branching callback per pixel, not the pixel count) and the
garbage (answers invented per pixel, then culled). Three culls failed that way
on 2026-09-03.

With a prebuilt buffer, a margin pixel is an array lookup like any other, and
nothing is invented so there is nothing to cull.

**This does not require a GPU renderer.** Earlier scoping framed it as an
OpenGL project against mgba's `gl.c`. That is more than is needed.

The widescreen implementation was removed entirely on 2026-09-04 to give this a
clean slate; the game renders 240x160 as a GBA does.

### State as of 2026-09-06

The day's arc, in order: the right-hand edge was fixed first; the vertical
jumping turned out to be a hard 8-bit ceiling in OAM rather than a bug, and was
solved by taking positions from the guest instead of reading them back. The
full account, including what was tried and rejected, is under "Sprite placement
-- how the jumping and the early culling were fixed" below. Everything in this
section is the earlier margin/compositor work that came first.


The margins fill. Three faults were found and two fixed, all in the wide
compositor in `gbarecomp/src/gba/gba_ppu.cpp`.

Fixed:

- **The margin was black even though the buffer was answering.** The room
  buffer's entry was overwritten one step later by the surviving
  `golden_sun_wide_tilemap_provider`, which returns Unavailable in an ordinary
  field scene, and the `continue` that follows dropped the pixel. The session
  counters said "91.5M entries supplied" while nothing appeared -- the supply
  counter is incremented before the drop. The provider is now consulted only
  when the field source did not answer.
- **NPCs cut off along the native window's edges, and NPCs wrapping to the
  bottom.** Two separate defects in the object path:
  - `sprite_trusted` was captured before both of its inputs were decided
    (`resolve_sx()` had not run; the expanded-placement block had not yet
    re-vouched for either axis), so any sprite without Y provenance was
    confined to the native columns and sliced there. Now evaluated at emit
    time.
  - The expanded block re-derived Y from the raw OAM byte, discarding the
    guest's full-precision value. Eight bits cannot separate row 155 from row
    -101 once the view is 240 lines tall, and the game's own provenance log
    records exactly that pair (session_20260906_001314, slot 23 raw_y=155
    logical_y=-101; slot 22 raw_y=154 logical_y=-102). The vouched position is
    now preferred; the byte is the fallback.

Second round, same day -- the right/bottom cut-off and the wrap are one fault.
The OAM position is read back with rules that only describe a 240x160 screen:
X "negative above 255" and Y "negative at or above 160". The view runs to x=300
and y=200, so sprites were lost from x=256 (16px into a 60px right margin, and
redrawn in the LEFT margin) and the entire bottom margin band, rows 160..199,
was read as negative and moved to the top. Fixed by splitting each axis at its
own far edge. Also: the expanded scanline loop never passed its vertical
geometry to the renderer, so every "is this still in view" test answered for a
160-row screen; `extra_top_`/`oh` are now passed. And the guest's own
right-hand object bands were carried ~10% of the view further out
(`kExpandedObjSlackX`), because the guest culls by an actor's reference point
while the sprite it owes extends past it. `gsr_widescreen_policy_test` updated
and passing.

**The residual bottom wrap cannot be fixed this way, and this is now proved.**
OAM Y is eight bits; the expanded view spans 304 rows (-104..199). 256 values
cannot name them, so every row below -56 shares a byte with a row in the bottom
margin. Measured: slot 23 sits at logical y=-101 and writes byte 155, and byte
155 also means row 155. No re-reading of the byte separates the two. The only
thing that can is the guest's own full-precision Y, and that capture exists but
goes stale -- `session_20260906_003634` shows
`reason=canonical-provenance-y-mismatch` with provenance from frame 211339
being consulted at frame 211385, for an OAM slot the game has since reused.
Fixing the wrap means widening that provenance's coverage: the object-side
equivalent of the room buffer, which is the direction the user proposed.

Unrelated and pre-existing: `ppu_smoke_tests` fails
"wide BG suppress did not expose backdrop". Confirmed pre-existing by reverting
every 2026-09-06 PPU edit and re-running -- it still fails. It comes from the
uncommitted PPU work already in the tree at the start of the session.

Open, observed 2026-09-06 and **not** diagnosed:

- **Sol Sanctum: stray tiles in the margin**, well outside the room, on an
  otherwise black background. Not seen in towns. Possibly specific to that map;
  possibly the same class as the stray text/menu tiles noted above. Do not
  attempt a fix without evidence -- read the refusal counters for that room
  first.
- Sprites still have no answer when the guest vouches for nothing: the raw OAM
  position is ambiguous over a 240-line view in both axes (X is nine bits and
  the expanded view needs -124..364). Those fall back to the hardware reading
  and can still land on the wrong side.

Decided with the user: the view stays 360x240 for now; 1080x720 is a "down the
line" question once the rest is in place.

### Sprite placement -- how the jumping and the early culling were fixed

Worked through on 2026-09-06 with the user testing each build. The measured
evidence behind every number here is in `FACTS.md`, "Sprites, NPCs and
shadows".

**The problem.** OAM names a sprite's row in eight bits. A 360x240 view spans
304 rows. 256 values cannot describe 304 rows, so a row above the view and a
row in the bottom margin share a byte -- row -101 and row 155 are the same
byte, measured. Reading a sprite's position back out of OAM therefore cannot
work at this view size, and gets worse at any larger one. The user's stated
goal was NPCs and objects rendering correctly with their shadows "without
jumping at any future resolutions as well", which rules out every fix that
still reads the byte.

**The fix, in the order the pieces were found.**

1. **Stop reading OAM; use the guest's own coordinate.** Golden Sun computes
   each sprite's position at full precision and only truncates it on the way
   into OAM. That capture already existed
   (`record_golden_sun_obj_staging`). OAM is demoted to an identity tag.
2. **Place rotation/scaling sprites too.** The lookup refused them, because
   the off-screen culler that owned it cannot compute their bounds. But
   bounds and position are different questions: a scaled sprite's top-left
   corner is as exact as anyone's. Golden Sun scales sprites constantly, so
   this alone was half the population. Coverage 46.48% -> 90.81%.
3. **Recover a shadow's position from its paired body.** A shadow carries no
   coordinate of its own. Its body's exact row plus the shadow's own
   truncated byte, read as a signed offset, recovers it exactly
   (`golden_sun_obj_paired_coordinate`). Without this step shadows would have
   been drawn on top of the characters they belong to. Validated in play:
   largest offset ever seen 54 rows against a 128 limit, zero near-misses.
4. **Write the record at commit time, publish it immediately, and watch both
   sprite tables.** The record used to be assembled later and made visible
   only by the upload DMA from one table address; on the 31% of frames
   uploaded from the other table it was never refreshed, going stale by
   several frames. A stale record fails its own consistency check, which
   marks the sprite untrusted -- and an untrusted sprite both falls back to
   the wrapping byte and is confined to the native 240x160 window. That one
   cause produced both reported symptoms at once: shadows jumping, and
   shadows culling early at the margins.

**What each step was worth**, on the sprites whose byte lands in the
ambiguous band -- i.e. the ones that can visibly jump:

| after | coverage of jump-prone sprites |
|---|---|
| reading the byte (before) | 0% |
| steps 1-2 | 97.55% of what one table shows |
| step 3 | shadows usable rather than stacked on their bodies |
| step 4 | half-landed -- shadows 100%, bodies regressed; see below |

Steps 1-3 were confirmed on screen by the user: "sprites don't jump around
anymore and load properly, except for their shadows". Step 4 fixed the
shadows and introduced a new fault in the bodies -- see "Where it stands
after the 13:40 build" below, which is the place to start.

**What did not work. Do not retry these.**

- **Widening the view, in either axis, to make room.** The ceiling is a
  property of the field width, not of the margins. Extending the vertical
  band makes it worse: every extra row collides with a row already on
  screen. Horizontally there is headroom, but only about 12px per side.
- **Splitting each axis at its own far edge** (2026-09-06 morning). This is
  the right fix for X, which has nine bits, and it did fix the right-hand
  edge. It cannot fix Y at all; the range simply does not exist.
- **Tie-breaking the ambiguous byte toward the bottom margin.** Trades
  top-margin jumping for bottom-margin jumping. There is no correct choice.
- **Any cleverer re-reading of the byte.** Even where one works at 360x240 it
  breaks at the next resolution, which fails the stated goal.
- **Validating the trusted coordinate by checking it truncates back to the
  committed byte, and rejecting on mismatch.** This is what logged
  `canonical-provenance-y-mismatch`. It discards good data whenever the
  record is stale instead of fixing the staleness. Exact ATTR0/1/2 identity
  is the check that actually bounds a record's lifetime.
- **Hooking writer route `D4` only.** It fires on a narrower gate than the
  route every committed sprite passes through.
- **Watching a single sprite table.** The game builds its list in more than
  one place and uploads whichever is current; one address missed 31% of
  frames.
- **Publishing a position record the moment the sprite is written.** Removes
  staleness, but breaks the synchronisation between the record table and what
  is actually in hardware OAM: while the next frame's table is being built,
  a published record can describe a sprite not yet on screen. Fixed shadows,
  regressed bodies (2026-09-06 13:40 build). Consult the pending record as a
  fallback instead -- see the recommended fix above.
- **Widening the commit observer's address range to reach the second sprite
  table.** Measured no-op: that table receives zero writes through the store
  path being observed, while still supplying about a third of uploads.
- **Keying a position record by hardware OAM slot alone.** The game recycles
  slots constantly -- measured: one slot held a sprite above the screen on
  one frame and different artwork at row 90 on the next.

**Where it stands after the 13:40 build -- read this first.**

Tested by the user: "It improved shadow handling, but sprites are jumping
again a little bit and so do shadows." So step 4 half-landed. Measured in
`objrec_20260906_134126` (1,882 frames of town play):

| | |
|---|---|
| overall full-precision | 83.98% |
| shadows | 4,300 of 4,300, **100%** |
| bodies | 1,032 of 2,049, 50.37% |
| jump-prone sprites covered | 85.88% (949 of 1,105) |
| still falling back | 156 |
| traced to no actor record | 1,017 (16.02%), all one call site |

Shadows are solved as a placement problem. What remains is bodies, and two
separate things are going on.

**1. The second sprite table cannot be reached the way step 4 tried.** The
recorder's new per-table count is decisive: 6,349 commits into `0x0300347C`
and **zero** into `0x03005AE0`, while uploads split 181/75 in the same
session. Whatever fills that table does not go through the store path we
observe, so widening the address range was a no-op. This is the single
biggest unknown left. Finding its writer is the next real investigation --
suggested approach: it is filled somehow, so watch writes to that range with
no writer-route filter at all (the OAM shadow trace already reports DMA into
the range; a plain CPU-store watch on `0x03005AE0..0x03005EE0` would say
whether it is CPU or DMA filled, and from where).

**2. The immediate publish is the likely cause of the new body jumping, and
should be reverted.** Step 4 made a record visible to the renderer the moment
the sprite was written, rather than when the sprite table is handed to the
hardware. That removed the staleness, which is what helped shadows -- but it
also broke the synchronisation the three-stage staging/pending/visible design
existed to keep: while the game is building the next frame's table, a
published record can describe a sprite that is not on screen yet, so it fails
its identity check against the OAM actually being rendered and the sprite
falls back to the wrapping byte. That is a new failure mode for bodies that
did not exist in the 13:17 build, and it matches the user's report exactly.

Supporting evidence, not yet conclusive: the share of sprites tracing to no
record has risen across the three builds -- 8.92%, 12.59%, 16.02% -- and body
coverage has fallen -- 64.56%, 61.81%, 50.37%. Those sessions cover different
rooms, so the trend is suggestive rather than proven; a like-for-like rerun of
the same route on the 13:17 build would settle it.

**The recommended fix, which nobody has built yet.** Do not choose between
fresh and synchronised -- take both:

- Restore publishing at the upload DMA only, and restore that handoff to the
  single source it recognised before. This puts bodies back where the 13:17
  build had them.
- Then, in `golden_sun_wide_obj_attr_y_provider` (and the X provider), when
  the visible record does not match the sprite being drawn, consult the
  pending record as a second candidate before giving up.

Exact ATTR0/1/2 agreement is necessary but is not a lifetime proof: positions
separated by an OAM wrap can share those bytes. Keep the matching visible
record first, keep X and Y from the same record, and bound any pending fallback
by its source and lifetime. The 2026-09-06 rerun below confirms the
jump/wrap result; early disappearance remains under investigation.

**Still open.**

- A third upload source at `0x03002000` (10 of 256 uploads in one session).
  That range is reused for actor records and a transient code image, so it is
  deliberately not recognised without evidence of its geometry.
- 9-13% of committed sprites still trace to no actor record, all from one
  call site (`return_pc=0x030038A4`). They are a small share of the
  jump-prone population, so this is a refinement, not a blocker.
- A hard-coded `*out_y = -97` edge-alias path remains in
  `golden_sun_wide_obj_attr_y_provider`. It did not fire in any measured
  session; it is a candidate for removal once the above is confirmed.

### Follow-up: body visibility is independent of the earlier component

The measured 162111 guest-cull case is corrected in the B328 policy. A
matching earlier B27E may have remained culled while the body itself is
still inside the bottom margin. The body now uses its own 160..199 band;
no cutoff or grace period was enlarged, and all execution-identity checks
remain. The policy regression test and runner compile pass. Normal full
build succeeded at 21:23:08; session 212401 reports bodies look correct.
The confirmed no-wrap renderer behavior is unchanged. Other missing-position
coverage remains open; aggregate renderer samples are not a sprite census.

### Correction rerun; jumping/wrapping fixed, early disappearance remains -- 2026-09-06

The premature publication and cross-table handoff are removed. Providers
prefer a matching visible record, then a same-frame primary-table pending
record with matching epoch, complete attributes and both coordinates. X and
Y are selected together. The compositor now actually uses the recorded X
and keeps unknown positions confined to the native rectangle; it no longer
turns wrapped OAM coordinates into trusted margin placement. Affine doubled
bounds and shifted screen coordinates are used for culling.

The user reran the full executable with Self-heal RAM, Record sprite placement
and Draw field from room buffer in `logs/session_20260906_162111.log`: sprite
and shadow jumping/wrapping were no longer observed, but sprites still
disappear too early while an NPC remained in the expanded view. The
`wide-obj-y-transition-summary` values are renderer/provider diagnostic
samples, not sprite counts: `no-provenance/offscreen=24,231,840`,
`stale-frame/native=3,360`, and `stale-frame/offscreen=2,274,720`
(`2,278,080` stale-frame samples total). The optional experimental culler
reported `logs=0 dropped=0`; this shows that path was inactive, not that
guest culling was inactive. A bounded `wide-obj-y-cull-decision` sample had
44 decisions: 36 overridden and 8 retained (`original=1 final=1
overridden=0`). Retained samples at frames 373434--373437 are record
`0x03002070`, with B27E operands `225..222` and B328 operands `199..196`;
matching objrec data has body commits `0` but shadow commits `274` (`263`
exact). The retained sample is bounded and is not a population count.

The normal full build succeeded (2026-09-06, 16:18 local), and no new gameplay
coverage percentage is claimed.

Follow-up session `20260906_212401`: bodies looked correct, but shadows still
clipped at the native 240x160 bounds. Matching recorder
`logs/objrec_20260906_212409/summary.txt` reports `4,677` shadow commits and
`4,658` full-precision (`99.59%`). The run completed `73` primary OAM DMA
uploads, each with `109/128` slots reporting `raw_y>=160`. Bounded transition
samples contain `61` no-provenance cases: `59` for slot 17 (`0x03003504`),
frames `374481..374552`, raw Y `161..194`, and one for slot 21
(`0x03003524`) at frame `374481`, raw Y `166`. The OAM shadow trace observed
writes to those slots from RAM PCs `0x030001A4` and `0x030001A8`, but the
final audit identifies a clear-fill routine, not a proven shadow producer.
Those writes were not matched to the failing frame/generation. The proposed
hook was stopped without implementation. Missing provenance causes native
clipping; its actual producer/upload lifetime must still be established.

Next acceptance should repeat the four-margin
walk and room change while checking record `0x03002070` and the writer relation
between B27E and B328. VRAM map write trace is only needed for a
background/map symptom. The second table writer and unsourced body commits
remain unresolved; same-frame pending fallback cannot recover older missing
records.

### Tooling -- the sprite recorder (`GSR_OBJ_RECORD`)

Built 2026-09-06 for the work above, on the same pattern as the scene
recorder: a launcher toggle the user enables, the user plays, the session is
analysed offline.

- Launcher: **Test variables -> "Record sprite placement"**. It implies
  `GSR_OAM_SHADOW_TRACE`, without which nothing reaches it -- the first two
  sessions recorded zero sprites for exactly that reason.
- Runtime: `src/obj_recorder.{h,cpp}`. It measures only; it never changes a
  guest write, a coordinate or a rendered pixel. It shares the renderer's own
  lookup (`golden_sun_obj_resolve_placement`), so it cannot drift from what
  it is measuring.
- Output: `logs/objrec_<stamp>/` -- `coverage.csv` (per frame, per outcome),
  `sites.csv` (per emitting call site), `records.csv` (per actor record),
  `unsourced.csv` (a bounded sample of the failures), `summary.txt`.
- Analysis: `python tools/decode_obj.py logs/objrec_<stamp>`, plus
  `--sites`, `--records`, `--frames`, and `--compare` across sessions.
- The summary reports the two assumptions this work rests on so they stay
  measured: the largest shadow-to-body offset seen, and the per-table
  committed counts.

For a useful session, ask for a town with several NPCs, walking to each edge
so sprites cross the margins, plus one room change.

## Milestone 3 — locate the data worth changing

Camera, entity list, spell and item tables, walk speed. Each is a targeted find
using the tracer plus memory-watching — the technique that located the room
bounds first time.

Data changes need no new architecture: find a table, edit values. That covers
item and Psynergy tables and probably walk speed.

## Milestone 4 — function replacement

**Nothing can currently substitute a native function for a guest one.** The
function-entry hook is observe-only
(`gbarecomp/src/armv4t/runtime_arm.h:815-816`).

This is the one missing primitive for changing behaviour rather than data. The
project is unusually well-shaped for it: every guest function is already a
separate generated C++ function reached through a dispatch table, so a redirect
sits at a natural seam.

Build it when a concrete need appears, not before.

## Tooling — the tracer

Built 2026-09-04 and working. Records per-function call counts with the most
recent argument registers at entry, splits captures on room-bounds change,
overlay bank change and window-register change, tags captures by room, and logs
raw hardware signals per frame. Launcher toggle "Function tracer", off by
default.

Verified deterministic: two independent headless runs produced identical
function sets and call counts.

Coverage from one 55-minute session: 1,289 of 2,981 main-ROM functions (43.2%),
plus 767 addresses in overlay code and 719 in IWRAM.

Use it in service of a milestone, not for its own sake.

## Shelved — audio

The native MP2K work is **shelved, not abandoned.** It is a real goal to return
to once the map and renderer work is done.

State when shelved: the native path was experimental and fail-closed, with the
canonical PSG/FIFO path as the oracle. Latest probation evidence failed at
correlation 0.68, ratio 0.19. Producer ownership, fidelity and independent
clock completion were all still open.

Detail is preserved in `docs/features/MP2K.md`, with the issue records in
`docs/OLD/issues/AUD-01.md` and `AUD-03.md`.

Do not pick this up without saying so first.

## Shelved — enhanced timing

Also **shelved, not abandoned.** A plan for smoother-than-hardware
presentation: stable 60 Hz guest updates and exact 120 Hz presentation with 2x
interpolation. Part of it is built; the dynamic CPU headroom steps are not.

Detail in `docs/features/ENHANCED_TIMING.md`. Do not resume without saying so
first.

## Out of scope

- **Full decompilation.** ~6,000 real functions, ~1.3 MB of code; comparable
  projects took communities years. Not the goal, and readability is explicitly
  not wanted.
- **Readable or idiomatic generated code.**
- **A GPU/OpenGL renderer**, unless the room buffer proves insufficient.
- **BG0 margin work** — was built on 2026-09-04, never viewed, and removed with
  the rest of the widescreen implementation.

## Open questions for the user

- Priority between milestone 1's two measurements once they can be taken.
- Whether the launcher's other test toggles stay commented out or come back.
