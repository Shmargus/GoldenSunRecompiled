# Overlay Import Baseline

Recorded for GS-005 on 2026-07-18.

## Evidence

```text
Golden Sun ROM SHA-1: 5c4695205413df7db52b9a184815a07783999971
gsret/goldensun revision: 0fa7b312199c10b96544e825be86cfc476493eb7
overlay ELFs: 96
```

The disassembly Makefile's `overlays/rom_%/orig.bin` rule invokes
`unpack_overlay` with `-a 0x$*`, establishing each directory suffix as a ROM
file offset. The importer requires three additional byte-level checks for every
overlay:

1. Rebuilt `overlay.bin` equals extracted `orig.bin`.
2. Generated `overlay.lz` equals the private ROM slice at the declared offset.
3. The ELF `PT_LOAD` file size equals the decompressed byte count.

All 96 overlays pass all three checks. The compressed ROM range is therefore
measured from exact private bytes rather than inferred from the next directory
name.

Overlay function `source_address` is in the decompressed ELF address domain,
which equals its runtime address in these ELFs. There is no linear function-byte
mapping back into a compressed ROM stream. The manifest separately records the
compressed ROM start, size, and SHA-256.

## Command

```powershell
python tools/import_overlay_inventory.py `
  --disasm-root "$env:GSR_DISASM_ROOT" `
  --rom "$env:GSR_ROM" `
  --manifest-output local/symbols/overlay-manifest.json `
  --symbols-output local/symbols/overlay-symbols.json `
  --unresolved-output local/symbols/overlay-symbols-unresolved.json `
  --evidence-revision 0fa7b312199c10b96544e825be86cfc476493eb7
```

## Result

```text
overlays: 96
runtime base values: 0x02008000
owned function symbols: 9,712
ARM entry modes: 0
THUMB entry modes: 9,712
confidence=proven: 874
confidence=unresolved: 8,838
excluded owned functions: 1
absolute external function symbols: 286,029
cross-overlay collision addresses: 2,283
total compressed bytes: 541,372
total decompressed bytes: 1,072,338
manifest SHA-256: e3f93c8e62f8adcfb7fd97ed52a61d6090749aa512a91bbe5a57b91cc3b1f90b
corpus SHA-256: a4119a24e9642ccedb4d972b5649fec68d2c09b16b90a3d3a4cbbddddcd8b041
report SHA-256: 21fd2e7e52da7d65c91809422fa356f51d8cdc9b9e75478e37d94370b2f70d9a
```

Two consecutive runs produced identical hashes. The manifest/corpus validator
accepts all 2,283 addresses reused by multiple overlay identities while still
rejecting duplicates inside one identity or symbols referring to an unknown
overlay.

The sole excluded owned entry is zero-size `OvlFunc_4560` in `rom_77dd1c`.
The 8,838 unresolved-confidence entries have proven THUMB entry modes but
internal mapping-symbol transitions, so they are not yet eligible as contiguous
static seeds. The 286,029 `SHN_ABS` function symbols are main-binary dependency
references, not overlay-owned code, and are summarized rather than duplicated
into the exclusion report.

This milestone inventories binaries and collisions. Overlay activation,
decompression lifetime, and stale-dispatch invalidation remain later runtime
work; no activation address is inferred here.
