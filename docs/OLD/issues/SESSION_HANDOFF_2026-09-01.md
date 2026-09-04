# Session handoff — 2026-09-01

Branch `local/gsrecomp-checkpoint-2026-08-27`. Nothing was committed this
session; all changes remain uncommitted in the working tree. The `gbarecomp`
submodule still has uncommitted work from prior sessions and was left alone.

## Start here tomorrow

1. Retest the WIDE-01 OBJ culling fixes (Fix A + Fix B, below). User already
   confirmed NPC sprites cull correctly; still open: shadows wrap slightly at
   the edges, and there is minor pop-in on bottom-edge sprites.
2. Diagnose why the turbo frame-render-gate fix had no effect (below) — the
   mechanism is confirmed but the fix did not change measured behavior and
   nobody has established why yet.
3. Decide whether to re-check `golden_sun_suppress_bg0_margin` now that BG0 is
   confirmed to carry world lighting, not just UI (see below) — that
   suppression was written on a different assumption and is now contradicted.
4. Find the game's map-painting routine — both remaining general approaches to
   the independently-scrolling-scene margin problem depend on it. See "Open
   threads" below.

## Done and confirmed working

**BG layer roles identified by the user**, by toggling each layer's new hide
checkbox individually — not derivable from code inspection alone:

- BG0 = lighting effects.
- BG1 = props (statues, doors, waterfall particles).
- BG2 = tiles, stairs, water.
- BG3 = the actual ground/terrain.

This overturns the prior working assumption (`docs/features/WIDESCREEN.md`,
pre-existing text) that terrain is "the lowest enabled regular BG among
BG1..BG3." That heuristic resolved to BG1, which is props, not terrain.

**Per-layer hide toggles**, added to debug/diagnose the above: F1 →
Enhancements → Visual → Layers, four live checkboxes (BG0..BG3), default all
visible, render-side only (no gameplay effect).
`gbarecomp/src/gba/gba_ppu.h:410`, `gba_ppu.cpp:33` (extern
`g_hide_bg0..3`), `gba_ppu.cpp:521` (`hide_bg_layer`),
`host_config_ui.h:107/166`, `host_config_ui.cpp:494`,
`host_window.cpp:23/3166`.

**Four-layer widescreen margins.** `src/runner_main.cpp:4759-4766` and
`:4805-4811` — removed the single-terrain-BG gate (was: reject any `bg !=
terrain_bg`, which resolved to BG1), replaced with `bg < 1 || bg > 3`.
BG1/BG2/BG3 now all reach the margins, each reading its own live HOFS/VOFS via
`golden_sun_field_tilemap_entry` (`src/widescreen_policy.h:1224`). Sentinel
checks unchanged; compositing/priority unaffected
(`gbarecomp/src/gba/gba_ppu.cpp:1283-1315`). User confirms the expanded view
now renders real terrain across the full width.
`golden_sun_field_terrain_bg` is kept (still covered by
`tests/cpp/widescreen_policy_test.cpp:1534`) but no longer gates the
provider.

**Binary size 205 MB → 168 MB (-18%).** `CMakeLists.txt:582` —
`objcopy --strip-debug` → `--strip-all` (the `.exe.debug` sidecar already
carries full DWARF; this removed roughly 37 MB of leftover COFF symtab).
`CMakeLists.txt:559-566` and `gbarecomp/CMakeLists.txt:12-13` added
`-ffunction-sections -fdata-sections` plus `-Wl,--gc-sections`. Built clean,
no symbols dropped. ICF/lld folding of the 398,832 `__alias_*` thunks
(~12 MB) was approved but **skipped**: no `lld` or `clang` in the pinned
MSYS2 mingw64 toolchain.

## Shipped, partially working — still open

**WIDE-01 OBJ culling.** Cause confirmed:
`golden_sun_expanded_viewport_branch_override` (`src/widescreen_policy.h:136`),
called from `golden_sun_wide_conditional_branch`
(`src/runner_main.cpp:5629`) whenever `g_ws_active`, intercepts the game's
native OBJ-cull branches and widens the native ±32px band by fixed amounts,
judging each OBJ entry independently. Body and shadow are separate OAM
entries, so a shadow past the band was culled while its body still drew. This
path is always-on in Expanded and is **not** the opt-in experimental culler.

