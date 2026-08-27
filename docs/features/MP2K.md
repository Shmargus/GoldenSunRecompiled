# MP2K optimization TODO

## Goal

Native MP2K is the target replacement renderer: it must accurately reproduce
the guest MP2K mixer, run on an audio wall clock independent of video/Turbo,
and cost less than canonical audio. Canonical GBA audio is the verification
oracle and temporary fallback during bring-up. Fail loudly; never silently
publish bad native audio. Probation and engine switching are scaffolding to be
removed only after the replacement and its independent clock are proven.

Scope is MP2K-generated music and SFX. PSG/FIFO remains canonical.

## Measured state

- Current native MP2K and Normal-speed Turbo paths are opt-in while the
  replacement is incomplete. The wall mixer is bounded to 2x-4x Turbo.
- `logs/session_20260821_200013.log`: probation failed at correlation `0.68`
  and level ratio `0.19`.
- Same log: judged `281`, passed `260`, rejected `21`, incomplete `51`;
  host gaps/resets/underruns/late `33/52/36418/79`; DMA matched `252`,
  unmatched `454408`; candidate missing `101277` of `262144` verifier samples.
- Serial playable build passed; `GoldenSunRecomp.exe` is updated. The latest
  assertion-independent focused configured CTest set passed `5/5`. No gameplay
  was run in this pass.
- Route-A/B, synthetic-seam, PWM, saw, and sequence fixtures are corrected.
  Both focused targets pass (`2/2`); the latest focused configured set is
  `5/5`. Native starvation remains open; continue with the ordered timeline
  work below.
- Focused configured tests now use assertion-independent checks and pass `5/5`.
  Ignore ad-hoc test EXEs; native starvation remains open.
- Latest saved-state session `20260823_114726` has two self-heal miss rows in
  the dispatch sidecar (frame 276: `0x030020F4`, `0x0300227C`). Boot stayed
  `engaged=no, hooks=0` (9-55). After slot 1 load (1023), C70 completed block
  0 (1049); realign 1050 showed `guest=352/352`,
  `native=348/348 expected=352`, and startup discarded that epoch (1051).
  Seven more C70 completions followed (1052-1064), but status remained
  `engaged=yes, proven=no` (1089-1160), with no judged/pass/reject,
  `FullProducerSeed`, wall-live, ownership, or MP2K fallback. The final line
  (1168) is self-heal aggregate telemetry, not an MP2K dispatch trace. The
  dedicated C70 state and five-slot non-C70 pool remain in place; reset/
  savestate reset clears both. The fixture saturates five non-C70 slots, then
  verifies C70 completion and guest A/B `352/352`
  (`gbarecomp/src/gba/gba_audio.h:60-65,813-814`,
  `gbarecomp/src/gba/gba_audio.cpp:494,2248-2257,2675-2700`,
  `gbarecomp/tests/audio_event_capture/integration_test.cpp:254-261,563-584`).
  Focused configured CTest passed `5/5`; serial playable build passed. No
  gameplay was launched. Separate lag/self-heal evidence remains non-causal.
- Bounded verifier diagnostics now log the first eight incomplete/alignment/
  metric stages with numeric IDs, guest/native counts, alignment, metric
  results, reverb/history readiness, and hook/order context. The synthetic
  short→full fixture proves no padding and successor judging. No behavior gate
  changed. User must retest with the rebuilt root launcher and return these
  lines plus a complete `FullProducerSeed` attempt.
- Session `logs/session_20260823_120850.log`: C70 completed blocks 0-7
  (695-710; post-load 888-906); the dispatch-miss sidecar was header-only and
  inline telemetry ended at `dispatch_misses=32` (927). After startup discard
  889-890, full blocks were judged but rejected with reverb `50`, history ready,
  and corr/ratio `0.102/5.735`, `0.555/6.002`, `0.447/3.880` (893, 897, 900).
  No `FullProducerSeed`, wall-live, ownership, or fallback appeared. The
  first-reject bounded diagnostic now logs numeric/hash reverb accumulator,
  finalizer, seed, and first-difference summaries. No arithmetic/order fix is
  proven; probation and fail-closed behavior are unchanged. CTest `5/5` and
  serial build passed. Next: manual root-launcher retest with the new lines.
