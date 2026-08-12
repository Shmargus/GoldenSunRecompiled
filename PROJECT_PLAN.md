# Project Plan

## Objective

Create a maintainable native Windows x86-64 recompilation of Golden Sun that executes the original ARM/THUMB game code through `gbarecomp`, models GBA hardware faithfully, requires a user-supplied legal ROM/BIOS, and can eventually complete a full playthrough in strict-static mode.

## Success criteria

The project succeeds when:

1. A fresh checkout can be bootstrapped from documented dependencies.
2. The exact supported ROM and BIOS are hash-gated.
3. Main-ROM code, IWRAM copies, and map overlays are represented by reviewed metadata.
4. The game boots, accepts input, starts a new game, saves/loads, enters battles, and completes a full playthrough.
5. Oracle comparisons show no unexplained baseline divergence.
6. A strict-static acceptance run records zero interpreted or healed PCs.
7. Release packages contain no copyrighted game or BIOS data.

## Explicit non-goals for the baseline

- Rewriting Golden Sun into clean hand-authored C/C++.
- Shipping a ROM or BIOS.
- Replacing the renderer/audio engine.
- Widescreen, HD, 60 FPS, randomizer, translation, or balance changes.
- Supporting all regional revisions from day one.
- Commercial distribution.

## Phase 0 — Repository and evidence discipline

### Deliverables

- Governance files from this starter accepted.
- Upstream repositories selected and pinned.
- Private workspace layout documented.
- CI builds non-ROM tools only.
- ROM verifier and environment inspector run on Windows.

### Acceptance

- Clean checkout passes non-ROM CI.
- No protected game material exists in Git history.
- Upstream commit SHAs and licenses are recorded in `UPSTREAM.md`.

## Phase 1 — Reproducible Golden Sun disassembly build

### Work

- Build the local `gsret/goldensun` checkout from the exact supported ROM.
- Capture main ELF, linker map, overlay ELFs/maps, and checksums outside the public repository.
- Resolve README/status differences against actual build output.

### Deliverables

- `docs/DISASSEMBLY_BUILD.md` with exact commands.
- Machine-readable local build inventory.
- Build verification script that reports but does not copy protected outputs.

### Acceptance

- Main image and every expected overlay build/compare successfully.
- The process is repeatable on a clean workspace.

## Phase 2 — Symbol and function-boundary import

### Work

- Derive symbols from ELF/map/nm/objdump output, not fragile source scraping where avoidable.
- Preserve address, size, name, section, source binary, and ARM/THUMB mode.
- Separate code labels from data labels.
- Detect duplicate runtime addresses caused by overlays.

### Deliverables

- `tools/import_gsret_symbols.py`.
- `symbols/main.tsv`.
- `symbols/overlays/<overlay-id>.tsv`.
- Validation report with unresolved/ambiguous entries.

### Acceptance

- Every emitted function has evidence for address and mode.
- Import is deterministic.
- Re-running produces no diff from identical upstream inputs.
- Ambiguous entries fail or remain explicitly unresolved; they are never guessed.

## Phase 3 — Main ROM static-recompile seed

### Work

- Run `gba_scan` on the exact ROM.
- Create reviewed per-binary TOML for the main ROM image.
- Mark data ranges conservatively.
- Identify code copied from ROM into IWRAM/EWRAM.
- Import direct functions and reviewed jump tables.

### Deliverables

- `config/usa/main.toml`.
- Initial generated coverage report.
- Recompile command script.

### Acceptance

- `gba_recompile` completes without structural contradictions.
- No known data range is decoded as code.
- Generated output builds on x86-64.

## Phase 4 — Golden Sun overlay model

Golden Sun's map code overlays are the project-specific critical path.

### Work

- Inventory every overlay binary.
- Record ROM source range, decompressed size, runtime destination, entry points, lifetime, and load trigger.
- Determine whether overlays are fixed-destination code copies or require a runtime section identity beyond address alone.
- Build a dispatcher key that cannot confuse two overlays sharing the same runtime PC.
- Verify overlay load/decompression events against the oracle.

### Deliverables

- `config/usa/overlays/*.toml`.
- `symbols/overlay_manifest.json`.
- Runtime overlay registration/eviction hooks.
- Overlay-specific unit and integration tests.

### Acceptance

- Every observed overlay execution resolves to the currently loaded overlay identity.
- Loading a second overlay at the same RAM address cannot dispatch stale code.
- Strict mode aborts loudly on unknown overlay execution.

## Phase 5 — Native runner: reset to first cartridge instruction

### Work

- Wire the game repository to the pinned `gbarecomp` target API.
- Hash-verify ROM and BIOS.
- Execute the recompiled BIOS and hand off to cartridge code.
- Add always-on trace and snapshot support.

### Acceptance

- Windows x86-64 executable builds.
- BIOS path is recompiled and measured.
- First cartridge instruction and first identified Golden Sun function execute at matching synchronized state.
- No silent fallback.

## Phase 6 — Early boot and title screen

### Checkpoints

1. Initial stack/CPSR/memory state.
2. IRQ/VBlank setup.
3. DMA and timers.
4. First palette/VRAM/OAM writes.
5. Audio initialization.
6. Title screen.
7. Menu input.

### Acceptance

- Earliest divergence is absent or documented with an approved tolerance.
- Title screen is stable over a fixed frame/event window.
- Menu navigation matches oracle state transitions.

## Phase 7 — New game and Vale vertical slice

### Work

- Intro sequence.
- Dialogue and scripted events.
- Map transitions and first relevant overlays.
- Field movement/collision.
- Psynergy use in the field.

### Acceptance

- Start a new game and reach a selected Vale save checkpoint.
- Save state/snapshot tests reproduce deterministically.
- No stale overlay dispatches.

## Phase 8 — Battle vertical slice

### Work

- Encounter transition.
- Battle backgrounds and affine effects.
- Commands, RNG, damage, animation, audio, and return to field.

### Acceptance

- Complete a chosen early battle with matching deterministic inputs and RNG seed/state.
- Important memory/event checkpoints match the oracle.

## Phase 9 — Cartridge save compatibility

### Work

- Empirically identify save chip/protocol/size.
- Implement through the generic GBA cartridge model.
- Test native save → emulator and emulator save → native.

### Acceptance

- New save, load, overwrite, and checksum behavior round-trip.
- No hard-coded Golden Sun save bypass exists in the runtime.

## Phase 10 — Full-game coverage

### Scenario matrix

- Every town, dungeon, world-map region, and map overlay.
- Djinn acquisition and menus.
- Shops/equipment/status.
- All summon tiers and major battle effects.
- Cutscenes and credits.
- Suspend/save/load behavior.

### Acceptance

- Full playthrough completed from clean save.
- No unresolved crash, hang, save corruption, or major deterministic divergence.
- Coverage misses are triaged into reviewed static seeds.

## Phase 11 — Fully static release gate

### Work

- Run strict-static mode with caches and interpreter/JIT healing disabled.
- Close every dispatch miss.
- Freeze a regression corpus.

### Acceptance

- Full acceptance playthrough reports fully static coverage.
- Reproducible release build on Windows x86-64.
- Release package has no ROM/BIOS/assets.

## Phase 12 — Enhancements, only after baseline

Potential follow-up tracks:

- Scalable presentation and display profiles.
- Widescreen research with explicit scene-policy opt-in.
- Audio quality shadow path with baseline fallback.
- Input remapping and accessibility.
- Mod/patch API at reviewed hook points.
- Additional regions.

Each enhancement must remain optional and preserve a faithful verification path.