- Fix A: `kGoldenSunObjMaxDimension = 64` (the GBA hardware OBJ maximum, read
  from `golden_sun_obj_dimensions` — not a guess) replaces the native-tuned
  32 in the three lower-bound gates where sprite extent matters:
  `widescreen_policy.h:100-125`, `:144-150`, `:253-263`. The seven
  upper/positive-direction gates were deliberately left at their existing
  values (top-left OBJ origin means extent does not apply there). Test
  expectations updated at `tests/cpp/widescreen_policy_test.cpp:212-286`.
- Fix B: `golden_sun_obj_pair_partner_admitted`
  (`src/runner_main.cpp:1741-1774`) pairs body/shadow by record identity and
  shares one admission decision. Wired at B27E and B328 only
  (`runner_main.cpp:5749-5767`) — the two of ten branch sites with a proven
  record-identity token. Fails open: unpaired, ambiguous, stale, or
  partner-not-yet-seen states all leave the existing decision untouched, and
  it only ever flips reject → accept.
- **User test result:** NPC sprites now cull correctly.
- **Still open:** shadows still wrap slightly, and there is minor pop-in on
  bottom-edge sprites. Both deferred by the user.
- Known gaps: Fix B is order-dependent within a frame; Fix B is not extended
  to B324/B3D2/B3DC/B3E6/B3EC/C6FA/C702/C708.
  `TODO-EVIDENCE`: whether R7 or another register carries the record staging
  address in `Func_b388`/`Func_c62c` — needs ROM disassembly.

**Widescreen margins in independently-scrolling scenes — regression accepted
deliberately.** In scenes where BG layers do not share a world (the
overworld), BG2/BG3 index the shared EWRAM atlas at their own raw scroll and
draw real-but-wrong-location content. Measured from `wide_scroll_trace.csv`
(repo root, untracked): BG2 differs from BG1 by -784px X / +704px Y, BG3 by
-784px X / +0px Y — a different 2D world location, not proportional
parallax. Both left and right margins are wrong; only the right side looks
obviously wrong, because whether wrong content resembles terrain depends on
scene content. The margin coordinate maths
(`widescreen_policy.h:1287-1296`) is symmetric and correct given its
inputs — **this is not a coordinate bug** and must not be "fixed" by adding
sentinel IDs or content filters. The user chose to leave this as-is rather
than restrict margins to scroll-matching layers.

Rejected option, recorded so it is not silently proposed again: extend a
layer into the margins only when its scroll matches the terrain layer's
scroll. Not implemented — the user's call, not made here.

**BG0 excluded from margins — now contradicted by evidence, unresolved.**
`golden_sun_suppress_bg0_margin` (`widescreen_policy.h:603/620`)
unconditionally suppresses BG0 margins in all scenes, citing a State1
measurement that BG0 is a screen-space/UI layer
(`docs/features/WIDESCREEN.md`, State1 section). This contradicts the user's
layer-toggle observation this session that BG0 carries world lighting.
Unresolved; needs a live re-check before anyone changes the suppression.

## Attempted and not working

**Turbo under Expanded runs at roughly half speed.** Cause identified:
`ppu->render_scanline()` is called on every HBlank during CPU execution
(`gbarecomp/src/runtime/runtime_bus_bridge.cpp:905`), routed to
`render_scanline_wide` when widescreen is active (`gba_ppu.cpp:944`,
dispatched `2093-2106`), plus 40 extra margin rows per frame via
`mark_framebuffer_latched` (`gba_ppu.cpp:2129`). `TurboPresentDecimator`
(`frame_timing.h:129-160`) throttles only SDL presents, never guest stepping
or rendering, so full render cost is paid on every guest frame regardless of
turbo multiplier. Cost is geometry-driven, not content-driven.

A fix was implemented — a frame-render-gate hook
(`runtime_bus_bridge.h:35`, `runtime_bus_bridge.cpp:872/904-943/951`,
`runtime.cpp:2029/3287-3300/3798`) moving the should-present decision to
frame start so discarded frames skip rendering — and **the user reports no
change**. The edits are still in the tree. Unresolved: the mechanism is
confirmed but the fix did not take effect and nobody has established why.

## Performance opportunities found, none implemented

