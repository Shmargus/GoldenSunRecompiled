# GoldenSunRecomp

A native x86-64 static recompilation of **Golden Sun (GBA)** built on
`gbarecomp`. The game runs and is playable on a local build at 240×160.

You supply your own legally obtained ROM and BIOS; both are hash-verified
locally. No ROM, BIOS, save or extracted asset is in this repository.

## What is here, and who owns what

`ATTRIBUTION.md` answers this in full, and is the file to keep current. In
short:

- **The game is not here.** No ROM, BIOS, save, or extracted asset, ever. You
  supply your own legally obtained ROM and BIOS; both are hash-verified
  locally, and what the build generates from them stays out of this repository.
  `docs/LEGAL.md` is the rule.
- **The engine is not ours.** `gbarecomp/` is an edited copy of
  [`gbarecomp`](https://github.com/mstan/gbarecomp) by mstan, licensed
  **PolyForm Noncommercial 1.0.0** (`gbarecomp/LICENSE`, kept in place). That
  licence permits exactly this -- use, modify, redistribute -- for
  noncommercial purposes, so **this project is noncommercial.** It is ordinary
  files here, not a submodule, because the engine carries changes Golden Sun
  depends on and no upstream commit describes them;
  `gbarecomp/MODIFICATIONS.md` says what they are. A fix about ARMv4T, GBA
  hardware or the recompiler belongs inside `gbarecomp/` and is a candidate to
  send upstream; anything about Golden Sun stays outside it.
- **The rest is ours** -- `src/`, `tools/`, `tests/`, `config/`, `scripts/` and
  the docs -- and `LICENSE` has not yet chosen terms for it, so all rights are
  reserved for now.
- **Two libraries are fetched at build time**, not stored here: Dear ImGui and
  toml++, both MIT, each pinned to an exact version.

**Read `AGENTS.md` first.** It holds the working rules, the build commands, and
a routing table saying which further file to read for a given task. Then
`ROADMAP.md` for the current milestone and `FACTS.md` for what has already been
measured.

Everything else is routed to from there.
