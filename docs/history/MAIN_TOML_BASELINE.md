# GS-007 Main TOML Baseline

## Status

The evidence-backed main-image proposal generates and all generated translation
units compile locally on Windows x86-64. The result currently depends on one
uncommitted generic fix in the pinned local `gbarecomp` checkout; the public pin
must not be advanced, and the production gate must not be called reproducible,
until that fix is reviewed upstream.

**This document records the original GS-007 baseline proposal and its
counts are stale.** Many regenerations have happened since (`rom_a1000`/
`rom_c9000` un-excluded, dozens of `REVIEWED_RESUME_FUNCTIONS` seeds, and most
recently 705 derived interworking-veneer resume points) and are tracked as
dated sections in `docs/GS011_TRANSIENT_IMAGES.md` instead of here. The most
recent measured regeneration, `local/gs011_task/after_regen.log`
(2026-08-07 10:41, ELF revision `84a80693003439acdbb78dd84538b0532461ad4b`),
reports `resume_range entries: 185`, `data_range entries: 3,534`,
`extra_func entries: 3,701`, and `TOTAL emitted: 34,923` functions
(`arm=619 thumb=34,304 indirect=13,492 undefined=1`), against
`config/usa/main.toml` sha256 `2c9e17bfb88a1d7a09746c02e45b66436fdcbb682145bd6852e3961380fe33db`
as of 2026-08-07 10:41. `config/usa/main.toml` is under active concurrent
regeneration as this note is written, so both the sha256 and every count above
will move again before the next reader checks them — treat this paragraph as
a pointer to the evidence trail, not as the current number.

No ROM bytes, generated function bodies, objects, or other protected artifacts
are stored in the repository. They remain under ignored `local/gs007/` paths.

## Inputs

- ROM SHA-1: `5c4695205413df7db52b9a184815a07783999971`
- `goldensun-disasm`: `0fa7b312199c10b96544e825be86cfc476493eb7`
- base `gbarecomp`: `af51d0e48e847ae2f056fe01560fb9f087ea27f8`
- main symbol corpus SHA-256:
  `8c4ba15914fb14c16483ce8a10176c191094169e1cb2b25c9208f5d84186d860`
- reviewed data-exception SHA-256:
  `74dd24b8184ef42bf874788a0ed5072d3279fc419e1a17a5aac4fe2c4c626438`

`tools/build_main_toml.py` hash-gates the ROM, decodes and verifies the reset
branch target, validates the imported corpus, reads ELF mapping symbols and
file-backed `PT_LOAD` segments, and writes `config/usa/main.toml`
deterministically.

## Reviewed metadata

The proposal contains:

- cartridge reset/discovery entry `0x08000000` (ARM)
- hash-verified reset branch target `0x080003c0`
- 358 `confidence=proven` function seeds
- one oracle-proven mid-function THUMB resume entry at `0x080047ae`
- 3,649 merged data ranges
- one code copy: ROM `0x08000770` to IWRAM `0x03000000`, size `0x1400`
- speculative literal harvesting disabled
- zero manually declared jump tables

The code-copy source, destination, and size come directly from the main ELF's
file-backed executable `PT_LOAD`. Twelve proven imported function entries use
that relocated runtime span and carry their immutable ROM `source_addr`.

The data proposal begins with every in-section `$d` mapping-symbol span plus the
non-executable complement of ROM sections. Seventeen two-byte spans are removed
by `config/usa/main-data-exceptions.json`: the pinned finder reports direct
sequential control flow into each span, and forced-THUMB disassembly confirms an
instruction at each address. This preserves the contradiction as reviewed
metadata instead of silently weakening all mapping-symbol data.

No jump table is declared because the pinned unseeded scan reported zero
auto-detected tables and no sized, mode-proven manual table has yet been
established. Zero is the conservative reviewed result, not an assertion that
the game contains no indirect tables.

The generated TOML SHA-256 is:

`95037ca9c55dfb595b751eb47afb29c45d48be361d298741ac2f02809d9f88e6`

