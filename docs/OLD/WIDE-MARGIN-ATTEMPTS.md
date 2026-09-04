# Widescreen margin content — attempts that did NOT work

Status: **unsolved**. Latest session: 2026-09-03 (see the dated section at the end). Every approach recorded here was implemented, tested by
the user, and judged non-functioning. All of them were reverted on 2026-09-02.

Read this before proposing anything for the widescreen margins. Several of
these look obviously correct on paper and still failed.

## The problem

Expanded widescreen shows more of the map than the original 240x160 screen.
The extra area is filled with content that a real GBA never displays: repeating
filler tiles, scenery belonging to the area around the room, and sprites the
game parked off-screen. The widescreen view itself is fine and is NOT the
problem — the goal was only to avoid drawing the garbage that is normally out
of sight.

The user's stated preference: real map content where it genuinely exists,
**black** where it does not, and the view must not get narrower.

## What was proven true (keep this, it cost real effort)

These are established facts, verified by live measurement, not inference.

### Margin content is genuine, game-written data

A full census of one room's margins (`GSR_MARGIN_TILE_LOG`, session
`logs/session_20260902_191039.log`) recorded **527 margin cells, every single
one `authored=1`** — i.e. genuinely written by the game.

- 3 background layers all draw margins: BG1 (177 cells), BG2 (177), BG3 (173).
- Filler tile `0x3280` accounts for 120 cells, appearing on top, bottom and
  right. This is the repeating pattern the user sees.
- The remaining ~400 cells are 118 distinct tiles — real, varied scenery.
- The left margin produced no cells at all; it already renders blank.

**Consequence: there is no validity/ownership signal to filter on.** Anything
that tries to distinguish "real" from "garbage" by looking at the map data
itself is doomed, because it is all real.

### Sprites are parked at a single fixed coordinate

A census of off-window sprite positions (`GBARECOMP_OBJ_PARK_CENSUS`) over a
play session found one overwhelmingly dominant position:

| Position (raw attr0&0xFF, attr1&0x1FF) | Count |
|---|---|
| **Y=192, X=192** | **9198** |
| everything else | 1–3 each |

Y=192 decodes to 64 pixels above the screen. This is the game's park/hide spot:
it leaves the OBJ enabled and moves it out of the hardware's visible rectangle.
The gap between 9198 and 3 makes this a reliable discriminator **if** sprite
culling is ever wanted. It was never applied, because sprites turned out not to
be the main complaint.

### Room bounds exist in memory and are readable

This is the most valuable find of the session, and the only one worth building
on.

- IWRAM pointer `0x03001E70` -> per-room struct `R` (observed `R = 0x02030CCC`).
- `[R+0xEC]` min X, `[R+0xF0]` min Y, `[R+0xF4]` room X extent,
  `[R+0xF8]` room Y extent. All Q16.16, same scale as HOFS/VOFS.
- Observed in the test room: `0, 0, 800.0px, 512.0px`.
- The game clamps its own camera with `scrollMax = roomExtent - screenExtent`.
  Verified: `512.0 - 160 = 352.0`, exactly matching the camera plateau observed
  when walking into the wall.
- Written once per room load, guest pc `0x0800FBEC`-`0x0800FC04`, immediately
  before the camera reset at `0x0800FC18`. No further writes during play
  (verified over 60 frames).
- Clamp code: `gf_Func_10230`, `local/gs011/main/recompiled_022.cpp:11921`,
  guest `0x08010230`-`0x080102B8`.
- Scroll registers are written by **DMA3**, not CPU stores: an immediate 32-bit
  DMA copies 4 words from IWRAM `0x03001AD0-0x03001ADF` into
  `0x04000010-0x0400001F` once per frame. The shadow table is global, stride 4
  bytes per BG, HOFS then VOFS. Writer: `gf_tfunc_080101CA`,
  `local/gs011/main/recompiled_025.cpp:13777-13857`.

