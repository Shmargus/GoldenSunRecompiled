# GS-006 Exact-ROM Discovery Baseline

## Scope

GS-006 measures the pinned `gbarecomp` discovery result against the GS-004
main-symbol corpus. It does not promote scanner output into reviewed symbols,
data ranges, code-copy declarations, or production configuration.

The run is bound to:

- Golden Sun USA/Europe ROM SHA-1:
  `5c4695205413df7db52b9a184815a07783999971`
- `gbarecomp` revision:
  `af51d0e48e847ae2f056fe01560fb9f087ea27f8`
- `goldensun-disasm` revision:
  `0fa7b312199c10b96544e825be86cfc476493eb7`
- conservative configuration: `config/usa-main-scan.toml`

The private ROM and generated C++ remain under ignored local paths. No ROM,
BIOS, extracted asset, or generated ROM-derived body is part of this evidence.

## Header gate

The pinned `gba_scan` reported:

- ROM size `0x00800000`
- reset branch word `0xea0000ee`
- decoded cartridge entry target `0x080003c0`
- title `Golden_Sun_A`, game code `AGSE`, maker code `01`
- valid Nintendo logo and header complement
- `FLASH_V` at ROM offset `0x00007a00`, classified as 64 KiB flash
- overall scan result `ok=1`

This corrected the stale `0x08000000` example entry. The GS-006 configuration
uses only the hash-gated cartridge layout and decoded entry target. It contains
no manual function seeds, data ranges, jump tables, exclusions, or code-copy
mappings, and speculative literal harvesting is disabled.

## Reproduction

Set the following paths to private/local files and pinned build products:

```powershell
$rom = '<private hash-verified ROM>'
$scan = '<pinned gbarecomp build>\gba_scan.exe'
$recompile = '<pinned gbarecomp build>\gba_recompile.exe'
$elf = '<pinned goldensun-disasm checkout>\goldensun.elf'
$python = '<Python 3.12 or newer>\python.exe'

& $python tools/compare_gbarecomp_discovery.py `
  --rom $rom `
  --gba-scan $scan `
  --gba-recompile $recompile `
  --config config/usa-main-scan.toml `
  --generated-dir local/gs006/unseeded `
  --main-corpus local/symbols/main-symbols.json `
  --main-report local/symbols/main-symbols-unresolved.json `
  --main-elf $elf `
  --output local/gs006/discovery-comparison.json `
  --gbarecomp-revision af51d0e48e847ae2f056fe01560fb9f087ea27f8 `
  --disasm-revision 0fa7b312199c10b96544e825be86cfc476493eb7 `
  --max-functions 4096
```

`--max-functions` is a bring-up bound, not a strict cap on the required
entry-point closure. The pinned finder propagates the required status to direct
branch and proven mode-switch targets, so this run legitimately emitted more
than 4,096 functions.

## Measured result

The unseeded entry-point closure contains:

- 9,115 scanner functions: 8 ARM and 9,107 THUMB
- 3,853 functions with an indirect control-flow site
- 78,107 observed direct branch targets
- 4 undefined decodes reported by the scanner summary

Comparison against 2,977 imported main functions produced:

- 982 exact entry-address and mode matches
- zero mode conflicts
- 8,133 scanner-only entries
- 1,995 imported-only entries

All 982 exact entry/mode matches have shorter scanner extents than the imported
`STT_FUNC` extents. The finder clamps emitted extents at later discovered
same-mode entries; this agrees with the 8,121 scanner-only entries located
inside an imported extent. These are boundary/alternate-entry discrepancies,
not evidence that either side should be accepted wholesale.

Scanner-only classification:

| Classification | Count | Interpretation |
| --- | ---: | --- |
| Interior entry in imported extent | 8,121 | Requires boundary/alias analysis. |
| ELF code without `STT_FUNC` | 6 | Review the contiguous THUMB span `0x0800230c..0x08002344`. |
| Entry in ELF-marked data | 3 | False-code candidates at `0x08084b1c`, `0x08307354`, and `0x0830735c`. |
| Outside an executable ELF section | 3 | False-code candidates at `0x08346bac`, `0x08387008`, and `0x08387014`. |

Imported-only classification:

| Classification | Count | Interpretation |
| --- | ---: | --- |
| Proven function not reached | 269 | Valid metadata not in the unseeded entry closure. |
| Unresolved extent not reached | 1,712 | Still requires boundary evidence. |
| Runtime code copy not scanned | 14 | IWRAM functions require reviewed source/destination/lifetime metadata. |

The six entries in ELF-marked code coincide with the GS-004 excluded zero-size
symbols around `0x0800231c`/`0x08002322` and adjacent fragments, but the scan
does not by itself prove function boundaries. No symbol is changed by GS-006.

## Determinism and artifacts

Two complete comparison runs produced the identical report SHA-256:

`70f8b542993678428a262d49673da97793b36d343ec38ebdcda941c452833d87`

Recorded input hashes inside the report are:

- `gba_scan.exe`: `ad3cf8a7179b10db4c23e9db498dad9a749d53edac38c6221f65371ec7b67575`
- `gba_recompile.exe`: `de64b46ab2e00993423829e5167df38c00ccdc35b4dafc4e6527d05152f79fc6`
- `config/usa-main-scan.toml`: `fb7d062986d83377a27be5d6d377dc00057e96ff4e7fe7a016a5105f51d0b619`

The detailed report and generated C++ live in `local/gs006/` and are ignored.

## Stop conditions and next evidence

The scanner output is not suitable as a seed corpus yet. Before production
generation:

1. Resolve the three data entries and three out-of-section entries without
   broad discovery heuristics.
2. Locate the four undefined decode PCs from a more detailed upstream trace or
   diagnostic output.
3. Review the six code-without-`STT_FUNC` entries against disassembly control
   flow.
4. Establish IWRAM code-copy source, destination, size, activation, and
   lifetime from ELF/runtime evidence.
5. Reconcile imported boundaries and scanner interior entries conservatively.

Until those items are evidenced, the production main-ROM TOML and generated
corpus acceptance gates remain open.
