# Widescreen investigation

Status: three optional fixed modes implemented, 2026-08-26: Native 240x160
(default), Widescreen 288x160, and Expanded Widescreen 360x240. Expanded uses
true signed margins of ±60 horizontal and ±40 vertical; strict-static and
framebuffer-capture runs force Native. Palace split-scroll map authorization
and authenticated equal-scroll OBJ-Y widening are implemented; manual
multi-scene acceptance remains pending. This change did not launch gameplay.

## Conclusion

True horizontal expansion is technically possible, but not as one global
renderer switch.

- **Overworld:** best first target. State3 already uses two populated 512x512
  affine backgrounds. The existing expanded PPU can extrapolate them.
- **Battle:** can widen the affine backdrop/effects, but State2's affine BG2 is
  only 128x128 with hardware wrapping. This is a repeated effect surface, not
  extra battle geometry. UI and transitions need an explicit margin policy.
- **Town/dungeon:** State1's measured Mode 0 field-layer signature gates a
  read-only provider. BG1/BG2/BG3 are 256x256 resident rings, while the
  measured ROM row/column writers resolve world metatiles through EWRAM
  0x02010000 (128x128 IDs) and 0x02020000 (2x2 raw tile entries). BG0 is a
  screen-space/UI layer and remains centered in the authentic canvas. Invalid
  source/transition frames fail closed; field objects use the measured cull
  widening below, while other scenes retain the guest cutoff. McCoy Palace is
  a separate exact Mode-0 split-scroll class and is authorized only after a
  complete clean frame.

Stretching a 240x160 texture is not widescreen. This design changes the
logical framebuffer geometry while keeping the native center authentic and,
in the expanded mode, rendering signed logical rows -40..199.

## Existing pipeline

```text
guest IO + PAL/VRAM/OAM writes
  -> per-HBlank GbaPpu::render_scanline (captures line IO/affine state)
  -> VBlank-latched RGB888 framebuffer (240x160 faithful, wider only if opted in)
  -> runtime present_first
  -> HostWindow streaming texture
  -> compute_presentation_layout
  -> SDL/OpenGL presentation
```

The engine already has the generic substrate:

- `gbarecomp/src/gba/gba_ppu.cpp`: `render_scanline_wide` renders signed
  horizontal and vertical margins with regular BG scroll, affine extrapolation,
  OBJ, windows, priority, and blending. The zero-margin path calls the separate
  faithful compositor, so Native is preserved by construction.
- `gbarecomp/src/runtime/runtime_bus_bridge.cpp`: visible lines render at
  HBlank; the completed frame and native scene inputs latch at VBlank.
- `gbarecomp/src/runtime/runtime.cpp`: `resolve_view_geometry` authorizes and
  applies a game-owned width, then presents `ppu.render_width()`.
- `gbarecomp/src/runtime/host_window.cpp`: texture dimensions follow the
  logical framebuffer; `compute_presentation_layout` scales or letterboxes it.
- `src/runner_main.cpp`: Golden Sun exposes the three fixed modes above;
  adaptive resizing remains disabled and capped at 240.

The policy is intentionally read-only and fail-closed. The game installs a
generic PPU margin callback only after an expanded view is selected; it permits
margins for the measured Mode 2 BG2/BG3 wrap+512x512 configuration or the
measured Mode 0 256x256 BG1/BG2/BG3 equal-scroll field signature, or the
separate exact Palace split-scroll signature, with no forced blank or
WIN0/WIN1/OBJ-window. Mode 0 BG0 remains suppressed outside the central 240px
canvas; BG1/BG2/BG3 field margins use the measured atlas provider keyed by each
scanline's raw scroll. Mode 1 and unknown scenes blacken both
margins. In the authenticated equal-scroll field, the object changes are exact
`Func_b168` culls: `0x0800B322` widens X `239` to the right edge, while
`0x0800B27C` and `0x0800B326` widen Y `159` to the active bottom edge. The
latter paths validate `r7+16`/`r7+4` against the measured 8-byte OAM-shadow
slots and record fresh per-slot provenance before the low-byte store; unmatched
raw OBJ-Y `160..255` remains hardware-signed.
No camera patch or tile sidecar is used.

Current limitations: expanded view disables the verified native supersampling
path and scene interpolation. Those paths explicitly require
`!ppu.view_expanded()` in `runtime.cpp`.

## Fixed modes

Native 240x160 is the default verification oracle. Widescreen 288x160 adds
24px per side; Expanded Widescreen 360x240 adds true 60px per side and 40px
above/below. Both are fixed, opt-in modes; adaptive/resizable width remains
disabled.

