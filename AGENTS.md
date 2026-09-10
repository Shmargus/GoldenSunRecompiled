# How to work on this project

Instructions for any agent working here. `ROADMAP.md` says where we are going.
`FACTS.md` holds verified findings — read it before investigating anything, so
you do not re-derive what is already measured.

Written 2026-09-04, replacing the previous `AGENTS.md` and `CLAUDE.md` (both
kept in `docs/OLD/`).

## The goal, in one line

Run Golden Sun on native C++ with the ROM used only for assets, and be able to
change how it works. Readable generated code is explicitly **not** a goal.

## Start of session — read these three, nothing more

1. `AGENTS.md` — this file. How to work.
2. `ROADMAP.md` — the goal and the current milestone.
3. `FACTS.md` — what has already been measured.

That is the whole default reading list. **Do not read the docs folder to "get
oriented".** Load a further file only when the task actually calls for it, using
the routing below, and load the specific file rather than its whole directory.

### Load only when the task needs it

| Working on | Read |
|---|---|
| Map data, room layout, reconstructing a room (milestone 1) | `ROADMAP.md` milestone 1 and the map section of `FACTS.md`; the tables live at `0x02010000` and `0x02020000` |
| Rendering, presentation, the room buffer | `ARCHITECTURE.md` — repository boundary and native presentation sections |
| NPCs, sprites, shadows, object placement | `ROADMAP.md` "Sprite placement" (it lists what was already tried and rejected) and the sprite section of `FACTS.md`; the recorder is `src/obj_recorder.h` |
| The function tracer | `src/function_tracer.h` first, then `.cpp` only if changing it |
| Overlays, RAM-resident code, dispatch identity | `docs/OVERLAYS.md`, `docs/GS011_TRANSIENT_IMAGES.md` |
| Symbols, importing names or addresses | `docs/SYMBOL_IMPORT.md` |
| Build failures, profiling, headless runs | the Builds section below, then `docs/DEBUGGING.md` |
| Why something was decided the way it was | `ARCHITECTURE.md` decision log |
| Repository layout, where things live | `docs/WORKSPACE_LAYOUT.md` |
| Audio — shelved, do not start unprompted | `docs/features/MP2K.md` |
| ROM, BIOS, asset or licensing boundaries | `docs/LEGAL.md` |

### Never load these into context

- `docs/OLD/` — superseded. Open a single named file from it only when
  explicitly auditing history, never to orient yourself.
- `local/gs011/` — the generated corpus, ~187 MB across 34 files. Grep it if you
  must; never read a file from it whole.
- `build/` — generated output.
- Subagent transcript files under the scratchpad — they will overflow context.

If you find yourself reading a fourth or fifth file before doing any work, stop
and ask instead.

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

**You are the orchestrator and brains. Luna Max workers are your hands.**

Decide scope, direct workers, and review their results. Use `gpt-5.6-luna`
with `max` reasoning for implementation and investigation. Do execution work
yourself only when Luna is not doing a good job on that task.

Only the orchestrator delegates. Workers never delegate. Give each worker a
bounded task and require it to report back when finished or unable to finish.
Worker reports are for the orchestrator, not the user. Keep them brief.

Never check whether a subagent is done unless the user asks. Require workers
to report back when finished. While a worker is working, wait without doing
other work; resume only when its report arrives.

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

**The user is not a programmer.** Write for someone who knows Golden Sun and
knows what they want the project to do, and who does not read code.

Short. Normally say only what was wrong, what changed, and what to test. No
long technical explanations unless asked, no narrating each step, no dumping
logs.

Always keep feedback concise and limited to necessary, relevant information.
Speak to the user as little as possible.

Say findings in terms of the game and what it means for the project, not in
terms of the machine:

- Not "8 writer PCs in three ROM loops" — "we found the game code that draws
  the world map, in three places".
- Not "the EWRAM observer was never installed" — "the recorder was listening
  on a channel nothing was sending to, so it recorded nothing".
- Not "512 records spanning cycles 7.8M–57.2M" — "it filled up in the first
  few seconds, before you reached the world map".

Addresses, register names, function names and file paths are fine when they
are the *answer* — the user files them away and passes them on. They are not
fine as the *explanation*. Give the plain sentence first; put the identifiers
after it, in brackets or on their own line, for the record.

Every finding should answer, in one sentence: does this move the milestone
forward, block it, or rule something out? A finding reported without that is
not finished.

`FACTS.md` is the exception: it is written for whoever investigates next, so
precision wins there. The message to the user is not.

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

When you measure something, add it to `FACTS.md`. When direction changes, update
`ROADMAP.md`. Neither is a chat log — record the finding, not the session.

`docs/OLD/` is superseded material, kept because several files hold real
measurements and dead ends worth not repeating. Nothing in it is current
guidance and none of it is a task plan.

`CLAUDE.md` is a pointer to this file so both Claude Code and Codex load the
same rules.
