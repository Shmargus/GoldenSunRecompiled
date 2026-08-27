# WIDE-01 — Widescreen, culling, and scene acceptance

Status: open; fixed modes are implemented, Palace map authorization is
implemented, and manual acceptance remains pending.

## Current evidence

Native 240x160, 288x160, and Expanded 360x240 modes exist. Measured world-map
and Mode-0 field-layer expansion is active. C6F2/C6FE hooks remain installed;
the equal-scroll field now widens the measured B27C/B326 bottom limit with
per-slot OAM provenance. McCoy Palace has a separate `AuthorizedMode0SplitScroll`
class: exact BG CNT `0x0709/0x060A/0x0503`, effective-equal HOFS with raw
BG2-BG1 `0x0200`, and raw VOFS `BG1-BG2=0x0180`, `BG2=BG3`. Runtime use waits
for a complete clean frame and resets on any invalid/mixed row; its provider
uses each layer's raw scroll and does not use BG3 as a BG1/BG2 boundary.
Manual Palace/Bilibin acceptance remains pending; this change did not launch
gameplay.

The bounded diagnostic trace now also emits a payload-free per-epoch writer-PC
histogram and address range, because the 64 detailed CPU-store records were
consumed by map writes before raw-table producers appeared. Generated fast-path
EWRAM stores feed that seam only when the launcher diagnostics toggle is on;
ordinary/native runs keep both observers null. Authored cells remain diagnostic-
only and are not wired into the provider without producer coverage. The
producer summary initializes `last_frame` from its first write instead of
retaining the `UINT64_MAX` sentinel. The
executed B322 horizontal cull is widened for the authenticated equal-scroll and
stable Palace map classes; B27C/B326 use the measured bottom limit only with fresh per-slot
provenance. All B388 routes remain unchanged and had zero calls. OBJ-X
authorization remains restricted to those authenticated Mode-0 B322 routes;
Mode-2/overworld OBJ and unproven OBJ-Y paths remain fail-closed. Full evidence is in
[`features/WIDESCREEN.md`](../features/WIDESCREEN.md).

The runner now emits payload-free `[wide-field-provider]` totals per auth epoch
and BG layer, separating successful margin replacements from precondition,
cross-layer boundary, raw-sentinel, and other lookup failures. This is
diagnostic-only and is intended to distinguish atlas-source loss from a
presentation/compositor defect in the next manual Palace capture.

The expanded compositor now emits bounded `[wide-margin]` samples and a
`[wide-margin-summary]` containing final layer/source counts for left/right,
top/bottom, provider replacement, explicit keep-wrapped, object, backdrop,
pillarbox, and forced-blank outcomes. C6F2/C6FE literal hooks separately report
invocation, gate-reject, compare-reject, and accepted-override counts. The
opt-in OAM trace reports shadow slot writes/overwrites and post-DMA OAM
`used_slots`/visibility totals without retaining attribute payloads. These
markers are observational only; no behavior change is justified from them
without a new capture.

The two requested non-OAM diagnostic requirements are implemented and covered
by focused tests. `WsMarginDiagnostics::provider_results[bg][result]` counts
provider `replace`, `keep-wrapped`, and `unavailable` results; its
`final_selected[layer][source]` matrix counts the actual final margin source,
and `horizontal_final_selected[left|right][layer][source]` separates the two
horizontal sides. `ppu_smoke_tests` exercises replace, keep-wrapped,
unavailable-to-backdrop, and the native no-callback path. The C6F2/C6FE
`GoldenSunLiteralTraceStats` counters are shared with the runner through
`widescreen_literal_trace.h`; `widescreen_literal_trace_test` verifies exact PC
indexing, call counts, accepted overrides, gate/compare rejects, and original
literal ranges for both hooks.

Build verification: the existing incremental command
`ninja -C build/gs011_opt GoldenSunRecomp -j 14` completed without errors after
the diagnostics changes. It produced the current
`build/gs011_opt/GoldenSunRecomp.exe` (878659736 bytes); a subsequent identical
build reported `no work to do`. No gameplay was launched during this build.

Session `20260826_182912` records BG1 `3,802,475` replacements and
`9,637,525` raw-unavailable samples, and BG2 `5,415,276` replacements and
`8,024,724` raw-unavailable samples. The upstream PPU contract is explicit:
`kWsTilemapUnavailable` (0) is not a wrapped-entry fallback; the wide regular-BG
compositor skips the sample unless the provider returns
`kWsTilemapKeepWrapped` (2). A focused synthetic PPU case with an opaque
wrapped tile 0 confirms that an unavailable horizontal margin shows only the
backdrop and leaves the native center unchanged. The session totals therefore
do not prove that the reported yellow pixels came from those unavailable
samples. No safe local or upstream behavior change is justified until a
payload-free final-margin source diagnostic identifies the selected layer and
whether it was `replace`, `keep-wrapped`, or backdrop; suppressing all BG1/BG2
replacements could hide valid authored terrain.

Generated `gs011_opt` disassembly shows that the B322/B326 block first computes
`r6 = r1 - r3`, then tests `r4` at B322 and the derived `r6` against `159` at
B326. Separate unexecuted routes establish `r2=32` at B3CA, establish
`r1=136` and double it to `272` at B3D6/B3D8, and directly compare `r3` with
`208` at B3EA. The B27C/B326 accepted paths write the low byte of that Y to
the measured OAM-shadow slots; invalid attr0 addresses remain fail-closed.
B388 is unchanged because the latest capture executed it zero times.

## Session `20260826_212025` evidence

The capture completed normally without a crash at frame `521045`, with
`frames_presented=1244` and `failed=0`. It remains **NOT_STATIC**:
`dispatch_misses=10`, `interpreted_insns=447`, `dynamic_ram_pcs=19`, and
`dynamic_ram_dispatches=28`.

The earliest proven yellow/right leakage stage is provider-backed margin
replacement: the right margin is `14,400` pixels, selected as
`right_bg1=0/3850/0`, `right_bg2=0/4178/0`, and `right_bg3=0/6372/0`, with no
wrapped/keep-wrapped selection. Restore starts with `auth_cells=16384`, but
ownership is boot/restored rather than current-epoch ownership and the provider
passes `authored=nullptr`. The smallest candidate fix is to gate `Replace` on
fresh current-auth-epoch ownership and return `Unavailable` until established.

Object evidence: cull/literal routes match and are mostly overridden
(`B27C/B322/B326` calls/overrides `4085/4058`, `4773/4739`, `4773/4739`;
`C6F2/C6FE` `7035/6965` and `5519/5467`, with zero compare rejects); B388 had
zero calls. OAM was populated (`used_slots=128`, `visible_slots=128`,
`nonzero_slots=128`), but shadow provenance covered only slots `0..88`; slots
`89..127` remain unproven. The missing NPC cause therefore remains unproven,
and no OAM behavior change is authorized.

## Next action

Follow [`NEXT_TASK.md`](../NEXT_TASK.md): use the root launcher diagnostics
toggle with cheats/mods disabled, enter/walk/exit/re-enter Palace once, and
return that session log. The capture should verify the split-scroll policy,
per-layer final margin sources, C6F2/C6FE literal decisions, and object
behavior; no manual success is claimed.

## Closure condition

Native center equality, all fixed-mode scene policies, Bilibin transitions,
objects/nameplates, Palace viewport, and vertical OBJ behavior are manually
accepted with measured evidence; unknown scenes remain fail-closed.