## State evidence

The local ROM and all three state headers match SHA-1
`5c4695205413df7db52b9a184815a07783999971`. State files and screenshots are
private evidence and must not be committed.

### State1: town

- Mode 0; BG0-3 and OBJ enabled.
- BG1/BG2/BG3 are each 256x256 text BGs with equal effective hardware scroll;
  the raw snapshot is BG1 `(115,751)`, BG2 `(627,239)`, BG3 `(115,239)`.
- Their 32x32 maps contain data, but no host-only sample can distinguish a
  legitimate edge tile from the ring column reused 256px away.
- BG0 is a separate unscrolled 256x256 layer and is likely screen-space
  composition; its exact role across dialogue/menus is **TODO-EVIDENCE**.
- The initial snapshot had no active OAM entry with X >= 240, so the culling
  threshold was measured from the ROM writer and a later full-session OAM
  trace.

The measured 256x256 equal-scroll signature classifies the field scene. The
provider resolves expanded columns from raw scroll plus the EWRAM map/atlas
tables, while the resident ring remains the canonical center. BG0 remains
suppressed outside the native canvas. The exact field-object cull is widened
only in the authenticated Mode-0 field; other scenes retain the guest cutoff.

The pre-provider 288px capture made the seam concrete: the right yellow/red
strip was an untrusted inner 8px regular-BG ring column (patterned, not the
flat backdrop), while the outer 16px were black. The provider now replaces
those margin entries from the measured atlas. The expanded 360x240 screenshots
then proved the partial-trace sentinel was wrong. Full state1-table measurement
finds metatile `0x017`/raw `0xF200` in 13,531/16,384 cells, including the solid
rows/columns outside the active 32x32 room; metatile `0x003` is authored. The
correct no-map coordinate now blackens all three field layers. Automated root-launcher
state1 capture `logs/session_20260825_094546.log` and
`local/widescreen_repro_inward3_20260825.png` show the full left black block
gone and the dummy checker excluded; the trace selected authored `x=16`, entry
`0x0300`. The root-launcher capture is 1080x720 at 3x; native center and the
288x160 slice are byte-identical (`channel_diffs=0`, `max_delta=0`). Focused
policy/PPU tests and the full serial playable build pass. User manual
multi-scene acceptance remains pending. Other valid atlas entries remain
authored. The NPC visible in the left margin is
OBJ. The ROM's `Func_b168` field-object
writer computes screen X in `r4` and rejects `X > 239` at `0x0800B322`.
The widescreen adapter overrides only that reviewed THUMB immediate in the
authenticated equal-scroll or stable Palace Mode-0 class, using
`239 + extra_right` as the inclusive limit
(263 at 288px, 299 at 360px); the PPU reinterprets emitted raw X `256..299`
positively. The `0x0800B27C` and `0x0800B326` cutoffs widen to the active
bottom edge only in those authenticated classes. Their accepted paths
validate `r7+16`/`r7+4` against the measured 8-byte OAM-shadow slots and record
fresh per-slot provenance before the low-byte store; unmatched raw Y `160..255`
remains hardware-signed. Provenance is accepted only in its writer frame or the
immediately following OAM-shadow handoff frame, preventing stale slot reuse.
Session `20260826_101559` confirms the exact executed cull route remains
`0x0800B322`: it accumulated 6,118 calls (4,515 authenticated overrides), while
all three configured B388 routes accumulated zero calls. Therefore no missing
horizontal override is proven on the current executed path. The six exact
routes remain configured for epoch-local attribution, but B388 and its
unproven pre-cull routes remain unchanged.

The generated `gs011_opt` disassembly adds operand evidence for the adjacent
vertical route: immediately before `0x0800B322`, the guest loads `r1` and `r3`
from the CPU state, computes `r6 = r1 - r3`, then `0x0800B322` tests `r4` and
`0x0800B326` tests that derived `r6` against `159`; the accepted B27C/B326
paths write its low byte to the measured OAM shadow. The unexecuted setup
routes are likewise not interchangeable: B3CA places `32` in `r2`, B3D6
places `136` in `r1` and the next instruction doubles it to `272`, while B3EA
directly compares `r3` with `208`; B388 remains unchanged after zero latest
calls.

Session `221115` confirms this is the displayed OAM path: DMA at `0x080036A4`
copied 1024 bytes from the OAM shadow `0x0300347C..0x0300387C` to `0x07000000`,
and the writer-PC histogram includes the `Func_b168`/OAM-write PCs.