- Session `logs/session_20260823_125816.log` had one dispatch-sidecar miss
  (`0x03005C74`, frame 510496, 8161 us/4096 interpreted instructions) and
  inline `dispatch_misses=39` (1010). The first reverb-enabled reject (836)
  reported reverb `50`, history/seed ready, current/rolling `352/352`,
  corr/ratio `.102/5.735`, first difference sample 0 route A delta 7, and
  `groups=0`; dry route context stayed `0x00` (831, 835). Bounded dry-writer
  identity/progress and explicit reverb-branch skip diagnostics were added.
  No normalization/arithmetic fix is proven; gates remain unchanged. CTest
  `5/5` and serial build passed. Next: manual root-launcher retest.
- Session `logs/session_20260823_131037.log` proved pre-ownership pool
  contamination: unrelated PCs filled capacity, then known B4/A8C/BCC/BD4
  stores were no-slot (713-720); C70 completed (734), but reverb routes/groups
  were `0/0` (735), metrics rejected (736-737), and the gate stayed
  `engaged=yes, proven=no` (751-752) before fallback (765). Inline telemetry
  ended at `dispatch_misses=20` (908), with a header-only sidecar. The fix
  reserves all four evidenced non-C70 PCs, normalizes Thumb matching/routes,
  and blocks unknown registration before ownership. The isolation fixture
  proves 24 unrelated PCs cannot consume those slots, all four known writers
  populate both routes/groups, and C70 publishes guest A/B `352/352`.
  Focused CTest `5/5` and serial build passed. Next: manual root-launcher
  retest for FullProducerSeed/ownership.
- Session `logs/session_20260823_132110.log` confirms reverb capture: history
  ready, routes `0x02003500/0x02003b30`, groups `88/88` (736). Block 1 rejected
  at corr/ratio/MAE `0.576/1.143/0.026`, first-sample delta `2` opposite sign
  (737-740); blocks 2/3 passed `0.966/1.037` and `0.954/0.956` (744, 747).
  C70 reached block 7 (749-753), but status stayed `engaged=yes proven=no`
  (763); no FullProducerSeed, wall-live, ownership, or fallback. Sidecar is
  header-only; inline `dispatch_misses=0` (770). Added bounded probation/
  timeline diagnostics for verifier, publication/DMA/candidate, active/live,
  and wall rejection counters/reason. Reverb export remains fail-closed.
  Focused CTest `5/5` and serial build passed. Next: manual retest returning
  `MP2K probation status` lines.
- Session `logs/session_20260823_133412.log` contains two epochs. The initial
  epoch rejected seedless blocks (`seed_ready=0`, lines 719-729) and entered
  `deferred hook needs owned state` fallback (789); after the savestate restart
  (897), startup realign discarded `native=348/352` (930-931), then reverb
  capture and signed producer passes succeeded (934, 942, 945). Status still
  showed `proven=0`, while envelope probation failures rose `1/2/3` with
  correlation/ratio `.674/.185`, `.659/.170`, `.520/.164` (955, 957, 965).
  DMA matched `51/51` through `211/211`, sequence gaps stayed zero, and wall
  export/live remained zero. Added a one-shot first-failing-window diagnostic
  for aggregate canonical/native/route output statistics plus resampler
  sequence/timestamp state; no audio bytes, behavior, or gates changed. The
  dispatch sidecar is header-only; inline telemetry ends at `dispatch_misses=32`
  (974). Focused configured CTest passed `5/5`; serial `GoldenSunRecomp`
  build passed. No gameplay was launched.
