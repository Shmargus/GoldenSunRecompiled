# Technical handoff — state as of 2026-08-09

## 2026-08-13 normal-speed presentation pacing fix

Unstable visible FPS was not throughput, VSync, or Enhanced Timing. On the
launcher's `GBARECOMP_PRESENT_IN_PLACE=0` path, `SDL_RenderPresent` ran before
the pacer wait. Callback exits were regular, but guest/render variation moved
the actual picture within each period. The 1x loop now waits immediately before
the visible present; cycles, audio, target rates, Turbo, 2x interpolation, and
the PIP/IRQ path are unchanged.

State-9, faithful timing, 1800 windowed frames on the 120 Hz display:

- present-gap standard deviation: 678.24 us -> 16.61 us;
- p1/p99: 14.571/18.627 ms -> 16.715/16.766 ms;
- gaps over 18 ms: 65 -> 0;
- mean stayed 16.7422 ms (59.7275 Hz); DWM refresh delta stayed 1.

Enhanced Timing, 600 frames: mean 16.6662 ms, standard deviation 18.48 us.
`frame_timing_tests` passes. Measurement CSVs remain local-only under `local/`.

## Current audio handoff — 2026-08-12

**Native MP2K work is PAUSED.** Full detail, corrected inferences, and the
point-20 finding are in `docs/history/MP2K_NATIVE_AUDIO_HANDOFF.md` — read its "Read
this first" section before touching native audio again. Summary for this
file:

- An architecture review found the native path, as built, would deliver **no
  benefit even if it fully succeeded**: everything published to
  `host_stream_` is quantized back to `int8_t` and bit-diffed against the
  guest's own 8-bit ~13.4 kHz output before it can substitute, so native is a
  bit-replica of the driver's requantization, not a higher-quality re-render.
  The stale `mp2k_shadow.h` header comment claiming otherwise has been
  corrected (comment only).
- The residual sample-level divergence chased across points 5-19 (sequence 8,
  passes 2/3/6) was finally isolated (point 20) to sub-sample interpolation
  **fractional-cursor phase**, not seed/gain/accumulator arithmetic. That
  finding is real, but per the review above it targeted only 6% of the
  measured block loss (27/419 rejected blocks); 31% (129 blocks) were
  incomplete-producer losses never investigated.
- Points 5, 6, 7, and their duplicated narrative later in this file (the
  "Point 5" ... "Point 18" paragraphs above `## 1. Where the project is`)
  treated the captured `before=0x00000000` accumulator value as the guest's
  starting accumulator. That is a misread: `before` comes from the probe's
  own zero-initialized shadow array (`probe.previous_voice`), not guest
  state, so it is zero by construction on a block's first store. The true
  prior accumulator is recoverable by inverting the (unsaturated,
  32-bit-modular) accumulate: `actual_before = value - operand_gain *
  operand_sample`. Measured for sequence 8 pass 0: `actual_before =
  0x07C908B1` — neither zero nor the native rolling seed `0x0459061D`. Do not
  reuse the old "accumulator starts from zero" / "not observable" conclusion.
- Resuming native MP2K requires first re-scoping its verification gate to
  compare voice-state identity and structural output rather than 8-bit
  sample equality (`compare_signed_stereo_block` currently forces bit
  equality against the guest's own quantized bytes). Until that re-scoping
  happens, do not resume the points-5-19 line of work or "recapture sequence
  8".
- Kept regardless of direction: the tested integer kernels, `audio_shadow.
  {h,cpp}`, DMA-consumer timestamping, and the measured Camelot boundary
  (dispatch `0x03000828` ARM, 52 cycles before the first accumulator store at
  `0x030008B4`).

State-5/worldmap, 600 frames, final AOT binary (historical measurement, last
taken under the now-paused line of work — not a target to keep chasing):

- native MP2K remains fail-closed/off;
- `bad_waves=0`, producer underruns `0`;
- judged/rejected/incomplete blocks: `290/27/129`;
- first rejected block: `r=.871`, ratio `1.070`, MAE `.021`;
- global host candidate: `r=.646`, ratio `.169`;
- published/released: `263/262`;
- host gaps/resets/underruns: `78/105/105546`;
- route is `NOT_STATIC` with `SelfHealRam=0` because of the known mutable RAM
  mixer fallback.

### Current project direction: performance, not native audio

Focus has moved to emulator performance. The user's actual reported symptom
— slow/dragging audio during normal play — occurs with native audio
**disabled** (native MP2K stays paused/off per the section above), i.e. on
the canonical path. Root cause was never audio: the whole emulator ran below
real-time. Seven fixes have landed this session, in order, each measured:

1. **Build type.** `build/gs011` had an empty `CMAKE_BUILD_TYPE`, so the
   shipping binary had no `-O2`/`-O3`/`-DNDEBUG`. `README.md`,
   `scripts/gs.ps1`, and `.claude/agents/gsrecomp-worker.md` all taught the
   build without a build type; all three fixed. Effect: ~59% -> ~87% of
   real-time.
2. **Overlay ABI gap.** `runtime_idle_backedge` was emitted by codegen into
   self-heal overlay shards but never declared in `GbaOverlayCallbacks`, so
   shard `0x030001E8` failed to compile every session and stayed pinned to
   the interpreter. Added to the ABI (version 3 -> 4) with its inline shim
   and function pointer. Effect: `failed` 1 -> 0; `ram_smc_fallbacks` down
   ~65%.
3. **Relocatable-identity cache.** `relocatable_resident_at` re-read the
   whole image byte-by-byte and re-ran a full SHA-1 on every dispatch. Now
   epoch- and local-word-guarded with a bulk-read fast path. Effect: ~97% of
   dispatches skip the SHA-1 (`hashes=40, cache_hits=1295` over 1800 frames).
4. **Multi-variant healed-code cache — the big one.** `overlay_try_dispatch`
   keyed `(pc, thumb) -> ONE HealedEntry`. Golden Sun's RAM code pool
   assembles code in place, so an address alternates among a small recurring
   variant set (`0x03000820` toggles among exactly 3, all already compiled
   on disk). Every toggle was a CRC mismatch -> dispatch miss -> synchronous
   full-subtree interpretation. Now: incumbent checked first (hot path
   unchanged), then up to 7 alternates, LRU-evicted at
   `kMaxVariantsPerKey=8`; every candidate still passes the same
   unconditional CRC32 over the same byte range before entry. Effect: bridge
   share of guest time 59.3% -> 16.3%; `sum(guest_us)` 12236ms -> 6116ms;
   interpreted instructions 4,104,012 -> 542,259 (-86.8%); frames over
   budget 164 -> 8; `guest_us` p99 28.0ms -> 7.5ms; uncapped headroom 2.23x
   -> 3.81x real-time.
5. **Background warm-load + multi-variant preload.** `warm_load_cache_dir`
   ran synchronously before the frame loop, LoadLibrary'ing every cached DLL
   (~1.7s startup, scaling with accumulated cache size) while installing
   only one variant per key. Now runs on a background thread publishing
   through the existing `s_ready` queue; RAM-backed keys are collected from
   cache filenames plus the `.c` sidecar's recorded end address (hint only —
   per-dispatch CRC32 remains the sole authority); up to 8 variants per key,
   most-recently-modified first. Effect: startup (`--frames 1`, 122-file
   cache) 2.38s -> 0.60s; 1800 frames windowed 33.35s -> 31.11s;
   `warm_loaded` 5 -> ~183; `guest_us` max 318ms -> 92ms.
6. **Bridge-to-native handoff at call boundaries.**
   `runtime_bridge_interpret` consulted `overlay_try_dispatch` only at its
   entry PC. Every `BL`/`BL_suffix`/`BLX_reg` was single-stepped into the
   callee even when that callee already had compiled native code, so one
   first-sighting miss could interpret an entire warm subtree — measured: a
   single dispatch miss at frame 294 interpreted 159,503 instructions and
   blocked the game thread 233ms. The bridge now checks
   `runtime_has_static_entry` and `overlay_query` at each call boundary and
   hands off via `runtime_call_push_return` + `runtime_dispatch`, the same
   idiom `arm_codegen.cpp`'s indirect-BL lowering uses, so entry still goes
   through the existing CRC32-verified path. On return-site mismatch it calls
   `runtime_call_cancel_return` and breaks. (An earlier revision continued
   interpreting instead and leaked a `g_call_return_stack` frame per
   mismatch, overflowing in real gameplay within ~22s; a regression test
   covers this.) Effect: interpreted instructions ~470k -> ~170k; windowed
   1800 frames 31.32s -> 30.95s. The worst single storm was NOT helped — an
   all-new subtree has no warm callees to hand off to.
7. **Env-gated headless cost probe.** `GBARECOMP_FRAME_PHASE` only records
   from two call sites gated on `args.window`, so headless runs produce an
   empty ring. Added `GBARECOMP_COST_PROBE`, off by default and verified
   free when off.

**Current state (measured):**

- Windowed 1800 frames: ~30.95s vs 30.14s target = **~97.4% of real-time**.
  About 0.6s of that is fixed startup cost that does not scale with play
  length, so steady-state gameplay is effectively at real-time.
- **Uncapped headless headroom: 5.73x real-time** (11 warm runs, mean 5258ms
  per 1800 frames, min 5100 / max 5575, 9.0% spread, no outliers). Was 2.23x
  at the start of the session.
- Audio DRC clean: `bridge_underrun=0`, `overflow_drops=0`, `stretch=0`.

**The cost breakdown — this is the map for future work.** 1800 frames
headless, 5566ms wall, via `GBARECOMP_COST_PROBE=1`:

| bucket | ms | % of wall | calls |
|---|---|---|---|
| halt/idle pump loop (WaitForVBlank) | 4152.2 | 74.6% | 1,709 halts / 1,349,926 iterations |
| `runtime_dispatch` (active guest execution) | 1224.5 | 22.0% | 4,961 |
| PPU per-scanline render (overlaps both above) | 1633.0 | 29.3% | 288,008 scanlines |
| timers tick | 243.7 | 4.4% | 2,382,734 |
| audio tick | 206.0 | 3.7% | 2,382,734 |
| bus slow path (MMIO/VRAM/OAM/PAL) | 122.0 | 2.2% | 650,967 (~187ns each) |
| DMA | 13.4 | 0.2% | 289,808 |

Dispatch and halt-pump are containers that include their own share of the
subsystem rows, so the rows do not sum to 100%.

**Ruled OUT by measurement this session — do not re-investigate:**

- The frame pacer. Measured accurate to ~2.2us/frame across 1663 steady
  frames; uses a high-resolution waitable timer plus a 1.2ms spin tail and
  advances `next_ += period_`, so it does not drift or oversleep.
- vsync/refresh mismatch. Display measured at 120.055Hz, not a multiple of
  59.7275Hz, but the cost sits inside `render_us` at 0.38ms/frame and is
  immaterial. `GBARECOMP_NO_VSYNC=1` exists if an A/B is ever wanted.
- Trace-ring events. A/B with `GBARECOMP_TRACE=0` showed no measurable
  difference despite ~1.89M events per 300 frames.
- Synchronous compilation. `overlay_game_thread_compile_ns()` returns 0 by
  design; gcc and tcc both run on a worker thread. Compilation was never on
  the game thread.
- The bus slow path. Previously flagged in an architecture review as a
  plausible-but-unquantified cost; now measured at 2.2% of wall across
  650,967 calls (see cost table above). It is minor; do not pursue it.

**NEXT STEP — state this as THE next step: the halt/idle pump loop, at
74.6% of headless wall time.** ~790 loop iterations per halt period, 1.35M
iterations across only 1,709 halts. When the guest halts waiting for VBlank
the runtime advances in small chunks rather than jumping directly to the
next scheduled event. Making the halt path compute the time to the next
event and skip straight to it should recover most of this. It is by far the
biggest remaining lever and nothing else is close. **Not yet attempted.** Do
not re-chase the pacer, vsync, trace ring, synchronous compilation, or the
bus slow path — all five are ruled out above.

**User-reported symptom, unresolved.** From real play on 2026-08-13
(canonical audio, native MP2K off): **battle lags, specifically on attacks
and VFX spawns, and it is ALWAYS slow, not just the first time.** "Always"
rules out cold-discovery self-heal, which only costs once per variant. A
recurring cost implies the executed code is new every time — consistent
with Golden Sun's sprite blitter pool, which assembles blit code from
per-effect parameters rather than copying a fixed body (see
`src/runner_main.cpp` ~1121-1136). Under that pattern the healed-code cache
cannot help: each variant is compiled, used once, and never seen again, so
both the compile and the interpreter bridge are paid.

Agreed candidate fix, on the list but **NOT started**: a native sprite
blitter — work out the builder's rules and implement one native
parameterized blitter covering all cases, instead of compiling endless
one-shot variants. Cheaper alternative: detect high-variant-churn addresses
and stop attempting to compile them.

**This is unproven.** No measurement of an actual battle attack has been
taken. Before building anything, capture a battle-attack frame and confirm
the burn is the blitter pool rather than PPU/sprite load, DMA, or sheer
sprite count.

**2026-08-13 measured follow-up and partial fix.** A real 9,307-frame play
session recorded 60 misses, 63 successful heals, 3,031,694 interpreted
instructions, and 2,876 bridge entries. 76% of bridge entries came from the
generated `0x03006000..0x03006fff` battle-code pool. PPU cost averaged 1.276
ms/frame; interpolation and Enhanced Timing were off. Identical
`(mode,length,CRC)` bodies appeared at 14, 6, 5, 3, and 2 distinct PCs.

Upstream self-heal ABI v5 now emits RAM overlays position-independently. A
completed body may bind at another RAM address only after same mode, proven
extent, CRC, and exact bytes match. Native calls set the live image base and
restore the prior base afterward. Mutation, wrong-mode, and wrong-length cases
fail closed. Candidate matching does no finder, emitter, or compiler work on
the game thread. ARM/THUMB relocation, nested base restoration, stale mutation,
and automatic exact-copy reuse are synthetic-tested. `ram_heal_tests` passes
in 25.6 s; relocatable-image, bus, DMA, PPU, frame-timing, and all 86 public
Python tests pass.

ABI v5 deliberately invalidates ABI v4 overlay DLLs, so old RAM cache entries
rebuild once. Known slots 1..7 were prewarmed. New ABI-v5 RAM compiles now
atomically persist an ignored local `.pic` sidecar containing schema/ABI/mode/
extent/CRC plus the exact function bytes. Warm loading validates every field,
bounds, alignment, CRC, and exact EOF before retaining those bytes, enabling
the same exact cross-address reuse after restart. Missing, corrupt, oversized,
truncated, or trailing metadata is ignored; the address-keyed cache still
works. Cache bytes remain local protected material and must never be committed,
packaged, logged, or uploaded. Truly new content may still compile once; a
native parameterized blitter remains a future option if another measured
battle still churns.

Possibly related, found while fixing a flaky test: `s_inflight` in
`overlay_request_compile` is keyed by `(pc, thumb)` only, so a second
variant's compile request for the same PC is dropped while the first is
still in flight. Self-correcting in production because
`runtime_mutable_ram_code_miss` re-requests on every dispatch, but it may
serialise compiles at addresses that churn through many variants quickly —
exactly the battle/VFX case. Worth checking.

