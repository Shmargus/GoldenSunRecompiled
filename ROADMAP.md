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

What needs answering:

1. The exact layout of both tables — entry size, meaning of each field, how a
   tile coordinate maps to an entry.
2. How the tables relate to each other and to the room bounds.
3. Which BG layers are sourced from them. (BG3 ground, BG2 tiles, BG1 props,
   BG0 lighting, per earlier observation — needs confirming against the data.)
4. How often the tables are written during play. Statues are entities, not
   tile edits, so churn is expected to be rare and event-driven — but this is
   unmeasured.
5. How often scroll changes mid-frame, and in which scenes. At least one
   split-scroll scene is on record. If it is one or two scenes they can be
   special-cased; if it is pervasive the design must accommodate it.

Points 4 and 5 are measurements, not opinions. See rule 1 in `AGENTS.md`.

## Milestone 2 — the room buffer

Build the room's tilemap once into a plain buffer and render from it, instead of
calling back into game-specific host code per pixel.

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