- Session `logs/session_20260823_135217.log`: despite 39 inline dispatch
  misses, the restarted MP2K epoch's DMA stayed matched (`51/51` then
  `211/211`) with zero sequence gaps. First envelope window: cursor
  `2173-75734`, corr/ratio `.674/.185`, canonical RMS `.127762/.127219`,
  native `.023664/.023538`, route `.031384/.031552`, masks `0x06/0x03`, and
  53 resampler underruns. Routing/full-volume is not the loss. Added one-shot
  same-sample canonical FIFO A/B aggregate evidence to distinguish native
  source amplitude from host timestamp alignment; no behavior/gate change.
  Focused CTest `6/6`, serial build passed; no gameplay launched.
- Session `logs/session_20260823_140430.log`: inline dispatch misses were 21
  distinct PCs (`interpreted_insns=0`, `healed_native=406`); the MP2K epoch
  again matched DMA (`51/51` then `211/211`) with zero sequence gaps. The first
  envelope failure is unchanged at cursor `2173-75734`, corr/ratio
  `.674/.185`, canonical RMS `.127762/.127219`, native `.023664/.023538`,
  producer-route `.031384/.031552`, and stable masks `0x06/0x03`. Same-sample
  FIFO RMS was `.169625/.170350` (peaks `.648438/.664062`), so routing and
  full-volume are not the loss; native producer output remains attenuated
  relative to the canonical FIFO, while timestamp evidence still has 53
  underruns, 7 late blocks, and 3 resets. Enhanced timing scale was
  `1.004562378x`; resampler sequence `66`, start/end
  `0x000123a600000000`/`0x000127ec23118880`, step `0x000000031e00c780`.
  Probation remains fail-closed; no arithmetic or gate change is proven.

  The same run records four host-pump stalls (`124107`, `139073`, `122506`,
  `108908us`, frames `510598`, `510703`, `510821`, `510887`) while guest and
  render work stayed about `4ms`/`0.8ms`; three to six RAM heals coincided with
  those rows, and background warm-load completed after `17640ms`. This is
  bounded evidence of a warm-load/self-heal host stall, separate from MP2K
  probation; it does not yet prove whether SDL pumping or loader contention is
  causal. Next diagnostic: correlate per-frame warm-load/ready-queue state with
  a split host-pump timer before changing loader scheduling. No gameplay was
  launched.
- Session `logs/session_20260823_141608.log`: the MP2K envelope failure remains
  `.674/.185`, with DMA `51/51` then `211/211` and zero sequence gaps. The
  phase CSV's largest stall is frame `510522` (`pump_us=103899`, guest
  `4719us`, render `885us`); its event row has seven installed heals and no
  dispatch miss. Warm-load completed in `5290ms`, but the stall's cause is not
  proven. Added bounded ready-pending, warm-load-done, and game-thread drain
  timing columns to the frame-events CSV. No behavior or gate change. Focused
  CTest `7/7` and serial playable build passed; no gameplay launched. Next:
  user retests and returns the new columns joined to the phase row.
- Session `logs/session_20260823_142651.log`: the same MP2K failure remains
  (`.674/.185`), with DMA `51/51` then `211/211` and zero sequence gaps. The
  two largest phase stalls are frames `510661/510861`
  (`pump_us=121969/132619us`); guest/render stayed `4156/1062us` and
  `4214/895us`. Both event rows had warm-load complete, no heals, no ready
  entries, and zero drain time, so the ready queue is not the direct stall
  work. Added env-gated HostWindow pump split columns to the phase CSV; no
  behavior or gate change. Focused CTest `7/7` and serial playable build
  passed; no gameplay launched. Next: user retests and returns the split
  pump columns.
