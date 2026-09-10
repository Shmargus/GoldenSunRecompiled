# GoldenSunRecomp

A native x86-64 static recompilation of **Golden Sun (GBA)** built on
`gbarecomp`. The game runs and is playable on a local build at 240×160.

You supply your own legally obtained ROM and BIOS; both are hash-verified
locally. No ROM, BIOS, save or extracted asset is in this repository.

## The engine

The emulation and recompilation engine is
[`gbarecomp`](https://github.com/mstan/gbarecomp) by mstan, included here as
the `gbarecomp` submodule and pinned to an exact commit. This project carries
an **edited** copy: the PPU, VRAM trace and ARM runtime have local changes that
Golden Sun depends on, so building against upstream `gbarecomp` will not
reproduce this build. Golden Sun specifics stay in this repository; general
ARMv4T or GBA hardware fixes belong upstream in `gbarecomp`. Upstream is
licensed PolyForm Noncommercial 1.0.0.

**Read `AGENTS.md` first.** It holds the working rules, the build commands, and
a routing table saying which further file to read for a given task. Then
`ROADMAP.md` for the current milestone and `FACTS.md` for what has already been
measured.

Everything else is routed to from there. `docs/OLD/` is superseded and is never
current guidance.
