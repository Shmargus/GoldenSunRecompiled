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
- Narrow presentation policy, later.
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

## Build boundary

Use two build concerns:

1. **Metadata toolchain**: GNU make and ARM binutils for the local Golden Sun disassembly build.
2. **Host runner toolchain**: CMake plus MSVC/Clang/GCC for native x86-64.

On Windows, WSL2 or MSYS2 may be convenient for the disassembly toolchain while the final runner remains a native Windows build. Keep paths and scripts explicit; do not assume one shell can transparently drive the other.
