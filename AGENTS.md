# Repository Guidelines

## Mission and roadmap

Build a faithful native x86-64 static recompilation of the USA/Europe release of **Golden Sun** for GBA using `gbarecomp`.

## New-session map

Read only these first, in order:

1. `AGENTS.md` — rules and safety boundaries.
2. `docs/STATUS.md` — current milestone and build.
3. `docs/NEXT_TASK.md` — one immediate acceptance task.
4. `docs/ACTIVE_ISSUES.md` — short open-issue index.

Then load exactly the linked detail file needed for the task:

- Crash/dispatch/RAM miss → `docs/issues/CRASH-*.md`,
  `docs/issues/PERF-08.md`, or `docs/issues/COV-01.md`; add
  `docs/OVERLAYS.md` for overlay/RAM work.
- Widescreen/culling/presentation → `docs/issues/WIDE-01.md` and
  `docs/features/WIDESCREEN.md`.
- MP2K/audio → `docs/issues/AUD-03.md`, `PERF-03.md`, or `AUD-01.md`, then
  `docs/features/MP2K.md`.
- Performance/timing → matching `docs/issues/PERF-*.md` or `PRES-*.md`, then
  `docs/features/ENHANCED_TIMING.md` when timing is involved.
- Planning/wishlist → `docs/BACKLOG.md`; deferred work → `docs/PARKED.md`.
- Cheats → `docs/features/CHEATS.md`.
- Closed evidence → `docs/history/` only when explicitly auditing history.

Do not load unrelated issue or feature files. Compatibility stubs retain old
paths for links; canonical content is under `docs/issues/` and
`docs/features/`.

`docs/history/` is closed evidence, not current guidance; never use it as a
task plan. `local/` and `logs/` hold machine-local notes and runtime logs;
`build/` holds generated output.
For gameplay, launch only the root `GoldenSunLauncher.exe`; the user performs
manual tests.

Work in this order:

1. Reach a correct, reproducible, fully playable baseline with original timing and 240x160 output.
2. Stabilize static coverage, performance, audio, graphics, saves, and GBA hardware behavior.
3. Add opt-in native PC subsystems and enhancements: higher-quality audio, higher-resolution graphics, widescreen, frame interpolation, accessibility, and mod support.

Enhancements are real project goals, but they must not hide regressions or replace the faithful verification path. Mods intentionally change behavior, so keep them disabled during baseline acceptance. A human-readable decompilation is a separate future project.

## Read before substantial work

After routing, read only the selected task file plus these general references
when relevant:

1. `README.md`, `PROJECT_PLAN.md`, `ARCHITECTURE.md`, `TESTING.md`, and `LEGAL.md` for broad work.
2. `docs/OVERLAYS.md` and `docs/GS011_TRANSIENT_IMAGES.md` for RAM/overlay work.
3. The pinned upstream `gbarecomp/PRINCIPLES.md`, `DEBUG.md`, `TCP.md`, and TOML schema when relevant.

Stricter upstream rules win. Prefer current code and measured results over stale notes, then update the docs.

`docs/history/` holds closed milestones and past sessions. It is kept for the
evidence behind decisions already made. Do not act on anything in it, and do
not cite it as current state.

## Agent orchestration

**Sol orchestrates; Sol is not the normal worker.**

### User session defaults

These are repository-scoped defaults for future sessions:

- Keep replies maximally concise: only essential facts, fewest possible words,
  no preamble, repetition, optional detail, or fluff.
- Always use Luna xHigh subagents for task work; Sol only orchestrates, compares
  evidence, integrates, and reports.
- Ask the user when a material choice is ambiguous; do not guess their intent.
- The user performs all gameplay and other manual testing.
- Always launch the playable build through the repository-root
  `GoldenSunLauncher.exe`, never by launching a child executable directly.
- Run only one build or other CPU-heavy job at a time. Avoid process fan-out that
  could push total CPU or memory usage above 90%; reduce build parallelism or
  pause work when either approaches that limit.

Sol should do only the work that requires global context:

