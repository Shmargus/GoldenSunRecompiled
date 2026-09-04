# Bootstrap Guide

## 1. Create the repository

Copy the contents of this starter into a new directory named `GoldenSunRecomp`, initialize Git, and make the governance-only first commit.

```bash
git init
git add .
git commit -m "Initialize Golden Sun recomp research scaffold"
```

## 2. Prepare private inputs

Keep outside the repository:

- Exact Golden Sun ROM.
- GBA BIOS.
- Save files and traces.

Verify the ROM:

```bash
python tools/verify_rom.py /path/to/goldensun.gba
```

## 3. Clone upstreams as siblings

```bash
git clone https://github.com/mstan/gbarecomp.git ../gbarecomp
git clone https://github.com/gsret/goldensun.git ../goldensun-disasm
```

Do not commit the second repository into this one. Review licenses and pin exact commits in `UPSTREAM.md`.

## 4. Build the public scaffold

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 5. Build and test gbarecomp separately

Follow the pinned upstream README and run its tests before integrating it. Record the exact commands and results.

## 6. Reproduce the Golden Sun disassembly build

The disassembly requires the user-owned ROM and ARM binutils. Build it in its own checkout/worktree. Do not copy its generated binaries into the public repo.

Capture a sanitized inventory containing only paths, sizes, checksums, addresses, and tool versions where appropriate.

## 7. Start GS-003, not the runner

Design and validate the symbol/overlay metadata format before adding the native game executable. Golden Sun's overlays are not a detail to postpone; they determine whether dispatch is correct.
