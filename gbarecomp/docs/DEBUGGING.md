# Debugging gbarecomp games

The execution contract lives in `DEBUG.md` at the repo root and is
mandatory reading. This file is the longer-form companion: it
explains why the rules exist and how the always-on observability
fits together.

## Why always-on rings

Static recompilers fail in subtle, history-dependent ways. The
divergence you see on frame 1200 was caused by a register / memory
/ IO state difference that took root much earlier — and that
earlier state is gone by the time you decide to look. Live
inspection is too late.

Therefore: the runtime records, continuously, into a frame ring and
a per-store / per-block / per-call ring. The debug surface
**queries** these rings. It never arms recording.

If the event you need isn't in the ring, you extend the ring. You do
not switch to "run again with this trace armed."

See `DEBUG.md §RULE 0b` and the global memory note about this.

## Frame ring vs reverse-debugger ring

Two granularities cohabit:

| Granularity | When to use |
|-------------|-------------|
| Per-frame snapshot ring (~36k frames, ~10 min @ 60 fps) | "What did the state look like at frame N?" / "When did VRAM byte X first diverge from oracle?" / "Step backwards through frames to find the first divergence." |
| `rdb_*` rings (per-store / per-block / per-call) | "Which instruction wrote VRAM byte X?" / "Show me every block entered between block 100k and 110k with PC in range Y." / "Reconstruct IWRAM at block 1.2M." |

Walk frame ring backwards first to localize **which frame** went
wrong. Then drop into `rdb_*` to identify **which write** went
wrong.

## Native ↔ oracle bridge

We run two processes:

1. **Native build** — the recompiled binary, exposing TCP on
   `19842` (default).
2. **Oracle build** — same project, linked against an mGBA bridge,
   exposing TCP on `19843`.

The two processes share the same TCP command grammar for the
overlapping subset. A `framebuf_diff` request hits both, diffs the
buffers, and reports the first differing pixel.

The oracle binary is a separate target so we never accidentally pull
mGBA's license into the native runtime.

## Sync points

Use hardware events, not frame number alone:

- VBlank IRQ count.
- DMA completion count (per channel).
- Timer overflow count (per timer).
- SWI count (and per-SWI count).
- BIOS-IRQ-return count.
- Specific PC at specific function entry.

Two processes that happen to be on the same "frame number" may have
diverged elsewhere. Two processes that just took their 412th VBlank
IRQ are at the same execution point.

## Classification

When you have a divergence:

1. **Decoder / codegen** — the same ARM/THUMB word is producing
   different semantics on our side. Confirm by feeding the word
   through `decoder_smoke` and comparing to a reference (mGBA
   debugger output, jsmolka tests).
2. **Runtime / timing** — semantics match, but cycle counts or
   scheduler ordering differ. Look at `scheduler_state` and
   `timer_state` diffs.
3. **Memory / bus** — load or store hit the wrong region or wrong
   mirror, or waitstates produced different values for unaligned
   reads. Look at `read_io` diffs near the suspect time.
4. **IO** — IO write produced different downstream effect. Look at
   `io_diff` and per-device state.
5. **IRQ / DMA / timer** — scheduler fired in the wrong order.
   Look at `irq_state`, `dma_state`, `timer_state`.
6. **PPU** — VRAM/OAM/PAL writes match but framebuffer differs.
   Almost certainly a render bug; compare `ppu_state` and
   `framebuf_diff`.
7. **Audio** — only matters when the game depends on FIFO/timer/DMA
   for game logic timing (it can — Minish Cap drives some timing
   off audio DMA).
8. **Save chip** — wrong save type detected, or save chip command
   state machine wrong.
9. **Game metadata** — symbols missing, function boundary wrong,
   ROM hash mismatch.

The classification determines who fixes it: decoder/codegen → tool;
runtime/timing/io/devices → `src/gba/`; metadata → `game.toml`.

## What never happens

- Editing `generated/*.c` by hand.
- Adding `if (game == "minish_cap")` to the GBA core.
- Stubbing an SWI to "return what the game expects."
- Silencing an unmapped IO read because it's noisy.
- Pausing both native and oracle and stepping in lockstep.

## Performance: guest throughput and LTO

Measured on Golden Sun (24,480-function corpus), headless, no window/audio/pacing,
3,600 frames of the `campaign` track:

