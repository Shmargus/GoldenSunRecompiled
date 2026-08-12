# Symbol Import Design

## Goal

Create a deterministic, reviewable function corpus from a local hash-matched `gsret/goldensun` build without copying its assembly into this repository.

## Preferred evidence order

1. ELF symbol table and mapping symbols.
2. Linker map and section addresses/sizes.
3. `arm-none-eabi-objdump` disassembly metadata.
4. Linker scripts.
5. Assembly source parsing only when the compiled metadata is insufficient.

## Required output fields

```text
binary_id
runtime_address
source_address
size
mode             # arm | thumb
name
overlay_id       # empty for main image
section
evidence_source
evidence_revision
confidence        # proven | unresolved; never guessed
```

The version-1 JSON schema uses `evidence_source` (rather than the older
`evidence_file` draft name) so an ELF symbol table, mapping-symbol stream, map
record, or other non-file locator can be cited precisely. The canonical schema
is `symbols/schema-v1.json`.

Each corpus is a JSON object with `schema_version: 1` and a `symbols` array.
Addresses are normalized lowercase strings (`0x` plus eight hexadecimal
digits); THUMB-state pointer bits are not stored in the address. Entries are
sorted by `(binary_id, overlay_id, runtime_address, name)`.

`size` is the declared evidence-source extent, not permission to decode every
byte as an instruction. If ARM mapping symbols expose an internal data or mode
transition, the record uses `confidence: unresolved` and the importer writes
the exact transitions to its companion report. Only `confidence: proven`
records may become static seeds without additional review.

## Mode inference

Do not infer mode from naming conventions alone. Use ARM ELF mapping symbols (`$a`, `$t`, `$d`), symbol metadata, and instruction-alignment/decoder validation. Odd function-pointer values may encode THUMB state but should be normalized carefully and supported by evidence.

## Boundary inference

A symbol address does not automatically prove its size. Determine boundaries from:

- STT_FUNC size where available.
- Next compatible function in the same executable section.
- Linker/disassembly declarations.
- Control-flow validation.

Never let an inferred boundary run across a mapping-symbol mode change or data region.

## Overlay collisions

Two overlay functions can share a runtime address. The primary key must include `binary_id`/`overlay_id`; a global address-keyed map is invalid.

The main image uses an empty `overlay_id`. Overlay identities must be stable and
must not depend on a local absolute path. Duplicate runtime addresses and names
are rejected only within the same `(binary_id, overlay_id)` identity, allowing
the intentional cross-overlay collision represented by the synthetic fixture.

## Validation

Importer must report:

- Duplicate names.
- Duplicate address within one binary.
- Same runtime address across overlay binaries.
- Unknown mode.
- Zero/negative/overlapping sizes.
- Symbols outside declared source/runtime range.
- Functions entering known data.

Excluded entries and unresolved mapping transitions are written to a companion
report. Both `confidence: unresolved` corpus entries and fully excluded entries
are barred from static seeds until reviewed.

Run the standard-library-only validator with:

```powershell
python tools/validate_symbol_corpus.py tests/fixtures/symbols/valid_main.json
```

Fixtures under `tests/fixtures/symbols/` are deliberately synthetic and contain
no ROM-derived bytes or Golden Sun addresses.
