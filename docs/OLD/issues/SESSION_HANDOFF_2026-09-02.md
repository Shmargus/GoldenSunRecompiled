# Session handoff — for 2026-09-02

Branch `local/gsrecomp-checkpoint-2026-08-27`. Nothing committed this session;
all changes are uncommitted in the working tree. The `gbarecomp` submodule still
carries uncommitted work from prior sessions and was left alone.

Supersedes `SESSION_HANDOFF_2026-09-01.md` for the items below; everything in
that file not mentioned here still stands.

## Start here tomorrow

1. **Revert the trusted-margin clamp** — it shrinks the widescreen view. See
   "Revert first" below. Smallest, clearest task; do it before anything else.
2. **Profile widescreen turbo.** Expanded turbo is still ~4x more expensive than
   native turbo after the render-gate fix. Use `GBARECOMP_COST_PROBE=1` — no
   rebuild needed. This is the measurement that should have happened before any
   of the last three rounds of optimisation.
3. **Out-of-bounds sprites need a new approach.** The trust-based cull shipped
   this session did not work; see below.

## Revert first

`kGoldenSunWideTrustedMarginPx = 16` (`src/widescreen_policy.h:1019-1030`)
rejects all BG margin content beyond 16px to the black/unavailable path. User
reports this **makes the visible widescreen view smaller** and wants it
reverted.

- Predicate: `golden_sun_wide_beyond_trusted_margin`,
  `src/widescreen_policy.h:1024-1030`.
- Sole call site: `src/runner_main.cpp:4842` (inside
  `golden_sun_wide_tilemap_provider`), returns `kWsTilemapUnavailable`.
- Added last session (2026-09-01) on the theory it would suppress margin junk.
  It was reported then as having *zero* visual effect; that assessment was
  wrong — the effect is a narrower view.

Ambiguity to resolve before acting: the user said "the margin change from
earlier", which most likely means this clamp. It is the only change that can
shrink the view. This session's sprite change only confines sprites and cannot
narrow the background. **Confirm with the user, then revert.** Raising the
constant vs removing the check entirely is also the user's call.

## Shipped this session, result known

**Untrusted-sprite margin culling — DID NOT SOLVE THE PROBLEM.**
`gbarecomp/src/gba/gba_ppu.cpp`: per-axis trust flags in `resolve_sx`/`resolve_sy`
(~1452-1497), `sprite_trusted` (~1513-1520), native-window bounds checks in the
emission path (~1528-1532, ~1560-1564). Sprites whose position resolved through
`g_ws_obj_attr_x_provider`/`_y_provider` stay trusted and draw full width;
raw-OAM fallbacks are confined to the native 240px window. Fails safe (defaults
trusted); inert in native rendering because `native_first`/`native_last` span
the whole row there.

**User result: out-of-bounds sprites are still visible.** User's read is that
the game places them there by design. So the trust signal is not the right
discriminator — those sprites are presumably resolving as *trusted* and are
genuinely where the game put them.

Decision needed tomorrow: keep this change (harmless but unproven) or revert it
too. It has not been shown to do anything useful. Ask the user.

**Turbo render gate — partial improvement, still 4x off.**
`gbarecomp/src/runtime/runtime.cpp` (~3291 install, ~3727 reuse of the cached
decision). Root cause found and fixed: the gate was only installed under
`args.window && present_in_place && !ws_sidecar_enabled()`, and
`present_in_place` is forced off whenever the widescreen sidecar is enabled —
so in Expanded mode the render-skip was *never installed at all*. A second
install site now covers the ordinary runner-loop path.

Why `present_in_place` is off for the sidecar (established, do not re-derive):
present-in-place resumes the guest from inside `runtime_should_yield`, bypassing
the ordinary per-frame loop, and the sidecar's per-frame capture
(`ws_sidecar_sync_frame`/`ws_sidecar_active_fill`, `ws_sidecar.cpp:274-425`) is
wired into that ordinary loop (`runtime.cpp:3676-3693`). The gate itself fires
from `tick_devices` on the PPU `frame_completed` event
(`runtime_bus_bridge.cpp:914-921`), independent of either presentation path, and
gates only `ppu->render_scanline()` — never DMA/IRQ/timers or guest-readable
state.

