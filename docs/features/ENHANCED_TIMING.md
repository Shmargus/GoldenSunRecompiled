# Enhanced Timing Plan

## Goal

Provide a smoother PC experience than original GBA hardware:

- stable 60 Hz guest updates;
- exact 120 Hz presentation when 2x interpolation is enabled;
- remove original fight-scene slowdown;
- keep gameplay capped at 60 Hz rather than globally speeding it up.

This is an opt-in enhancement. Faithful 59.7275 Hz timing remains available for
compatibility, debugging, and acceptance tests.

## Design

### 1. Exact display lock

- Pace completed guest frames at 60.000 Hz.
- Pace interpolation at the display's measured refresh rate, using the DXGI
  refresh numerator/denominator rather than a rounded mode label.
- On a true 120 Hz display, present one canonical and one interpolated frame per
  guest frame with no 119.455/120 phase drift.
- Keep vsync enabled. If horizontal tearing remains, treat it as a swap-chain or
  present-mode defect rather than a guest-frequency problem.

### 2. Stable audio

- Resample canonical audio from the 59.7275 Hz timebase to the 60 Hz wall clock.
- Do not periodically drop or duplicate audio buffers.
- Track buffer fill, underruns, overruns, pitch, and long-run A/V drift.

### 3. Remove original slowdown

- Keep PPU presentation and game logic capped at 60 updates per second.
- Detect frames where canonical CPU work would miss the next presentation
  deadline.
- Grant extra CPU execution budget only for overloaded frames.
- Do not advance PPU, audio, or timers multiple times merely because extra CPU
  work was allowed.
- Return to the normal budget immediately after the overloaded frame.
- Count and log every rescued frame and the extra budget used.

This is dynamic CPU headroom, not a global fast-forward mode.

## Implementation order

Steps 1-4 are **done and shipped**: Enhanced Timing exists as a default-off
setting with exact 60/120 pacing and resampled audio, and the user's choice
persists. Step 5 (the one-hour A/V drift test) has **not** been run and is the
gate blocking Enhanced Timing from ever becoming a default — see PRES-01 in
`docs/ACTIVE_ISSUES.md`. Steps 6-7, dynamic CPU headroom, are **not built**.

Note that the manual guest CPU overclock that did ship is a *different* thing
from step 6: it is a blunt user-chosen multiplier, not a measured per-frame
rescue budget.

1. Record a deterministic fight replay that visibly slows on the faithful path.
2. Instrument guest work per frame, VBlank deadlines, repeated visual frames,
   audio fill, and interpolation/present timestamps.
3. Add a default-off `Enhanced Timing` setting and keep strict-static acceptance
   forced to faithful timing.
4. Implement exact 60/120 pacing without CPU headroom; verify tearing/cadence.
5. Run the one-hour A/V drift test against the resampled audio path.
6. Add dynamic CPU headroom, starting with the smallest measured budget that
   eliminates the fight slowdown.
7. Compare enhanced and faithful state at scene boundaries. Investigate any
   gameplay, input, RNG, save, timer, or transition divergence.

## Acceptance

- 60.000 Hz guest pacing remains phase-locked over at least one hour.
- A true 120 Hz display receives exactly two presentations per guest frame.
- No visible cadence correction, tearing, audio underrun, or accumulating A/V
  drift.
- The reference fight no longer repeats visual frames because of CPU overload.
- Input remains one logical sample per guest update.
- Disabling the enhancement restores the current faithful behavior and existing
  strict-static acceptance invariants.
- Enhanced runs identify themselves in logs and are never reported as faithful
  acceptance results.

## Main risks

- Timers, DMA, IRQ ordering, or RNG may depend on elapsed guest cycles.
- Extra CPU budget could change polling-loop behavior or expose races.
- Rounded display refresh reporting could recreate slow phase drift.
- Fixing cadence will not fix true swap-chain tearing.

Use measured fight evidence and the smallest required headroom. Do not apply a
global cycle multiplier without first proving why dynamic headroom is
insufficient.

## Exact next step

Exact 60/120 host pacing was implemented on 2026-08-09 as a default-off F1
Enhancements option. Strict-static and frame-capture runs force faithful timing.
A fractional-nanosecond deadline accumulator has zero measurable accumulated
error in the one-hour 60/120 unit test. A 600-guest-frame windowed smoke on the
reference display measured 120.001920 Hz and delivered exactly 1,200 presents;
the cadence ring reported about 120.01 presents/s with vsync blocking.

This stage changes wall-clock guest updates from 59.7275 to 60.000 Hz while
leaving cycles, timers, DMA, PPU and input sampling per guest frame unchanged.
That is a 0.456% wall-time speed increase. Audio resampling is implemented in
the shared host bridge: Enhanced Timing consumes canonical source frames at
`60 / (16777216 / 280896) = 1.0045623779x`, while faithful timing remains 1.0x.
The bridge keeps its continuous polyphase history and applies the ratio under
the audio mutex, so the timing conversion does not drop or duplicate samples.
The deterministic `audio_drc_tests` and existing `frame_timing_tests` pass.

Next: complete the one-hour hardware A/V drift gate, capture one reproducible
slowdown-heavy fight replay, then add measured dynamic CPU headroom.
