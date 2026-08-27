# GoldenSunRecomp

A native x86-64 static recompilation of **Golden Sun (GBA)** built on the
`gbarecomp` ecosystem.

The game runs. The prologue, Vale, the overworld, battles, menus and saves are
playable on a local build. Faithful 240x160 output at original timing is the
default; every PC enhancement is opt-in. See `docs/STATUS.md` for current
state and `docs/ACTIVE_ISSUES.md` for what is still open.

The project is evidence-driven by rule: no guessed addresses or CPU modes, no
hand-edited generated code, and no emulator-style fallback hidden behind a
"native" claim. `AGENTS.md` is the binding rulebook.

## Target

- Game: Golden Sun, USA/Europe ROM
- Expected SHA-1: `5c4695205413df7db52b9a184815a07783999971`
- Original CPU: ARM7TDMI / ARMv4T, ARM + THUMB
- Host target: Windows x86-64
- Faithful default output: 240×160
- Runtime base: `mstan/gbarecomp`
- Symbol/layout reference: a local checkout of `gsret/goldensun`

## No game data is included

You supply your own legally obtained ROM and BIOS. Both are hash-verified
locally. No ROM, BIOS, save state or extracted asset is in this repository,
and public CI never requires any. See `LEGAL.md`.

## Repository map

- `AGENTS.md` — binding rules for coding agents. Read first.
- `docs/STATUS.md` — where the project actually is.
- `docs/NEXT_TASK.md` — the one immediate acceptance task.
- `docs/ACTIVE_ISSUES.md` — the open issue index.
- `docs/issues/` — one current detail file per active issue.
- `docs/features/` — current feature-track evidence and plans.
- `docs/BACKLOG.md` — wishlist/future enhancements.
- `docs/PARKED.md` — deferred work and resume conditions.
- `ARCHITECTURE.md` — technical boundaries.
- `TESTING.md` — oracle and regression strategy.
- `LEGAL.md` — ROM, BIOS, asset and licensing boundaries.
- `PROJECT_PLAN.md` — phases and acceptance gates.
- `docs/` — routed live design and reference notes.
- `docs/history/` — closed milestones and past sessions. Do not act on them.
- `tools/verify_rom.py` — ROM hash verifier.
- `tools/inspect_environment.py` — local dependency check.
- `tools/audit_public_repo.py` — guard against committing private files.
- `scripts/bootstrap.ps1` / `.sh` — non-destructive setup checks.

For a new session, read only `AGENTS.md`, `docs/STATUS.md`,
`docs/NEXT_TASK.md`, and `docs/ACTIVE_ISSUES.md`. Then route to exactly the
relevant file under `docs/issues/` or `docs/features/`; use `BACKLOG.md` or
`PARKED.md` only for planning/deferred work. `docs/history/` is closed evidence,
not current state.

## Recommended workspace layout

```text
GoldenSunWorkspace/
├─ GoldenSunRecomp/        # this repository
├─ gbarecomp/              # upstream platform core
├─ goldensun-disasm/       # local checkout of gsret/goldensun
└─ private/                # never committed
   ├─ goldensun.gba
   └─ gba_bios.bin
```

## First commands on Windows

```powershell
py tools/inspect_environment.py
py tools/verify_rom.py C:\path\to\goldensun.gba
.\scripts\bootstrap.ps1 `
  -RomPath C:\path\to\goldensun.gba `
  -GbarecompPath ..\gbarecomp `
  -DisasmPath ..\goldensun-disasm
```

The bootstrap script verifies prerequisites and paths. It does not generate
guessed config.

## Building

Public builds keep `GSR_BUILD_LOCAL_RUNNER=OFF` and need no ROM or BIOS. A
local playable build supplies the separate core checkout and the ignored
generated directory explicitly:

```powershell
cmake -S . -B build/gs011_opt -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DGSR_BUILD_LOCAL_RUNNER=ON `
  -DGBARECOMP_ROOT=<pinned-gbarecomp-checkout> `
  -DGSR_GENERATED_DIR=<ignored-generated-main-directory>
cmake --build build/gs011_opt --target GoldenSunRecomp
```

Pass your own files with `--bios` and `--rom`, or copy `game.toml.example` to
the ignored `game.toml` and fill in only its private paths. `docs/STATUS.md`
explains why `gs011_opt` is the build to run.

## Tests

The Python suites need no ROM and are the cheapest honest check:

```powershell
python -m unittest discover -s tests -p "test_*.py"
```

ROM-dependent acceptance runs are local-only and hash-gated.

## Upstream references

See `UPSTREAM.md`. Upstream projects change quickly; pin commits and re-check
their licenses before publishing binaries.
