# Initial Task Backlog

Tasks are ordered. Do not skip ahead because a later task looks more exciting.

## GS-000 — Initialize repository

- Copy this starter into a new Git repository.
- Replace `TODO-PIN` entries after reviewing upstream.
- Enable non-ROM CI.
- Confirm `.gitignore` blocks private data.

Acceptance: Phase 0 repository checks pass.

## GS-001 — Pin and build gbarecomp

- Select an exact commit.
- Build platform tools with CMake.
- Run its ARM/THUMB and GBA unit tests.
- Record compiler/CMake versions.

Acceptance: upstream test suite passes locally without project patches.

## GS-002 — Reproduce gsret Golden Sun build

- Select an exact commit.
- Build from the exact ROM.
- Verify main output and all overlays.
- Record actual outputs and resolve README/status discrepancies.

Acceptance: deterministic inventory report created.

## GS-003 — Design symbol import format

Required fields:

```text
binary_id, overlay_id, name, runtime_address, source_address,
size, mode, section, evidence_source, evidence_revision
```

Acceptance: schema and synthetic fixtures approved.

## GS-004 — Implement main symbol importer

Prefer ELF/map/binutils output over source-text heuristics.

Acceptance: deterministic main function corpus with zero guessed modes.

## GS-005 — Implement overlay inventory importer

- Enumerate overlay IDs.
- Source and runtime ranges.
- Functions/modes.
- Duplicate runtime addresses.

Acceptance: manifest validator proves collisions are partitioned.

## GS-006 — Scan exact ROM with gbarecomp

- Capture header and entry information.
- Compare discovered code to imported functions.
- Produce differences report.

Acceptance: discrepancies classified; none silently merged.

## GS-007 — Produce main TOML proposal

- Conservative function seeds.
- Data ranges.
- Code-copy ranges.
- Jump tables.

Acceptance: `gba_recompile` completes and generated corpus compiles.

## GS-008 — Overlay runtime design spike

Implement the smallest synthetic proof that two binaries at the same runtime PC dispatch according to active overlay identity.

Acceptance: automated test catches stale dispatch and passes with invalidation.

## GS-009 — Runner bootstrap

Wire Golden Sun target to the pinned platform core without copying Minish Cap game-specific code.

Acceptance: hash-gated executable reaches the first cartridge instruction.

## GS-010 — Oracle harness

Create synchronized state comparison around BIOS handoff and first game functions.

Acceptance: first divergence report identifies exact event/write/PC.
