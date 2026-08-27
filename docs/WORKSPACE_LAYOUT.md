# Workspace layout

Keep the repository root focused on source, build entry points, and routed
documentation:

```text
Golden Sun Recompiled\
├─ src\                 launcher and project runtime
├─ gbarecomp\           upstream recompilation engine
├─ generated\           generated code; never hand-edit
├─ assets\ symbols\    project assets and symbol data
├─ tests\ tools\       tests and developer utilities
├─ config\ scripts\    build configuration and developer scripts
├─ docs\
│  ├─ STATUS.md         current milestone/build only
│  ├─ NEXT_TASK.md      one immediate acceptance task
│  ├─ ACTIVE_ISSUES.md  short linked issue index
│  ├─ issues\           one canonical file per active issue
│  ├─ features\         widescreen, MP2K, timing, and cheats
│  ├─ BACKLOG.md        wishlist/future enhancements
│  ├─ PARKED.md         deferred work and resume conditions
│  └─ history\          closed evidence; never current guidance
├─ build\               ignored build output
├─ local\ logs\        ignored machine-local data and runtime logs
└─ private\ roms\      user-supplied ROM, BIOS, saves, and states
```

New session: read `AGENTS.md`, then `STATUS.md`, `NEXT_TASK.md`, and
`ACTIVE_ISSUES.md`. Route to exactly one relevant issue or feature file. Do
not load unrelated tracks or use `docs/history\` as current state.

Run the playable build through the repository-root `GoldenSunLauncher.exe`.
Keep ROMs, BIOS files, saves, traces, and extracted assets under `private\` or
`roms\`; never commit them.

Compatibility stubs remain at the former MP2K, Widescreen, and Enhanced Timing
filenames. Canonical content is under `docs/features\`.