**User measurement: Expanded turbo now gives ~+60% speed; native turbo gives
~+300%.** So widescreen still costs roughly 4x in turbo. Either the gate is
still not effective on this path, or the dominant cost is not scanline
rendering. **Do not guess — profile it (item 2 above).**

**LTO enabled.** `CMakeLists.txt:8-38` (`GSR_ENABLE_LTO` option, default ON;
`check_ipo_supported` gate; `GSR_LTO_JOBS` default 8), `:574-576`, `:598-605`;
`gbarecomp/CMakeLists.txt:370-384` applies it to the seven gbarecomp targets.
`third_party/` and `gbarecomp_imgui` deliberately excluded.

- Disable: `-DGSR_ENABLE_LTO=OFF`. Serial LTRANS: `-DGSR_LTO_JOBS=1`. Either
  forces a full recompile.
- Build: `cmake --build build/gs011_opt --target GoldenSunRecomp -j16`
- `GSR_LTO_JOBS=8` (not 16) was chosen because GCC's WPA stage is
  single-threaded and is the memory peak; machine has 15.9 GB. Unproven number.
- Not set, next lever if memory is tight: `-flto-partition=balanced` with
  `--param lto-partitions=16..32`.

## Verified facts worth not re-deriving

- **Build config**: the user's build is `build/gs011_opt`,
  `CMAKE_BUILD_TYPE=RelWithDebInfo`, resolving to `-O2 -g -DNDEBUG`. The
  presets in `CMakePresets.json` define **Debug only** and are NOT what the
  user builds. A prior report claiming the normal build is unoptimised Debug
  was wrong.
- **`-O3` has never been measured on this project.** The 1.5x LTO figure in
  `gbarecomp/docs/DEBUGGING.md:109-153` is `-O3` vs `-O3`+LTO — both rows are
  `-O3`, so it measures LTO, not the optimisation level. Deliberately left at
  `-O2` so this build measures one variable. Trying `-O3` alone is a valid
  future experiment.
