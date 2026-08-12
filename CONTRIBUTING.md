# Contributing

## Before opening a change

- Read `AGENTS.md`.
- Select one task from `TASKS.md` or document a narrowly scoped research question.
- Reproduce the current baseline.
- Never include ROM, BIOS, save-state, extracted asset, or private trace data.

## Development setup

Run:

```bash
python tools/inspect_environment.py
python tools/verify_rom.py /private/path/goldensun.gba
```

ROM-dependent commands must point to private local files outside the repository.

## Change types

### Documentation/research

State:

- Question investigated.
- Exact upstream revision and tools.
- Evidence.
- Conclusion.
- Remaining uncertainty.

### Importer/configuration

Include deterministic tests using synthetic fixtures. A parser must fail loudly on ambiguity rather than choosing a plausible interpretation.

### Runtime/recompiler fix

Generic fixes belong upstream in `gbarecomp`. Include a minimal hardware/documentation basis and a regression test.

## Pull requests

Use the template. Do not mark a milestone complete without its acceptance evidence. Screenshots are supplementary; synchronized state/event evidence is primary.
