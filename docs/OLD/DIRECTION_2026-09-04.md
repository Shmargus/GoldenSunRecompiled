# Direction, prerogative and scope — 2026-09-04

Supersedes the direction sections of `NATIVE-CODE-DIRECTION.md`,
`SESSION_HANDOFF_2026-09-02.md` and `PSYNERGY-RE-PLAN.md`. Those remain
accurate as records of what was true when written; where they conflict with
this file, this file wins.

Written at the end of a long session with Jimmy in which the goal was stated
explicitly for the first time.

## 1. The goal, in Jimmy's words

Run the game on native C++ with the ROM used only for assets, and be able to
change how it works — PC-native widescreen, turbo, walk speeds, potentially
replacement item and Psynergy tables.

The concrete test he gave for "done": widescreen becomes *"render more of the
world at once"* by changing a camera or render-resolution value, rather than a
per-pixel reconstruction problem.

Readability of the generated code is **explicitly not a goal**. A faithful but
ugly native implementation counts as success.

## 2. Where the project actually is

**The code half of that goal is already substantially met, and this was not
previously stated plainly.** The recompiler translated 26,935 functions plus 95
overlay banks into C++ ahead of time; that generated C++ is what executes. The
interpreter is a fallback for dispatch misses only — a coverage snapshot on
2026-09-03 recorded 1 distinct miss against 186,079 native calls. The ROM is
already just a data source.

What is still emulation is the **hardware** layer: PPU, DMA, timers, IRQ,
sound. That is hand-written native C++ modelling a GBA and was never derived
from the ROM.

So the remaining distance to the goal is in the hardware layer and in the
ability to *modify* behaviour — not in translating game code.

## 3. Root cause of the widescreen problem — diagnosed 2026-09-04

Both the margin garbage and the widescreen cost have the same cause.

`golden_sun_wide_tilemap_provider` (`src/runner_main.cpp`) is called **per
pixel** from the PPU's inner texel loop (`gbarecomp/src/gba/gba_ppu.cpp:1289`).
Each margin pixel triggers a host callback that reads emulated EWRAM, branches
on several mutable globals, and consults a stateful authored bitmap.

Consequences:

- **Cost is not about pixel count.** 360x240 against 240x160 is ~2.25x the
  pixels, trivial for the host. The cost is that each margin pixel is a
  branching callback rather than a table read.
- **Correctness is subtractive.** The renderer asks "what belongs at this
  pixel?" one pixel at a time and the answer must be invented, so wrong answers
  are then culled. Three culls failed this way on 2026-09-03
  (`WIDE-MARGIN-ATTEMPTS.md`).

This design was the smallest change that could bolt widescreen onto an existing
scanline renderer. It was never the right shape.

## 4. Proposed direction — the room buffer

**Build the room's tilemap once into a plain buffer; render from that.**

The room's contents are fully known on entry — the metatile table at
0x02010000, the tile-entry table at 0x02020000, and the room bounds struct, all
located during the widescreen work. A margin pixel then costs an array lookup,
identical to any other pixel, and nothing is invented per pixel so there is no
garbage to cull.

**This does not require a GPU renderer.** Earlier scoping (2026-09-02,
`NATIVE-CODE-DIRECTION.md` Option A) framed this as an OpenGL project against
mgba's `gl.c`. That is more than is needed. Replacing a per-pixel callback with
a prebuilt buffer is a much smaller change and fixes both cost and correctness.

Jimmy's challenge to the "map changes during play" objection was correct and is
accepted: a pushed statue is an entity, not a tilemap edit. Tile changes (a door
opening, a hole appearing) are occasional scripted events. The buffer is valid
almost always, with rare rebuilds on write.

## 5. Two measurements required before designing this

Recorded because this project has repeatedly built on unmeasured assumptions,
including several times today.

1. **How often are the map tables actually written during play?** Watch writes
   to 0x02010000 and 0x02020000. Determines whether rebuild-on-write is nearly
   free or a real cost.
2. **How often does scroll change mid-frame?** The game's own code writes
   hardware registers mid-frame and the renderer must honour that or the picture
   is wrong. At least one split-scroll scene is on record. If it is one or two
   scenes, handle them specifically or fall back to the current path; if it is
   pervasive, the buffer design must accommodate it.

A third, deferred three times and still never run: `GBARECOMP_COST_PROBE=1`
gives a per-subsystem breakdown with no rebuild. Run it before any performance
claim about the renderer.

## 6. The one missing primitive for modification

**Nothing can currently substitute a native function for a guest one.** The
function-entry hook is observe-only (`gbarecomp/src/armv4t/runtime_arm.h:815-816`).