- **LTO costs, recorded 2026-08-12 on this same Golden Sun corpus**: link
  20m31s; binary grew 48.7 → 56.7 MB (LTO does *not* shrink it — the 205 → 168
  MB shrink was last session's strip/gc-sections work). Behaviour-preservation
  was verified, not assumed: identical cumulative cycle counts across 1,800 and
  5,400 frame runs on two tracks.
- **Existing measurement tooling, no rebuild required**:
  `GBARECOMP_COST_PROBE=1` (per-subsystem ns/call-count breakdown at exit;
  `runtime_bus_bridge.cpp:104-212`, `runtime.cpp:1512-1695`),
  `GBARECOMP_PRESENT_CADENCE=1` (CSV + `tools/analyze_present_cadence.py`),
  FPS overlay via F1. Headless benchmark methodology:
  `gbarecomp/docs/DEBUGGING.md:109-153` (`--frames N`, `GBARECOMP_DEMO_INPUT`
  tracks). Gotcha: `GBARECOMP_DEMO_INPUT=` with an empty value still enables a
  demo track — it must be unset.
- **Overclock does help perceived smoothness** — user confirms 4x CPU overclock
  substantially reduces the in-game slowdown during spell effects. That is the
  game's own CPU falling behind, so it is fixed by overclock and would NOT be
  helped by GPU rendering. 4x ≈ 67 MHz.

## GPU rendering — scoped, not started

Verdict: possible, large, and it collides head-on with the widescreen work.

- **No app-owned GPU code exists.** Host uses SDL2 2.32.10 with
  `SDL_CreateRenderer(SDL_RENDERER_ACCELERATED)` and a plain
  `SDL_UpdateTexture`/`SDL_RenderCopy` upload
  (`gbarecomp/src/runtime/host_window.cpp:925, 1005, 1451-1577`). No shaders, no
  GL/D3D context of our own anywhere in `src/` or `gbarecomp/src/runtime`.
- **Volume**: ~1,300 lines of scanline compositor
  (`render_scanline_internal` `gba_ppu.cpp:535-966`, `render_scanline_wide`
  `:968-1750` — duplicated, not shared) plus the ~170-line widescreen provider,
  plus a new VRAM/OAM/PAL sync layer that does not exist today.
- **The collision**: `golden_sun_wide_tilemap_provider`
  (`src/runner_main.cpp:4785-4955`) is called **per pixel** from the inner texel
  loop (`gba_ppu.cpp:1289`). It does raw `bus->ewram_ptr()` reads at
  0x02010000/0x02020000, branches on mutable globals (`g_ws_active`,
  `g_golden_sun_mode0_field`, `g_golden_sun_mode0_split_scroll`,
  `g_golden_sun_palace_table_authorized`, `g_golden_sun_wide_line_io_valid`) and
  consults a stateful authored bitmap (`widescreen_policy.h:1232-1310`). This is
  branching host state, not a lookup table — it cannot become a texture/uniform
  without being ported into shader logic. Since margins are BG-layer output,
  "GPU for BG layers only" is exactly the slice that breaks widescreen.
  **Widescreen margins are what would break first.**
- **Reference available in-tree**:
  `gbarecomp/third_party/mgba/src/gba/renderers/gl.c` (1,971 lines) is a real
  hardware renderer that solves per-scanline state properly — hooks
  VRAM/OAM/palette writes, tracks `scanlineOffset[y]`/`scanlineAffine[y*4]`
  (`gl.c:1345-1351`), batches with dirty flags (`:1203-1302`). It has no
  widescreen concept.
- Per-scanline register state that would need capturing: ~76 bytes/scanline.
  The hard part is bulk memory (VRAM 96KB, OAM 1KB, PAL 512B), which can change
  mid-frame via HBlank DMA.
- Recommendation on record: do not start until item 2 (profiling) shows drawing
  is actually a dominant cost. It would not help the spell-effect slowdown.

## Incidental findings, not acted on

- The renderer implements **video modes 0/1/2 only** — no bitmap modes 3/4/5
  (`gba_ppu.cpp:787-798`, `:1419-1428`). Whether Golden Sun needs them is
  unverified.
- **Mosaic is not implemented at all** — no `mosaic`/`MOSAIC` token anywhere in
  `gba_ppu.cpp`. Whether Golden Sun uses it is unverified.
- User asked whether these matter; not investigated. Ask before spending time.

## Still open from before, unchanged

- Shadow wrapping and bottom-edge sprite pop-in (WIDE-01) — deferred by user.
- BG0 margin suppression (`golden_sun_suppress_bg0_margin`) contradicted by the
  user's layer-toggle observation that BG0 carries world lighting. Needs a live
  re-check before changing.
- Independently-scrolling-scene margins (overworld) — regression accepted
  deliberately; not a coordinate bug. Do not "fix" with sentinels or content
  filters.
- Find the game's map-painting routine — still never located.
- Cycle parity interpreted vs AOT, never verified.
- Hang watchdog false positive on the boot scanline wait loop.

## Dead ends — do not retry

Carried forward from `SESSION_HANDOFF_2026-09-01.md` (authored-map write
bitmap, no room-dimension header, widening the guest VRAM tilemap, world-indexed
shadow tilemap). Added this session:

- **Trust-based sprite culling does not remove the out-of-bounds sprites.**
  Implemented and tested; sprites still visible. They are evidently placed
  deliberately by the game and resolve as trusted.
- The 32-bit `map_word` at 0x02010000 has 20 unused upper bits
  (`widescreen_policy.h:1392` masks `& 0x0FFF`) that *might* flag never-visited
  cells. Unverified — would need a visited/unvisited census run. User parked
  this ("not now").
