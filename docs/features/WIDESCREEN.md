# Widescreen investigation

Status: three optional fixed modes implemented, 2026-08-28: Native 240x160
(default), Widescreen 288x160, and Expanded Widescreen 360x240. Expanded uses
true signed margins of ±60 horizontal and ±40 vertical; strict-static and
framebuffer-capture runs force Native. Palace split-scroll map authorization
is implemented. Current uncommitted source adds a strict 360x240-only bypass
for ten reviewed guest viewport branches; manual multi-scene acceptance remains
pending. This change did not launch gameplay.

The 2026-08-28 build fixes B27E/B328 OAM-Y provenance (R7+16/R7+4) with
bounded payload-free `[wide-obj-y]` diagnostics. Palace table authorization
requires a complete-clean `AuthorizedMode0SplitScroll` frame and exact map/raw
CRC `0aef4a71/11020c66`; CPU/DMA writes or CRC mismatch invalidate it. Measured
fill ID `0x026`, raw `0xFFFF`, and finite out-of-bounds coordinates fail closed;
other residual IDs remain unmeasured. WIDE-01 remains open.

`build/gs011_opt/GoldenSunRecomp.exe` was rebuilt non-LTO at 2026-08-28
09:41:15 (878649847 bytes). Python tests passed `86/86`, focused CTest passed
`2/2`, and the public audit passed. No gameplay or static-status claim was
made.

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
  margins. Separately, the old object immediate/literal threshold widening is
  removed. In strict
Expanded 360x240 mode, the runner bypasses the ten exact guest conditional-
branch decisions reviewed in `src/widescreen_policy.h`: B27E/B324/B328,
B3D2/B3DC/B3E6/B3EC, and C6FA/C702/C708. The callback receives the original
CPSR-derived decision and changes only those exact branches; it is globally
active across scene classes, independent of field/Palace authentication or
coordinates. Native 240x160 and Widescreen 288x160 preserve the guest
decision. The unchanged PPU still performs pixel clipping and retains 128 OAM
slots.
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
suppressed outside the native canvas. Guest viewport-branch bypasses are a
separate strict Expanded-mode policy and are not gated by this field classifier.

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
writer computes screen X in `r4`; its historical `X > 239` compare is at
`0x0800B322`, followed by the reviewed conditional branch at `0x0800B324`.
The current adapter no longer overrides a THUMB immediate or literal. It
bypasses only the ten exact conditional branches listed above, and only in
strict Expanded 360x240 mode. The callback is independent of field/Palace
authentication and coordinate values; the PPU reinterprets no new values and
continues to perform its normal pixel clipping. Existing OBJ presentation
providers retain their measured OAM-shadow and provenance rules; the visible
signed-Y record is latched by the exact OAM DMA handoff and remains valid while
the current ATTR0/1/2 exactly match the measured 12-byte staging record. A
changed or reused slot fails closed; raw-Y, epoch, and DMA gates remain.
The branch hook does not widen those providers. Session cull/literal counters below are
historical evidence from the removed implementation.

The generated `gs011_opt` disassembly adds operand evidence for the adjacent
vertical route: immediately before `0x0800B322`, the guest loads `r1` and `r3`
from the CPU state, computes `r6 = r1 - r3`, then `0x0800B322` tests `r4` and
`0x0800B326` tests that derived `r6` against `159`. The branch PCs are B27E,
B324, and B328. B388's four bound branches are B3D2/B3DC/B3E6/B3EC.
Func_c62c's direct reject branches are C6FA/C702/C708, associated with the
C6F2/C6FE literal-load block. These are decisions, not immediate/literal
rewrites.

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
restored/unchanged table path now additionally requires map/raw CRC
`0aef4a71/11020c66`; CPU/DMA writes and CRC mismatches invalidate authorization.
Measured fill ID `0x026`, raw `0xFFFF`, and finite out-of-bounds coordinates
are rejected. Other residual IDs inside the exact fingerprint remain
unmeasured and are a known limitation.
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

Session `20260828_111007` confirmed that wiring this bitmap into equal-scroll
lookups was a regression: restored Bilibin had its valid measured table CRCs
but zero current-epoch authored cells, producing no replacements. The runtime
gate was removed again for equal-scroll fields; Palace's separate fingerprint
authorization is unchanged.

The same historical capture reports stable Palace cull totals for the removed
immediate-hook implementation: B27C `45,257/45,245/12`, B322
`50,816/50,800/16`, and B326 `50,607/50,591/16` (calls/overrides/gate
rejects); all three B388 routes were unexecuted. No OAM-shadow/DMA evidence
was present, so these aggregate rejects do not prove an earlier producer loss
or justify a speculative cull override.
Manual Palace/object acceptance remains pending.

Session `20260828_143521` confirmed Bilibin backgrounds expanded again and
exposed unconditional earlier object-cull branch overrides. B388 now admits
only padded X `-92..332` and Y `-92..248`; C62C widens only its measured
16.16 upper-X and vertical bands. Values outside those bounds retain the guest
reject. Raw Y `192` still requires slot-specific provenance because it is also
the game's hidden-sprite value.

Session `20260828_150103` showed the remaining left-edge loss at C6FA: signed
negative `x+32` values were cast to unsigned. Expanded now admits its added
signed lower band through `-60px`. Palace margin leakage was provider-backed,
so its exact authenticated table now uses separate BG1/BG2/BG3 connected-room
masks seeded from the native canvas, treating `0x026` as a barrier. The mask
resets on scroll, auth epoch, or table invalidation; equal-scroll fields do not
use it.

Session `20260828_153341` replaced the Palace mask's full-table flood with the
exact Expanded margin envelope: four horizontal and three vertical metatile
cells from native seeds. Authenticated Expanded fields also render unambiguous
bottom-margin OBJ Y `160..199` without provenance; raw `192` remains gated.

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
4. Keep guest object presentation and PPU clipping unchanged. The exact
   conditional-branch hook is configured only for the ten reviewed
   viewport-reject PCs and is enabled only for strict Expanded 360x240 mode;
   it is global across scene classes, preserves the original decision in all
   other modes/unknown PCs, and does not rewrite immediates or literals.
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
5. **Object coverage:** after corpus regeneration and playable rebuild,
   manually verify NPCs, doors, particles, shadows, gap jumps, collision, and
   room edges. Confirm all ten reviewed viewport branches bypass only in strict
   360x240 mode; manual acceptance remains pending.
6. **Product surface:** keep the three fixed modes with 240x160 default;
   strict/capture remain Native; manual root-launcher acceptance remains.

Public tests should cover width clamping, literal 240 path, center-crop equality,
scene-policy fallback, save-state invalidation, per-layer suppression, OBJ
signed-X boundaries, affine wrapping, and nonuniform window fail-closed behavior
using synthetic fixtures. Manual gameplay remains the user's responsibility.

## Main risks

- Town tile rings alias plausible-looking but wrong scenery.
- Unreviewed guest culling, despawn logic, or OAM exhaustion can still leave
  empty margins or pop-in.
- Screen-space BG/OBJ UI repeats or shifts incorrectly.
- Affine effects extrapolate mathematically but reveal unauthored edges.
- WIN0/WIN1/OBJ-window and blend masks leak scenery during transitions.
- World/room boundary wrapping can expose unrelated map data.
- Save-state loads can reuse stale host margin caches.
- Widescreen currently loses native supersampling and frame interpolation.

Every unknown address, threshold, map structure, or scene flag remains
**TODO-EVIDENCE** until measured on the supported ROM.
