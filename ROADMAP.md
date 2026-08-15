# Roadmap Status

Update this file only when an acceptance gate has evidence.

> Current work is GS-011 (transient RAM code images), which has no checkbox
> below because it is a mechanism rather than a gate. No box changed state as a
> result of it. See `docs/GS011_TRANSIENT_IMAGES.md` for the live boundary;
> the GS-010-era "first architectural divergence" framing elsewhere in `docs/`
> is superseded. The relocatable RAM code pool is solved with
> position-independent images, closing the combinatorial per-base registration
> problem. **The FULLY_STATIC claim is frame- and track-bounded, and now
> reaches 10,800 frames on `campaign`.** A non-LTO build (140,228,928 bytes,
> 2026-08-07 10:46) completes strict-static, self-heal-off, cache-load-off at
> 5,400 frames on both `campaign` (36,298,166 trace events) and no-input
> (33,909,353), and at **10,800 frames on `campaign`** (84,942,401 trace
> events, `dispatch_misses=0`) — the track that previously aborted at
> `Func_a3ef0+0x7c` now completes. Extending `campaign` to 21,600 frames is
> **not** static: it aborts at THUMB `0x08094944` (trace event #96,264,422),
> an ordinary interior entry inside ELF-sized `Func_94820`, and a route other
> than `campaign` will find its own gaps regardless. The relocatable registry
> is now fifteen images and the fixed transient registry nineteen, after a
> runtime-patched-literal-pool defect in `Func_9bb8` was found and fixed (a
> third case of the image-identity rule below) and a fifth EWRAM overlay,
> `rom_78603c`, was registered. **A defect in the image identity model was
> found and fixed**: the game copies one routine at two different lengths, so
> an identity must cover the bytes the writer wrote, not the bytes an ELF
> `STT_FUNC` spans — see `docs/GS011_TRANSIENT_IMAGES.md`. Separately,
> `tools/build_main_toml.py` now derives all 705 linker-emitted interworking
> veneer stubs from the hash-verified ELF/ROM (`scan_veneer_stubs`) instead of
> hand-seeding them one at a time; this derivation is what carried the
> 10,800-frame track to FULLY_STATIC. See `docs/GS011_TRANSIENT_IMAGES.md` for
> the full evidence behind every figure in this paragraph.

## Baseline

- [x] Starter governance and planning pack created.
- [x] Upstream commits pinned.
- [ ] Non-ROM CI green.
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
- [ ] Main generated corpus compiles x86-64.
- [x] Local patched-upstream main corpus compiles x86-64 (pin update pending).
- [x] All 96 overlay corpora compile x86-64 (95 unique code identities).
- [x] Local generic same-PC overlay activation/eviction proof passes (pin update pending).
- [ ] Golden Sun overlay load/identity adapter matches the oracle.

## Execution milestones

- [x] Recompiled BIOS runs.
- [x] First cartridge instruction runs.
- [x] First Golden Sun function identified and runs.
- [x] BIOS handoff CPU and writable-memory state synchronized with mGBA.
- [x] First architectural divergence localized to an exact instruction/read.
- [ ] First VBlank/IRQ matches oracle.
- [ ] First meaningful VRAM/OAM/PAL writes match.
- [ ] Title screen renders.
- [ ] Menu input works.
- [ ] New game starts.
- [ ] Vale vertical slice completes.
- [ ] Early battle completes.
- [ ] Save compatibility round-trips.
- [ ] Full playthrough completes.
- [ ] Strict-static full playthrough completes with zero misses.

## Enhancements

Blocked until strict baseline is healthy.

- [x] Raster-aware 2x scene interpolation (default off; faithful fallback;
  battle 65/120 midpoint frames, world map still safely falls back).
- [x] Enhanced Timing: default-off exact 60/120 Hz host pacing.
- [x] Enhanced Timing: audio resampling ([plan](docs/ENHANCED_TIMING_PLAN.md)).
- [ ] Enhanced Timing: measured dynamic CPU headroom
  ([plan](docs/ENHANCED_TIMING_PLAN.md)) — distinct from the manual overclock
  below; not yet built.
- [x] Guest CPU overclock (manual 1x/2x/4x/8x, default-off, live-toggleable):
  scales only per-instruction execution cost, never halt/idle time or
  DMA/IRQ-added cycles. 1x verified bit-exact across 8.4M fingerprint records
  (20 fields). User-confirmed smooth at 2x/4x in battle; throughput gain not
  yet instrumented (needs a CPU-bound savestate — see
  `docs/SESSION_2026-08-13B.md`). See `docs/CODEX_HANDOFF.md`.
- [x] Display scaling/profile work (faithful raw default; live color profiles;
  verified integer layout).
- [x] Widescreen research ([scope](docs/VISUAL_ENHANCEMENTS_2026-08-13.md));
  Golden Sun implementation remains disabled pending scene evidence.
- [x] Native scene supersampling with faithful per-frame fallback; battle is
  mostly supported, world-map raster effects currently fall back.
- [ ] Enhanced audio shadow path.
- [ ] Mod hook policy.
- [ ] Additional ROM regions.
