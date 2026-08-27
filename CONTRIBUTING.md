# Contributing

## Before opening a change

- Read `AGENTS.md`.
- Read `docs/STATUS.md` for where the project is.
- Read `docs/NEXT_TASK.md` and `docs/ACTIVE_ISSUES.md`, then select one linked
  detail file under `docs/issues/` or `docs/features/`. Use `BACKLOG.md` or
  `PARKED.md` only for planning/deferred work.
- Do not use `docs/history/` as current guidance.
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
