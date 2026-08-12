# Testing Strategy

## Primary principle

Compare synchronized machine state and hardware events, not only screenshots or raw frame numbers.

## Oracles

- Primary software oracle: a pinned, accurate mGBA build/configuration.
- Secondary cross-check: another accurate emulator where useful.
- Final authority for disputed hardware behavior: documented hardware tests or real-hardware evidence.

Record oracle version, settings, BIOS identity, ROM identity, save state, and input script for every reusable trace.

## Synchronization points

Prefer event anchors such as:

- BIOS handoff.
- Function entry at a known PC.
- VBlank IRQ count.
- DMA completion count.
- Timer overflow count.
- SWI entry/return.
- Overlay load completion.
- Save command completion.

Frame number alone can hide scheduler drift.

## State comparison order

When a divergence appears, inspect in this order:

1. PC, instruction mode, CPSR, banked registers, SP/LR.
2. IRQ/IME/IE/IF and scheduler event queue.
3. DMA channels and completion ordering.
4. Timers and audio FIFO triggers.
5. IWRAM/EWRAM writes.
6. Palette, VRAM, and OAM writes.
7. PPU register state and framebuffer.
8. Audio samples.

Fix the earliest differing write/event.

## Test layers

### Unit tests

Belong upstream when generic:

- ARM/THUMB instruction semantics.
- Bus access widths/alignment.
- DMA, timers, IRQ, PPU, APU, save chips.

Belong here when Golden Sun-specific:

- Symbol parser fixtures.
- Overlay-manifest parser fixtures.
- Duplicate runtime-address detection.
- ROM/checksum gating.
- Overlay activation/invalidation metadata.

Use synthetic fixture bytes and text; never commit ROM-derived binary fixtures.

The GS-003 symbol-schema tests use only synthetic JSON fixtures and run through
CTest as `symbol_corpus_validation`. They can also be run directly:

```powershell
python -m unittest discover -s tests -p test_symbol_corpus.py
```

GS-004 adds an in-memory synthetic ARM ELF32 builder. It tests `STT_FUNC`,
mapping-symbol, THUMB-state-bit, `PT_LOAD` source-address, internal-data, and
overlapping-range behavior without committing a binary fixture. Run both suites
with:

```powershell
python -m unittest discover -s tests -p "test_*.py"
```

GS-005 adds synthetic same-PC overlay identities and verifies that cross-overlay
collisions are accepted while unknown identities and manifest/corpus count
mismatches fail.

GS-006 adds synthetic generated-comment parsing and discrepancy classification
tests. The ROM-dependent comparison is a local acceptance run documented in
`docs/GBARECOMP_SCAN_BASELINE.md`; it hash-gates the ROM, validates the scanned
entry target against the TOML, runs the pinned recompiler, and classifies every
scanner/import discrepancy without committing generated output.

GS-007 adds synthetic reset-branch, ELF load-mapping, data-range, reviewed
exception, and proven-seed proposal tests. The ROM-dependent generation and
34-translation-unit x86-64 compile are documented in
`docs/MAIN_TOML_BASELINE.md`.

GS-008 adds a generic upstream runtime test that dispatches two immutable image
identities at the same guest RAM PC and THUMB mode through the real
`runtime_dispatch` hook. It verifies overlapping activation evicts the stale
identity, a partial-range overwrite invalidates the active image, and a mode
mismatch reaches the existing dispatch-miss path. The local 15-test upstream
result and remaining game integration work are documented in
`docs/OVERLAY_RUNTIME_SPIKE.md`.

GS-009 adds an opt-in local runner that links the ignored generated cart corpus
to the pinned platform runtime. Its strict-static acceptance run verifies both
asset identities, executes the recompiled BIOS, statically dispatches the ARM
cartridge reset vector at `0x08000000`, and stops at the first later miss
(`0x080047ae`, THUMB). See `docs/RUNNER_BOOTSTRAP.md`.

GS-010 adds `tools/compare_bios_handoff.py`. It uses the native and mGBA TCP
interfaces with the same real BIOS and exact ROM, normalizes mGBA's pipelined
R15, compares CPU state plus writable memory at `0x08000000`, and diffs
pre-instruction GFP1 records without changing guest control flow. The measured
first architectural divergence is the `DISPSTAT` read at THUMB `0x080030b8`:
native returns VBlank status `1`, while mGBA returns HBlank status `2`. See
`docs/ORACLE_HANDOFF_BASELINE.md`.

### Metadata validation

Required checks:

- Address ranges are within their declared source/runtime image.
- Function boundaries do not overlap incompatible data ranges.
- ARM functions are word-aligned; THUMB functions are halfword-aligned.
- Duplicate runtime PCs are partitioned by overlay identity.
- Every overlay manifest entry references an existing local build output.
- Import is deterministic.

### Integration scenarios

Create deterministic input scripts and checkpoints for:

1. BIOS → cartridge handoff.
2. Title screen idle window.
3. Title menu navigation.
4. New-game intro.
5. First Vale movement/dialogue/map transition.
6. First field Psynergy action.
7. First battle.
8. Save and reload.
9. Representative summon/affine effect.
10. Representative overlay replacement at the same RAM range.

### Full-playthrough regression

Maintain a scenario matrix rather than relying on one casual save:

- Areas and overlays visited.
- Battles and effects exercised.
- Menus/subsystems used.
- Save operations.
- Known optional branches/cutscenes.
- Coverage misses encountered.

## Strict-static gate

A release candidate must run the acceptance suite with the upstream strict-static mechanism enabled, caches/healing disabled as required, and a report proving:

- Zero interpreted PCs.
- Zero JIT-healed PCs.
- Zero cache-healed PCs.
- No unknown overlay dispatch.

## CI policy

Public CI must not require or fetch ROM/BIOS data.

CI may run:

- Formatting/linting.
- Python unit tests using synthetic fixtures.
- CMake configure/build for non-ROM tools.
- Manifest/schema validation using synthetic examples.

ROM-dependent acceptance remains a documented local/private workflow unless a legally redistributable synthetic test ROM is used.
