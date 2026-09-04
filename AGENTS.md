# How to work on this project

Instructions for any agent working here. `ROADMAP.md` says where we are going.
`FACTS.md` holds verified findings — read it before investigating anything, so
you do not re-derive what is already measured.

Written 2026-09-04, replacing the previous `AGENTS.md` and `CLAUDE.md` (both
kept in `docs/OLD/`).

## The goal, in one line

Run Golden Sun on native C++ with the ROM used only for assets, and be able to
change how it works. Readable generated code is explicitly **not** a goal.

## Two hard rules

**1. Never pick a number without measuring it.**

Thresholds, timings, limits, sizes, debounce values, sensitivity. If there is no
measurement, do not choose a value — go and measure, or say plainly that you
cannot. On 2026-09-04 every number chosen without data was wrong, twice in
opposite directions on the same value, while everything measured resolved
immediately.

**2. Stop and ask on any ambiguity.**

The cause, which fix, whether behaviour should change, patch or refactor, which
of several approaches, whether scope should grow, whether a request could be
read two ways. Default when unsure is **ask**. Do not guess the user's intent,
do not pick the approach you personally prefer, and do not keep investigating
just to avoid asking. Keep the question short and in plain language.

## Who does what

**Opus decides and delegates. Sonnet workers implement.**

Opus works directly on:

- Small things, where delegating costs more than doing it
- Anything needing full conversation context or judgement — architecture,
  scope, direction, reading evidence, writing documentation
- Reviewing what workers produce

Sonnet workers handle everything else: implementation, bug fixing, code
inspection, build changes, focused investigations, mechanical refactors.

Only Opus spawns workers. Workers never delegate. One worker per task, never a
chain. If a worker cannot finish, it reports back and Opus decides.

Worker reports are for Opus, not the user. Keep them to a few lines.

## Working principles

Keep it simple. Make the smallest change that solves the current problem.

Do not:

- Build for hypothetical future problems
- Refactor unrelated working code, or clean up while you are there
- Add abstractions, diagnostics or test harnesses not needed right now
- Turn a small fix into an architecture change
- Fix a second problem you noticed — mention it, ask first

Once the cause and fix are clear and unambiguous, make the fix and stop
investigating.

## Testing

The user is the tester. Verify that things compile; beyond that, ask the user to
test rather than building elaborate proofs.

Do not invent tests, create smoke tests on a whim, or rebuild repeatedly to
check theories. If further testing means choosing between approaches, ask first.

## Communication

Short. Plain language, not jargon. Normally say only what was wrong, what
changed, and what to test. No long technical explanations unless asked, no
narrating each step, no dumping logs.

## Builds

One normal build: `build/gs011_opt`, `RelWithDebInfo`.

```
MAKE=C:/msys64/mingw64/bin/mingw32-make.exe cmake --build build/gs011_opt --target GoldenSunRecomp -j16
```

**The `MAKE` variable is mandatory and the forward slashes are mandatory.**
Without it the LTO link runs single-threaded and takes hours instead of
minutes. LTO is ON by default (`GSR_ENABLE_LTO`).

A full link is 15-20 minutes, so batch changes and ask before rebuilding.
Compile-check single files while iterating:

```
ninja -C build/gs011_opt CMakeFiles/GoldenSunRecomp.dir/src/<file>.cpp.obj
```

Do not create alternative, experimental or comparison builds. If an
experimental behaviour is needed, put it behind a toggle in the existing
launcher; normal mode keeps normal behaviour.

Public checks, no protected data:

```
python -m unittest discover -s tests -p "test_*.py"
python tools/audit_public_repo.py
```

For gameplay, launch the root `GoldenSunLauncher.exe`, never a child executable
directly — the launcher sets environment the game needs.

## Evidence rules

- Never guess addresses, sizes, ARM/THUMB modes, ranges, overlays or entry
  points. Cite evidence or write `TODO-EVIDENCE`.
- Prefer a loud dispatch miss over decoding data as code.
- Fix the earliest synchronised divergence, not a later symptom.
- The running emulator is the oracle. A theory that fits the code is not
  evidence; run it.
- Golden Sun copies, relocates and generates ARM/THUMB code in RAM. Never
  assume RAM code identity without measured evidence.
- Keep Golden Sun specifics in this repo. General ARMv4T or GBA hardware fixes
  belong upstream in `gbarecomp`.

## Never

- Commit, upload, paste, encode or expose ROM/BIOS bytes, extracted assets,
  saves, private traces or generated files containing protected data. The user
  supplies their own ROM and BIOS.
- Hand-edit anything under `generated/**`.
- Shrink a valid data or code range just to pass a gate.

Before ROM-dependent work, verify SHA-1
`5c4695205413df7db52b9a184815a07783999971`. Stop on any ROM or BIOS identity
mismatch, and never carry addresses or layouts across revisions.

## Documentation

Three files, kept current:

- `AGENTS.md` — this file. How to work.
- `ROADMAP.md` — the goal, the milestones, what is shelved, what is out of scope.
- `FACTS.md` — verified findings with their evidence. Add to it whenever
  something is measured; never record a guess here.

`docs/OLD/` is superseded material. It is kept because several files hold real
measurements and dead ends worth not repeating, but nothing in it is current
guidance and none of it should be treated as a task plan.

`CLAUDE.md` is a pointer to this file so both Claude Code and Codex load the
same rules.
