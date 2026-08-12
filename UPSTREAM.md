# Upstream Inventory

Review and pin exact commits before implementation. Do not develop against a floating default branch indefinitely.

## gbarecomp

Repository: `https://github.com/mstan/gbarecomp`

Role:

- ARM7TDMI / ARMv4T ARM+THUMB decoder, discovery, IR, and code generation.
- GBA bus, PPU, audio, DMA, timers, IRQ, input, saves, BIOS, runtime, and debug infrastructure.
- Tools including `gba_scan`, `gba_recompile`, and symbol import support.

Current public design states that original GBA machine code is translated into native C/C++ and that generated code must not be hand-edited.

License observed when this pack was generated: PolyForm Noncommercial 1.0.0.

Pinned baseline reviewed on 2026-07-18:

```text
gbarecomp commit: af51d0e48e847ae2f056fe01560fb9f087ea27f8
license reviewed: PolyForm Noncommercial License 1.0.0
```

## MinishCapRecomp

Repository: `https://github.com/mstan/MinishCapRecomp`

Role:

- Reference game-target repository showing how a GBA title consumes `gbarecomp`.
- Useful for repository layout, build wiring, ROM gating, launcher integration, and coverage workflow.

It is a reference, not a template to copy blindly. Golden Sun's map overlay architecture requires a game-specific design.

Pinned reference reviewed on 2026-07-18:

```text
MinishCapRecomp commit reviewed: 201a377f54bc1619691d6acd08734a298a8ab176
license reviewed: PolyForm Noncommercial License 1.0.0
```

## Golden Sun disassembly

Repository: `https://github.com/gsret/goldensun`

Role:

- Symbol and function-boundary evidence.
- Main ROM and overlay layout evidence.
- Local reconstruction/build outputs.

The public README reports that all known code is disassembled and symbolized while data remains largely undisassembled. Its status text may lag later commits, including a commit titled `Compress overlays during build`; verify the actual checked-out build rather than relying on README prose alone.

No root license was visible when this pack was prepared. Do not copy source into this repository.

Pinned local evidence checkout reviewed on 2026-07-18:

```text
goldensun disassembly commit: 0fa7b312199c10b96544e825be86cfc476493eb7
license/permission review: no root license present; local metadata/reference use only
```

The importers do not consume that pin directly. The local checkout carries a
local branch, `data/symbolize-pointer-tables`, built on top of the pin with
the user's own symbolization work and one build-system fix (commits
`2934bde`, `207d3af`, `bc344df`, `f246637`, `6663cd1`, `84a80693003439acdbb78dd84538b0532461ad4b`,
authored 2026-08-05). `local/symbols/main-symbols.json`,
`local/symbols/main-symbols-unresolved.json`, and
`config/usa/main-data-exceptions.json` record `evidence_revision =
84a80693003439acdbb78dd84538b0532461ad4b` - the actual branch HEAD the
`goldensun.elf` used to produce them was built from - not the upstream pin
above. That commit was hash-gate verified on 2026-08-06: a clean `make`
rebuild of `goldensun.gba` from this checkout matches ROM SHA-1
`5c4695205413df7db52b9a184815a07783999971` exactly (`sha1sum -c
goldensun.sha1` -> `goldensun.gba: OK`). A byte-identical ROM rebuild is
strong identity evidence for the source layout even though the branch has not
been reviewed/pinned as an upstream revision; re-running
`tools/import_main_symbols.py` against this ELF reproduced the existing
2977-symbol corpus byte-for-byte (aside from the revision stamp), so this is
a provenance correction, not a re-import.

## mGBA

Repository: `https://github.com/mgba-emu/mgba`

Role:

- Primary software oracle for synchronized execution comparisons.
- Debugger/GDB and state inspection where helpful.

Pin executable/source revision and all settings used to produce reusable traces.

Pinned oracle baseline reviewed on 2026-07-18:

```text
mGBA release: 0.10.5
mGBA source tag commit: 26b7884bc25a5933960f3cdcd98bac1ae14d42e2
oracle settings hash: TODO-BIOS-GATE (configure after the user-supplied BIOS is verified)
```

## Golden Sun ROM identity

Supported first target:

```text
SHA-1: 5c4695205413df7db52b9a184815a07783999971
Size:  8,388,608 bytes
```

Do not add additional regions by title or filename. Add them only as separate checksum-anchored targets.
