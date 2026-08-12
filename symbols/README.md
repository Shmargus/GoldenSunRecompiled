# Symbols

This directory will contain minimal deterministic metadata produced by reviewed importers.

Do not paste or vendor Golden Sun assembly. Before committing generated symbol tables, review `LEGAL.md` and ensure they contain only the minimal factual metadata required by the project.

`schema-v1.json` defines the reviewable JSON interchange format. Validate a
corpus with `python tools/validate_symbol_corpus.py <corpus.json>` before it is
used as an importer result or static seed input.

`overlay-manifest-schema-v1.json` defines compressed source, decompressed image,
runtime, and per-overlay count metadata. Validate it together with its
partitioned corpus using `tools/validate_overlay_manifest.py`.