The source path is instrumented by a bounded launcher-owned
`GBARECOMP_VRAM_MAP_TRACE` probe (`=0` is the explicit kill switch). Session
`20260826_101559` exhausted the former lifetime 512-CPU-record budget before
Palace and showed map/raw CRC mutations without matching CPU records. Session
`20260826_113445` rearmed once at the field exit, but still had no second
authentication epoch, no EWRAM DMA descriptor, and no Palace-unique VRAM
writer signature. The probe now rearms CPU/DMA budgets per authentication epoch,
capped at 16 windows per process, and a payload-free DMA descriptor observer
separately attributes writes overlapping the measured EWRAM map/raw ranges.
Generated stores use an upstream fast-EWRAM observer seam, while the existing
GbaBus observer covers slow/interpreter stores; both are armed only by the
diagnostics toggle. A bounded `[wide-field-producer]` summary aggregates each
CPU writer PC's write count, byte count, address range, and frame range per
table/epoch, so raw producers remain visible when the 64 detailed records are
consumed by map writes. DMA copies never mark cells authored; only separately
visible CPU stores do so, and the bitmap remains diagnostic-only/unwired. Cull
counters also reset and report with each `auth_epoch`. Enable
**Widescreen diagnostics (WIDE-01)** in the root launcher for acceptance; the
session-only toggle resets off on every launcher start and passes `1`/`0`
explicitly to the child. It derives
the active Mode 0 BG1/BG2/BG3 screenblocks from live DISPCNT/BGxCNT (including
map size) and records CPU writer PC/address/size and DMA writer
PC/source/destination/size records, with BG1-3 CNT/scroll metadata. It emits no
tile/map bytes and does not alter rendering. ROM disassembly identifies the
row/column writers and the two EWRAM tables consumed by the provider; no
sidecar/cache is needed.

Palace now has a distinct `AuthorizedMode0SplitScroll` map policy based on the
two measured entries: exact BG CNT `0x0709/0x060A/0x0503`, effective-equal HOFS
with raw BG2-BG1 `0x0200`, and raw BG1-BG2 VOFS `0x0180` with BG2=BG3. It
requires a complete clean frame, resets on an invalid/mixed row, uses each
BG's raw scroll, and does not use BG3 as a BG1/BG2 boundary. Producer
completion remains supporting evidence only and is not a runtime gate. The
next root-launcher capture with the diagnostics toggle—enter, walk, exit,
re-enter Palace—must verify this route and object behavior. Manual acceptance
remains pending; no success is claimed.

The temporary field-tile and frame/source probes were removed after session
`221115`: they classified whole frames/source histograms, not same-coordinate
pixels. The policy now uses the bounded BG3 metadata predicate directly at
each expanded coordinate, including valid raw `0xFFFF` metadata;
BG1/BG2 are not suppressed by a cross-layer tuple.

Session `20260826_172126` does not justify wiring the authored-cell bitmap into
the provider globally. In authentication epoch 0, the bitmap reached all
16,384 cells from frame-0 initialization writes by `0x00000C08`, while the
restored field table was observed at frame `519890`; those writes cannot prove
ownership of the restored savestate contents. Epoch 2 also observed only
2,665 then 3,675 authored cells during a transition. Epoch 3 did reach all
16,384 cells after the Palace map/raw producers ran, so producer completion is
useful diagnostic evidence but is not yet a safe general runtime gate (raw
entry ownership and restore-boundary invalidation still need an explicit
contract).

The same capture reports the stable Palace cull totals as B27C
`45,257/45,245/12`, B322 `50,816/50,800/16`, and B326
`50,607/50,591/16` (calls/overrides/gate rejects); all three B388 routes were
unexecuted. No OAM-shadow/DMA evidence was present, so these aggregate rejects
do not prove an earlier producer loss or justify a speculative cull override.
Manual Palace/object acceptance remains pending.

### State2: battle

- Mode 1; BG0, BG1, affine BG2, and OBJ enabled.
- BG2 is only 128x128 with wrap enabled. A wider compositor can extend the
  affine pattern, but cannot reveal a larger stored arena.
- BG0/BG1 are 256x256 regular layers and need a screen-space policy so HUD,
  menus, text, and status panels do not repeat into margins.
- `BLDCNT=0x3F90` in the state, so blend/effect behavior is active evidence,
  not an edge case to defer.

