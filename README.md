# GoldenSunRecomp

A native x86-64 static recompilation of **Golden Sun (GBA)** built on the
`gbarecomp` ecosystem.

The game runs. The prologue, Vale, the overworld, battles, menus and saves are
playable on a local build. Output is faithful 240×160 at original timing.

The project is evidence-driven by rule: no guessed addresses or CPU modes, no
hand-edited generated code, and no emulator-style fallback hidden behind a
"native" claim. `AGENTS.md` is the binding rulebook.

## Target

- Game: Golden Sun, USA/Europe ROM
- Expected SHA-1: `5c4695205413df7db52b9a184815a07783999971`
- Original CPU: ARM7TDMI / ARMv4T, ARM + THUMB
- Host target: Windows x86-64
- Output: 240×160
- Runtime base: `mstan/gbarecomp`
- Symbol/layout reference: a local checkout of `gsret/goldensun`

## No game data is included

You supply your own legally obtained ROM and BIOS. Both are hash-verified
locally. No ROM, BIOS, save state or extracted asset is in this repository, and
public CI never requires any. See `LEGAL.md`.

## Where to start

Read `AGENTS.md`, `ROADMAP.md` and `FACTS.md` — that is the whole default
reading list. `AGENTS.md` routes to anything further based on what you are
actually working on. Do not read the docs folder to get oriented.

- `AGENTS.md` — binding rules and the session routing table. Read first.
- `ROADMAP.md` — the goal, the current milestone, what is shelved or out of scope.
- `FACTS.md` — measured findings, so they are not re-derived.
- `ARCHITECTURE.md` — technical boundaries, the decision log, upstream pins.
- `LEGAL.md` — ROM, BIOS, asset and licensing boundaries.
- `docs/` — reference notes, routed to when relevant.
- `docs/OLD/` — superseded material. Not current guidance, never a task plan.
- `tools/verify_rom.py` — ROM hash verifier.
- `tools/inspect_environment.py` — local dependency check.
- `tools/audit_public_repo.py` — guard against committing private files.
- `scripts/bootstrap.ps1` / `.sh` — non-destructive setup checks.

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
```

To build:

```
MAKE=C:/msys64/mingw64/bin/mingw32-make.exe cmake --build build/gs011_opt --target GoldenSunRecomp -j16
```

**The `MAKE` variable is mandatory and the forward slashes are mandatory.**
Without it the LTO link runs single-threaded and takes hours instead of
minutes.

Pass your own files with `--bios` and `--rom`, or copy `game.toml.example` to
the ignored `game.toml` and fill in only its private paths.

For gameplay, launch the root `GoldenSunLauncher.exe`, never a child executable
directly — the launcher sets environment the game needs.

## Tests

The Python suites need no ROM and are the cheapest honest check:

```powershell
python -m unittest discover -s tests -p "test_*.py"
python tools/audit_public_repo.py
```

ROM-dependent acceptance runs are local-only and hash-gated. See
`docs/TESTING.md` for oracle and regression strategy.

## Sensitive data

Never submit or commit ROMs, BIOS files, save states, extracted assets, full
memory dumps, or private filesystem paths. ROM-dependent commands must point at
private files outside the repository.

Crash and report tooling defaults to metadata-only output. Memory ranges and
user files are opt-in private exports and must never be uploaded automatically.

For a vulnerability in this project's code, reduce it to the smallest
reproduction using synthetic data. For an upstream `gbarecomp` vulnerability,
report it to the upstream maintainer as well.

## Making changes

Read `AGENTS.md` first — it holds the working rules, the hard rules, and the
routing table.

- **Research or documentation**: state the question, the exact revision and
  tools, the evidence, the conclusion, and the remaining uncertainty.
- **Importer or configuration**: include deterministic tests using synthetic
  fixtures. A parser must fail loudly on ambiguity rather than picking a
  plausible interpretation.
- **Runtime or recompiler fix**: generic fixes belong upstream in `gbarecomp`.
  Include a minimal hardware or documentation basis and a regression test.

Do not mark a milestone complete without its acceptance evidence. Screenshots
are supplementary; synchronised state and event evidence is primary.

## Upstream references

See the upstream inventory in `ARCHITECTURE.md`. Upstream projects change
quickly; pin commits and re-check their licences before publishing binaries.
