# Next task — widescreen/Bilibin manual acceptance

Date: 2026-08-26. Keep dynamic RAM **NOT_STATIC**. Use only the root
`GoldenSunLauncher.exe`; the user performs the manual test.

## Current evidence

- Fixed modes: Native 240x160 (default), Widescreen 288x160, Expanded
  Widescreen 360x240. Expanded uses signed ±60 horizontal/±40 vertical
  margins; strict-static/capture uses Native.
- Regenerated C6F2/C6FE literal hooks are installed. Serial playable build
  passes. B27C/B326 now widen the equal-scroll field bottom edge only with
  measured per-slot OAM provenance.
- Replays `logs/session_20260825_221446.log` and
  `logs/session_20260825_190416.log` cross the former crash window cleanly.
  This is not a behavior-fix claim; the widened pool-LDM probe is diagnostic.
- Session `20260826_152239` contains two user-identified Palace entries with
  the repeated split-scroll signature. The measured Palace class is now a separate
  `AuthorizedMode0SplitScroll` policy requiring a complete clean frame;
  producer completion remains supporting evidence, not runtime gating.
- `gs011_opt` rearms VRAM CPU/DMA metadata within a lifetime-bounded
  authentication-epoch budget, records payload-free EWRAM table DMA
  descriptors separately from CPU stores, and labels cull counts by epoch.
  A bounded `[wide-field-producer]` writer-PC/address-range summary now
  complements the 64-record detail cap so raw-table producers remain visible.
  Generated fast-path EWRAM stores reach these diagnostics only when this
  toggle is on; both EWRAM observer paths remain disabled otherwise. Authored
  cells remain unwired until producer coverage proves ownership. The executed
  `0x0800B322` horizontal route was already widened; the three B388 routes had
  zero calls, so no speculative cull override was added. Mode-2/overworld
 OBJ-X and unproven OBJ-Y routes remain fail-closed. Open visual evidence remains
 Bilibin bottom garbage, NPC/object/nameplate culling, and McCoy Palace
 viewport expansion.
- Session `20260826_182912` reports millions of BG1/BG2 raw-unavailable
  provider samples. The upstream contract and synthetic PPU test prove those
  results skip the wrapped regular-BG entry; the log has no final-pixel source
  attribution, so the yellow/right leakage is not yet attributable to those
  samples. Do not broaden suppression or alter the wrapped-entry contract;
  obtain a payload-free margin source count (`replace`/`keep-wrapped`/
  `unavailable->backdrop`) first.

## Manual action

1. In the root launcher, enable **Widescreen diagnostics (WIDE-01)**, then
   load slot 1 with cheats/mods disabled. This session-only opt-in passes
   `GBARECOMP_VRAM_MAP_TRACE=1` to the playable build; it resets unchecked on
   every launcher start and passes `0` when unchecked.
2. Select Expanded Widescreen.
3. Enter McCoy Palace, walk the interior, then exit/re-enter once.
4. Return the newest session ID/log and exact result. This acceptance capture
   should contain epoch-local `[vram-map-cpu]` /
   `[vram-map-dma]`, separate `[wide-field-table]` /
   `[wide-field-table-dma]`, `[wide-field-producer]`, per-layer
   `[wide-field-provider]`, `[wide-auth-epoch]`, `[wide-cull]`,
   `[wide-literal]`, `[wide-margin]`/`[wide-margin-summary]`, and
   `[oam-shadow]`/`[oam-dma]` summary records. No extra environment setup is
   needed.

## Required marker interpretation

- `[wide-margin]` has `pixels`, side totals `left`/`right`/`top`/`bottom`,
  provider triples for `bg0..bg3` in `replace/keep-wrapped/unavailable`
  order, whole-margin `final_bg0..final_bg3` triples in
  `wrapped/provider-replace/provider-keep-wrapped` order, and final
  `final_obj`, `final_backdrop`, `final_pillarbox`, and `final_forced_blank`
  counts. Its `left_bg*`/`right_bg*` triples are the same source counts split
  by horizontal side; `right_obj`/`right_backdrop`/`right_pillarbox` complete
  the right-side non-BG categories. `reason=frame` is a bounded sample;
  `reason=summary` is the cumulative exit report.
- `[wide-literal]` reports one line for each reviewed field-list literal,
  `pc=0x0800C6F2` (X upper) and `pc=0x0800C6FE` (Y lower). `calls` is hook
  invocation count; `overrides` is accepted expanded-view substitution;
  `gate_rejects` means inactive/unauthorized/null-output or zero-margin; and
  `compare_rejects` means the original literal did not match the reviewed
  value. `original=lo..hi` is the observed literal range, not guest payload.
- `[oam-shadow]`/`[oam-shadow-dma]` identify bounded writer/DMA slot ranges and
  overwrite counts; `[oam-dma]` is a post-copy summary, with
  `used_slots`/`visible_slots`/`nonzero_slots` and raw-coordinate counters.
  None of these markers prints guest attribute, tile, or table bytes.

The minimal next capture is therefore: enable **Widescreen diagnostics
(WIDE-01)** in the root launcher, keep cheats/mods disabled, load slot 1,
select Expanded Widescreen, enter and walk McCoy Palace, exit and re-enter
once, then stop and return the newest session ID plus log path. Include the
first `[wide-auth-epoch]` through the re-entry `[wide-margin]`,
`[wide-literal]`, and OAM records; do not launch `GoldenSunRecomp.exe`
directly and do not claim visual acceptance from the log alone.

If the crash recurs, include the `[pool-ldm-crc] reason=restore` line and
bounded `pool-ldm` write rings, including fixed slots
`0x03007E24/0x03007E28`. Do not patch guest state before the writer is proven.

After this acceptance, route MP2K work to [`features/MP2K.md`](features/MP2K.md).
