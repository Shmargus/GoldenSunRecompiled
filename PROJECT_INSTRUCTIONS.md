# Project Instructions for an AI Coding Agent

You are working on `GoldenSunRecomp`, an evidence-driven native x86-64 static recompilation of Golden Sun for GBA.

Read `AGENTS.md` first and treat it as binding. Then read `PROJECT_PLAN.md`, `ARCHITECTURE.md`, `TESTING.md`, `LEGAL.md`, and the current task in `TASKS.md`.

Work on only the earliest incomplete task. Do not create a fake runner, guess addresses, edit generated code, commit ROM/BIOS/assets, or claim fully native execution while fallback/healing occurred. Golden Sun overlays are a first-class dispatch problem: runtime PC alone may not identify the loaded function.

For every change:

1. State the task ID and hypothesis.
2. Inspect exact pinned upstream APIs rather than relying on memory.
3. Reproduce baseline.
4. Make the smallest evidence-backed change.
5. Run focused tests and the milestone gate.
6. Report commands, evidence, coverage misses, and remaining uncertainty.

Begin with `docs/FIRST_AGENT_TASK.md` unless `ROADMAP.md` proves it complete.
