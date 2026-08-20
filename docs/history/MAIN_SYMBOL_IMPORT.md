# Main Symbol Import Baseline

Recorded for GS-004 on 2026-07-18.

## Evidence gate

```text
Golden Sun ROM SHA-1: 5c4695205413df7db52b9a184815a07783999971
gsret/goldensun revision: 0fa7b312199c10b96544e825be86cfc476493eb7
input ELF: goldensun.elf (locally reproduced from the exact ROM)
```

The importer reads ELF32 metadata directly. Function identity and declared size
come from `SHT_SYMTAB` `STT_FUNC` entries. Entry mode must be proven by both the
ARM `$a`/`$t` mapping-symbol state and the function symbol's THUMB bit. Runtime
addresses come from symbol virtual addresses; ROM source addresses are derived
from file-backed `PT_LOAD` physical addresses. This is significant for
`rom_770`, whose runtime base is `0x03000000` and source base is `0x08000770`.

The importer does not parse assembly sources, infer mode from names, or derive
ROM addresses from section-name arithmetic.

## Command

The real outputs are private, reproducible metadata under ignored `local/`:

```powershell
python tools/import_main_symbols.py `
  --elf "$env:GSR_DISASM_ROOT/goldensun.elf" `
  --rom "$env:GSR_ROM" `
  --output local/symbols/main-symbols.json `
  --unresolved-output local/symbols/main-symbols-unresolved.json `
  --evidence-revision 0fa7b312199c10b96544e825be86cfc476493eb7
```

The ROM size and SHA-1 are checked before the ELF is read.

## Result

```text
ELF STT_FUNC entries: 2,984
Imported corpus entries: 2,977
ARM entry modes: 49
THUMB entry modes: 2,928
confidence=proven: 358
confidence=unresolved: 2,619
excluded entries: 7
corpus SHA-256: 8c4ba15914fb14c16483ce8a10176c191094169e1cb2b25c9208f5d84186d860
report SHA-256: 46bace6fa63fdaefe391a87a572027bcd68e1bb6944fb7510db29ef3f2c2466d
```

Two consecutive runs produced identical corpus and report hashes. The corpus
passes `tools/validate_symbol_corpus.py`.

The 2,619 unresolved-confidence records have proven entry modes and declared
ELF extents, but contain one or more internal mapping-symbol transitions. Most
are normal inline literal pools, such as a THUMB veneer followed by a `$d`
literal. They remain in the factual inventory, while the transition report
prevents them from being treated as contiguous static-code seeds.

Seven entries are excluded completely:

- Three `STT_FUNC` records have zero size.
- `Func_8d4`/`Func_8d8` and `Func_b60`/`Func_b6c` are two pairs of overlapping
  IWRAM extents. Both sides of each ambiguity are reported; neither is selected.

No unresolved-confidence or excluded entry is eligible for a static seed until
its code/data spans or shared-tail semantics are represented without decoding
data as code.