- Session `logs/session_20260823_143613.log`: the same MP2K failure remains
  (`.674/.185`), with DMA `51/51` then `211/211` and zero sequence gaps. The
  largest stall is frame `510525`: `pump_us=79875`, of which `pump_events_us`
  is `79870`; setup/input/apply/finalize are `0/2/0/0us`. Its event row has
  warm-load complete, no heals, ready entries, or drain time. This localizes
  the freeze to the SDL event phase but does not yet distinguish polling from
  event handling. Added bounded event-count, poll, dispatch, and slowest-event
  type columns; no behavior or gate change. Focused CTest `7/7` and serial
  build passed. Next: user retests the root launcher and returns the columns.
- Session `logs/session_20260823_144238.log`: the MP2K envelope remains
  fail-closed (`.674/.185` first window, `3` failed windows, DMA `211/211`,
  zero sequence gaps). Frame `510595` spent `123666us` in SDL polling with
  `event_count=0`, `dispatch=0`, warm-load complete, and no heal/ready/drain
  work; frame `510609` spent `109728us` polling. This rules out event
  dispatch, warm-load, and overlay queue work, but not SDL backend pumping
  versus dequeue. Added diagnostic-only `SDL_PumpEvents`/`SDL_PeepEvents`
  columns while keeping normal runs on `SDL_PollEvent`; no behavior or gate
  change. Focused CTest `7/7` and serial playable build passed. Next: user
  retests and returns the new split columns.
- Session `logs/session_20260823_144926.log`: the MP2K envelope remains
  `.674/.185`, with DMA `211/211` and zero sequence gaps. Before slot 1 load,
  status was `engaged=no hooks=0` at cursor `65536`: deferred VBlank arms a
  pending epoch but waits for a SoundMainRAM/control/writer boundary. Earlier
  boot evidence reached C70 through the guarded VBlank fallback, then failed
  closed on an unowned deferred epoch; canonical boot audio is not ownership
  proof. The largest freeze (frame `510502`) is
  `pump_sdl_pump_us=107112`, `pump_sdl_peep_us=1`, event count `2`, dispatch
  `1`; backend pumping owns it. Added bounded boot-probe stage logging; no
  gate or audio behavior change. Focused CTest `6/6`; serial `gs011_opt`
  build passed.
- Session `logs/session_20260823_145915.log`: boot probing is active, not
  state-load-only. It sees no SoundInfo, then `spv=224/pcm=13379`, then
  `spv=352/pcm=21024`; C70 reaches blocks 0-7 before fallback at
  `deferred hook needs owned state`. Slot 1 resets that failed epoch, so
  probation appears after load. The largest stall is frame `510560` with
  `SDL_PumpEvents=100650us`, `SDL_PeepEvents=0us`, two events, and `1us`
  dispatch; warm-load/queue work is absent. Added terminal boot-probe-stage
  logging; no gate or audio behavior change. Focused CTest `7/7`; serial
  `gs011_opt` build passed.
- Session `logs/session_20260823_151248.log` repeats the boot ownership
  failure: SoundInfo transitions `224→352`, C70 completes blocks 0-7, then
  `deferred hook needs owned state` fails the established epoch after mutable-
  RAM misses. Added one-shot canonical-fallback context logging; no gate or
  audio behavior change. SDL remains separate: `pump_sdl_pump_us=99698` at
  frame 510567 with no warm-load/queue work.
- Session `logs/session_20260823_151856.log` has a header-only dispatch sidecar
  and inline `dispatch_misses=0` (`interpreted_insns=0`). C70 completes blocks
  0-7; the first complete signed block rejects at corr/ratio/MAE
  `0.576/1.143/0.026` (route-A sample 0, delta `2`, opposite sign), while
  blocks 2/3 pass (`0.966/1.037`, `0.954/0.956`). The first envelope window
  remains `.674/.185`; native/route RMS is `.023664/.023538` /
  `.031384/.031552`, versus same-sample FIFO `.169625/.170350`. DMA is
  `51/51` with zero sequence gaps. Source-stage versus timestamp alignment
  remains unresolved; no behavior change is justified. The separate largest
SDL stall is frame 510513 (`pump_sdl_pump_us=108786`) with no heal or
  ready-queue work.
