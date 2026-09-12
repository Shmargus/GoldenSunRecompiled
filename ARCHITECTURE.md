# Architecture

## High-level execution path

```text
User-owned Golden Sun ROM
        │ hash verification
        ▼
ROM layout + reviewed symbols + overlay manifest
        │
        ▼
gbarecomp ARMv4T discovery / code generation
        │ generates host C/C++
        ▼
Host compiler (MSVC or Clang/GCC)
        │
        ▼
GoldenSunRecomp x86-64 executable
        │
        ├─ native translated game functions
        ├─ native translated BIOS functions
        ├─ GBA bus/memory model
        ├─ PPU/APU/DMA/timers/IRQ/input/save model
        └─ debug/oracle/coverage infrastructure
```

## Repository boundary

This repository owns only Golden Sun-specific integration:

- Exact ROM identities.
- Main binary and overlay metadata.
- Symbol importers.
- Game runner configuration.
- Native presentation drawn from the game's own data — see below.
- Game-specific regression scenarios.

The `gbarecomp` platform core owns:

- ARM/THUMB decoding and semantics.
- Function discovery and generated-code conventions.
- Runtime dispatch.
- GBA memory map and hardware models.
- Generic debug protocol, snapshots, coverage, and oracle tooling.

## Main ROM versus runtime code

The cartridge image maps at `0x08000000`, but not every executed function necessarily stays there. Golden Sun may copy code into IWRAM/EWRAM and load decompressed map overlays into shared runtime addresses.

The static model therefore needs at least:

```text
Program identity
ROM source address/range
Runtime address/range
ARM or THUMB mode
Function boundaries
Load/unload lifetime
Optional overlay generation/instance identity
```

An address alone is insufficient when two overlays can occupy the same RAM address at different times.

## Overlay dispatch model

Preferred conceptual key:

```text
(runtime_pc, active_overlay_id, instruction_mode)
```

Possible implementation depends on upstream `gbarecomp` APIs. Do not invent a second execution engine. The game repo should register load/copy events and let generic runtime dispatch select the correct translated corpus.

Required properties:

- Overlay activation is tied to an observed ROM→RAM load/decompression event.
- Old entries are invalidated when memory is overwritten.
- Direct branches inside an overlay remain efficient.
- Indirect branches cannot resolve to stale overlay functions.
- Unknown overlay identity becomes a loud miss in strict mode.

## Data ownership

```text
Public repository
├─ source code
├─ importer scripts
├─ symbol names/addresses where legally appropriate
├─ checksums
├─ tests using synthetic data
└─ documentation

Private user workspace
├─ ROM
├─ BIOS
├─ built disassembly binaries
├─ extracted assets
├─ save files
├─ full traces/snapshots
└─ generated corpora containing ROM-derived bytes, unless later proven safe to distribute
```

## Accuracy boundary

The baseline path must use original hardware semantics:

- PPU through palette/VRAM/OAM/register behavior.
- Audio through GBA sound hardware and DMA/timer behavior.
- Saves through cartridge protocol.
- BIOS through recompiled BIOS execution.
- Timing through the runtime scheduler.

Optional enhanced presentation may observe or shadow baseline output later, but it must never become the verification oracle.

## Native presentation

The intended direction (see `ROADMAP.md`) is to draw the field natively from
the game's own map data — reconstructing a room into a buffer when it is
entered, rather than resolving each pixel while rendering. This lives in this
repository, not upstream, because it depends on Golden Sun's map layout.

Two constraints follow from the accuracy boundary above:

- The emulated hardware path remains the oracle. Native drawing is a layer over
  it, never the reference it is checked against.
- The game's own code still runs and still writes hardware registers. Whatever
  it asks for mid-frame must be honoured or the picture is wrong, regardless of
  how the pixels are produced.

An earlier attempt inverted this: it hooked the per-pixel tile lookup and
invented margin content, then culled whatever came out wrong. That approach was
removed on 2026-09-04. It caused both the cost and the visual defects, since a
margin pixel meant a branching host callback and an answer that had to be
guessed.

## Build boundary

Use two build concerns:

1. **Metadata toolchain**: GNU make and ARM binutils for the local Golden Sun disassembly build.
2. **Host runner toolchain**: CMake plus MSVC/Clang/GCC for native x86-64.

