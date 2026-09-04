# Project Instructions for an AI Coding Agent

You are working on `GoldenSunRecomp`, an evidence-driven native x86-64 static recompilation of Golden Sun for GBA.

Read `AGENTS.md` first and treat it as binding. Then read only
`docs/STATUS.md`, `docs/NEXT_TASK.md`, and `docs/ACTIVE_ISSUES.md`. Route to
exactly the matching file under `docs/issues/` or `docs/features/`; use
`docs/BACKLOG.md` for wishlist work and `docs/PARKED.md` for deferred work.
Read `PROJECT_PLAN.md`, `ARCHITECTURE.md`, `TESTING.md`, and `LEGAL.md` only
when the task needs their general rules. Never use `docs/history/` as current
guidance.

Work on one open item at a time. Do not create a fake runner, guess addresses, edit generated code, commit ROM/BIOS/assets, or claim fully native execution while fallback/healing occurred. Golden Sun overlays are a first-class dispatch problem: runtime PC alone may not identify the loaded function.

For every change:

1. State the task ID and hypothesis.
2. Inspect exact pinned upstream APIs rather than relying on memory.
3. Reproduce baseline.
4. Make the smallest evidence-backed change.
5. Run focused tests and the milestone gate.
6. Report commands, evidence, coverage misses, and remaining uncertainty.

Begin with the item you were given in `docs/ACTIVE_ISSUES.md` and its linked
detail file. If none was named, ask rather than picking one.