- Added a one-shot aggregate producer-source diagnostic for the next root
  launcher run. It reports C70-vs-DMA and native-vs-DMA corr/ratio/MAE before
  host resampling, with no gate, gain, or published-audio change. The
  synthetic seam passes (`c70_dma_corr=-1.000`, `native_dma_corr=1.000`),
  focused CTest passes `7/7`, and serial `GoldenSunRecomp` build passes.
  Collect the producer-source line, first probation window, first probation
  resampler, and probation status; do not launch gameplay from this worker.
- Session `logs/session_20260823_160140.log` shows that first diagnostic was
  emitted by rejected block 1 (`c70_dma_corr=.619`, `native_dma_corr=-.065`),
  while blocks 2/3 pass the signed gate. Do not infer source loss from block 1.
  The diagnostic is now gated on an accepted full block and paired dry-route
  block IDs; focused CTest `7/7` and serial build pass. The same run retains
  probation `.674/.185`, DMA `51/51`, zero sequence gaps, and resampler
  resets/underruns/late `3/53/7`. Next manual retest: collect the accepted
  `MP2K producer-source diagnostic` plus probation window/resampler/status.
- Session `logs/session_20260823_161206.log` provides the accepted source
  discriminator on block 2: C70/DMA `corr=.630 ratio=5.061`, native-post/DMA
  `corr=.684 ratio=.205`; their product is `1.037`, exactly native/C70 for
  that accepted block. DMA matched `107/107` with zero sequence gaps. The
  earliest proven divergence is therefore the C70-versus-EWRAM-DMA producer
  boundary, before host resampling. The bounded fix gates and publishes native
  dry-route Q15 state against paired EWRAM routes, retaining reverb history and
  finalizer state; no gain or gate relaxation. Synthetic route-boundary and
  focused CTest pass `7/7`; serial `GoldenSunRecomp` build passes, and manual
  retest remains.
- Session `logs/session_20260823_163732.log` shows the DMA fix improving the
  accepted source pair to `native_dma_corr=.949`, ratio `1.186`; the signed
  block passed. A later envelope window passed `.988/1.015` with DMA `103/103`
  and zero sequence gaps, then the signed gate failed `.472/.388`. The first
  reject diagnostic was already consumed by startup block 1. Added a bounded
  one-shot `MP2K post-probation reject diagnostic` for the first failed block
  after an actual good envelope window and signed publication, including block
  IDs/routes/reverb and the first dry guest/native Q15 pair even before
  `vf_.proven()` is true. No gate, gain, or probation change. Focused CTest
  `7/7` and serial playable build pass. The assertion-independent fixture
  proves one good window (`verifier_windows=1`, `vf_.proven()=false`) still
  latches the diagnostic; its raw diagnostic vectors are seeded to keep the
  synthetic reject bounded and crash-free.
- Session `logs/session_20260823_170531.log` confirms the gate is not stuck:
  the run reached frame `511173`, then restored canonical output after signed
  mismatch. Block `77` is the one post-probation reject (`dma`, corr/ratio/MAE
  `.503/.564/.146`, published block `76`); the later status has one good
  `.988/1.015` envelope window but remains `proven=0` until the second window.
  Its first route-B sample delta is only `1`, so the diagnostic now adds max
  signed error/index, best lag/permutation, contiguous mismatch span, and
  native voice/source state transition at that index. No threshold change.
- Session `logs/session_20260823_171716.log` confirms the later user crash is
  separate from MP2K. After slot 1, MP2K reached `engaged=1` but stayed
  `proven=0/live=0`, then restored canonical output on signed mismatch
  `.472/.388`; no `FullProducerSeed` was reached. After a later savestate load,
  the bridge aborted on interpreter `Undefined` at `0x030061EC` while healing
  miss `0x03006048`. Added bounded decoded-opcode/history and CPU context at
  that abort; no opcode or gate change. Focused CTest `7/7` and serial
  playable build pass.
