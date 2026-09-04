# Verified facts

Measured findings and their evidence. **Read this before investigating
anything** — it exists so nobody re-derives what is already known.

Only measured or directly observed things belong here. Never record a guess.
Add the date and how it was established.

## Map and room data

- **The room pointer is useless as an identity.** The value at `0x03001E70` is
  always `0x02030CCC`. Measured across a 55-minute session, 2026-09-04.
- **The room bounds beside it do track rooms.** The four bounds values took 23
  distinct combinations across that session, changing as the player moved
  between rooms. Use the bounds tuple, not the pointer.
- Metatile table at `0x02010000`, tile-entry table at `0x02020000`. Located
  during the widescreen work; exact layout still unknown — that is milestone 1.
- Camera clamp function `Func_10230`, scroll shadow writer `tfunc_080101CA`.
  Two confirmed landmarks.
- BG layer roles, from layer-toggle observation: BG3 ground, BG2 tiles, BG1
  props, BG0 lighting. Not yet confirmed against the map data itself.

## Display signals

- **BLDY (brightness) is a dead signal.** Constant at 16 for 2,614 of 2,700
  logged frames, 0 for the rest. It does not fade. 2026-09-04.
- **Room transitions are window-register driven and animate for 17-18 frames.**
  Measured across two sessions: seven at 18 frames and three at 17 in one, five
  at 18 and one at 17 in the other. These are the black-bars and iris-circle
  effects the user described — not fades. A 30-frame debounce covers one
  cleanly.
- **Text boxes do not use the window registers.** Tested with dialogue actually
  on screen: window enable was 0 for 3,195 of 3,300 frames, non-zero only
  during transitions. Detecting dialogue needs a different signal; VRAM
  glyph-write bursts are the untested candidate.

## Runtime behaviour

- **Savestate loads jump the frame counter.** The PPU frame count is restored
  from the save, so any code baselining against a frame number must re-sync.
  This caused three sessions of confusion and two wrong diagnoses on
  2026-09-04. A `set_savestate_load_hook` extension point now exists.
- **Headless runs need `GBARECOMP_SELFHEAL_RAM=1`.** Without it, IWRAM audio
  code is interpreted forever and the run crawls at ~4 fps. With it and a warm
  `recomp_cache`, ~286 fps. The launcher sets it by default, which is why
  windowed play was always fast and a raw CLI run was not.
- **Execution is deterministic.** Two independent headless runs produced 1,697
  functions and 1,946,675 calls each — identical, same capture count, no
  function present in one and not the other. 2026-09-04. The project had
  previously flagged replay determinism as never verified.
- **Input record and replay already exist**
  (`gbarecomp/src/runtime/runtime.cpp`, `GBARECOMP_INPUT_RECORD` /
  `GBARECOMP_INPUT_REPLAY`, `scripts/gs-replay.ps1`). Recordings now pair a
  savestate automatically.

## Code corpus

- 26,935 canonical translated functions in the main corpus; 95 overlay banks
  fully recompiled ahead of time. The generated C++ is what executes.
- Dispatch coverage snapshot 2026-09-03: 1 distinct miss, 380 healed native,
  186,079 native calls.
- **Real function count is ~6,000, not the ~28,000 generated bodies.** Most
  generated bodies are basic-block dispatch stubs so indirect branches always
  have a callable target. Main ROM 2,259 real named functions / 533 KB;
  overlays 3,731 / 777 KB.
- Function size distribution: median 80 bytes, mean 242. About 115 functions
  over 1 KB account for 42% of all code bytes.
- **Static analysis cannot identify the code.** The translated corpus never
  contains literal addresses — every bus access takes a computed effective
  address. Grepping all shards for the map tables and save memory returns zero
  hits. 89% of functions are unidentified and only running the game can fix
  that.
- **No type information exists anywhere.** Every function is `void fn(void)` on
  a global register struct. The symbol schema has no field for arguments,
  return type or calling convention.
- **Nothing can replace a guest function with a native one.** The
  function-entry hook is documented observe-only
  (`gbarecomp/src/armv4t/runtime_arm.h:815-816`).

## Tracer coverage

From one 55-minute session, 2026-09-04:

- 1,289 of 2,981 main-ROM functions reached (43.2%), plus 767 overlay-code
  addresses and 719 IWRAM addresses.
- Only 47 functions appear in >=90% of captures — the always-on engine.
- 820 functions appear in <=5% of captures — context-specific, the separable
  material.
- Revisiting the same rooms stops yielding new functions quickly, which is
  coverage saturating as it should.

## Build

- The build is `build/gs011_opt`, `RelWithDebInfo` (`-O2 -g -DNDEBUG`). The
  presets in `CMakePresets.json` are Debug only and are **not** what the user
  builds.
- **LTO is ON by default** (`GSR_ENABLE_LTO`). An older note claiming LTO was
  off and had failed twice was wrong and has been removed.
- **The LTO link runs single-threaded unless `MAKE` is set with forward
  slashes**: `MAKE=C:/msys64/mingw64/bin/mingw32-make.exe`. GCC's lto-wrapper
  parses it with a routine that treats backslash as an escape, so a normal
  Windows path is silently mangled and the link takes hours instead of minutes.
  Verified good state: 8 concurrent `lto1.exe`, ~1000s wall clock.
- `-O3` has never been measured on this project.

## Things that were wrong and why — 2026-09-04

Kept because the pattern matters more than the individual errors.

- A fade threshold was guessed twice, in opposite directions: too loose gave 457
  spurious boundaries, too strict gave zero. Both were guesses about BLDY, a
  value that never moves.
- The screenshot gate was guessed too tight — 12 images across 699 captures —
  after predicting the opposite failure.
- A tracer fault was diagnosed twice from reading code alone: first as a
  hot-path performance bug, then as a stuck frame counter. Both wrong. One
  headless run found the real cause, a savestate frame jump.
- An apparent 1 FPS collapse was not performance at all — clicking Mark while a
  prompt was open left the emulator permanently paused.

Every number chosen without data was wrong. Everything measured resolved
quickly.
