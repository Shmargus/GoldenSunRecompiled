# Roadmap Status

Update this file only when an acceptance gate has evidence. Current state in
prose is `docs/STATUS.md`; the immediate task is `docs/NEXT_TASK.md`; open work
is indexed in `docs/ACTIVE_ISSUES.md` and detailed under `docs/issues/`.
Feature tracks are under `docs/features/`; wishlist and deferred work live in
`docs/BACKLOG.md` and `docs/PARKED.md`.

Two kinds of evidence appear below and they are not interchangeable:

- **oracle/measured** — a recorded comparison, trace or benchmark;
- **play-proven** — the user reached it and it behaved correctly in real play.

Play-proven is real evidence, but it is not a recorded oracle diff. Boxes
resting on it are marked.

## Baseline

- [x] Starter governance and planning pack created.
- [x] Upstream commits pinned.
- [ ] Non-ROM CI green — `heal_gate_tests` is the one known non-pass left.
  `public_repository_audit` was failing permanently because it audited the
  gitignored `private/` tree; it now audits repository membership instead and
  passes.
- [x] Exact ROM verified locally.
- [x] Exact BIOS verified locally.

## Disassembly and metadata

- [x] `gsret/goldensun` builds reproducibly.
- [x] Main ELF/map inventory captured.
- [x] All overlay ELF/map outputs inventoried.
- [x] Symbol import schema and synthetic validation fixtures completed.
- [x] Main symbols imported deterministically.
- [x] Overlay symbols imported deterministically.
- [x] ARM/THUMB modes validated.
- [x] Data/code ambiguities reported.

## Recompiler integration

- [x] Exact-ROM unseeded discovery baseline classified deterministically.
- [ ] Main ROM TOML reviewed.
- [x] ELF-backed IWRAM code-copy source/destination/size reviewed.
- [ ] Overlay manifest reviewed.
- [ ] Main generated corpus compiles x86-64 against the public upstream pin.
- [x] Local patched-upstream main corpus compiles x86-64 (pin update pending).
- [x] All 96 overlay corpora compile x86-64 (95 unique code identities).
- [x] Local generic same-PC overlay activation/eviction proof passes (pin
  update pending).
- [x] Golden Sun overlay load/identity adapter works in play. Not re-verified
  against the oracle as a recorded diff.

## Execution milestones

- [x] Recompiled BIOS runs.
- [x] First cartridge instruction runs.
- [x] First Golden Sun function identified and runs.
- [x] BIOS handoff CPU and writable-memory state synchronized with mGBA.
- [x] First architectural divergence localized to an exact instruction/read.
- [x] VBlank/IRQ, VRAM/OAM/PAL writes drive a correct picture — play-proven
  across hours of real play, not a recorded per-event oracle diff.
- [x] Title screen renders.
- [x] Menu input works.
- [x] New game starts.
- [x] Vale vertical slice completes (play-proven).
- [x] Early battle completes (play-proven).
- [ ] Save compatibility round-trips — saving and loading work in play; a
  round-trip against original-hardware/emulator save data is unproven.
- [ ] Full playthrough completes.
- [ ] Strict-static full playthrough completes with zero misses. Bounded
  strict-static tracks pass; see `docs/STATUS.md`.

## Enhancements

All are opt-in and default-off. The faithful path stays the default.

- [x] Raster-aware 2x scene interpolation (battle midpoints; world map falls
  back safely).
- [x] Enhanced Timing: exact 60/120 Hz host pacing.
- [x] Enhanced Timing: audio resampling
  ([plan](docs/features/ENHANCED_TIMING.md)).
- [ ] Enhanced Timing: measured dynamic CPU headroom
  ([plan](docs/features/ENHANCED_TIMING.md)). Distinct from the manual overclock
  below; not built.
- [x] Guest CPU overclock (manual 1x/2x/4x/8x, live-toggleable): scales only
  per-instruction execution cost, never halt/idle time or DMA/IRQ-added
  cycles. 1x verified bit-exact across 8.4M fingerprint records. Smooth at
  2x/4x in play; throughput gain is not instrumented.
- [x] Display scaling and live color profiles (faithful raw default;
  verified integer layout).
- [x] Native scene supersampling with faithful per-frame fallback. Battle is
  mostly supported; world-map raster effects fall back.
- [x] Temporal blend / "LCD ghosting", and soft-filter shimmer reduction.
- [x] Hotkeys bindable to keyboard or controller; Turbo in Held and Toggle
  forms.
- [ ] Widescreen. Three optional fixed modes are implemented: Native 240x160
  (default), Widescreen 288x160, and Expanded Widescreen 360x240 with true
  ±60 horizontal/±40 vertical signed margins. Strict-static and framebuffer
  capture force Native. State1 root-launcher capture is 1080x720 at 3x; native
  center and the 288x160 slice are byte-identical
  (`channel_diffs=0`, `max_delta=0`). Focused tests and the full serial
  playable build pass. Manual multi-scene acceptance remains pending, and
  vertical guest OBJ cull coverage is not proven beyond resident OAM; see
  WIDE-01 in `docs/ACTIVE_ISSUES.md`.
- [ ] Enhanced audio shadow path. An opt-in verified MP2K wall mixer exists;
  manual acceptance is still open.
- [ ] Turbo-decoupled native audio. Current experimental scope is launcher-
  gated MP2K music only, bounded to 2x-4x; PSG/FIFO and normal-speed SFX are
  future work. Canonical guest audio remains the oracle, and unsupported,
  reverb, queue/ring failure, or uncapped mode falls back to canonical coupled
  audio; explicit MuteDuringTurbo still mutes.
- [ ] Mod hook policy.
- [ ] Custom items / weapons (recolour, new unleash, new entries). Not yet
  investigated: need the item and unleash tables, and the width of the item
  ID field (a 1-byte ID caps the roster at 256 regardless of added space).
- [ ] Custom Psynergy. Same shape of problem: locate the Psynergy table and
  its ID width before promising new entries.
- [ ] Additional ROM regions.
