# Current status

Updated 2026-08-26. Faithful 240x160 output and original timing remain the
default; enhancements are opt-in.

## Milestone

Golden Sun is playable through the early game. The current baseline still has
dynamic RAM marked **NOT_STATIC**. The battle/menu compile-stutter regression
is fixed; residual small hitches are **PERF-08**. **CORE-01** is parked; see
[`PARKED.md`](PARKED.md).

## Build and launch

- Playable build: `build/gs011_opt`, selected by the repository-root
  `GoldenSunLauncher.exe`.
- Launch only `GoldenSunLauncher.exe`; the user performs gameplay tests.
- Normal non-LTO builds are preferred. Strict-static and capture runs force
  Native 240x160.
- Latest serial playable build passed. Latest validated-ROM replay
  `logs/session_20260825_221446.log` crossed the former Bilibin crash window
  through frame `522170`; this does not prove a behavior fix.

## Acceptance boundary

Current manual boundary is widescreen/Bilibin. Session `20260826_152239`
provided two user-identified Palace entries and the measured split-scroll
evidence now implemented as a separate
`AuthorizedMode0SplitScroll` Palace map class; runtime authorization still
requires a complete clean frame and fails closed on invalid/mixed rows. The
latest `gs011_opt` rearms bounded VRAM metadata per authentication epoch,
separates CPU/DMA EWRAM-table provenance, reports culls per epoch, and now
emits a bounded payload-free `[wide-field-producer]` PC/range summary so raw
producers are not hidden by the 64-record detail cap. Generated fast-path
EWRAM stores reach this diagnostic
seam only when the launcher toggle is enabled; authored cells remain unwired.
The executed B322 horizontal cull and authenticated Mode-0 OBJ-X route cover
the equal-scroll and stable Palace classes; B27C/B326 widen those same classes
with fresh OAM-slot provenance. B388 and Mode-2 OBJ remain stock/fail-closed. The
focused widescreen test and rebuilt `gs011_opt` pass. Re-run
the single Palace capture through the root launcher with
[`NEXT_TASK.md`](NEXT_TASK.md).

CRASH-03 provenance now arms at the savestate restore boundary, clears stale
host-only rings, and emits a payload-free `reason=restore` CRC before resumed
guest execution. The focused synthetic test and rebuilt `gs011_opt` pass; this
is diagnostic-only and the poison writer remains unproven.

Open work is indexed in [`ACTIVE_ISSUES.md`](ACTIVE_ISSUES.md). Load exactly
the linked issue or feature file for the task. Do not use
[`history/`](history/) as current guidance.

## Protected-data gate

ROM SHA-1 must be `5c4695205413df7db52b9a184815a07783999971`. Never commit or
expose ROM/BIOS bytes, saves, private traces, screenshots, or generated files
containing substantial protected data.