- Bounded cursor provenance now accompanies the post-probation reject: the
  guest block-boundary snapshot is captured before judging, projected by
  aggregate index/fraction through the max-error sample, and paired with
  native pre/post cursor state plus loop-wrap/provenance flags. The synthetic
  loop fixture is assertion-independent and passes; no PCM bytes, gain, gate,
  or probation behavior changed. Focused CTest `7/7` and serial playable build
  pass. Next retest must collect `[audio] MP2K post-probation cursor`; the
  171716 log predates this line.
- Session `logs/session_20260823_174435.log` paired block `77`'s cursor: guest
  start `1979/0x7f0880`, projected max `2001/0x435a80` after `179` advances,
  native max `2046/0x612a80`, equal step `0x000f6600`, zero loop wraps, and
  lag/permutation `0/0`. Native block-start state was unavailable, so the
  logged `provenance=1` is not seed proof. Native pre-state capture is now
  unconditional with explicit validity and provenance `0=unavailable`; no
  behavior, gate, or gain change. This run had no interpreter-gap/Undefined or
  crash line, but had five miss rows and inline `dispatch_misses=68`,
  `interpreted_insns=217`. Focused CTest `7/7` and serial build pass. Next:
  collect `native_valid=1/1` before classifying seed versus phase advance.
- Session `logs/session_20260823_174950.log` has valid native pre/post cursor
  state and an exact seed (`native_start=1979/0x7f0880`, `start_match=1`). The
  prior `guest_max=2001` versus native `2046` / `provenance=2` comparison was
  made with the host-rate step over producer-rate advances, so it is not proof
  of an audio phase bug. The diagnostic now separates PCM-rate `guest_source`
  from host-grid `guest_host`, reports `host_advances` and `rates`, and uses
  `provenance=3` only when both seed and host trajectory match. The synthetic
  `21024 -> 65536` trajectory fixture passes; no audio/gate/gain change.
  Focused CTest `7/7` and serial `GoldenSunRecomp` build pass. No crash or
  interpreter-gap appeared; bounded recursion stayed separate. Next: fresh
  root-launcher log with the corrected source/host cursor fields.
- Session `logs/session_20260823_181213.log` contains two epochs: the first
  degraded at startup block `11` (`native=351/351`, `guest=352/352`, expected
  `352`) and fell back without publication; the second reached block `77` and
  still failed aggregate DMA metrics `.503/.564/.146`. Its corrected cursor
  is exact (`guest_source=2047/0x0f0880/0x00300000`,
  `guest_host=2046/0x612a80`, `host_advances=555`, `native_valid=1/1`,
  `provenance=3`), ruling out seed/phase/loop as the cause. The run then had
  one good `.988/1.015` window but ended before the second proof window;
  `proven=0/live=0` is pending, not stuck. Added bounded max-error per-voice
  source/ctype/sample/gain/packed-delta and native accumulator/seed/guest
  writer hashes; no behavior/gate/gain change. Focused CTest `7/7` and serial
  build pass. Next: collect the new voice summary and per-voice lines.
- Replacement slice: the verified producer seed now crosses the existing
  publication boundary into every queued wall update at normal speed and
  Turbo. Normal native requests render a wall-clock candidate; canonical
  PSG/FIFO remains host output and the guest mixer remains active. The normal
  candidate now composes synchronized canonical PSG taps with captured routing
  and sample timing. Captured FIFO bytes are the evidenced MP2K buses being
  replaced; additive separate-FIFO mode is explicit and tested only. Missing/
  stale taps and rate changes fail closed. Focused composition,
  cadence/order/starvation, queue, wall, and seam tests pass. First-frame
  output ownership switching is implemented with canonical backlog drain and
  immediate fallback; guest-mixer bypass and independent audio-clock
  completion remain.