**Unverified:** only one room was exercised. Whether the pointer at
`0x03001E70` changes on room transition, and whether `0x02030CCC` is a single
reused slot, was never confirmed. The X axis clamp was never actually
triggered (only vertical movement was tested).

### The expanded view's vertical structure

Native rows occupy canvas rows [40, 200) of a 240-row canvas
(`kNativeHeight=160` `widescreen_policy.h:23`, `kExpandedExtraY=40`
`widescreen_policy.h:30`). This boundary is the horizontal line where the user
observed pillars and other tiles being cut off. That cut-off is long-standing
and predates this session's work.

---

## Attempts, all non-functioning

### 1. Trusted-margin clamp (16px) — REVERTED

`kGoldenSunWideTrustedMarginPx = 16` rejected all BG margin content beyond 16px
from the native window.

**Result:** made the visible widescreen view **narrower**. Rejected by the user.
An earlier session had recorded it as having "zero visual effect"; that
assessment was wrong.

### 2. Authored-cell ownership check — REVERTED

The code already tracked which map cells the game had actually written
(`GoldenSunFieldAuthoredMap`, `g_golden_sun_field_authored`), but both
`golden_sun_field_tilemap_entry` call sites passed `nullptr`. Enabling it made
never-written cells report unavailable (black).

**Result:** partially worked — large regions correctly went black in one
screenshot. But it cannot solve the problem, because **all 527 sampled margin
cells are `authored=1`**. The junk is genuinely painted data. Reverted.

### 3. Fade-triggered reset of the authored bitmap — REVERTED

The authored bitmap was only reset on scene-*type* change, so two consecutive
rooms of the same type shared stale authorship. A fade-to-black was used as the
room-transition signal: `golden_sun_wide_full_fade_active()` detecting
BLDCNT brightness-decrease + BLDY at max.

**Result: never fired once.** Zero `fade_edge` / `fade_reset` events across
three real room transitions in a logged session. Golden Sun's map-change fades
do not set forced blank, and the BLDCNT/BLDY condition used did not match what
the game actually does. The change was completely inert. Reverted.

**If retried:** find a real room-change signal first and prove it fires before
building anything on it.

### 4. Counting DMA-sourced map writes — REVERTED

`golden_sun_wide_ewram_write_observer` deliberately skipped DMA writes before
`mark_write`, so bulk-loaded room data was never marked as authored. The skip
was narrowed so `mark_write` also ran for DMA writes.

**Result:** no observable effect, and moot once attempt 2 was shown to be
unable to help. Reverted.

### 5. Room-bounds blackout — REVERTED

Using the confirmed room bounds above, any margin pixel resolving outside
`[min, min+extent)` returned unavailable (black).

**Result:** it did remove the junk, but the user's verdict was that it is
"hardly a decent fix" — it crops the rendering of several BG layers rather than
addressing why the view is looking outside the room at all. Visually it left
hard cropped edges. Reverted.

### 6. Widescreen camera shift (clamp the view into the room) — REVERTED

Instead of cropping, apply the game's own `scrollMax = extent - viewExtent`
formula using the **wide** view extents, and shift the widescreen view inward
so it never needs to look outside the room. Sprites outside the native window
received the same shift. Rooms smaller than the view were centred.

**Result: clearly worse.** Produced disconnected floating fragments of map, and
still cropped several BG layers. The fundamental flaw: the central 240px region
is still drawn by the game untouched, so shifting only the margin extension
desynchronises the two. Any correct version of this must move the entire view
as one, which is a substantially larger change. Reverted.

---

## Dead ends carried forward from earlier sessions

Do not retry these either:

- Authored-map write bitmap as a general solution (superseded by the
  all-cells-are-authored finding above).
- Searching for a room-dimension header — never found. Note this is a
  *different* thing from the camera clamp, which WAS found (see above).
