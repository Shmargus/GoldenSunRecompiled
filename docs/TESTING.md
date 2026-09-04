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

Historical milestone notes and closed acceptance records belong in
`docs/history/`; they are not current test status. Use `docs/STATUS.md` for the
current configured build and test result, `docs/NEXT_TASK.md` for the immediate
acceptance scenario, and `docs/ACTIVE_ISSUES.md` for the short issue index.
Open the linked `docs/issues/` or `docs/features/` file for the test's evidence
and closure condition. Public Python tests remain the cheap check:

```powershell
python -m unittest discover -s tests -p "test_*.py"
```

Do not treat ad-hoc executables, `NDEBUG` runs with disabled assertions, or
unrecorded local traces as acceptance evidence.

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
