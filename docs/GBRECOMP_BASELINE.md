# gbarecomp Baseline

Recorded for GS-001 on 2026-07-18.

## Pin and license

```text
repository: https://github.com/mstan/gbarecomp
commit: af51d0e48e847ae2f056fe01560fb9f087ea27f8
license: PolyForm Noncommercial License 1.0.0
```

The required `PRINCIPLES.md`, `DEBUG.md`, `TCP.md`, and
`docs/TOML_SCHEMA.md` were reviewed at this revision. The upstream BIOS-intro
gate is binding: cartridge/game execution must not begin until the real BIOS
passes the visual, audio, and memory oracle checks.

## Tool versions

```text
OS: Windows 11 10.0.26200 x86-64
Git: 2.55.0.windows.1
Python: 3.12.10
CMake: 4.4.0
Ninja: 1.13.2
MSVC: 19.44.35228
LLVM/Clang: 22.1.8
MSYS2 MinGW64 GCC: 16.1.0
MSYS2 GNU Make: 4.4.1
ARM GNU binutils: 2.46.1
SDL2 (MinGW64): 2.32.10
```

## Commands and results

The public GoldenSunRecomp scaffold builds in Debug and Release with MSVC and
its `bootstrap_help` test passes.

The pinned core was configured with MSVC using:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

MSVC configuration succeeds, but compilation stops in
`src/armv4t/interpreter.cpp` because the source uses GCC-only
`__builtin_popcount`. `src/runtime/bios_hle.cpp` also contains
`__builtin_clz`. No upstream source was patched locally.

An independent Clang/Ninja build was configured with:

```powershell
cmake -S . -B build-clang2 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang.exe" `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe" `
  -DCMAKE_EXE_LINKER_FLAGS="-Xlinker /stack:16777216"
```

Twelve independent tests pass:

```text
decoder_smoke
function_finder_tests
selfheal_cluster_test
interpreter_smoke
armv4t_tests
thumb_tests
timer_tests
irq_tests
ppu_smoke_tests
presentation_layout_tests
heal_gate_tests
codegen_tests
```

The larger stack is required for `ppu_smoke_tests` on Windows; with the default
linker stack it exits with `0xC00000FD` (stack overflow).

The documented MinGW64 ABI configures with SDL2 when CMake receives the native
prefix explicitly:

```powershell
cmake -S . -B build-mingw2 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe `
  -DGBARECOMP_MINGW_PREFIX_UNIX=C:/msys64/mingw64 `
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--stack,16777216
```

After the exact private BIOS was supplied and hash-verified, `gba_recompile`
generated the ignored `src/runtime/generated_bios/` corpus. The MinGW64 build
then completed, including the SDL runtime and oracle targets. CTest passed all
14 registered tests:

```text
decoder_smoke
function_finder_tests
selfheal_cluster_test
interpreter_smoke
armv4t_tests
thumb_tests
bus_tests
dma_tests
timer_tests
irq_tests
ppu_smoke_tests
presentation_layout_tests
heal_gate_tests
codegen_tests
```

The BIOS identity used locally is 16,384 bytes with SHA-1
`300c20df6731a33952ded8c436f7f186d25d3492`. The image and generated output
remain outside this repository or under ignored paths.

## BIOS gate reproducibility note

The pinned upstream `docs/ROADMAP.md` marks the recompiler-only BIOS intro gate
closed on 2026-05-28 and says a `bios_intro_flawless` CTest target enforces it.
That test is not registered or implemented in this exact checkout: the full
CTest inventory contains only the 14 tests listed above. The available
`bios_smoke` TCP comparison path is explicitly interpreter-driven and therefore
cannot re-certify `runtime_dispatch` under upstream's own rules.

As a diagnostic only, sampled frame comparisons against the patched mGBA 0.10.5
oracle matched OAM, PAL, VRAM, and IWRAM. The diagnostic still showed an IO-page
difference at offset `0x10`, one transient framebuffer-channel difference in
the sampled frames, and 12 audio samples with an absolute difference greater
than 100 out of 260,330 samples. These results are not presented as closure evidence. Game
metadata work relies on the exact pinned revision's recorded closed-gate status;
any runtime/static-execution claim still requires restoring and passing the
missing recompiled-BIOS regression.

## Unresolved gates

- Upstream `gba-suite` ARM/THUMB ROM fixtures are absent, so their optional
  `test_rom_arm` and `test_rom_thumb` targets are skipped by CMake.
- The documented recompiled-BIOS `bios_intro_flawless` regression target is
  absent at the pinned revision, so its historical gate result cannot currently
  be reproduced by CTest.
- MSVC compilation still requires upstream portability fixes for GCC builtins;
  the supported local full build uses MinGW64.

No ROM, BIOS, generated BIOS code, or protected assets were added to this
repository.
