# GoldenSunRecomp Starter

A planning and bootstrap repository for a **native x86-64 static recompilation of Golden Sun (GBA)** using the `gbarecomp` ecosystem.

This repository is deliberately not presented as a working port. Its first purpose is to make the research reproducible and to prevent agents from hiding uncertainty behind generated code or emulator-style shortcuts.

## Target

- Game: Golden Sun, USA/Europe ROM
- Expected SHA-1: `5c4695205413df7db52b9a184815a07783999971`
- Original CPU: ARM7TDMI / ARMv4T, ARM + THUMB
- Initial host target: Windows x86-64
- Faithful default output: 240×160
- Runtime base: `mstan/gbarecomp`
- Symbol/layout reference: a local checkout of `gsret/goldensun`

## What this starter contains

- `AGENTS.md`: binding rules for coding agents.
- `PROJECT_PLAN.md`: phased implementation plan and acceptance gates.
- `ROADMAP.md`: concise status checklist.
- `ARCHITECTURE.md`: intended technical boundaries.
- `TESTING.md`: oracle and regression strategy.
- `LEGAL.md`: asset, ROM, BIOS, attribution, and licensing boundaries.
- `TASKS.md`: first actionable backlog.
- `PROJECT_INSTRUCTIONS.md`: compact instructions for a coding-agent project.
- `docs/`: focused research and implementation notes.
- `tools/verify_rom.py`: functional ROM hash verifier.
- `tools/inspect_environment.py`: local dependency check.
- `tools/audit_public_repo.py`: guard against accidental private/protected files.
- `scripts/bootstrap.ps1` and `.sh`: non-destructive setup checks.

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

Using sibling checkouts is preferable during early research. Add `gbarecomp` as a submodule only after the upstream revision is intentionally pinned.

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
guessed config. GS-009 now provides a separate opt-in local runner target after
the reviewed corpus has been generated; see `docs/RUNNER_BOOTSTRAP.md`.

## Local runner status

The hash-gated headless runner synchronizes with the mGBA oracle at the BIOS
handoff (`docs/ORACLE_HANDOFF_BASELINE.md`). Execution has since advanced well
past the `DISPSTAT` phase difference recorded there, into sound-driver
bring-up, by way of the GS-011 transient RAM code-image mechanism.

Current boundary: the observed ROM resumes, two compressed EWRAM overlay
identities, the identity-checked `Func_6abc` stack thunk, and two synthesized
ARM identities at `0x03000828` are reviewed AOT inputs. A 600-frame run passes
strict-static. After proving the `0x030035C0` scheduler resume and four
subsequent overlay/callback entries, a reviewed five-way jump table,
`Func_908e0`, and `Func_cacc`, the 1,800-frame boundary is THUMB
`0x0800D7E8`. See
`docs/GS011_TRANSIENT_IMAGES.md`. The build is **not** fully static and this is
a measured bring-up point, not a playable-port claim.

Public builds keep `GSR_BUILD_LOCAL_RUNNER=OFF`. A local build supplies the
separate core checkout and ignored generated directory explicitly:

```powershell
cmake -S . -B build/gs009 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DGSR_BUILD_LOCAL_RUNNER=ON `
  -DGBARECOMP_ROOT=<pinned-gbarecomp-checkout> `
  -DGSR_GENERATED_DIR=<ignored-generated-main-directory>
cmake --build build/gs009 --target GoldenSunRecomp
```

Pass the user-owned assets with `--bios` and `--rom`, or copy
`game.toml.example` to the ignored `game.toml` and fill only its private paths.

## First milestone

The first meaningful milestone is:

> Produce a reviewed symbol corpus and overlay manifest from the exact ROM/disassembly build, then execute the first cartridge instruction through a native x86-64 runner with zero silent fallback.

Do not start widescreen, UI, modding, or higher-framerate work before the game reaches strict, measured baseline milestones.

## Upstream references

See `UPSTREAM.md`. Upstream projects change quickly; pin commits and re-check their licenses before publishing binaries.
