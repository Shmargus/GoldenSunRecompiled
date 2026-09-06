# Roadmap

Written 2026-09-04. Replaces the previous roadmap, status, next-task and
backlog files, all kept in `docs/OLD/`.

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
fine detail while walking was **confirmed native** � it happens with every
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
4. **Performance** — never measured. The hook is consulted per pixel, which is
   the shape that made the old widescreen slow. Use `GBARECOMP_COST_PROBE=1`.
5. **Remove the temporary diagnostics** once the above settle:
   `g_ws_expanded_diag` in the PPU, and the refusal/rect reports in
   `room_buffer.cpp`. The room-rect report also has a display bug (it prints
   one line per frame instead of deduplicating).

Method note, because it kept paying off: every real cause this session was
found by a counter, not by reasoning about the code. Three separate wrong
diagnoses were stated confidently before the numbers arrived. Instrument
first.

Still open:
- **Cost.** The hook is consulted per pixel, which is the shape that made the
  old widescreen slow. Not yet measured; use `GBARECOMP_COST_PROBE=1`.
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

That is safe by construction and needs no new judgement: a record carries its
sprite's exact ATTR0/1/2, and is only ever used on a sprite whose bytes match
exactly. Offering a second candidate can never place a record on the wrong
sprite -- it can only rescue one that would otherwise have fallen back to the
byte. It should recover the freshness that helped shadows without the
prematurity that hurt bodies.

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
