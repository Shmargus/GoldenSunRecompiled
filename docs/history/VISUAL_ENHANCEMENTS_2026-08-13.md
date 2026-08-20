# Visual enhancements — 2026-08-13

## Delivered

- The faithful default is unchanged.
- F1 > Video now exposes live color profiles: Raw, original GBA, frontlit SP,
  backlit SP, and Classic vivid. Raw is the exact default.
- The menu states the crisp-pixel recipe: Integer scaling ON, Linear filtering
  OFF. At 3440x1440 the faithful 240x160 image is a uniform 9x (2160x1440).
- The experimental native compositor now verifies every original-resolution
  sample against the faithful framebuffer. Any mismatch falls back for that
  frame.
- Native rendering now retains per-scanline IO and affine state. A synthetic
  raster-effect test covers different effects on adjacent lines.

## Measured native-compositor result

The exact ROM identity gate passed before local slot runs.

| Scene | 2x/4x verified frames | Result |
|---|---:|---|
| World map, slot 6 | 0/120 | Safe fallback every frame |
| Battle, slot 7 | 108/120 | Native supersampling on most frames |

The world map changes presentation state during each frame. Its faithful image
cannot yet be reconstructed from one final VRAM/palette/OAM snapshot, so the
verifier correctly rejects it. This is unsupported, not a regression.

On the battle sample, native 2x and 4x added about 0.04 ms and 0.06 ms per guest
frame respectively. These figures exclude first-use self-heal compilation.

## Smoother animation result

The 2x interpolator now captures Golden Sun's register and hidden affine state
for every scanline instead of trying to rebuild a frame from final registers.
It also interpolates short sprite movements only when the OAM slot, tile,
palette, mode, and shape remain stable. Large jumps, affine sprites, mask
sprites, and content transitions stay at the newer faithful endpoint.

Measured with Enhanced Timing on a 120.001920 Hz display:

| Scene | Before | After |
|---|---:|---:|
| Battle, slot 7 | 0/120 midpoints | 65/120 midpoints |
| World map, slot 6 | 0/120 midpoints | 0/120 midpoints |

Battle presentation cost was about 1.35 ms per guest frame, roughly 16% of one
8.33 ms refresh interval. Every rejected frame duplicates the exact canonical
image. The world map still changes memory during scanout, which cannot be
reconstructed from the current end-of-frame VRAM/palette/OAM snapshot.

Next useful work is a compact per-scanline memory-change timeline for the world
map and remaining battle effects. It must preserve exact endpoint verification;
do not weaken the fallback gate to raise the midpoint count.

Image-based motion interpolation is possible later, but spell/UI ghosting and
latency make it a harder optional experiment.

## Widescreen scope

The generic PPU can draw horizontal margins, but Golden Sun deliberately keeps
the maximum at 240 and hides the option. Enabling width blindly can expose
wrapped or stale tile memory.

Safe order:

1. Add a scene policy that pillarboxes every unknown scene.
2. Investigate the world map first. Its affine map may support wider sampling,
   but map bounds, wrap, and scanline effects need measured proof.
3. For field maps, prove camera position, tile ownership, streaming bounds, and
   map edges before showing any margin.
4. Keep menus, dialogue, battles, and cutscenes centered at 240 initially.
5. Whitelist only scenes whose margins are proven guest-authored.

A 16:9 target is roughly 284-288x160, about 20% more pixels. Widescreen cannot
invent higher-detail art; it only reveals valid world data where it exists.

## Verification

- Public Python tests: 86 passed.
- Focused PPU, interpolation, and frame-timing CTests: 3 passed after the
  interpolation change.
- 25 built CTest targets passed, including PPU, layout, interpolation, timing,
  DMA, IRQ, bus, audio, and codegen.
- `GoldenSunRecomp.exe` rebuilt successfully.
- No generated-source edits or protected material were added.

Known unrelated workspace gates:

- The public audit scans local `private/` and the full upstream checkout, so it
  reports existing private/upstream files.
- `heal_gate_tests` has a pre-existing link-stub conflict with dirty upstream
  runtime instrumentation.
- `ram_heal_tests` exceeded five minutes; no result is claimed.
