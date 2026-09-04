# First Agent Task

## Task

Initialize Phase 0 and complete GS-001 without touching game execution.

## Instructions

1. Read `AGENTS.md`, `PROJECT_PLAN.md`, `LEGAL.md`, and `UPSTREAM.md`.
2. Inspect the current `mstan/gbarecomp` repository and select a specific commit.
3. Record its commit SHA and license in `UPSTREAM.md`.
4. Add it as a submodule only after the license review is acknowledged.
5. Build it using its documented CMake commands.
6. Run its full available test suite.
7. Do not add a ROM, BIOS, Golden Sun symbols, generated code, or a game runner.
8. Add a concise `docs/GBRECOMP_BASELINE.md` containing tool versions, commands, test results, and unresolved warnings.
9. Keep the starter's public scaffold build green.

## Acceptance

- Upstream pin is exact and reproducible.
- Tests pass or failures are documented without being hidden.
- No protected files are present.
- No generated code was edited.
- PR includes the evidence required by `AGENTS.md`.