Policy proposal: keep the stock 240px UI centered, suppress its regular-BG
margin samples, and expand only the affine battle/effect layer where the
per-line endpoint gate passes. Nonuniform iris/window transitions fail closed
to black margins.

### State3: overworld

- Mode 2; affine BG2 and BG3 are both 512x512 with wrap enabled.
- Their maps are substantially populated (BG2: 2877/4096 nonzero entries;
  BG3: 2865/4096). This supports real background samples beyond 240px.
- OBJ still needs culling/placement evidence; no active snapshot entry has
  X >= 240, and vertical guest cull coverage is not proven beyond resident
  OAM.
- Horizontal and vertical world-edge behavior is **TODO-EVIDENCE**. The GBA
  wrap bit proves buffer wrapping, not the intended gameplay-map boundary.

The 288x160 slice remains the smallest oracle-backed widening: measured field
backgrounds, stock center pixel equality, no guest camera change, and
black/fail-closed margins when scene classification is uncertain. Expanded
360x240 rendering adds true signed margins, but world-edge and manual scene
coverage remain open.

## Architecture needed

Keep the generic PPU/presentation mechanism upstream. Add Golden Sun policy in
this repository through a game-owned `extended_view_init` adapter:

1. Scene classifier from measured guest state: overworld, regular field,
   battle, UI/menu/dialogue, transition/unknown.
2. Per-layer margin policy: extend, provider-backed, suppress/transparent, or
   pillarbox. The current regular-BG and OBJ hooks are useful; affine-layer
   margin suppression may need a small generic upstream hook.
3. Town/dungeon BG1/BG2/BG3 margins use the measured EWRAM map/atlas provider
   and exact per-scanline raw scroll. Equal-scroll field samples retain the
   BG3 cross-layer boundary; the Palace split-scroll class uses each BG's own
   raw scroll. Invalid pointers, scenes, or source data return unavailable so
   the PPU suppresses the margin. BG0 remains screen-space and is suppressed
   outside the native canvas.
4. Keep guest object culling unchanged outside the authenticated field. The
   exact `Func_b168` compare at `0x0800B322` is widened for field margins, and
   B27C/B326 widen only in the equal-scroll field with fresh OAM-slot
   provenance. Raw Y `160..255` without a matching record remains signed;
   B388 and unproven routes remain unchanged.
5. UI rules by scene: centered stock canvas first; edge-anchored redesign is a
   later option after exact BG/OBJ ownership is known.

The Golden Sun adapter does not use `ws_sidecar.cpp` or any guessed guest
address. The canonical center remains the verification oracle.

## Phased test plan

1. **Renderer proof:** exercise all three fixed modes. Require every native
   center pixel to equal faithful output; record layer/window/blend state and
   inspect horizontal/vertical margins locally.
2. **Overworld policy:** test camera motion plus map edges/wrap, landmarks,
   encounters, menus, transitions, and save/load. Trace missing offscreen OBJ.
3. **Battle policy:** expand affine BG2 only; test commands, damage, summons,
   status UI, fades, windows, and return to field. Pillarbox uncertain effects.
4. **Town/field behavior:** test resident regular-BG wrap/cutoff, landmarks,
   doors, menus, transitions, and save/load. Do not add a tile sidecar to hide
   naturally missing data.
5. **Object coverage:** manually verify NPCs, doors, particles, shadows, gap
   jumps, collision, and room edges. The horizontal and bottom field cull
   limits are exact; manual acceptance remains pending.
6. **Product surface:** keep the three fixed modes with 240x160 default;
   strict/capture remain Native; manual root-launcher acceptance remains.

Public tests should cover width clamping, literal 240 path, center-crop equality,
scene-policy fallback, save-state invalidation, per-layer suppression, OBJ
signed-X boundaries, affine wrapping, and nonuniform window fail-closed behavior
using synthetic fixtures. Manual gameplay remains the user's responsibility.

## Main risks

- Town tile rings alias plausible-looking but wrong scenery.
- Stock object culling leaves empty margins or pop-in.
- Screen-space BG/OBJ UI repeats or shifts incorrectly.
- Affine effects extrapolate mathematically but reveal unauthored edges.
- WIN0/WIN1/OBJ-window and blend masks leak scenery during transitions.
- World/room boundary wrapping can expose unrelated map data.
- Save-state loads can reuse stale host margin caches.
- Widescreen currently loses native supersampling and frame interpolation.

Every unknown address, threshold, map structure, or scene flag remains
**TODO-EVIDENCE** until measured on the supported ROM.
