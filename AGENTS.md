# Repository Guidelines

## Mission and roadmap

Build a faithful native x86-64 static recompilation of the USA/Europe release of **Golden Sun** for GBA using `gbarecomp`.

Work in this order:

1. Reach a correct, reproducible, fully playable baseline with original timing and 240x160 output.
2. Stabilize static coverage, performance, audio, graphics, saves, and GBA hardware behavior.
3. Add opt-in native PC subsystems and enhancements: higher-quality audio, higher-resolution graphics, widescreen, frame interpolation, accessibility, and mod support.

Enhancements are real project goals, but they must not hide regressions or replace the faithful verification path. Mods intentionally change behavior, so keep them disabled during baseline acceptance. A human-readable decompilation is a separate future project.

## Read before substantial work

Read:

1. `TECHNICAL_HANDOFF.md` for the current state, commands, measurements, failed approaches, and recommended next work.
2. `README.md`, `PROJECT_PLAN.md`, `ARCHITECTURE.md`, `TESTING.md`, and `LEGAL.md`.
3. `docs/OVERLAYS.md` when work touches RAM code or overlays.
4. The pinned upstream `gbarecomp/PRINCIPLES.md`, `DEBUG.md`, `TCP.md`, and TOML schema when relevant.

Stricter upstream rules win. Prefer current code and measured results over stale notes, then update the handoff.

## Agent orchestration

**Sol orchestrates; Sol is not the normal worker.**

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

Use the exact commands and deterministic reference route in `TECHNICAL_HANDOFF.md`. Prefer normal non-LTO builds for iteration unless LTO is the subject being measured.

1. Confirm whether the change belongs in `GoldenSunRecomp` or upstream `gbarecomp`.
2. Reproduce the current baseline.
3. Make the smallest evidence-backed change.
4. Run focused tests, then the milestone acceptance scenario.
5. Check semantic invariants, dispatch misses, static coverage, and relevant performance numbers.
6. Rebuild GoldenSunRecomp after upstream changes and confirm which binary ran.
7. Update `TECHNICAL_HANDOFF.md`, `ROADMAP.md`, or supporting docs when durable evidence changes.

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

Keep `TECHNICAL_HANDOFF.md` compact and durable: conclusions, measurements, changes, failed approaches worth avoiding, unresolved issues, and the exact next step. Do not turn it into a chat log.