- Architecture mapper evidence: SDL's device callback already pulls from the
  host ring (`gbarecomp/src/runtime/host_window.cpp:741-748`). Canonical guest
  mixing remains guest-paced in `GbaAudio::run_sample_event`
  (`gbarecomp/src/gba/gba_audio.cpp:2647`), while normal native candidate
  rendering and Turbo use the wall-time service
  (`gbarecomp/src/runtime/runtime.cpp:2863`). Guest-mixer bypass and
  independent audio-clock behavior are the remaining clock/engine boundary
  work.

## Ordered work

1. **Make test evidence real.** Run configured CTest with explicit checks
   active under `NDEBUG`; remove raw-`assert` false passes. Keep test seams
   labeled synthetic. Do not loosen gates.
2. **Implement the native replacement boundary.** Use an evidenced guest
   MP2K update boundary to capture complete state and advance native rendering;
   do not skip unsupported state or guess addresses. Keep canonical output as
   the oracle until the replacement is independently proven.
3. **Compose and select normal-speed replacement safely.** Combine the wall
   MP2K candidate with synchronized canonical PSG taps (the captured FIFO is
   the MP2K replacement bus unless a separate source is proven), then select
   the composed wall ring on the first valid frame and drain canonical backlog
   at that boundary. Keep the guest mixer running as oracle; unsupported
   reverb, stale taps, sample-rate changes, and stale/overflow updates
   fail-closed to canonical.
4. **Fix the earliest producer-state boundary first.** Startup realignment now
   survives partial epochs, deferred VBlank IDs remain contiguous, and the
   dedicated-C70/saturated-writer fixture passes. Manually reproduce the
   saved-state run, confirm C70 completion and guest `352/352` before each
   351/352 realign, then prove a complete engaged/proven `FullProducerSeed`
   publication. The next diagnostic target is the first rejected full block's
   reverb/finalizer state; treat pump/self-heal stalls separately and do not
   claim ownership or wall-clock fidelity before a `FullProducerSeed` is logged.
5. **Restore fidelity.** Use independent reference vectors for PCM8, DPCM,
   synth/PWM/saw, gain, routing, and 4x wall-time trajectories. Explain the
   `0.68/0.19` result, then pass the existing probation gate; do not lower it.
6. **Prove startup and fallback.** Native must start only when ready;
   fallback must retain canonical coupled audio. Debug overlay and bounded
   logs must report actual `Native MP2K` vs `GBA`, requested state, live or
   fallback status, and failure reason.
7. **Verify Turbo decoupling.** At capped 2x and 4x, video may Turbo while
   MP2K audio stays normal speed. Test enter/exit, rapid toggles, savestate
   reset, and `MuteDuringTurbo`. Verify unsupported state, reverb, queue/ring
   failure, and uncapped Turbo fall back to canonical audio.
8. **Profile and optimize.** Compare canonical/native cost at 1x, 2x, and 4x.
   Optimize measured hot paths only after timeline and fidelity pass. Keep
   diagnostics off the default path.
9. **Lock safety rules.** Preserve canonical output, loud bounded fallback,
   and strict-static behavior (experimental audio forced off). No PSG/FIFO
   expansion in this pass.
10. **Final acceptance.** Build serially, launch only root
   `GoldenSunLauncher.exe`, then user-check music, battle, Turbo, and fallback
   behavior. Record the session log and exact result before enabling defaults.

## Acceptance gates

- Configured CTest passes with explicit checks; no invalid manual-EXE result.
- Supported native run has no unexplained candidate loss, host timeline gaps,
  resets, underruns, or late blocks; DMA misses are explained or eliminated.
- Existing probation correlation and level-ratio gates pass on representative
  music/battle coverage. Canonical fallback remains bit-faithful.
- Overlay/log state matches the engine actually producing audio.
- Turbo changes video speed only; audio cadence stays nominal.
- Native path shows measured CPU benefit, with no default-path regression.
- Strict-static and disabled-toggle runs remain canonical.
