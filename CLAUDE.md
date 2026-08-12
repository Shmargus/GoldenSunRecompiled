# CLAUDE.md — GoldenSunRecomp

`AGENTS.md` is the binding rulebook for all work in this repository. Read it. It overrides this file wherever they differ.

## Division of labor: Opus plans, Sonnet executes

This project runs a two-tier model split. **Delegate to the `gsrecomp-worker` subagent automatically — no need to ask first.** This is a standing instruction; treat it as the default for the repo rather than something to confirm each time.

### Delegate to `gsrecomp-worker` (Sonnet)

Hand off once the *what* and *why* are settled and only the *doing* remains:

- Implementing a change you've already specified
- Running builds, the Python suites, CTest, linters
- Running importers, validators, and comparison tools
- Reproducing a documented baseline or a known repro
- Locating code, symbols, or evidence across the tree
- Mechanical refactors, docs and config updates
- Verifying that a fix holds

Give it: the milestone/task ID, the hypothesis, the concrete change, and the acceptance criteria. Vague handoffs produce vague work.

### Keep in Opus (do not delegate)

- Whether a fix belongs here or upstream in `gbarecomp`
- Whether the evidence for an address, mode, or data range is actually sufficient
- Diagnosing a divergence that isn't yet synchronized to a first differing event
- Deciding what the first divergence *means*
- Scope, architecture, and milestone sequencing
- Anything where two readings of the task lead to materially different work

### Reviewing the handoff

The worker reports what it changed, the real command output, evidence gaps, and what it deliberately did not fix. Read that report critically — a clean report is not the same as a correct change. Verify claims that matter before building on them, especially anything touching evidence status or `generated/**`.

Multiple independent well-scoped tasks can go to several workers in parallel. Dependent work does not — sequence it.

## Verification defaults

The Python suites need no ROM and are the cheapest honest check:

```powershell
python -m unittest discover -s tests -p "test_*.py"
```

ROM-dependent acceptance runs are local-only and hash-gated. Public CI must never require ROM or BIOS data.

## Reminders that survive delegation

Delegation does not relax any rule in `AGENTS.md`. In particular: `generated/**` is never hand-edited, no address or ARM/THUMB mode is ever guessed, fixtures are synthetic, and no ROM/BIOS/asset bytes enter the repository — including in a subagent's report.