1. `runtime_should_yield()` is emitted as an out-of-line call on every guest
   instruction (`gbarecomp/src/armv4t/arm_codegen.cpp:1462`; callee
   `runtime_bus_bridge.cpp:1444`) and runs ~6 checks even with no observers
   armed. Largest single per-instruction multiplier. Touches
   yield/frame-pacing/IRQ logic — needs care.
2. `runtime_trace_event(RUNTIME_TRACE_MEM_WRITE)` is called before every
   guest store (`arm_codegen.cpp:1015, 1022, 1029, 1099, 1186, 1355, 1370`;
   callee `runtime_arm.cpp:862`), running 7 hook checks before returning when
   nothing is armed. Safest first fix: one combined "any observer armed"
   flag checked first.
3. No LTO/IPO anywhere, so those cross-TU calls cannot be inlined; also
   forces guest registers to reload from `g_cpu` every instruction.
   Behaviour-preserving but raises already-slow build times.

Already correct, do not re-investigate: NZCV flags are computed only when
`ins.set_flags` (`arm_codegen.cpp`), memory fast paths are static inline with
direct pointers (`runtime_arm.h:466-591`), cycle counting batches device
catch-up at event budgets (`runtime_bus_bridge.cpp:1127`), branch linking
uses direct goto/calls with dispatch only for genuinely indirect targets.

## Dead ends established this session — do not retry

- The `GoldenSunFieldAuthoredMap` write-tracking bitmap cannot gate margin
  content: map load fills all 16,384 atlas cells with initialisation writes
  before real terrain exists, so "cell was written" is true everywhere
  (already documented at `docs/features/WIDESCREEN.md:213-220`). Its DMA
  exclusion is irrelevant — the ID table at `0x02010000` is written purely by
  CPU stores (0 DMA bytes across 12 sessions); only the raw tile table at
  `0x02020000` is DMA-filled.
- No readable room-dimension header was found. The two identifiable atlas
  writers use immediate trip counts — `Func_a37c` (ROM `0x0800A37C`, IWRAM
  `0x03002030`) uses `mov r6,#0x4000` (16384) and `mov r8,#0x1000` (4096);
  `Func_108e4` (`0x080108e4`, DMA writer PC `0x08010966`) uses `cmp #0xf`
  bounds. Both are fixed full-buffer operations, not room-sized. A third
  cell-by-cell writer (IWRAM `0x0300213c`/`2144`/`2134`) could not be
  resolved because that IWRAM slot is reused during a run and the registered
  code image no longer matches what was resident at capture.
- Widening the guest's own VRAM tilemap is blocked while staying
  hardware-faithful: field BGs use 256x256 text maps with screen-base blocks
  BG0=4, BG3=5, BG2=6, BG1=7 packed with zero gap (`wide_scroll_trace.csv:2`),
  so there is no free screenblock. Note the user has since lifted the
  hardware-fidelity and guest-patching constraints, on condition that timing,
  RNG, and game behaviour stay accurate — so this is blocked only under the
  old constraints, not permanently.
- Building a world-indexed shadow tilemap from observed VRAM writes cannot
  work: the game's resident tilemap is a ring only ~1-2 tiles larger than
  the 30-tile viewport, so write observation can never accumulate more than
  ~8-16px of margin data however it is implemented.

## Open threads to carry forward

- **Find the game's map-painting routine.** Both remaining general
  approaches to the independently-scrolling-scene margin problem depend on
  it: painting margins ourselves needs to know where the room ends, and
  making the game paint a wider strip needs the routine itself. Never
  located in this project. Archived prior art for a different game hooked
  the equivalent routine and read exact world x/y from its arguments
  (`local/archive/.../src/debug/ws_sidecar.cpp`, `ws_provenance.cpp`) — that
  is the shape of the solution, but it was hardcoded to that game's named
  symbols and Golden Sun's equivalent is unidentified.
- **Stored assets parked off-screen in map data.** The user identified that
  some margin content (a door panel, blue pillar strips) is graphics the
  game stores outside the visible area as scratch, not room terrain. These
  should never be drawn. Separate from the missing-lower-layers problem and
  needs its own fix.
- Turbo half-speed under Expanded (above) — fix in tree, confirmed no effect.
- Shadow wrapping and bottom-edge pop-in (above) — deferred by user.
- Cycle parity, interpreted vs AOT, still never verified — carried over from
  2026-08-31.
- Hang watchdog false positive on the boot scanline wait loop — carried over
  from 2026-08-31.