- understand the request and split it into narrow tasks;
- delegate research, searching, implementation, debugging, profiling, and build/test loops;
- compare evidence and make architectural decisions;
- integrate results, perform final validation, and report to the user.

Use lightweight workers for bounded searches, file inspection, log filtering, and evidence summaries. Use implementation workers for code changes, difficult debugging, compiler/recompiler work, and focused test loops.

Sol may work directly only when the task is trivial, delegation would cost more than the work, or integration genuinely requires Sol's context. Do not redo delegated work unless its evidence is incomplete or independent verification is valuable. Avoid assigning multiple agents the same task.

Keep prompts narrow and outputs concise. Filter large logs locally; do not dump them into model context.

## Evidence and correctness rules

State the current milestone and exact hypothesis. Then reproduce, instrument, measure, implement the smallest root-cause change, and verify it.

- Never guess addresses, sizes, ARM/THUMB modes, ranges, overlays, or entry points. Cite evidence or use `TODO-EVIDENCE`.
- Prefer a loud dispatch miss over decoding data as code.
- Static recompilation is the primary execution model. Any supported interpreter or self-healing bridge must be logged, reported, proposed for review, and excluded from fully-static claims.
- Fix the earliest synchronized divergence, not a later symptom.
- Keep Golden Sun metadata here. Put general ARMv4T or GBA hardware fixes upstream.
- Never hand-edit `generated/**`.
- Never shrink valid data/code ranges merely to pass a gate.

Golden Sun copies, relocates, rewrites, and generates ARM/THUMB code in RAM. Never assume RAM code identity without measured evidence.

## Native subsystems and mods

Native audio and graphics paths must be optional layers over a working canonical path. The recompiled guest remains the verification oracle. Where practical, compare native output against the canonical output and fall back loudly on mismatch.

Enhancement and mod builds must remain clearly distinguishable from faithful acceptance runs. Removing an enhancement or mod must restore baseline behavior without changing the static corpus or hardware semantics.

## Protected material and identity gates

Users supply their own ROM and BIOS. Never commit, upload, paste, encode, package, or expose ROM/BIOS bytes, extracted assets, saves, private traces, screenshots, or generated files containing substantial protected data. Use documented synthetic fixtures.

Before ROM-dependent work, verify SHA-1 `5c4695205413df7db52b9a184815a07783999971`. Stop on any ROM or BIOS identity mismatch. Do not transfer addresses or layouts from another revision.

## Build and test workflow

Build and run the build named in `docs/STATUS.md`; it explains which binary is the playable one and why. Prefer normal non-LTO builds for iteration — LTO is off deliberately and has failed here twice.

1. Confirm whether the change belongs in `GoldenSunRecomp` or upstream `gbarecomp`.
2. Reproduce the current baseline.
3. Make the smallest evidence-backed change.
4. Run focused tests, then the milestone acceptance scenario.
5. Check semantic invariants, dispatch misses, static coverage, and relevant performance numbers.
6. Rebuild GoldenSunRecomp after upstream changes and confirm which binary ran.
7. Update `docs/STATUS.md`, `docs/ACTIVE_ISSUES.md`, `ROADMAP.md`, or supporting docs when durable evidence changes.

Public checks must use no protected data. Typical commands are:

```powershell
python -m unittest discover -s tests -p "test_*.py"
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
python tools/audit_public_repo.py
```

## Git, PR, and handoff discipline

Preserve unrelated user changes. Keep commits to one research conclusion or root-cause fix where practical. Do not mix formatting sweeps with behavior changes.

Implementation PRs must state the milestone, root cause or question, changed subsystem, exact commands, before/after evidence, static coverage, remaining divergence, and confirmation that no protected material was added.

Keep `docs/STATUS.md` short and current: where the project is, which build to run, and what is in flight. Keep `docs/ACTIVE_ISSUES.md` as a short index — one link per open issue to its canonical file under `docs/issues/`, with feature, wishlist, and parked links routed to their canonical files.

When an issue closes, remove its open-index entry and record only durable closure evidence in the canonical issue file or `docs/history/` when it is closed evidence. Neither file is a chat log; session-by-session accounts belong in `docs/history/` or nowhere.
