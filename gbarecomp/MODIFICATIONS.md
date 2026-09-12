# This is a modified copy of gbarecomp

Upstream is [`mstan/gbarecomp`](https://github.com/mstan/gbarecomp) by Matthew
Stan, licensed PolyForm Noncommercial 1.0.0 (`LICENSE`, unmodified). This
directory is a **changed** copy of it, carried inside
[GoldenSunRecomp](https://github.com/Shmargus/GoldenSunRecompiled) so that a
checkout of that project always describes an engine that builds the game.

Do not mistake this for upstream. It diverged from upstream commit `af51d0e`
(2026-07-17) and has not been merged back.

## What was changed

12 commits, roughly 47,000 added lines across 130 files. By area:

- **PPU** (`src/gba/gba_ppu.{cpp,h}`) — the expanded view: per-layer
  background sample remapping for regular and affine layers, with the window
  registers asked about the source pixel and an opt-out for layers whose
  placement the game adapter owns; whole-frame assembly at VBlank
  (`g_ws_defer_native_rows`) for frames that magnify a layer; margin
  reconstruction policy hooks; and the diagnostic dumps the Golden Sun work is
  measured with.
- **VRAM trace** (`src/gba/gba_vram_trace.cpp`) — tile and text tracing used to
  identify what the game draws where.
- **ARM runtime** (`src/armv4t/runtime_arm.{cpp,h}`,
  `src/runtime/runtime_bus_bridge.cpp`, `src/runtime/overlay_loader.cpp`) —
  native hand-off at bridge call boundaries, multi-variant healed-code cache,
  background warm-load, removal of a dispatch recursion cap, and RAM overlay
  work.
- **Audio** (`src/gba/gba_audio.*`, `src/gba/mp2k_shadow.*`,
  `src/gba/mp2k_wall_mixer.cpp`, `src/gba/turbo_audio_scheduler.cpp`) — the
  MP2K shadow mixer and its scheduling.
- **Host** (`src/runtime/host_window.cpp`, `src/runtime/host_config_ui.*`,
  `src/runtime/runtime.cpp`, `src/runtime/crash_handler.cpp`) — presentation,
  the configuration UI, and crash reporting.
- **Tests and tools** — new suites under `tests/` covering the above, and
  options in `tools/gba_recompile/`.

The game's own rules and findings live outside this directory, in the parent
repository's `AGENTS.md`, `ROADMAP.md` and `FACTS.md`. Anything here is meant to
be general to ARMv4T, GBA hardware or recompilation, and is a candidate to send
upstream.

## What was left out of this copy

Present upstream, not carried here because none of it is needed to build:

- `.github/` and `.claude/` — upstream's CI config and editor skills.
- `oracle/` — the mGBA-backed co-simulation oracle. It is off by default
  (`GBARECOMP_BUILD_ORACLE=OFF`) and needs a `libmgba` this repository does not
  carry.
- `third_party/mgba` — mGBA's own source. `THIRD_PARTY_ATTRIBUTION.md` (kept
  intact, as upstream wrote it) says mGBA is vendored at that path; in this copy
  it is not. The files here that are derived from mGBA are
  `src/runtime/bios_hle.{h,cpp}`; they remain under MPL-2.0
  (<https://mozilla.org/MPL/2.0/>) and mGBA's source is at
  <https://github.com/mgba-emu/mgba>.

Upstream's own documentation is still here: `README.md`, `PRINCIPLES.md`,
`docs/`, and the rest.
