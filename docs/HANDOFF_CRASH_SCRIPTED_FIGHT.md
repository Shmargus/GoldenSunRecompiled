# Handoff — crash on the first scripted fight

Written 2026-08-07 at the end of a long session. The next session should be
able to start work from this file alone.

## The task

**The first scripted fight crashes the game on load.** Reported from real
interactive play, not from a headless track. Nothing in this repo's automated
coverage reaches it, which is exactly why it went unnoticed.

The reporter has the full terminal output. **Get it and paste it into
`local/crash/terminal.log` before doing anything else** — the abort line names
the failure class, and the three classes below want completely different
investigations. Do not start guessing before reading it.

### Read the abort line first, then pick the path

| Abort text contains | Class | Where to look |
|---|---|---|
| `STRICT_STATIC dispatch miss for pc=...` | Missing translation | Ordinary crawl. See "The seed loop" below. |
| `unknown transient code identity at ...` | RAM code image | `docs/GS011_TRANSIENT_IMAGES.md`. Needs writer attribution (which DMA/store installed those bytes). |
| `verified transient image ... has no AOT entry for ...` | Missing dispatch entry *inside* an already-identity-verified image | A distinct class from the above — the image is right, the entry table is short. |
| `mode mismatch at ...` | Same address hosting ARM and THUMB code at different times | The D-005 problem. Registry checks mode before identity. |
| anything else / a host-level crash | Genuine codegen or runtime defect | This would be the first of its kind here. Treat with suspicion; verify it reproduces. |

If it is a plain dispatch miss, this is routine and the loop below handles it.
If it is anything else, it is more interesting than the usual crawl.

## How to run anything

One command does the whole loop. Read `scripts/gs.ps1` before using it.

```powershell
.\scripts\gs.ps1                      # regenerate -> gate -> build -> verify
.\scripts\gs.ps1 -From verify         # just re-run the tracks
.\scripts\gs.ps1 -From build -To build
.\scripts\gs.ps1 -Frames 21600        # frontier track length
.\scripts\gs.ps1 -Lto                 # ~25 min link; only for throughput numbers
```

Private paths live in `config/local.json` (gitignored). The script refuses to
use a recompiler from `build/`, refuses to run two emulators at once, and fails
if any architectural number moves. All three of those guards exist because each
problem actually bit us today.

## State as of this handoff

| | value |
|---|---|
| `config/usa/main.toml` | regenerated; 16 seeded sections, 187 resume ranges from 81 functions |
| main corpus | ~35,400 functions |
| code coverage (translated) | **94.96%** — `tools/measure_coverage.py` |
| code actually executed by any test | **~17%** — see "Coverage vs execution" |
| campaign 5,400 / no-input 5,400 / campaign 21,600 | all **FULLY_STATIC**, zero misses |

Baselines for regression checks live in `config/usa/acceptance-baseline.json`,
with the reasoning for each figure recorded inline. Read its comments before
changing a number there.

**Coverage is frame-bounded and track-bounded.** `campaign` is ONE deterministic
route. Quote the frame count and the track, always. The crash under
investigation is itself proof that a route we do not drive finds things ours
does not.

## What changed today, and why it matters to this crash

1. **The recompiler learned to resume into a THUMB `BL_suffix`.** A THUMB `BL`
   is architecturally two instructions and an IRQ can land between them; the
   engine previously asserted that was impossible and dropped such seeds. It is
   not impossible, and the game does it. This unblocked **717** entry points at
   once. Fixed in the `gbarecomp` checkout, `src/recompile/function_finder.cpp`,
   with tests. **If the crash is a dispatch miss at an odd-looking interior PC,
   check whether it is a BL suffix before assuming anything else.**
2. **A follow-on fix** made that path honour the existing non-returning
   long-branch rule (a `BL` whose continuation lands in a `[[data_range]]` is a
   long branch, not a call).
3. **Eight more ROM sections seeded** (`rom_b5000`, `rom_b0000`, `rom_f6000`,
   `rom_f9000`, `rom_f2000`, `rom_f4000`, `rom_f0000`, `rom_185000`), taking
   coverage 87.69% -> 94.96%. Zero collisions.
4. **`GBARECOMP_FUNC_COVERAGE=<path>`** now records which translated functions
   actually executed. Free when unset, no effect on execution (proven by
   identical fingerprints), deterministic, and it discriminates between input
   sequences. **This is the most useful new tool for this crash**: capture
   coverage from a run that reaches the fight and diff it against one that does
   not.
