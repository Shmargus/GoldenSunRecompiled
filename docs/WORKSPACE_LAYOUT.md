# Workspace layout

Keep the repository root focused on source, build entry points, and routed
documentation:

```text
Golden Sun Recompiled/
  AGENTS.md             how to work; read first
  ROADMAP.md            goal, milestones, scope
  FACTS.md              measured findings
  ARCHITECTURE.md       boundaries, decision log, upstream pins
  README.md             short pointer to the above
  src/                  launcher and project runtime
  gbarecomp/            upstream recompilation engine
  generated/            generated code; never hand-edit
  assets/ symbols/      project assets and symbol data
  tests/ tools/         tests and developer utilities
  config/ scripts/      build configuration and developer scripts
  docs/                 routed reference notes
    docs/features/      MP2K, timing, cheats
    docs/LEGAL.md       ROM/BIOS/asset boundaries
    docs/TESTING.md     oracle and regression strategy
    docs/OLD/           superseded; never current guidance
  build/                ignored build output
  local/ logs/          ignored machine-local data and runtime logs
  private/ roms/        user-supplied ROM, BIOS, saves, and states
```

New session: read `AGENTS.md`, `ROADMAP.md` and `FACTS.md` - nothing else
by default. `AGENTS.md` carries a routing table for anything further.
Never treat `docs/OLD` as current state.