The resume entry is backed by the GS-010 native/mGBA fingerprint comparison in
`docs/ORACLE_HANDOFF_BASELINE.md`. It rolls into the existing THUMB function at
`0x080047a4`; it does not add or split a function.

## First-divergence upstream fix

The first structural run stopped because `gbarecomp` tried to decode the data
table at `0x08084b1c` as ARM code. The earliest false edge came from Golden Sun
`Func_799b0`: it loads the table address, copies it through `r10` into `lr`, and
uses `lr` as a general scratch register. The finder treated any tracked `lr`
constant as the return PC for a later dynamic `bx rN`.

The generic local fix adds constant provenance and permits that inferred-return
edge only when the `lr` value is PC-derived. A synthetic upstream regression
test reproduces a literal data pointer copied into `lr` followed by `bx r1`.
The relevant local upstream diff changes only:

- `src/recompile/function_finder.cpp`
- `tests/recompile/function_finder_test.cpp`

The binary Git diff SHA-256 is:

`04a5ccdcb2d597c67c9defdcd243173ed2872e8d3d7fb22e0bb8afdb97d1a7f2`

All 15 current upstream CTest targets pass with the cumulative local fixes. The
current local
`gba_recompile.exe` SHA-256 is:

`f8d62e22ef07ba423942ef894864afc300020ea1ddc68e22ad1420702442fc6a`

This executable hash is evidence for the local run, not a replacement upstream
pin.

## Recompile and host-compile result

The reviewed proposal reports:

- 12,580 emitted functions: 141 ARM and 12,439 THUMB
- 12,221 finder-only entries
- 95 redundant manual seeds
- 264 manual-only proven seeds
- 5,035 functions with indirect control flow
- 103,806 direct branch targets
- 3,649 data ranges honored with zero collision
- one code copy honored
- zero literal-pool seeds
- one undefined decode

The generated-source aggregate SHA-256 is:

`40b1d5c5c7ec11c0ae7f6de3813aa4e850f19de944a56268c000ca22cf16e164`

The aggregate feeds each `.cpp` file's raw bytes to SHA-256 in lexical filename
order. All 34 generated
`.cpp` files—32 body shards, the dispatch table, and the symbol map—compiled to
x86-64 objects with MSYS2 GCC 16.1.0. Generated code currently emits numerous
unused-temporary warnings, but no compile errors.

## Reproduction

Paths below are intentionally placeholders for private and separate-checkout
inputs:

```powershell
$python = '<Python 3.12 or newer>\python.exe'
$rom = '<private hash-verified ROM>'
$elf = '<pinned goldensun-disasm checkout>\goldensun.elf'
$recompile = '<locally patched pinned gbarecomp build>\gba_recompile.exe'

& $python tools/build_main_toml.py `
  --rom $rom `
  --elf $elf `
  --corpus local/symbols/main-symbols.json `
  --data-exceptions config/usa/main-data-exceptions.json `
  --output config/usa/main.toml `
  --disassembly-revision 0fa7b312199c10b96544e825be86cfc476493eb7

& $recompile --rom $rom --config config/usa/main.toml `
  --out local/gs007/main --max-functions 20000
```

Compile the 32 shards plus `dispatch_table.cpp` and `symbol_map.cpp` with a C++20
x86-64 compiler, including the generated directory and pinned upstream
`src/armv4t` and `src/runtime` headers.

## Remaining limitations

- The single undefined decode PC must be exposed and classified.
- The 2,619 functions with unresolved internal mapping transitions remain
  excluded from manual seeds.
- No manual jump table has enough evidence yet.
- The code-copy activation timing has not been synchronized against an oracle.
- The first architectural divergence is now the PPU-phase-dependent `DISPSTAT`
  read at `0x080030b8`; see `docs/ORACLE_HANDOFF_BASELINE.md`.
- The local generic finder fix must be reviewed upstream and represented by a
  new exact pin before clean-checkout reproducibility can pass.
- This is static-generation evidence only; no execution or coverage claim is
  made.