- Widening the guest VRAM tilemap.
- World-indexed shadow tilemap.
- Trust-based sprite culling: sprites resolving through the widescreen
  providers stay "trusted" and draw full width, raw-OAM fallbacks are confined
  to the native window. Implemented and tested; the out-of-bounds sprites are
  still visible, because they are genuinely placed there by the game and
  resolve as trusted. Trust is not the discriminator.
- Sentinel/content filters on the overworld independently-scrolling case — that
  regression is deliberately accepted.

## Untested ideas, in rough order of promise

1. **Move the entire view as one unit**, including the native 240px region, so
   the wide camera stays inside the room. This is the honest version of attempt
   6 and the only approach not yet ruled out. Substantially larger: it means
   the native rendering path no longer renders at the game's own scroll origin.
2. **The 20 unused upper bits of `map_word`.** `widescreen_policy.h:1392` masks
   with `& 0x0FFF`. Nothing in the project has ever inspected the other 20
   bits; they may flag never-visited cells. One census run would settle it.
   Cheap to check, and the census tooling now exists.
3. **Palette-bank test.** The junk renders magenta because those tiles
   reference palette slots the current room never populated. Culling tiles
   whose palette bank is unused in the current room is a principled signal
   rather than a hardcoded tile ID. Risk: could wrongly hide legitimate tiles.
4. **Blank the specific filler tile `0x3280`.** Would remove roughly a quarter
   of the junk (120 of 527 cells) and specifically the repeating pattern. Not a
   general fix; the filler tile may differ per area.

## Diagnostics left in place

Both are off by default, gated behind the launcher's "Test variables" master
checkbox, and do not affect rendering. Output goes to `logs/latest.txt` via the
launcher's existing capture.

- **`GBARECOMP_OBJ_PARK_CENSUS`** — launcher label "Log off-screen sprite
  positions". Dumps off-window sprite position buckets at exit, top 40 by
  count. Implementation: `gbarecomp/src/gba/gba_ppu.cpp`, tallied once per
  frame.
- **`GSR_MARGIN_TILE_LOG`** — launcher label "Log margin tile decisions".
  Periodic census of every distinct margin map cell across all four sides, with
  a per-census summary of distinct tile values and which sides they appear on.

**Known issue:** the user reported the margin-tile diagnostic as very slow
(single-digit FPS) even after the per-pixel path was reduced to a single bool
test and the frame lookup was hoisted out. The cause was never established.
Do not assume the diagnostic is cheap; measure with `GBARECOMP_COST_PROBE=1`
before trusting it.

## Still open, untouched today

- **Widescreen turbo is ~4x more expensive than native turbo.** Expanded turbo
  gives ~+60% speed, native turbo ~+300%. Never profiled. Use
  `GBARECOMP_COST_PROBE=1` (launcher toggle exists) before attempting any
  optimisation — three previous unmeasured optimisation rounds all failed.
- Shadow wrapping and bottom-edge sprite pop-in (WIDE-01).
- BG0 margin suppression contradicted by the user's layer-toggle observation
  that BG0 carries world lighting.
- The game's map-painting routine has still never been located.
- Cycle parity interpreted vs AOT, never verified.
- Hang watchdog false positive on the boot scanline wait loop.

## Build note

The LTO link runs single-threaded — hours instead of ~17 minutes — unless
`MAKE` is set with **forward slashes**:

    MAKE=C:/msys64/mingw64/bin/mingw32-make.exe

GCC's `lto-wrapper` parses `$MAKE` with libiberty's `buildargv()`, which treats
backslash as an escape character, so a normal Windows path is silently mangled
and LTRANS falls back to serial (`warning: using serial compilation of N LTRANS
jobs`). GNU Make 4.4.1 was installed for this on 2026-09-02
(`pacman -S mingw-w64-x86_64-make`); it installs as `mingw32-make.exe`, there is
no `make.exe`. `-flto=8` and `lto-partitions` were never the problem.

---

# Session 2026-09-03 — NOT THE SOLUTION

