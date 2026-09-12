# Enhancement notes: higher frame rate and internal upscaling

Experimental 2x scene interpolation shipped on 2026-08-09 for game runners that
explicitly opt in. Internal upscaling remains design-only. Both default OFF and
remain subordinate to each game's faithful acceptance path.

Both are generic engine features and belong here, not in a game repository. Both
must default OFF and be opted into per game, exactly as widescreen already is
(`launcher_expose_widescreen`, `max_view_width`).

## 1. Higher frame rate without touching game logic — experimental 2x shipped

The first implementation targets 119.455 Hz presentation while guest logic
stays at 59.7275 Hz. It interpolates BG scroll/affine registers, retains current
OAM, and verifies every captured alpha=1 endpoint against the canonical
per-scanline framebuffer before showing a midpoint. Structural changes,
VRAM/palette changes, or endpoint mismatches log `DEGRADED` and duplicate the
canonical frame. The F1 option is availability-gated by the game runner and a
display mode of at least 119 Hz; strict-static runs force it unavailable.

**The trap:** static recompilation raises throughput, not frame rate. Guest logic
is frame-locked to 59.7275 Hz — animation, the M4A sound driver, input polling
and scripted timing all advance once per VBlank, and the pacer, not vsync, is the
game clock. Running the recompiled code faster produces fast-forward, which
already exists (`set_speed` / `set_uncapped`).

**The approach that works:** keep logic at 59.7275 Hz and interpolate the SCENE
between two logic frames. On the GBA the scene is almost entirely IO registers,
which makes this unusually tractable:

- BG scroll: `BGxHOFS` / `BGxVOFS`
- affine: `BGxPA..PD`, `BGxX`, `BGxY`
- sprites: OAM positions (and affine groups)

Sketch:

1. At end-of-frame, snapshot the PPU-relevant IO block plus OAM.
2. Between logic frames, for each intermediate step, lerp the previous and
   current snapshots, install them, run the existing scanline renderer, restore.
3. Present. Logic tick count is unchanged, so no accuracy claim is affected.

**Known hazards, in the order they will bite:**

- **Mid-frame raster effects.** Any game that rewrites scroll or affine
  parameters per scanline (HBlank IRQ / DMA) is describing a scene that a
  whole-frame snapshot cannot represent. Interpolating those layers shears the
  image. Detect per-scanline writes and fall back to duplicating the frame for
  the affected layer rather than interpolating it.
- **Non-continuous motion.** Screen transitions, menu opens and battle wipes are
  discontinuities; lerping across them produces a visible slide. A cheap
  magnitude threshold (skip interpolation when a register jumps more than N)
  handles most of it.
- **Sprite identity.** OAM slots are reused between frames; interpolating slot N
  across a reuse moves an unrelated sprite. Only interpolate a slot when its tile
  base and palette are unchanged.

Do the simplest version first: interpolate BG scroll and affine only, leave OAM
un-interpolated, and measure. Sprites are where the artifacts live.

**Prerequisite:** enough throughput headroom to render 2x the frames. Measure
before building — a corpus that has grown during a static-coverage crawl can eat
the headroom that made this look affordable.

## 2. Internal upscaling — only worth it for affine content

For plain text/tile BG modes, "internal resolution" is meaningless. The art is
1:1 pixel art; a bigger render target yields bigger pixels and nothing else. Only
post-process filtering can change that appearance.

Affine layers are the exception, and they behave like a 3D render target: the
rasterizer samples a source through a transform, so evaluating that transform at
2x/4x removes rotation aliasing and stair-stepping for real.

**So the feature is not "render at 4x" — it is "rasterize AFFINE layers and
affine sprites at Nx, composite with the native-resolution tile layers."**

`render_affine_bg` in `src/gba/gba_ppu.cpp` is the site. Scope:

- affine BG2/BG3 in modes 1 and 2
- affine (rotscale) sprites
- everything else stays native and is point-scaled to the same output grid

**Honest limit:** this smooths SAMPLING, it does not add source detail. Tiles
still come from VRAM at native size. The win is removing shimmer on rotated and
scaled backgrounds — which is large or negligible depending entirely on how much
the game uses affine modes.

**Measure first.** `GBARECOMP_MMIO_DUMP` plus a histogram of `DISPCNT & 7` says
whether a game is affine-heavy. Golden Sun measured 95.9% mode 2 over 400 frames
of boot/title/file-select, which is the favourable end of the range; a mostly
mode-0 game would gain nothing and should not pay the cost.

Note the MMIO ring wraps quickly under audio FIFO traffic — use a short run, or
the histogram comes back empty.