**User's targets:** solid 100% of real-time with correct audio; a turbo
mode giving a many-times boost (turbo already exists — Tab key /
fast-forward, plus an "uncapped" setting in the config menu's Speed tab);
game logic stays locked to ~59.73Hz — do NOT propose decoupling render from
the guest tick.

**Repo state:** the repo now has commits. Top-level `main` has an initial
commit (246 files, 5MB; ROM/BIOS/build artifacts/debug logs excluded,
`gssplash.jpg` excluded pending provenance confirmation) plus a docs commit.
`gbarecomp/` is on branch `perf/selfheal-multivariant-cache` with commits
covering: multi-variant healed-code cache + overlay ABI fix; background
warm-load + multi-variant preload; bridge call-boundary handoff; headless
cost probe; deterministic warm-load cap test. `gbarecomp/` is currently an
embedded git repo rather than a registered submodule —
`.gitmodules.example` exists but no `.gitmodules`; worth resolving.

**Known open items:**

- The 25-run standalone confirmation loop and the ctest parallel-contention
  pass for the newly-deterministic `ram_heal_tests` had NOT finished when it
  was committed. Re-run before trusting it.
- LTO (`CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`) configures cleanly on this
  MinGW/gcc 16.1.0 toolchain in ~11s and puts `-flto=auto` in the ninja
  file, but a full LTO build was never completed (~20+ min, link never
  produced a non-zero exe). Unassessed. A partial scratch dir
  `build/gs011_lto_probe` was left behind (gitignored, 0-byte exe).
- `warm_load_cache_dir`'s ROM/BIOS path still does a live-byte recompute —
  safe (immutable) but unoptimised.
- `s_failed` still gates by `(pc,thumb)` regardless of content variant.
- `ram_heal_tests` now runs ~23s (was ~5.6s) because of added real-compile
  tests.
- `gbarecomp/` is an embedded git repo, not a registered submodule.
  `.gitmodules.example` exists, no `.gitmodules`.
- A user-mentioned "new toml file" could not be located: no `game.toml`
  anywhere, `config/usa/main.toml` unchanged since Aug 9, no toml modified
  on Aug 13. Ask before assuming.
- `gssplash.jpg` is excluded from git pending provenance confirmation (may
  be ROM-derived artwork).

Validation commands:

```powershell
cmake --build build/gs011 --target GoldenSunRecomp codegen_tests mp2k_shadow_tests audio_drc_tests -j 8
ctest --test-dir build/gs011 -R "^(codegen_tests|mp2k_shadow_tests|audio_drc_tests)$" --output-on-failure
python -m unittest discover -s tests -p "test_*.py"
.\local\probe_fifo.ps1 -Frames 600 -SelfHealRam 0
```

Written for the next agent. Supersedes `docs/history/HANDOFF_CRASH_SCRIPTED_FIGHT.md`
entirely. Read §1 and §7 before touching anything. The previous, append-ordered
version of this file is at `local/TECHNICAL_HANDOFF_prev.md` (gitignored) if you
need the raw session narrative.

## 0. 2026-08-11 issue fixes

The next-issue log showed native MP2K becoming LIVE, then degrading immediately
after a savestate load. The load restored the canonical mixer but left native
voices, verifier windows, native samples, and the host resampler on the old
timeline. Fixed in upstream `gbarecomp`: savestate/reset boundaries now clear
that derived state; host output uses canonical audio until native verification
passes; speculative native samples are not queued; and the host DRC is flushed
on load. New savestates also append a backward-compatible audio extension for
channel 3/4 and wave RAM state, which the old fixed prefix omitted.

The same log exposed unpaced present-in-place callbacks from inside VBlank IRQs.
Pacing is now deferred to the first safe mainline yield. A 120-frame window run
measured 119/119 cadence deltas of one display refresh at 120 Hz; enhanced timing
measured 16.645 ms average frame gaps.

The native-mixer repro had one more root cause: its shadow hook period was latched
before the BIOS changed SOUNDBIAS from 32768 to 65536 Hz, producing 120 hooks per
65536 samples. The first correction updated the rate but used nominal 60 Hz; the
final correction schedules hooks at the measured GBA frame cadence (280896 cycles,
1097.25 samples at 65536 Hz) with fractional accumulation. A boot and save-state
repro now remain on canonical audio; the native shadow is disabled after four
failed probation windows instead of consuming CPU indefinitely or becoming
audible. Two adjacent passing windows are now required before any native handoff.

Golden Sun's mixer is a Camelot-compatible MP2K variant, so the generic shadow is
not yet a verified full replacement. Native audio remains opt-in and fail-closed;
the canonical guest mixer is the accepted path until a variant-specific model is
validated.

The pre-fix state-5 replay confirmed that conclusion: after 600 frames the
native shadow failed closed at correlation 0.18 / level ratio 0.13, while the
guest run closed normally with the same three known IWRAM dispatch misses. The
canonical stream itself is unchanged between the recompiled guest and the
forced interpreter; Direct Sound A/B are the measurable affected buses (state 5
correlation -0.23), not a recompiler-only timing divergence.

The follow-up MP2K fix corrected three concrete issues. FIX voices
(`ctype & 0x08`) now step at `pcm_freq / render_rate`, already-playing voices
seed from a bounds-checked live sample cursor, and invalid cursors fail closed.
The shadow verifier now compares against the final canonical routed/bias-clipped
stereo tap rather than raw FIFO A/B bytes, and native conversion uses the full
signed-16-bit range. Native output still requires two passing windows and is
never substituted while on probation.

The canonical mixer now follows the vendored mGBA GBA path for PSG volume,
SOUNDBIAS clipping, wave/noise DAC polarity, and noise feedback. More
importantly for Golden Sun, an opt-in native-audio session now opens a stereo
canonical fallback while native MP2K is under probation or after it fails. It
preserves the authored Direct Sound A/B routes instead of averaging them into
mono; faithful non-native sessions retain the old mono API. The ROM and BIOS
identity gates were verified during the replay.

The final rebuilt TCP replay loaded state 5, ran 600 frames, and exited
normally. Native probation failed closed at correlation 0.15 / level ratio
0.04; native-on and native-off capture windows were bit-identical. State 8,
which previously reached an unsafe native LIVE state, now failed closed at
correlation 0.01 / level ratio 0.00. States 1, 2, and 4 also failed closed with
invalid-wave evidence; none handed audio to the native ring. Focused validation
passed: audio DRC, bus tests, and all 86 game Python tests.

Verified with:

```powershell
cmake --build build/gs011 --target GoldenSunRecomp
cmake --build build/gs011 --target audio_drc_tests
ctest --test-dir build/gs011 -R audio_drc_tests --output-on-failure
```

The runtime-generated IWRAM path remains interpreter-only by default and is not
fully static. No new RAM identity was added from the issue log.

The follow-up overworld/menu replay measured the source of the short freezes:
with RAM healing disabled, the saved-state route interpreted 3,093,833
instructions and repeatedly bridged the mutable `0x03000000` code image. With
`GBARECOMP_SELFHEAL_RAM=1`, the same 600-frame route dropped to 30,372
interpreted instructions; 197,533 entry CRC checks passed with zero mismatches.
A 1,200-frame replay also exited normally with zero CRC mismatches and zero
failed heals. The launcher and `local/Play Golden Sun.ps1` now opt into this
runtime-verified RAM tier; strict/static acceptance behavior is unchanged, and
the jump-table proposal remains unmerged.

The launcher also sets `GBARECOMP_AUDIO_STEREO=1` while keeping
`GBARECOMP_AUDIO_NATIVE=0`. This selects the canonical GBA mixer with authored
left/right Direct Sound buses, without enabling the unverified native MP2K
shadow. A 1,200-frame window replay measured zero audio underruns, zero bridge
overflow drops, zero stretch events, and zero game-thread compilation time.

The 2026-08-11 MP2K pointer follow-up compared matched fresh-boot traces from
the normal recompiled route and `GBARECOMP_FORCE_INTERP=1`. The first observed
`0x39D0` source-record writes and subsequent live-channel copies matched exactly,
including cycle stamps and `r0-r11`; this is not a recompiler-only pointer
creation divergence. The next owner should trace the shared guest producer/data
population. See `docs/history/MP2K_NATIVE_AUDIO_HANDOFF.md` for measured addresses and
the TCP savestate-load caveat.

The subsequent native-shadow pass established that `0x39D0` is an
archive-relative wave offset rather than a corrupt live pointer. The current
tree resolves it, synchronizes PCM/FIX playback from the live guest `cp` every
frame, and reads Camelot synth controls from payload `wave + 16`. Synthetic
`mp2k_shadow_tests` and all 86 public Python tests pass. A rebuilt state-5 replay
still failed closed after 120 frames (`correlation=0.134`, `level_ratio=0.189`,
`bad_waves=0`). Matched guest/native observation proved the signed sample
addresses and bytes agree; the remaining divergence is the guest fixed-point
interpolation/envelope/pan/reverb accumulator transform near RAM mixer PC
`0x03000744`. Native remains disabled in both launchers until that arithmetic is
matched and the broader acceptance states pass.

A one-time Sol audit then found mismatched pre/post hook boundaries, discarded
23-bit `fw` phase, wrong gain scale, producer/FIFO timeline mismatch, incorrect
GS1 reverb topology, weak rectified verification, discontinuous source
switching, and unsupported voice coverage. Luna implemented the proven state-5
foundations and synthetic tests. Focused audio tests and all 86 public Python
tests pass. Independent state-5 replay for 600 frames was stable and clean but
still failed signed verification (`correlation=-0.001`, `level_ratio=1.068`,
`bad_waves=0`). The gain magnitude is now plausible; block/sample ordering or
exact packed-lane arithmetic remains wrong. Keep native disabled. Next compare
the captured guest and native producer blocks by sequence and first differing
sample before changing mixer arithmetic.

The 2026-08-12 continuation closed most of that gap. The Golden Sun integer
producer now matches state 5 at `dry_r=0.9952`, `dry_ratio=0.9956`,
`post_r=0.9837`, and `post_ratio=1.0170`. Fixes include Q22 PCM interpolation,
FIX gain/cursor semantics, rolling accumulator seeding, exact packed-lane
saturation/finalization, A/B `+0x630` routing, old-ring byte history, and DMA
consumer timestamps. The public host-rate candidate improved to correlation
`0.660` but remains attenuated at ratio `0.171`; the remaining issue is circular
producer-block availability/association, not gain conversion. Native stays off.

The same work proved that the canonical RAM mixer self-patches
`0x03000A5C..0x03000A84`. Generic dirty-word dispatch validation now prevents
stale AOT execution and falls back loudly to interpreter/self-heal. This route
is therefore `NOT_STATIC`; the measured strict state-5 replay recorded 1,402
SMC fallbacks and about 6.16 million interpreted instructions. Five focused
runtime/audio suites passed after the guard/overlay-ABI integration. See
`docs/history/MP2K_NATIVE_AUDIO_HANDOFF.md` for the single next trace and acceptance
order.

The follow-up bounded TCP audio counters isolate the host-rate failure without
changing behavior: state 5/600 judged 427 producer blocks, with the first
incomplete block at sequence 0 and first rejected block at sequence 4. Only
264 blocks published/released; DMA queue max was 4, overwrites 0. The host
resampler observed 78 sequence gaps, reset 107 times, and had 105,547
underruns; canonical/native zero counts were 640/6,566 and held/repeated
counts 180,676/57,543. The follow-up trace identifies the first predicates:
sequence 4 is a complete block rejected by the level-ratio gate (signed
correlation 0.956, ratio 1.650, MAE 0.056), with completion cursor 0x00000FB0
and C70 producer address 0x03000DF8. The earliest incomplete block is sequence
0 at cursor 504: guest samples 352, native samples 76 of 352 expected, on dry
routes 0x02003660/0x02003C90. Complete blocks publish exactly (427 judged -
133 incomplete - 30 rejected = 264 published), and published/releases are
264 with zero overwrites, so this is an initial producer/native epoch gap,
not circular loss or a silent guest block. Point 3 adds a fail-closed startup
alignment gate: partial blocks are recorded but neither compared nor
published, and comparison starts only after a full 352-sample pair. The
synthetic `352/76 -> 352/352` attach regression passes. State 5/600 now opens
the gate (`producer_startup_aligned=true`): 413 complete blocks judged, 187
incomplete observations, 156 rejected observations, and 257 published/
released; DMA overwrites remain 0. Host gaps/resets/underruns are
25/142/51,328, while correlation/ratio remain 0.661/0.165. Alignment alone was
therefore insufficient; point 4 traced sequence 4 to index 0 route A: before the
fix guest was 5 versus native 10 because native old reverb words were
0x585F666C/0x3F4B5254 while canonical guest history was
0x3D3A3B3A/0x322F302F at EWRAM routes 0x02003660/0x02003C90. The smallest
fix re-anchors an observed native ring slot to its canonical old word before
post-reverb finalization. Sequence 4 then measured r=.9954, ratio=.9921,
MAE=.0012; its first residual is index 3 route A, guest 6 versus native 5,
with raw guest (1546,2204), native accumulator 0x0398FEDE, guest dry (-2,7),
native dry (-3,7), and equal old words. The remaining one-LSB dry-lane error
does not justify gain or packed-arithmetic tuning. The synthetic regression
uses the measured stale/canonical history operands. The full post-fix state
5/600 replay remains fail-closed (`native_enabled=false`): 290 complete blocks
judged, 27 rejected, 129 incomplete, 263/262 published/released, host
gaps/resets/underruns 78/105/105,546, and global correlation/ratio 0.646/0.169;
the first rejected sequence moves to 8.

> **Superseded (see "Current audio handoff" above, 2026-08-12):** Points 5-18
> below treat the captured `before=0x00000000` accumulator as the guest's
> starting accumulator and describe the canonical guest intermediate
> accumulator as unobservable. Both are wrong: `before` is read from the
> probe's own zero-initialized shadow array (`probe.previous_voice`), not
> guest state, so it is zero by construction on a block's first store. The
> true prior accumulator is recoverable by inverting the accumulate —
> `actual_before = value - operand_gain * operand_sample`, exact because the
> per-voice accumulate is plain 32-bit modular arithmetic — and was measured
> for sequence 8 pass 0 as `0x07C908B1`, equal to neither zero nor native's
> rolling seed `0x0459061D`. This history is kept for the record; do not
> reuse its "starts from zero" / "not observable" conclusions.

Point 5 adds bounded `producer_diff` data to `audio_state` and traces sequence
8. At cursor 8404 (`0x20D4`), index 0 route A is guest -1 versus native 0;
guest dry is (-17,-16), native dry (-9,-7), native post (0,-1), guest raw
(-223,-468), and native accumulator 0xFC92FBFD. EWRAM reverb starts are
0x02003660/0x02003C90; guest and native old words are equal
0xFEF9FDFF/0x07020608, so reverb history and DMA association are ruled out.
Seven voice cursor/frac/step/sample/gain operands are retained in the bounded
trace. This leaves dry packed accumulation/seed arithmetic as the unresolved
cause; no safe producer fix is proven and fail-closed behavior remains.

Point 6 reconstructs seq8 index 0 from the bounded trace: seed sequence 7,
guest/native seed `0x0459061D`, and seven guest-order packed contributions
(`0x00180012,0x00090009,0x000D000D,0x001A001A,0x00020001,0x000D000D,
0x00110015` with samples `-8,77,-8,12,6,2,-161`) reproduce native
accumulator `0xFC92FBFD` exactly. The synthetic replay passes, ruling out
rolling-seed identity/order and native lane carry as the proven cause. The
canonical guest intermediate accumulator is not observable from final dry
bytes; no root fix is justified without that evidence.

Point 7 adds bounded `canonical_mix` TCP state, latched for the first rejected
complete block. State 5/600 sequence 8 at `0x03000DF8` exposes eight index-0
canonical writes (PCs `0x030008B4`/`0x03000A8C`). The first is
`before=0x00000000`, `after=0x03B905A5`, sample `-52`, gain `0x0014000F`,
while native starts from `0x0459061D` with sample `-8`; canonical samples are
`-52,42,-4,7,5,-1,-70,-11` versus native `-8,77,-8,12,6,2,-161`.
The first divergence is canonical step 0, before reverb/DMA (hook mode is not
carried). This is a different eight-voice invocation, so no behavior fix is
proven; native remains fail-closed.

Point 8 adds identity fields. For sequence 8, the native pre-snapshot is block
8 at cursor `8404`, generation 9, SoundInfo `0x02003050`, 7 active voices, and
vblank fallback PC `0x03007000` at cycle `128764`; canonical writes for that label begin at cycle `2387762`
and end at `2428683`, completing at cursor `9544` / cycle `2443667`. The first
divergence is block association: the retained vblank snapshot is from an older
mixer invocation, confirmed by the 7-versus-8 voice count. Rebinding on the
first canonical store was tested but produced 600 incomplete blocks, 78 bad
waves, and zero judged blocks; it was reverted. No safe fix is proven.

Point 9 adds bounded channel snapshots. Vblank/pre-snapshot packed
status/ctype words are `[17,81,1,2049,81,2113,17,2048]`; the first canonical
write sees `[81,2176,1,2049,81,128,17,128]`, while SoundInfo remains
`0x02003050`. Channel state changed before cycle `2387762`, but no sanctioned
hook timestamps those writes, so the exact latest stable pre-mix boundary and
PC/mode remain unproven. Behavior is unchanged. Next step: bounded channel
field write ring with PC/mode/cycle/address/value for status, ctype, cursor, and
wave fields; do not retry first-write rebinding.

Point 10 adds the 64-entry channel-field ring to TCP `audio_state`. Sequence 8
captured 44 writes, but the earliest is cycle `2393751`, after canonical's
first accumulator write at `2387762`; the final pre-completion write is channel
7 wave pointer at cycle `2434669` (ARM). Preceding mutations are not observed
by the current after-write callback, and `before` is zero on this path. The
latest stable boundary and PC/mode remain unproven; no association fix is safe.
Next step: pre-write observation at the generated bus store boundary.

Point 11 adds a phase-tagged generic-bus pre-write callback with true before
values. The state5 static generated path bypasses those adapters for channel
writes: sequence 8 still has `channel_write_total=0`, so no pre-write event is
available before canonical cycle `2387762`. No snapshot timing change was made.
Next step is wiring the same callback into the generated store observer before
its RAM mutation, retaining PC/mode/cycle.

Point 12 completes generated-store plumbing in the generic runtime helper. The
final `-j8` AOT rebuild and state5/600 replay preserved the baseline: 290
judged, 27 rejected, 129 incomplete, first reject `0.871/1.070/.021`, global
`0.646/0.169`, native fail-closed. The ring reports 70 writes with 64 retained.
Latest before canonical accumulator cycle `2387762` is channel 0 field `0x09`
at `0x020030A9`, PC `0x030006E4`, Thumb, cycle `2387207`, `255 -> 215`; no
writes occur before the accumulator boundary. This is the latest observed
stable boundary but does not prove invocation transition; no association fix
was made.

Point 13 adds per-pass canonical SoundInfo identity to bounded `audio_state`:
channel base, status/type, count/phase/sample cursor, wave pointer and loop
header, frequency, gains, and pass PC/mode/cycle. State5/600 remains unchanged
(290 judged, 27 rejected, 129 incomplete; first reject `0.871/1.070/.021`,
global `0.646/0.169`, native fail-closed). Sequence 8 passes channels 0..7 at
cycles `2387762, 2394016, 2398461, 2404596, 2408992, 2415758, 2422327,
2428683`.

The seven native voices came from the cycle-`128764` snapshot: channels 0..6
were active, channel 7 was inactive (`pre_channel_status[7] = 0x0800`). The
first post-snapshot identity transition is channel 7 activation at cycle
`2380713`, PC `0x080FA148` Thumb (`status 0 -> 0x80`), making canonical pass 7
the extra voice. Channel 5 then changes type/wave at cycles `2382205/2382210`,
and channel 1 at `2384407/2384420`, before their canonical passes; channel 1's
pass at `2394016` is the first canonical pass using a changed instrument. This
proves a stale native snapshot rather than producer loss, but not a safe new
association boundary. No behavior/policy fix was made; only bounded
observability and synthetic identity/timing coverage were added.

Point 14 adds a 16-entry bounded canonical-boundary ring. Each entry captures
the final SoundInfo preparation write, the existing runtime-ring control event,
the first accumulator store, and all per-pass channel identities. Across
sequences 0..11 the final write is consistently PC `0x030006E4` Thumb, channel
0 field `0x09`; the transition is dispatch PC `0x03000828`, 52 cycles before
the first store at PC `0x030008B4` ARM. The final-write-to-store gap is
552--555 cycles. Sequences 8..11 contain all channel identities 0..7 with
stable wave mapping; sequences 0..7 contain seven voices and inactive channel
7. The boundary is therefore proven across multiple blocks. No snapshot
association or audio policy change was made; the synthetic test only asserts
the observed ordering. State5/600 remains 290 judged, 27 rejected, 129
incomplete, first reject `0.871/1.070/.021`, global `0.646/0.169`, native
fail-closed.

Point 16 adds bounded `bad_waves` TCP evidence. The two point-15 observations
are sequence 0 channels 1 and 5, both reason 4: resolved ROM wave headers are
valid, but live cursors are outside their data ranges. Channel 1 resolves wave
`0x08100600`, data `[0x08100610,0x0810334B]`, cursor `0x0810C6ED`; channel 5
resolves wave `0x0811EB2C`, data `[0x0811EB3C,0x0812067F]`, cursor
`0x08102460`. Note-on/type/wave writes precede the boundary, so this is a
transient/unsupported cursor state, not archive-relative decode failure. The
existing cursor predicate remains fail-closed; no relaxation or behavior fix
was made.

Final restored state5/600 is unchanged from point 15: bad waves `2`,
judged/rejected/incomplete `290/27/129`, first reject `0.871/1.070/.021`,
global `.646/.169`, producer underruns `0`, native fail-closed. The
boundary-only experiment (reverted) caused 48 bad waves, 6763 judged blocks,
and global `.471/.162`, proving the boundary is not safe for full live cursor
state. Synthetic tests cover valid archive-relative resolution plus rejected
out-of-range cursors.

Point 15 applies the measured association for the Camelot path: VBlank fallback
snapshots are deferred while the shadow is enabled, and exactly one snapshot is
taken at dispatch PC `0x03000828` ARM before the first accumulator store.
Direct SoundMainRAM snapshots and canonical execution are unchanged. Final
state5/600 shows seq8 snapshot cycle `2387710`, source `3` (boundary), first
store `2387762`; judged/rejected/incomplete remain `290/27/129`,
published/released `263/262`, DMA gaps/resets/underruns `0/0/0`, host
gaps/resets/underruns `78/105/105546`, global correlation/ratio `.646/.169`,
and native remains fail-closed/off. Diagnostics changed: bad waves `0 -> 2`,
first incomplete cursor `504 -> 548` (native samples `76 -> 62`), DMA unmatched
`123058 -> 123030`; no zero-judged or canonical-output regression. Synthetic
coverage checks one boundary snapshot for seq7 (seven voices) and seq8 (eight
voices).

Point 17 extends the bounded pre-write/TCP evidence with write width and all
wave/cursor identity fields. In the first bad record, channel 5 writes type and
wave at cycles 136399/136412, then status at 136823; channel 1 writes them at
134184/134189, then status at 134600. Neither record has a pre-boundary cursor
write. The complete seq8 ring shows cursor writes for channel 1 at 2393812 and
channel 5 at 2415069, after the first canonical accumulator store at 2387762.
Thus the bad cursors are previous-wave state while note-on installs the new
wave; cursor initialization is intentionally completed inside mixing. Moving
the snapshot or relaxing the range predicate is unsafe, so no behavior fix was
made. State5/600 remains bad waves `2`, judged/rejected/incomplete `290/27/129`,
first reject `.871/1.070/.021`, global `.646/.169`, native fail-closed. The
synthetic regression checks bounded write width and field ordering.

Point 18 implements the proven fresh-note cursor rule. After a successful
`note_on`, the render preserves its note-on-derived phase and ignores stale
pre-mix `cp`; an already-playing PCM voice continues to require an in-range
live cursor. State5/600 changes `bad_waves 2 -> 0`; judged/rejected/incomplete
stays `290/27/129`, first reject `.871/1.070/.021`, global `.646/.169`,
producer underruns `0`, and native remains fail-closed/off. Seq8 cursor writes
still occur after the first accumulator store, so later authoritative
snapshots consume the updated live cursor. Synthetic coverage asserts fresh
note-on acceptance with stale `cp` and existing-voice rejection.

---

## 1. Where the project is

The entire prologue plays through the post-title roof transition: intro, boulder
scene, four fights (including a critical hit), and title drop. Throughput went
from 30.5 fps to 264.2 fps on the reference route.

| | session start | now |
|---|---|---|
| reference route (14,000 frames, headless) | 458.7 s / 30.5 fps | **53.0 s / 264.2 fps** |
| binary | 154.8 MB | 198.2 MB |
| full rebuild | hours | ~5 min (LTO not remeasured) |
| overlays registered | 5 | all 96 (95 unique `.text` identities) |
| acceptance tracks | FULLY_STATIC | FULLY_STATIC, invariants unchanged |

The opcode-indexed resolver in §5.3 removed the measured RAM-dispatch bottleneck.
A windowed steady-state check now holds 59.74 fps; fights and the boulder scene
still need a fresh qualitative player check.

---

## 2. What this project is (and is not)

This is a **static recompilation**: the ROM's ARM code is mechanically
translated to C, compiled natively, and run against an emulated GBA (PPU, DMA,
timers, IRQs). Ship of Harkinian is a **decompilation** — readable C, no console
emulation. The ROM-fed-assets half already matches SoH; what differs is what
executes game logic. **Zelda64Recomp is the accurate reference for the target.**

Chosen roadmap (Jimmy, 2026-08-08): **fast recomp → native subsystems →
decompilation.**

For the native-subsystem phase: the N64 has a display list, which is what makes
a native renderer interceptable (RT64). The GBA has none — games write hardware
registers and raw tile data, composed per scanline, so a native GBA renderer
must re-render from hardware state. Golden Sun is unusually suited anyway
because it is ~96% affine mode 2 (sampled, not tile-blitted).

**Do audio before the renderer.** The GBA mixes music in software on the CPU
every frame — the m4a mixer was 2,532 of 4,096 records in one hang trace.

> **Correction (see "Current audio handoff" above, 2026-08-12):** the
> "native mixer removes real per-frame work AND raises quality" rationale
> above is false for the current design and invalidated the original
> "audio first" sequencing decision. Under the `AGENTS.md` canonical-oracle
> rule the guest mixer always runs, so a native mixer only *adds* per-frame
> work rather than removing any; and the native candidate is quantized back
> to `int8_t` before publication, making it a bit-replica of the guest's own
> ~13.4 kHz 8-bit output, not a quality gain above it. Kept for the record
> as the (mistaken) reasoning that originally ordered the roadmap.

---

## 3. The working loop

```powershell
.\scripts\gs.ps1                 # regenerate -> gate -> build -> verify
.\scripts\gs.ps1 -From verify    # just the acceptance tracks
.\scripts\gs.ps1 -To recompile   # regenerate + gate, no build
cmake --build build/gs011 --target GoldenSunRecomp    # ~100 s
```

Reference perf route (deterministic; repeat runs agree within ~1 s):

```powershell
$env:GBARECOMP_HEAL_CACHE='<workspace>\recomp_cache'
$env:GBARECOMP_INPUT_REPLAY='local\play-sessions\20260808-004722.input'
.\build\gs011\GoldenSunRecomp.exe --bios <bios> --rom <rom> `
  --load-state "<private-rom-dir>\Golden Sun.state2" `
  --no-window --frames 14000
```

Host profiler (added this session — the most useful tool in the repo):

```powershell
$env:GBARECOMP_HOST_PROF='local\perf\hostprof.txt'   # then run as above
python tools/symbolize_host_profile.py local\perf\hostprof.txt `
       build\gs011\GoldenSunRecomp.exe --top 40 --group
```

Samples the emulation thread's instruction pointer at ~1 kHz; costs nothing when
the env var is unset. `--group` collapses generated code so runtime overhead
stands out. External profilers are NOT usable: MinGW emits DWARF, Windows
Performance Analyzer wants PDB, so a 29,000-function binary appears as raw
addresses.

---

## 4. Correctness: fixed, and still open

### 4.1 The fight crash — runtime-generated code

`unknown transient code identity at 0x03006220` was never a missing seed.
**Golden Sun generates code at runtime**: `Func_ed408` (ROM 0x080ed408, THUMB)
writes a sprite blitter into IWRAM word by word after a 3-word DMA3 seeds the
prologue from template `Data_edcc4` (0x080EDCC4). About half the template's
words are patched or dropped, so the live image matches no ROM range. Proven by
store-watch writer attribution plus word-level alignment (0.474 exact-word
ratio), not inferred.

No static corpus can ever cover it. `verified_ram_dispatch` in
`src/runner_main.cpp` now aborts as before under `GBARECOMP_STRICT_STATIC`;
otherwise it reports once per PC, counts, and routes through
`overlay_try_dispatch` then `runtime_dispatch_miss`. It dispatches **explicitly**
rather than returning 0, because the main dispatch table contains RAM addresses
from declared code copies — falling through could run a translation built from
different bytes.

### 4.2 Self-heal for RAM code (upstream)

`region_bytes` returned `bytes=rom_ptr(), base=0x08000000` for ANY pc ≥ 0x4000,
so an IWRAM pc computed `pc - base`, wrapped, and found nothing. Fixed: honest
region classification; mutable regions get an owned snapshot taken synchronously
on the game thread; **RAM-backed healed entries record CRC32 of `[pc,end)` and
re-verify at entry**, dropping and re-healing on mismatch. Not theoretical — it
fires in real runs (`ram_crc_mismatches=1`), catching the game re-emitting a
different variant at an address it already ran.

### 4.3 Halt inside the interpreter bridge (upstream)

Second fight froze on a critical hit. `runtime_bridge_interpret` had no halt
handling — the main loop pumps idle cycles while `bus.io().halted()`, the bridge
did not, so a wait-for-VBlank inside a bridged subtree never woke. Fixed by
pumping to the nearest device event exactly as `step_once` does, bounded to one
frame per instruction so a genuine deadlock still reports loudly.

### 4.4 Plaza freeze — a missing overlay

Same class as the fight crash, not a new one. The bridge for `0x02008BBC` ran
200 M instructions without returning. Dumped the live window with
`GBARECOMP_TRANSIENT_DUMP`, matched byte-for-byte against the 96 pinned
decompressed overlays → **rom_784360**, complete match, all 16,084 bytes. All six
observed PCs are exact `STT_FUNC` entries with `$t` mapping symbols in that
overlay's own ELF. Registered `.text`-only (0x02008000..0x0200A854, sha1
`ed3b6e11906e313a8a1aaefaa4e6aef4374c6d15`) — `.data` at 0x0200a854 is
game-written and would make identity depend on mutable state.

All 96 pinned overlays are now proactively registered as 95 unique immutable
`.text` identities. `rom_78dd40` and `rom_78de18` have identical code and
dispatch metadata and deliberately share one corpus. Writable `.data` remains
outside every identity gate.

The state8 post-title freeze was the same class. Live prefix SHA-1s and all six
self-heal CRCs uniquely identified `rom_780898`; its pinned ELF proves the six
observed THUMB PCs. Registered `.text` only at
`0x02008000..0x0200E190` (SHA-1
`ab81b675700afa375749790d84da3ae5ffafe9fa`). The deterministic state8 replay
(`local/play-sessions/20260809-092536.input`) now completes 6,000 frames in
strict mode: **FULLY_STATIC**, zero dispatch misses and zero interpreted
instructions. Two trace-backed IRQ resumes required whole-function reviewed
ranges: `Func_f2ebc` and `Func_f26ec`, split around their ELF `$d` regions.

### 4.5 Self-heal cache path (launcher)

A play session showed `warm_loaded=0` and `cannot write recomp_cache\...` for
every heal, so the whole session ran interpreted — slow text, slow transitions,
a crawling boulder scene, a freeze. The runtime resolves its cache path relative
to the **working directory**; the launcher ran the exe by absolute path but never
set one. Fixed in `local/Play Golden Sun.ps1` (absolute `GBARECOMP_HEAL_CACHE` +
`Set-Location`).

**Diagnostic:** `warm_loaded=` on the startup banner. Healthy is 100-250. A `0`
with a populated `recomp_cache/` means the path is wrong, not that the cache is
cold. **Any performance complaint from such a session is meaningless** — check
this line before believing it.

### 4.6 Corpus duplication — 42x (upstream; the biggest single change)

A `[[resume_range]]` expands to an alias-candidate seed at every instruction
boundary. Mid-function aliasing can only roll a candidate into a NON-candidate
host, so when a range spanned the containing function's own entry, no host
existed and EVERY candidate became a standalone function emitting the whole
shared tail. One 944-byte function → ~450 copies.

Fixed with a promote-and-roll pass in `function_finder.cpp`: the lowest unhosted
candidate of a contiguous run IS the function the range describes; promote it,
clamp its extent against the next real function, roll the rest in as interior
labels. Identical instruction stream.

| | before | after |
|---|---|---|
| overlay rom_77dd1c | 268 MB / 3,488 functions | 7.7 MB / 211 |
| its duplication | 42x | **1.00x** |
| main corpus | 364.7 MB | 178.9 MB (−51%) |

8,394 dispatch entries before and after; 8,397 distinct guest addresses before
and after, now emitted once each. This is what makes registering overlays and
seeding gaps cheap enough to do routinely — before it, adding a resume range
over a 6 KB function would have exploded the corpus.

A new opt-in diagnostic, `GBARECOMP_FINDER_DEBUG_ADDR=<addr>`, prints each
function's addr/mode/source_bias/end/alias-candidate flag after the boundary
clamp. It turned three hours of source reading into a five-minute answer. Keep it.

### 4.7 Open correctness items

* **Sol Sanctum first-battle hang (2026-08-09):** the forced-close status was
  `0xCFFFFFFF` (`STATUS_APPLICATION_HANG`), so the shutdown-only miss fragment
  was not written. The session log plus post-regeneration self-heal cache
  contained 23 PCs. They collapse to 15 ROM THUMB functions and ARM `Func_984`
  in the declared boot code-copy; every PC was checked against the pinned ELF
  mapping run and no THUMB PC is a BL suffix. Added the 16 functions to
  `REVIEWED_RESUME_FUNCTIONS`; regeneration emits 325 reviewed resume ranges
  and 28,858 entries with zero data-range collisions. Public unit tests (86)
  and the repository audit pass. **Runner rebuild/replay remain required:** this
  Codex environment could not execute the configured WinGet Ninja because its
  sandbox helper was missing/denied. Next command: `cmake --build build/gs011
  --target GoldenSunRecomp`, then replay the recorded first encounter with
  self-heal disabled and confirm zero misses.
* **`trace_events` moved on the 5,400-frame tracks** (+1.68 M / +1.13 M) while
  every semantic invariant stayed byte-identical. Explained but unverified:
  interior entries now arrive through a host's resume prologue and emit
  different dispatch records for identical execution.
  `config/usa/acceptance-baseline.json` has NOT been re-baselined — decide
  deliberately.
* No unregistered pinned overlay identities remain. Future RAM misses must still
  be classified: runtime-generated/modified code and invalid interior resumes
  are not made safe merely by registering overlays.
* Mutable IWRAM/EWRAM self-heal overlays are now interpreter-only by default;
  `GBARECOMP_SELFHEAL_RAM=1` explicitly opts into native RAM healing. The
  post-battle hang showed changing images near `0x03006000`/`0x03007a90`, a
  bogus `0xE1D0B458` return, and a busy loop at `0x03000DB8`.
* A second Sol Sanctum freeze (Psynergy Stone room) stopped in the same
  `0x03000000` RAM/DSP image (`pc=0x03000B08`, `vblank_starts=49146`) while
  VBlank continued. The play launcher had been forcing
  `GBARECOMP_PRESENT_IN_PLACE=0`; it now enables present-in-place and pins RAM
  healing off so VBlank resumes preserve the interrupted guest PC/context.
* **Black-screen freeze after overlay replacement (2026-08-09):** the first
  bad branch was `0x02008580` loading `0x1C06FF37`. A shared EWRAM overlay had
  been replaced by direct/DMA stores, but the cached RAM identity still called
  the old native function. `verified_ram_dispatch` now checks the two live
  words at the current RAM PC before using a cached identity; a mismatch forces
  a full identity scan. A 600-frame windowed replay completed with zero misses,
  zero watchdog hangs, and `frames_presented=677` (native audio both off and
  opt-in on).
* The five verified seed candidates from the earlier crawl are applied; the
  fight route reached **zero ROM-space dispatch misses**. Remaining misses on
  that route are all the generated-RAM class, which is correct and not seedable.

---

## 5. Performance

### 5.1 What is measured

Reference route, headless. PPU scanline rendering is 4.6% of frame time; the
translated game code itself is **1.6%** of host time. The game is not the cost.

| step | time | fps |
|---|---|---|
| session start | 458.7 s | 30.5 |
| corpus dedup + inlined flags + EWRAM/IWRAM fast path | 331 s | 42.3 |
| **+ ROM/cartridge fast path** | **222 s** | **63.1** |
| **+ relocatable-RAM opcode index** | **55.6 s** | **251.9** |
| LTO on top (separate build dir) | −14.5% | ~71 |

### 5.2 What worked

1. **Inlined flag helpers** (`runtime_arm.h`) — 6.2%. 39% of translated
   instructions call one; they were defined out-of-line in a different TU from
   every generated caller, so un-inlinable without LTO.
2. **EWRAM/IWRAM inline fast path** — 27%. Region 0x02/0x03 needs none of the
   general path's machinery; masks are exactly `resolve_offset`'s. Writes still
   bump the idle epoch.
3. **ROM/cartridge inline fast path** — 38% (362 s → 224 s). `bus_fast_rom()`
   guarded by `g_fast_rom_ok`, computed in `set_active_bus()` from the bus's own
   `rtc().active()` / `save().eeprom_enabled()` accessors — the same predicates
   the slow path keys on, not assumptions about this cartridge. `off >= 0xD0` is
   strictly wider than the 0xC4..0xC9 RTC window; `g_rom_read32_override` is
   checked live per access; reads only. Afterwards `GbaBus::read32`,
   `bus_read_u32_slow`, `classify`, `resolve_offset` vanish from the top 40
   (were 15.1 / 7.1 / 2.7 / 2.6%).
4. **LTO** — +14.5%, 26-minute link, built in `build/gs011-lto` so it never
   disturbs the working binary. Release builds only; keep the 100 s non-LTO
   build for iteration.

### 5.3 Relocatable-RAM dispatch search — solved

Opt-in `GBARECOMP_RELOCATABLE_PROFILE=1` counters measured 6,467,341 resolver
calls, 139,144 wins and 262 distinct PCs on the reference route. The old
all-entry scan tried 9,273,844,530 candidates. Only 2.92 relocatable images were
resident on average (maximum 5), while the one-base-per-image hints missed
constantly because several placements could be live.

The resolver now indexes immutable-ROM-backed entry points by their live first
opcode. A dispatch binary-searches that index for exact opcode matches, derives
only those candidate bases, then keeps the full image SHA-1 as the authority.
Images without immutable ROM backing retain the exhaustive fallback. Candidate
checks fell to 385,568 and three repeat 14,000-frame runs measured 56.566 s,
55.851 s and **55.583 s** (251.9 fps). Host samples for RAM dispatch fell from
about 63% to about 5%.

Validation on 2026-08-09: `runner_help` and `relocatable_identity_test` pass;
campaign-5400, noinput-5400 and campaign-10800 are FULLY_STATIC and all semantic
invariants match the baseline. The first two tracks still report explained
`trace_events` movement; the baseline was not re-written.

### 5.3.1 Sol Sanctum state4 RAM-identity cost

The slow angel/cloud and inner-Sanctum rooms were not PPU-bound: state4 rendered
about **1.68 ms/frame** (`ppu_render_total_ms=1009.9` over 600 frames), with zero
dispatch misses. Host samples instead put the hot path in SHA-1 image checks
inside `verified_ram_dispatch`. Fixed transient identities now cache their
image result once per reviewed RAM-code write generation; revalidation first
compares the cached bytes, so unrelated writes in a shared code/data page do
not pay SHA-1 again. The generation is invalidated by code-page CPU/DMA writes
and savestate loads, with page masks derived from `kTransientCodeImages`.
The same 600-frame headless state4 run improved from about **19.5 s** to
**6.5 s** with self-heal RAM disabled and interpolation/enhanced timing off.
State2 improved to **11.2 s/600 frames**; its remaining host samples are
`verified_ram_dispatch` (39.7%), SHA-1 (21.2%), and PPU scanline work (~8.7%).
The guest sampler places the cloud-scene hot loop in the `rom_7a8c8c` EWRAM
overlay veneer around `0x0200A3F0..0x0200A3F4`, not in the PPU renderer. No TOML
or static coverage changes were made.

State1's deterministic walk-up replay showed the same resolver failure mode:
600 headless frames took **8.6–9.3 s**, with about 48% of guest samples in the
EWRAM veneer and roughly half of host samples in transient-RAM dispatch/SHA-1
validation. The registry has several compressed overlays sharing the same
`0x02008000` staging slot; the first matching identity was cached, but a page
epoch change made the old path re-hash every earlier non-matching overlay before
reaching that winner. The stale-PC path now revalidates its previously verified
identity first and only performs the full overlapping-candidate scan after that
identity fails. The same replay is **4.7–5.0 s/600 frames** (three repeats),
with 20,725 identity hashes reduced to **39** and verified-dispatch host samples
down to **4.8%**. Final PC, guest steps/cycles, PPU counts, and the 25-test suite
remain identical/pass. This is a cache-path optimization only; no renderer or
static metadata change is involved.

The 2026-08-09 15:52 cloud-room session produced one proposal,
`0x03000828` ARM. This is the already-registered self-modifying DC8 template
entry (not a missing ROM function); it writes the Direct Sound FIFOs
`0x040000A0/A4`. The sparse identity shortcut incorrectly treated those mutable
templates like immutable compressed overlays, leaving the writer on the
interpreter path and making audio sound broken. The shortcut now applies only
to `overlay_*` identities. A 600-frame windowed replay with the recorded input
then reports **zero dispatch misses / zero interpreted instructions** and the
audio probe reports **zero underruns and zero overflow drops**. No TOML merge is
needed for this fragment.

**Two attempts failed before the opcode index and should stay avoided:**
* *Direct-RAM reads inside the identity check* — 0%. The reads were already fast
  after change 2; the cost was never reading.
* *Per-PC resolution cache* — 224 s → 262 s, **17% slower**. These images are
  relocatable (the game moves them), so a cached base is often stale, and a
  stale hit pays a full identity hash *before* the exhaustive scan runs anyway;
  and factoring the inner lambda into a shared function stopped it inlining into
  the scan loop, slowing the miss path too. Reverted; 222 s restored.

### 5.4 Refuted

**Binary size does not drive throughput here.** Cutting 154.8 MB → 54.3 MB
changed throughput by nothing; later, throughput improved 27% while the binary
GREW. The 1.50x → 1.09x regression recorded in earlier sessions should not be
attributed to I-cache pressure without new evidence.

### 5.5 Windowed performance

A 600-frame windowed replay from the reference savestate on the 3440x1440
120 Hz display used the D3D11 renderer with vsync active. The always-on frame
phase ring measured 599 steady-state intervals at **16.7404 ms average / 59.74
fps**, with a **16.773 ms maximum**. Average phase costs were 2.825 ms guest,
0.366 ms render and 13.540 ms pacing; no overlay compilation occurred. The
initial whole-process timing included about 2.5 s of startup/cache loading and
must not be mistaken for per-frame slowdown.

Still not measured: whether the 26-minute LTO link is acceptable in the release
flow, and a human replay of the fights/boulder scene after the RAM resolver fix.

### 5.6 First optional enhancement — 2x scene interpolation

Implemented 2026-08-09 in upstream `gbarecomp` and explicitly exposed by the
Golden Sun runner. F1 → Enhancements offers a default-off experimental 2x mode
on displays reporting at least 119 Hz. Guest logic, audio, timers and input stay
at 59.7275 Hz; the host presents at 119.455 Hz.

The midpoint interpolates BG0..3 scroll and BG2/BG3 affine state from the
captured state of each scanline. Stable non-affine sprites also interpolate for
short moves when their OAM slot, tile, palette, mode, and shape are unchanged.
Large jumps, affine/OBJ-window sprites, and content transitions stay at the
newer endpoint. Each frame-end snapshot is taken at VBlank before VBlank
DMA/IRQ mutation. Before any midpoint is shown, a complete render of the
captured current endpoint must byte-match the faithful per-line latch;
otherwise the runtime logs `DEGRADED` and duplicates the canonical frame.
VRAM/palette or structural display changes also fall back.

Measured windowed smoke on the 120 Hz reference display: 120 guest frames,
118 interpolated midpoints, 2 safe fallbacks, vsync active. The upstream 19-test
suite passes. Golden Sun rebuilt, and campaign-5400, noinput-5400 and
campaign-10800 remain FULLY_STATIC with semantic invariants unchanged while the
enhancement is off. Strict-static runs force the menu option unavailable.

The 2026-08-13 raster-aware revision changed the slot-7 battle sample from
0/120 to 65/120 midpoints. Slot-6 world map remains 0/120 because its
mid-scanout memory changes cannot yet be reconstructed; all 120 frames fall
back exactly. Battle presentation cost measured about 1.35 ms per guest frame
on a 120.001920 Hz display. Two faithful-OFF slot-7 runs produced the same
frame-dump hash. Focused PPU/interpolation/timing tests and all 86 public Python
tests pass.

### 5.7 Exact 60/120 Hz host pacing

Implemented 2026-08-09 upstream and enabled for Golden Sun. F1 → Enhancements
now has a separate default-off `Enhanced Timing (exact 60/120 Hz)` toggle;
`GBARECOMP_ENHANCED_TIMING=1` is the environment equivalent. It changes the
wall cadence to 60.000 guest updates/s and, with 2x interpolation, 120.000
presents/s. Per-frame guest cycles and hardware ordering are untouched. This is
therefore a deliberate 0.456% wall-time speed increase over 59.7275 Hz, not a
CPU multiplier. Strict-static and frame-capture runs force the option off.

`FramePeriodStepper` carries fractional nanoseconds instead of rounding every
deadline independently. Its automated one-hour checks land within 1 ns at both
60 Hz (216,000 ticks) and 120 Hz (432,000 ticks). The host now reports the DWM
rational refresh (SDL integer mode is fallback only); the reference display
measured 120.001920 Hz.

Windowed smoke: 600 guest frames, exactly 1,200 presents, about 120.01 presents/s,
550 interpolated midpoints and 50 safe canonical fallbacks. DWM refresh deltas
were 1 for 1,196/1,199 intervals; three intervals crossed two refreshes. The
full 25-test GSRecomp suite passes. All three strict-static tracks remain
FULLY_STATIC and semantic invariants match the faithful baseline.

Audio resampling is now implemented in the upstream clock-domain bridge. When
Enhanced Timing is enabled, the bridge consumes canonical source frames at
`60 / (16777216 / 280896) = 1.0045623779x`; faithful timing remains `1.0x`.
The ratio is applied under the existing audio mutex, so toggling the option
cannot race the SDL callback and no samples are dropped or duplicated by the
timing conversion. `audio_drc_tests` covers 65536->48000 conversion, faithful
consumption, enhanced consumption, and invalid-ratio fallback. The one-hour
hardware A/V-drift run remains open. Fight slowdown/headroom is a separate
measured step; do not add a global cycle multiplier.

### 5.8 Native compositor, stages 1–2

The opt-in native-renderer path now has two layers. Stage 1 is the safe
present-time fallback: it keeps the canonical 240x160 guest framebuffer and
uploads a crisp nearest-neighbour 1x to 10x texture through accelerated SDL.
Stage 2 (enabled when interpolation and widened view are off) feeds the final
guest register/VRAM/OAM/palette state into the existing BG/OBJ/window/blend
compositor at 1x to 10x output width. Regular BG pixels and sprites stay crisp;
affine BG2/BG3 are sampled per native subpixel instead of duplicating a 240px
result. The canonical path remains the fallback/oracle.

Enable with `GBARECOMP_NATIVE_RENDERER=1` and select the factor with
`GBARECOMP_NATIVE_RENDER_SCALE=1..10`; F1 exposes the same toggle when the game
allows it. Strict-static, frame capture, interpolation, and widened-view runs
continue to use the canonical path. Stage 2 is deliberately final-state
rendering (not a per-scanline hardware snapshot), so visual comparison and
fallback instrumentation are still required before treating it as faithful.
Native scale is independent of window size: enlargement stays crisp nearest;
when the destination is smaller, the native texture switches to linear
downsampling so the extra samples improve small-window quality.
The scene buffer is now consistently two-dimensional: `240*scale` by
`160*scale`. A previous horizontal-only allocation paired with a 2D upload
caused an access violation at 10x; the PPU smoke test covers 1x/2x/4x/10x
buffer bounds. The first implementation also left the first row of each
vertical scale group pointing at the wrong source row; native rows now write
directly to their `y*scale` destination and a vertical-pattern smoke check
covers the fix. Higher factors are intentionally CPU-heavier.
Native scene inputs are copied at VBlank start, before VBlank DMA, and the
native compositor consumes that snapshot. Scrolling registers, tile data,
sprites, and palettes therefore share the canonical scanline framebuffer's
frame boundary instead of being sampled live at presentation time.

### 5.9 Opt-in native MP2K audio

`GBARECOMP_AUDIO_NATIVE=1` opens a stereo host stream and feeds an opt-in native
MP2K renderer. The guest GBA mixer remains the verifier/fallback; native output
is substituted only after the differential verifier proves a complete window.
Golden Sun's driver keeps SoundInfo at the steady `Smsh` marker between ticks,
so the shadow accepts both `kMp2kMagicBase` and transient `kMp2kMagicLive`.
The launcher leaves this disabled by default after an unverified takeover caused
a startup hang; set the variable manually for testing. Strict acceptance never
requests native output.
The same host bridge now applies the exact Enhanced Timing audio clock ratio to
both canonical and native streams; the native MP2K verifier/fallback policy is
unchanged. When probation fails, the stereo stream continues from the
canonical Direct Sound A/B buses rather than duplicating a mono average. The
generic shadow still fails on Golden Sun and must not be forced live; a real
one-hour windowed A/V-drift measurement is still required.

---

## 5.1 Launcher and Camelot-screen compatibility (2026-08-11)

`GoldenSunLauncher.exe` is the click-to-play entry point. `Pick ROM` always
opens the ROM picker; `local/launcher-rom.txt` remembers the last valid path
and uses its folder as the next dialog's starting location. The selected image
is still SHA-1 gated against `5c4695205413df7db52b9a184815a07783999971`.

The windowed Camelot reproduction was a Windows stack overflow
(`0xC00000FD`) with `GBARECOMP_PRESENT_IN_PLACE=1`. The same 1,200-frame run
completed with `GBARECOMP_PRESENT_IN_PLACE=0`; disabling native MP2K audio did
not change the crash, so the launcher and `local/Play Golden Sun.ps1` now use
the stable frame-boundary unwind path and canonical audio by default.

---

## 6. Traps that cost real time

1. **Two checkouts.** `gbarecomp` (upstream) and `GSRecomp` (game) build
   separately. Changing upstream and building upstream does NOT change the game
   binary — rebuild GSRecomp too. Cost a wasted profiling run and, earlier, a
   mixed binary.
2. **Never edit a header mid-build.** One TU picked up a half-finished header
   and the build failed.
3. **Long builds die as background shell tasks.** Launch them detached
   (`Start-Process cmake ... -RedirectStandardOutput`) and poll. Objects
   completed before a kill survive, but with ~19 parallel jobs nothing in flight
   lands, and the link is one step that either completes or leaves nothing.
4. **One emulator at a time.** `gs.ps1` refuses two, and concurrent runs make
   every timing meaningless. Check before starting anything.
5. **`Start-Process -ArgumentList` splits on spaces.** Quote paths containing
   them, or the savestate silently fails to load and the run exits in 30 s.
6. **`--frames`/`--steps` default to 1 frame** if neither is given.
7. **Savestates drift.** Slot 2 has been overwritten since older notes; frame
   numbers in `docs/` and `local/gs011/oracle-bracket/` are stale. Pin a copy.
8. **A killed window loses the input recording**, which is what makes a freeze
   reproducible. Close normally if at all possible.

---

## 7. Where to go from here

### 2026-08-13 battle-stutter status

The latest real fight (`session_20260813_170322.log`) ruled out presentation
cost: optional render/interpolation/timing modes were off and canonical PPU
work averaged 1.098 ms/frame. The run instead entered the interpreter bridge
12,596 times (6,000,449 instructions), chiefly mutable low RAM (7,676 calls)
and Golden Sun's generated battle-sprite code pool (4,521 calls).

RAM self-heal requests are now keyed by exact immutable content snapshots, so
changed variants at one PC no longer suppress or poison one another. At every
dispatch miss the game thread also nonblocking-drains completed worker results
and retries exact verified native dispatch before interpreting; this removes
the former once-per-frame installation delay. The launcher automatically emits
matching `.events.csv`, `.events.csv.misses.csv`, and `.phase.csv` sidecars for
cleanly closed sessions. Focused self-heal/PPU/interpolation/timing tests and all
86 Python tests pass. Next evidence gate: repeat one Attack and one Psynergy
three times each, then correlate per-miss bridge cost with the following phase
frame. Persistent sprite-pool stalls justify a fail-closed shadow/native version
of the measured `Func_ed408` generated blitter; do not substitute it before
exact write/register/cycle comparison passes.

The longer post-bandit session (`session_20260813_172202`) settled the timing
question. Enhanced Timing did **not** remove CPU lag: its Enhanced-only battle
window contained the worst burst, 7.90 seconds of bridge work over six frames
(all exact variants still queued/inflight). The later no-enhancement field
window had only three bridges over 19,956 frames. Enhanced Timing's real visible
benefit is cadence: on the measured 120.006240 Hz panel, faithful 59.7275 Hz
creates an uneven refresh about every 1.8 seconds; exact 60 Hz pushes that to
about every 160 seconds. D3D11 reported VSync active, so the reported “tearing”
currently fits judder/shimmer better than an unsynchronized swap.

The next progression session (`session_20260813_182110`, Enhanced Timing on
throughout) recorded 599 bridge events, 4.140 s of bridge wall time, and 2.839 M
interpreted instructions. Generated battle RAM accounted for 3.936 s. One exact
variant at `0x03006340` bridged 207 times for 1.842 s while its compile request
was in flight. The compiler finished, but an already-running outer interpreter
bridge could keep calling that callee without returning to the dispatch/frame
boundary that installed ready results. The bridge now nonblocking-drains only
completed worker results at its existing BL/BLX handoff boundary before the
verified native query. A synthetic regression holds a completed callee
undrained and proves the bridge hands it to native (five caller instructions
interpreted, not seven including the callee). `ram_heal_tests` passes in 28.3 s
and GoldenSunRecomp was rebuilt. This removes post-compile repeats inside one
large bridge; the first asynchronous compile/bridge cost remains.

The compile worker now promotes repeatedly-hit exact RAM variants ahead of cold
one-shot jobs, with a three-hot-job burst limit so FIFO work cannot starve. The
game thread still never compiles or waits. The same route exposed 98 ROM PCs;
48 whole functions were proven against the pinned ELF's sized STT_FUNC/$t data,
added to the reviewed resume corpus, regenerated, and rebuilt. Three unsafe
addresses were rejected. Ordinary-play unknown-transient reporting is concise;
strict mode or `GSR_TRANSIENT_VERBOSE=1` retains the full candidate dump.

Interpolation produced only 2,885 midpoints and 6,233 safe duplicate fallbacks
(31.6% useful) in that mixed session. Its changing fallback reasons also caused
1,998 synchronously flushed launcher log lines; progress logging is now
power-of-two rate-limited while exact per-reason totals print at shutdown. World
map remains fail-closed. F1 Video exposes a default-off whole-image soft filter
as `Reduce shimmer (soft filtering, experimental)`; it is presentation-only and
may look blurry. Speed now persists `Mute audio while Turbo is held`, raises the
slider ceiling from 16x to 32x, flushes audio on press/release, and leaves
Uncapped as the true host maximum. See `docs/ACTIVE_ISSUES.md`.

The next user session (`session_20260813_182110`) confirmed the improvements but
left bounded battle stalls: 599 bridges, 4.14 s total, and 2.84 M interpreted
instructions; 449 bridge calls/3.00 s came from `0x03006000` battle code. The
worst single bridge fell from 909 ms to 139 ms. Hot-queue telemetry recorded 90
requests, 48 promoted jobs, and 26 fairness selections. One exact body at
`0x03006340` still bridged 207 times while its compile was in flight. The active
interpreter bridge now nonblocking-drains completed overlays at each safe
BL/BLX boundary before deciding whether to interpret a callee; a synthetic
regression proves an already-ready callee immediately hands off to native.

Turbo was separately capped by presentation: with VSync at 120 Hz it presented
every 60 Hz guest frame, limiting observed speed to about 2x even when Uncapped
was selected (which intentionally ignores the 32x slider). Turbo now skips
intermediate render/VSync work while still pumping input, draining audio, and
refreshing the display within 16.7 ms; release immediately presents the current
canonical frame. Current measured compute headroom is about 4x, so 32x remains
a ceiling rather than a promise. Enhanced Timing remains globally default-off,
but its saved `[Enhancements] EnhancedTiming` preference now persists; explicit
environment overrides still win and strict/frame-capture routes force it off.

An observer-only foundation is available behind `GSR_BLITTER_SHADOW=1`. It is
installed only after the loaded ROM matches the built-in USA/Europe SHA-1.
Exact THUMB entries `Func_ed408` (`0x080ED408`) and allocator `Func_48b0`
(`0x080048B0`) capture the five-word builder descriptor and allocator slot/size;
the measured slot table is `0x03001E50`. Later ARM RAM dispatches correlate to
the live slot range. Exit telemetry is bounded and reports only counts, sizes,
and one-way fingerprints, never code bytes. The canonical guest always runs;
there is no native substitution.

**1. Run a broader player progression sweep.** Missing-overlay freezes should
now be prevented. Stop at the first remaining loud dispatch miss or divergence
and classify it before changing metadata. The bulk workflow is:

```
python tools/build_all_overlays.py
python tools/build_all_overlays.py --compile-only \
  --recompiler ..\gbarecomp\build-mingw\gba_recompile.exe
cmake --build build/gs011 --target GoldenSunRecomp
```

The tool SHA-gates every decompressed binary, verifies ELF `.text` bytes, derives
function/mapping coverage, chooses shard counts, deduplicates equivalent code,
and stages private output before replacement. Do not weaken its gates.

**2. Measure windowed performance and re-check the previously slow fights and
boulder scene.** Everything quantified so far is headless.

Then implement the opt-in exact-60/120 Hz pacing and fight-slowdown removal in
`docs/ENHANCED_TIMING_PLAN.md`. The exact next step is a deterministic
slowdown-heavy fight replay plus frame-deadline/VBlank/present/audio counters;
do not start with an unmeasured global CPU multiplier.

**3. Native audio subsystem.** PAUSED — see "Current audio handoff" above; the
"performance and quality in one job" framing in §2 is superseded (native only
adds per-frame work under the canonical-oracle rule and is quantized to a
bit-replica, not a quality gain). If resumed, build a
Golden Sun-specific shadow from the live SoundInfo/channel state and the
private audio capture, including the driver’s instrument variants, loop/cursor
rules, reverb, and exact stereo gain. Gate every change against the canonical
fallback; do not replace it based on generic m4a assumptions. The current
generic shadow is intentionally left fail-closed.

**4. Audio oracle and host validation.** Add a deterministic state-5 capture
comparison against mGBA’s stereo stream, then run a windowed replay with
underrun/overflow and frame-cadence counters. This is the acceptance gate for
the native mixer and enhanced timing.

**5. Native renderer.** Scene interpolation now exists as an isolated optional
presentation layer. Widescreen and high internal resolution remain later work.

**6. Static coverage and freezes.** The state-5 resolver profile saw 452,059
`Func_2cf4` candidate visits but zero identity wins, so the three recurring
IWRAM misses are not currently proven copies of that ROM image. Capture the
writer and live bytes at the miss; only add a sized image if that evidence shows
a stable identity. Otherwise keep the mutable stack code in the interpreter
tier. Do not merge per-PC TOML entries. Then run the broader fight/overworld
progression sweep and investigate the remaining random freeze.

**7. Then decompilation.**

### Rules that do not relax

`generated/**` is never hand-edited. No address or ARM/THUMB mode is ever
guessed — evidence or `TODO-EVIDENCE`. Fixtures are synthetic; no ROM/BIOS/asset
bytes in the repo, tests, or any report. A `FULLY_STATIC` headline alone does not
establish "no regression" — the semantic invariants in
`config/usa/acceptance-baseline.json` are what does. Never resolve a
`[[data_range]]` collision by shrinking the range to make a gate pass.

### Method note, earned the hard way

Four wrong predictions on the performance question in one session:
per-instruction tick, dispatch binary search, hashing, and the resolution cache.
Every win came from measurement; every failure came from reasoning about what
ought to be fast. The same pattern hit correctness — the plaza freeze was called
data corruption from a register dump, and reproduction showed a runaway bridge
in ten minutes. The tools to measure are now in the repo and cheap to run
(§3). Use them first.