Everything in this section that changed rendering was tested by the user and
**rejected**. Two things were kept, both non-rendering. The margin problem is
still unsolved.

Read "What was learned" before proposing anything. Several facts here contradict
assumptions made earlier in this document.

## Kept (do not revert these)

**`GSR_MAP_RECORD` diagnostic.** Launcher label "Record map data per room",
under the existing "Test variables" master checkbox, off by default.
`src/runner_main.cpp` (`golden_sun_map_record_*`), `src/launcher_test_policy.h`.
Per room it writes `logs/maprec_<seq>_<Rptr>_<minXxminY>_<extXxextY>.txt` plus a
companion `.bin` (~197 KB). The text file carries the room bounds raw and in
pixels, an ASCII picture of the 128x128 metatile grid marking inside/outside the
room rect, the authored grid, the hex grid, an arrival-vs-final diff, and a
per-frame timeline of out-of-rect occupancy. The binary (magic `GSRMAP1\0`,
version 2, 6 sections, header 0x68) carries merged/arrival/final metatile grids,
the 0x02020000 tile-entry table, 64 KB of VRAM character data, and the 512-byte
BG palette, plus DISPCNT and BG0-3CNT. Layout documented at
`golden_sun_map_record_write_binary`.

A renderer for these dumps lives at `logs/tools/render_maprec.py` (inside the
gitignored `logs/` tree, deliberately not part of the repo). It decodes the
metatile grid to PNG using stdlib zlib only, since PIL is not installed on this
machine, and produces per-layer images plus a merged composite with the room
rectangle outlined. Run it as `python logs/tools/render_maprec.py logs`.

**Layer-coverage fix.** `golden_sun_wide_tilemap_provider` in
`src/runner_main.cpp` no longer has the `if (terrain_field && bg != terrain_bg)`
gate that commit 759af82 introduced. That gate meant only one layer supplied
margin content in Mode0 scroll-mismatch rooms, so the expanded view read as
"native with one or two layers". BG1/BG2/BG3 now each resolve through their own
scroll register in every room class, matching what the equal-scroll branch had
always done. The font/menu-tile leak that 759af82 was written to prevent did not
return.

## What was learned (verified, do not re-derive)

**The map table is never cleared between rooms.** Measured across 15 rooms:
essentially every one of the 16384 cells holds a nonzero id in every room (1
empty cell in two of them). The game loads a room into its rectangle and leaves
the rest of the table exactly as the previous room left it.

**The out-of-rect content is present from the first frame and never changes.**
The out-of-rect occupancy timeline is flat for the entire visit and equals every
cell out there — 15872 of 15872 in one room, 14784 of 14784 in another. The
arrival-vs-final diff is zero written, zero cleared. **There is no function that
paints the garbage.** Do not go looking for one. It is residue, full stop.

**The out-of-rect content is not the current room in another form.** An exact
plus x-shifted metatile-id comparison of the inside-rect region against the
region to its right returned 0.0000 match at every offset in two rooms, and the
id sets are numerically disjoint (one room: 72 distinct ids inside, a single
repeated id outside, zero overlap). It is also not "another BG layer's copy" —
all layers read the same shared 128x128 id grid per cell, so layer choice cannot
relocate content to different cells. Rendering the dumps confirms it visually:
coherent, structurally different rooms sitting next to the current one.

**Room struct facts, extended.** The pointer at `0x03001E70` resolves to the
same slot `0x02030CCC` in every room recorded — it is a single reused slot, so
the value of R cannot identify a room. min_x/min_y were 0 in every room
observed, including the Palace room where culling failed. Extents observed:
512x256, 800x512, 752x496. Between rooms the struct reads all zeros, which is a
clean and reliable room-transition marker — this is the transition signal that
attempt 3 (fade detection) went looking for and never found.