Data changes do not need it — a located table can be edited directly, which
covers item and Psynergy tables and probably walk speed. Behaviour changes do.

This project is unusually well-shaped for adding it: every guest function is
already a separate generated C++ function reached through a dispatch table, so a
redirect sits at a natural seam. Smaller here than in a conventional
decompilation project.

## 7. Scope

**In scope:**

- The room buffer renderer and the two measurements above
- Locating specific data: camera, entity list, spell/item tables, walk speed
- The function-replacement primitive, when a concrete need for it appears
- Tracer work only insofar as it serves the above

**Out of scope:**

- Full decompilation. ~6,000 real functions, ~1.3 MB; comparable projects took
  communities years. Not the goal, and readability is explicitly not wanted.
- Readable or idiomatic generated code.
- A GPU/OpenGL renderer, unless the buffer approach proves insufficient.
- BG0 margin work — parked mid-session 2026-09-04, built but never viewed.

## 8. Tracer — status as of end of 2026-09-04

Built today. Records per-function call counts with the most recent r0-r3/r14 at
entry; splits captures on room-bounds change, overlay bank change and window
register change; tags captures by a room signature; logs raw hardware signals
per frame to `signals.csv`. Launcher toggle "Function tracer", off by default.

**Verified deterministic**: two independent headless runs produced 1,697
functions and 1,946,675 calls each, identical, same capture count, no function
present in one and not the other. The project's own notes
(`SESSION_HANDOFF_2026-08-31.md`) had flagged replay determinism as never
verified. It now is, at least for identical runs.

Coverage from one 55-minute session: **1,289 of 2,981 main-ROM functions
(43.2%)**, plus 767 addresses in overlay code and 719 in IWRAM. Only 47
functions appear in >=90% of captures (the always-on engine); 820 appear in
<=5% (context-specific, the separable material).

Screenshots are being removed — they fired on scene changes rather than on
anything the player did, and the useful bounding comes from manual marks.

## 9. Verified facts from 2026-09-04 — do not re-derive

- **Room identity**: the pointer at 0x03001E70 is *always* 0x02030CCC and is
  useless as an identity. The four bounds values beside it took 23 distinct
  combinations across a session and do track room changes. Use the bounds tuple,
  not the pointer.
- **BLDY is a dead signal.** Constant at 16 for 2,614 of 2,700 logged frames,
  0 for the rest. It does not fade. Both thresholds guessed for it (too loose,
  then unreachable at 28/31) were guesses about a value that does not move.
- **Transitions are window-register driven and animate for 17-18 frames.**
  Measured across two sessions: seven at 18 frames, three at 17, in one session;
  five at 18, one at 17, in another. A 30-frame debounce covers one cleanly.
  They are the black-bars and iris-circle effects, not fades.
- **Text boxes do not use the window registers.** Tested with dialogue present:
  window enable was 0 for 3,195 of 3,300 frames, non-zero only during
  transitions. Detecting dialogue needs a different signal (VRAM glyph-write
  bursts are the untested candidate).
- **Savestate loads jump the frame counter.** This caused three sessions of
  confusion and two wrong diagnoses. The tracer now re-baselines on load via a
  new `set_savestate_load_hook`.
- **Headless runs need `GBARECOMP_SELFHEAL_RAM=1`.** Without it, IWRAM audio
  code is interpreted forever and the run crawls at ~4 fps; with it and a warm
  `recomp_cache`, ~286 fps. The launcher sets it by default, which is why
  windowed play was always fast.
- **Input record/replay already existed** (`gbarecomp/src/runtime/runtime.cpp`,
  `GBARECOMP_INPUT_RECORD`/`_REPLAY`, `scripts/gs-replay.ps1`). It now pairs a
  savestate with each recording automatically.

## 10. Stale claims corrected today

- `SESSION_HANDOFF_2026-09-02.md` says to revert `kGoldenSunWideTrustedMarginPx`.
  It no longer exists anywhere in the tree; it had already been removed.
- The same file's "GPU rendering — scoped, not started" section frames the
  renderer as an OpenGL project. Section 4 above supersedes that.
- `PSYNERGY-RE-PLAN.md` remains valid as a method, but its step 1 (build a call
  tracer) is done.

## 11. Process note

Every time a number was chosen without data today it was wrong — fade
thresholds twice in opposite directions, the screenshot gate, the debounce
values, and two confident diagnoses of a fault that turned out to be a savestate
frame jump. Every time something was measured or run, it resolved quickly.

The signal log, the determinism check and the two measurements in section 5
exist for that reason.