| build | fps | realtime |
|---|---|---|
| Release `-O3` | 64.1 | 1.07x |
| Release `-O3` + LTO | 90.1 | **1.50x** |

Without LTO the recompiled core clears 60 Hz by about 5%, which leaves nothing
for presentation and audio — the symptom is a windowed run that stutters while
a headless run of the same frames "passes".

The cause is structural, not algorithmic. Every recompiled instruction calls
out to `runtime_should_yield()`, `runtime_mem_cycles()` and `runtime_tick()`,
and memory ops add `bus_read_*`/`bus_write_*` plus `runtime_trace_event()`. All
of those are out-of-line declarations in `runtime_arm.h` implemented in the
runtime libraries, while guest bodies live in the generated corpus TUs, so none
of them can inline. LTO is what lets them.

Enable with:

```
cmake -S . -B build-lto -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

Two costs, both real: the link took **20m31s** for that corpus (versus a few
minutes without), and the binary grew 48.7 MB -> 56.7 MB. Keep LTO off for the
generate/build/run iteration loop and on for anything anyone actually plays.

The optimisation is behaviour-preserving and was verified as such rather than
assumed: strict-static runs at 1,800 and 5,400 frames on both the no-input and
`campaign` tracks report FULLY_STATIC from both binaries with **identical
cumulative cycle counts**, which a codegen-visible change would not survive.

Two things that were measured and are NOT the bottleneck: the always-on trace
ring costs ~2.7% (`GBARECOMP_TRACE=0` to measure), and idle-loop elision is
already on by default.

Benchmark gotcha: `GBARECOMP_DEMO_INPUT=` with an EMPTY value enables a demo
track (the test is `demo_env != nullptr`). To measure a genuine no-input run the
variable must be unset — `env -u GBARECOMP_DEMO_INPUT` — or the two "tracks"
silently become the same one.

## Presentation scaling: the "Integer scaling" checkbox was inert

**Symptom:** resizing the window produced visibly uneven pixels — wobbling
character edges, inconsistent text weight — even with the F1 video tab's
"Integer scaling (no stretched pixels)" checkbox ticked (its default).

**Cause:** two independent scaling paths. `integer_scale` was applied with
`SDL_RenderSetIntegerScale`, which only affects the `SDL_RenderSetLogicalSize`
path. The frame is actually presented by `compute_presentation_layout` followed
by an `SDL_RenderCopy` into an explicit destination rect, which bypasses SDL's
logical-size machinery completely. So the preference never reached the code that
sizes the frame.

`compute_presentation_layout` maximised the destination at the exact logical
aspect in reduced-ratio units (3x2 for 240x160). That fills the window nicely
but lands on a whole multiple of 240x160 only rarely:

| window | destination | scale | source pixel widths |
|---|---|---|---|
| 720x480 | 720x480 | 3.000x | 3 (clean) |
| 800x600 | 798x532 | 3.325x | 3 and 4 |
| 1000x700 | 999x666 | 4.162x | 4 and 5 |
| 1280x720 | 1080x720 | 4.500x | 4 and 5 |
| 1920x1080 | 1620x1080 | 6.750x | 6 and 7 |
| 2560x1440 | 2160x1440 | 9.000x | 9 (clean) |

Under nearest-neighbour, "4 and 5" means some source columns are drawn 4 pixels
wide and others 5. That is the wobble.

**Fix:** `compute_presentation_layout` now takes a `ScalingMode`
(`IntegerLetterbox` default, `AspectFill` = the old behaviour, `Stretch`), and
the present path passes the user's preference. Integer mode floors to a whole
scale, centres, and letterboxes the remainder; a drawable smaller than one
logical frame falls back to the aspect fit rather than showing nothing, because
cropping the guest image is never acceptable.

`presentation_layout_tests` asserts the property that matters rather than just
the numbers: for a sweep of real window sizes, every source pixel must occupy
the same number of destination pixels, and the destination must stay inside the
drawable. The old `AspectFill` expectations are retained so the alternative mode
cannot silently change.

**Verified:** the full upstream suite (17/17) passes, and a 5,400-frame headless
Golden Sun run is unchanged in every semantic fingerprint — `steps=21071`,
`cycles=1040556882`, `ppu_vcount=214`, `pal_nonzero=937/1024`,
`vram_nonzero=50205/98304`, `oam_nonzero=515/1024`, `trace_events=33936510`.
Headless never presents, so this is expected; it was checked rather than assumed.