5. **`tools/explore_inputs.py`** — coverage-guided input explorer. 250
   iterations took reached 6,139 functions vs 4,108 for the best scripted
   track. It plateaus once it hits content that needs real play skill.

## The seed loop (for the ordinary case)

1. Run until it stops; note the PC.
2. `python tools/resolve_miss_functions.py --elf <elf> <log>`
3. If it reports a containing sized `STT_FUNC`: verify against the ELF yourself
   — entry mapping symbol (`$t`/`$a`) matches the observed mode, and the PC is
   NOT inside a `$d` run. Then add one `REVIEWED_RESUME_FUNCTIONS` entry in
   `tools/build_main_toml.py`.
4. If it reports `NO CONTAINING STT_FUNC`: it needs writer attribution, not a
   seed. See `docs/GS011_TRANSIENT_IMAGES.md`.
5. `.\scripts\gs.ps1` and let the gate decide.

A resume range may sit inside a declared `[[code_copy]]` runtime span — that is
how the RAM-space `Func_8d4` at `0x03000164` was seeded today. RAM images
reached through an `[[extra_func]]` `source_addr` have no declared span and
cannot use resume ranges at all.

## Coverage vs execution — the number that reframes everything

95% of the game's code is translated. **About 17% of it has ever executed.**
Our best headless track touches 4,108 of 36,181 functions.

So "94.96% translated" says almost nothing about correctness. Most of the
corpus has never run once. Expect more crashes of this kind as real play
reaches new content, and do not treat the coverage percentage as reassurance.

## Open questions, all genuinely unresolved

- **~1% cycle drift between 10,800 and 21,600 frames.** Bisected: a
  10,800-frame run is bit-identical to the pre-change baseline, so nothing moved
  before that point. Likely mechanism — IRQ recognition happens at host function
  boundaries rather than at every guest instruction, so changing how functions
  are partitioned shifts interrupt timing. **Not confirmed.** Related to the
  still-open GS-010 `DISPSTAT` item. This could plausibly be relevant to a crash
  in timing-sensitive code, which a scripted fight may well be.
- **No oracle comparison has ever been run against the current state.** We have
  never checked our execution against a reference emulator at this revision.
  For a crash, that is the sharpest available tool and it is sitting unused.
- **142 mapping symbols in `goldensun.elf`** have values outside their own
  section's range (all high byte `0x10`). Meaning unknown. `measure_coverage.py`
  excludes them rather than guessing. A question for the disassembly project.
- **The coverage denominator is inflated.** `_call_via_lr` at `0x0800731e`,
  6,470 bytes counted as "code", is an alignment pad plus a pointer table plus
  ~1,037 words of zeros. Real coverage is better than 94.96%. Do not seed it.
- **Performance.** ~1.09x realtime, which is poor given the hardware gap. Known
  causes: per-instruction cycle bookkeeping, unconditional flag computation,
  bus-function memory access, and I-cache pressure from a 144 MB binary (the
  corpus growing 34% dropped throughput 1.50x -> 1.09x). Not a crash concern,
  but the reporter has noticed slowdowns in the boulder scene, psynergy casting,
  and text boxes. **Fix the crash first.**

## Rules that do not relax

From `AGENTS.md`, which overrides everything else:

- `generated/**` is never hand-edited.
- No address or ARM/THUMB mode is ever guessed. Evidence or nothing.
- Fixtures are synthetic. No ROM/BIOS/asset bytes enter the repository — not in
  code, not in tests, not in a report.
- Never resolve a `[[data_range]]` collision by shrinking the data range to make
  a gate pass. The data ranges come from the ELF's own mapping symbols. A
  collision is a finding.
- A self-heal-enabled run is a DISCOVERY mechanism, never evidence of static
  coverage. Never quote one as proof of anything.
- A `FULLY_STATIC` headline alone does not establish "no regression" — the
  semantic invariants in `acceptance-baseline.json` are what does.

## Suggested first moves

1. Get the terminal output into `local/crash/terminal.log`. Read the abort line.
   Classify it with the table at the top.
2. Get the reporter's savestate from just before the fight, and a
   `local/play-sessions/*.input` replay that reaches it. Without a reproduction
   this is guesswork. **Pin a copy** — a savestate drifted under us today and
   invalidated a comparison.
3. Capture `GBARECOMP_FUNC_COVERAGE` from the crashing run. The last functions
   executed before the abort are the immediate neighbourhood of the fault.
4. Only then decide whether this is a crawl step or something new.
