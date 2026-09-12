# GoldenSunRecomp

A native x86-64 static recompilation of **Golden Sun (GBA)** built on
`gbarecomp`. The game runs and is playable on a local build at 240×160.

You supply your own legally obtained ROM and BIOS; both are hash-verified
locally. No ROM, BIOS, save or extracted asset is in this repository.

## The engine

The emulation and recompilation engine is
[`gbarecomp`](https://github.com/mstan/gbarecomp) by mstan. This project
carries an **edited** copy of it, in `gbarecomp/` -- ordinary files in this
repository, not a submodule. The PPU, VRAM trace and ARM runtime have changes
Golden Sun depends on, so building against upstream `gbarecomp` will not
reproduce this build, and there is no upstream commit that describes it.
Golden Sun specifics stay outside `gbarecomp/`; general ARMv4T or GBA hardware
fixes are made inside it and are candidates to send upstream.

Upstream is licensed **PolyForm Noncommercial 1.0.0** (`gbarecomp/LICENSE`,
copyright Matthew Stan), which permits noncommercial use, modification and
redistribution provided the licence travels with the copy. That licence file is
kept in place, and it governs this whole project's use of the engine: **this
project is noncommercial.** Excluded from the copy, as nothing to do with the
engine's working: its `.github/`, `.claude/` and the optional mGBA-backed
`oracle/` (off by default, and it needs a libmgba this repository does not
carry).

Anything the upstream README describes is still in `gbarecomp/README.md`.

**Read `AGENTS.md` first.** It holds the working rules, the build commands, and
a routing table saying which further file to read for a given task. Then
`ROADMAP.md` for the current milestone and `FACTS.md` for what has already been
measured.

Everything else is routed to from there.