On Windows, WSL2 or MSYS2 may be convenient for the disassembly toolchain while the final runner remains a native Windows build. Keep paths and scripts explicit; do not assume one shell can transparently drive the other.

## Decision log

Record durable decisions here. Add an ADR in `docs/adr/` if a decision needs deeper analysis.

### D-001 — Windows x86-64 is the first host target

Status: accepted.

Reason: it matches the initial user's environment and proves ARM/THUMB → host-native translation clearly. Architecture must remain portable enough for later Linux/macOS builds.

### D-002 — Faithful 240×160 baseline precedes enhancements

Status: accepted.

Reason: expanded view, timing changes, and replacement presentation make oracle comparison harder and can conceal baseline faults.

### D-003 — `gbarecomp` is the platform core

Status: accepted. Commit and license pinned 2026-07-18 (see the upstream section below and `docs/GBRECOMP_BASELINE.md`).

Reason: it already provides the ARMv4T and GBA runtime layers. Rebuilding those from scratch would multiply scope.

Caveat: the engine copy in `gbarecomp/` carries generic fixes not yet sent upstream, so gates depending on them are not reproducible from upstream alone. Resolved as of 2026-09-12 for the repository's own consistency: the engine is committed here as ordinary files rather than a submodule pin, so a checkout of this repository always describes a buildable engine. Sending those fixes upstream is still open.

### D-004 — `gsret/goldensun` is consumed through local metadata import

Status: accepted.

Reason: it is valuable layout evidence but has no visible root license. The project should not vendor its assembly.

### D-005 — Overlay identity is part of dispatch correctness

Status: accepted.

Reason: multiple Golden Sun map overlays can use shared runtime addresses. Runtime PC alone may select stale translated code.

### D-006 — Generated code is not reviewed source

Status: accepted.

Reason: correcting generated output by hand hides defects in discovery/config/codegen and breaks reproducibility.

### D-007 — The goal is native execution plus modifiability, not readable source

Status: accepted 2026-09-04.

Reason: stated directly by the user. Run the game on native C++ with the ROM as
assets only, and be able to change how it works — widescreen, turbo, walk
speeds, replacement item and Psynergy tables. Readable or idiomatic generated
code is explicitly not wanted; a faithful but ugly native implementation counts
as success.

Consequence: the code half of this is already largely met by the existing
recompilation. The remaining work is in the hardware layer and in the ability
to modify behaviour.

### D-008 — Draw the field from map data, not per pixel

Status: accepted 2026-09-04. Supersedes the per-pixel margin approach.

Reason: the previous widescreen implementation hooked the PPU's per-texel tile
lookup and invented margin content, then culled wrong answers. That single
design caused both the cost — a branching host callback per margin pixel, not
the pixel count — and the visual defects. Three culls failed against it on
2026-09-03.

Decision: reconstruct a room into a buffer when it is entered and render from
that, so a margin pixel is an array lookup and nothing is invented. The
widescreen implementation was removed entirely to give this a clean slate.

A GPU/OpenGL renderer is **not** required for this and is not planned. Earlier
scoping framed it that way; that was more than is needed.

### D-009 — Full decompilation is out of scope

Status: accepted 2026-09-04.

Reason: roughly 6,000 real functions and 1.3 MB of code, comparable to
decompilation projects that took communities years. It is also unnecessary —
D-007 does not require readable source. Targeted finds of specific data and
functions serve the goal; wholesale translation does not.

### D-010 — Native audio work is shelved, not abandoned

Status: shelved 2026-09-04.

Reason: focus is on map data and native presentation. The native MP2K path was
experimental and fail-closed, with the canonical hardware path as the oracle;
its last probation evidence failed on correlation and ratio, and producer
ownership, fidelity and clock completion were all still open.

State is preserved in `docs/features/MP2K.md` and `docs/OLD/issues/AUD-*.md`.
Do not resume without saying so first.

---

## Upstream inventory and pins

Review and pin exact commits before implementation. Do not develop against a floating default branch indefinitely.

### gbarecomp

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

### MinishCapRecomp

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

### Golden Sun disassembly

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

### mGBA

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

### Golden Sun ROM identity

Supported first target:

```text
SHA-1: 5c4695205413df7db52b9a184815a07783999971
Size:  8,388,608 bytes
```

Do not add additional regions by title or filename. Add them only as separate checksum-anchored targets.