**Why the margins look clean during a fade, and why it is useless.** During a
transition the game has the field BG layers switched off, so
`golden_sun_wide_margin_policy_reason` (`src/widescreen_policy.h:731-785`) does
not return an authorized result, the guard at the top of
`golden_sun_wide_tilemap_provider` (`src/runner_main.cpp:5770-5774`) rejects
every margin pixel, and the PPU leaves the backdrop fill in place
(`gbarecomp/src/gba/gba_ppu.cpp:1382-1392`). It is the absence of drawing, not a
mechanism that hides anything. It cannot be extended: the check is a live "are
the field layers on right now" test with no state, and cannot distinguish
"layers just came back" from "layers have been on for 500 frames". Holding it
longer needs an independent transition signal, which is exactly the dead end
attempt 3 already hit.

## Attempts, all rejected by the user

### 7. BG0 into the margins — REVERTED

BG0 was routed through the same world-map lookup as BG1-3
(`golden_sun_suppress_bg0_margin` disabled, `bg < 1` guards widened to `bg < 0`).

**Result: misplaced textures throughout the margins.** BG0 is a screen-space
layer; sending it through a world-map lookup draws map tiles at screen
positions. BG0 has never been in the margins in any commit, so this would be new
behaviour rather than a restoration. Do not retry without solving that mismatch
first.

### 8. Room-bounds culling, attempt A (cell coordinates, two sites) — REVERTED

Cached the room rect, converted to metatile-cell coordinates, rejected when
`metadata.map_x/map_y` fell outside. Applied at two sites: the BG3 cross-layer
boundary-oracle result, and each layer's own resolved cell. A matching sprite
cull was added at the same time (`g_ws_obj_room_cull_provider` in
`gbarecomp/src/gba/gba_ppu.cpp`, world position = raw BG3 scroll + screen coord).

**Result: the view collapsed to a wrongly-placed region, and the garbage was
still visible.**

### 9. Room-bounds culling, attempt B (world pixels, one site) — REVERTED

Same idea, corrected for what was assumed to be the cause of A: compared world
pixel coordinates (`abs_x = hofs + hw_x`, populated inside
`golden_sun_field_tilemap_entry`) instead of cell coordinates, and applied the
rejection at exactly one site — each layer's own resolution — leaving the shared
BG3 oracle untouched. Palace scenes excluded. Sprites left out entirely.

**Result: identical failure. Content confined to a box, garbage still present.**

### Why this line of attack is stopped

Three attempts (5 from 2026-09-02, plus 8 and 9 here) at room-bounds culling
have produced the same constrained-box failure. All three assumed that the
position the margin path resolves and the room rectangle are in the same
coordinate space. **That assumption has never been measured.** The room bounds
data is known good — the Palace room that failed reports a perfectly sane
0,0 800x512, so "bad bounds" is a dead hypothesis.

**Before any fourth attempt: instrument first.** Log, for real margin pixels in
a real room, the resolved position the provider computes alongside the room
rectangle, and confirm whether they agree. One build, one short recording. If
they disagree, that log says exactly where. Writing another cull without that
measurement will fail a fourth time.

## Still open, not touched tonight

- Black squares appearing in walls after the layer-coverage fix. User's
  priority: low. Likely a cell where one of the newly-enabled layers cannot
  resolve, punching a hole the old single-layer path used to cover. Unconfirmed.
- The graphics snapshot in `GSR_MAP_RECORD` uses a brightest-palette heuristic
  and lands on transition frames where the field layers are off (one room was
  even in display mode 2). Structure in the renders is right, colours are not.
  A better rule would require the field layers enabled and mode 0. Not worth
  doing unless the renders are needed again.
- No layer showed the room's water inside the room rectangle. Unexplained, but
  the graphics snapshot above is unreliable, so this may be an artefact.
- Palace rooms' bounds were never verified to live in the same struct.
- Widescreen turbo is still ~4x more expensive than native turbo and has still
  never been profiled. `GBARECOMP_COST_PROBE=1` toggle exists in the launcher.
  Four unmeasured optimisation rounds have now failed; do not attempt a fifth.
