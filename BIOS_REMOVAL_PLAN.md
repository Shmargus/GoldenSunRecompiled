# BIOS removal plan

Status: SWI-route and BIOS-PC inventory probes added 2026-09-09; no no-BIOS implementation or
gameplay result is claimed.
The current shadow status in `ROADMAP.md` is unchanged.

## Goal

Run GoldenSunRecomp with the user’s verified Golden Sun ROM, without asking
for, loading, or executing a user-supplied GBA BIOS. The ROM remains the
required asset source and keeps its existing SHA-1 gate. The faithful,
BIOS-backed path remains available as the comparison oracle until this work is
accepted.

“No BIOS” means all of the following:

- no BIOS path, picker, sidecar, size check, or BIOS SHA-1 check is required;
- the guest does not execute the recompiled BIOS or fall through to an
  interpreter/self-heal path sourced from BIOS bytes;
- IRQ entry, every Golden Sun SWI that is reached, boot handoff, and protected
  BIOS-region reads have an explicit replacement policy;
- no BIOS bytes, generated BIOS corpus, or BIOS-dependent cache is needed by
  the no-BIOS run.

## What the source audit establishes

- `gbarecomp/src/runtime/runtime.cpp` always resolves a BIOS asset, then calls
  `GbaBios::load_from_file` before it reads the ROM, creates the bus, or loads a
  savestate. `GbaBios` enforces the 16 KB size and configured SHA-1. The bus is
  wired to that object, and `overlay_loader_init` receives it for BIOS-backed
  self-healing.
- `gbarecomp/src/runtime/bios_hle.cpp` already handles arithmetic, memory-copy,
  affine, decompression, filtering, sound-bias, and MIDI SWIs. Its default
  case returns zero, which deliberately falls through to LLE. The existing
  boot skip only synthesizes post-boot registers and jumps to the cartridge;
  the recompiled BIOS still services IRQs and unhandled SWIs.
- `gbarecomp/src/armv4t/runtime_arm.cpp` sends an unhandled SWI to the
  recompiled BIOS vector at `0x08`. `runtime_irq` enters the recompiled BIOS
  vector at `0x18` and drives it until its IRQ return. `runtime_dispatch` also
  selects the BIOS dispatch table for PCs below `0x4000`.
- `gbarecomp/src/gba/gba_bus.cpp` serves live BIOS bytes while BIOS access is
  enabled. After handoff, protected reads use the latched BIOS prefetch value;
  `runtime_bus_bridge.cpp` updates that state while the executing PC is in the
  BIOS. Passing a null BIOS pointer would return fallback values, but that is
  not yet an intentional compatibility policy.
- `src/launcher_main.cpp` refuses to start when `config/local.json` has no BIOS
  path and always adds `--bios` to the child command. Removing only the
  runtime check would therefore leave the normal launcher unusable.
- `gbarecomp/src/debug/snapshot.cpp` stores the ROM SHA-1 and gameplay state,
  but not ROM or BIOS bytes. `run_game` still loads the BIOS before restoring a
  state. A no-BIOS state must therefore be handled without reopening that
  dependency; a state whose CPU is still inside the BIOS cannot be resumed by
  the no-BIOS backend.

The first inventory probes are now opt-in and preserve the normal path when
unset: `GBARECOMP_SWI_LOG` adds a route column (`0=LLE`, `1=HLE handled`,
`2=HLE fallback`), and `GBARECOMP_BIOS_PC_LOG` writes the executed BIOS PC/mode
set without allocating the full fingerprint ring. Protected BIOS-read
inventory and attribution of BIOS snapshot use by overlay healing remain
pending; no log path or acceptance claim exists for them yet. Both delivered
probes save their CSV on normal runtime exit.

The existing design note in `gbarecomp/ENHANCEMENTS.md` is accurate: current
HLE is “skip intro plus shortcut SWIs on top of a required BIOS.” Its deferred
no-BIOS tier lists the three missing areas: full SWI coverage, an HLE IRQ entry,
and a BIOS-region/open-bus policy.

The implementation choices still depend on measurements: which default-case
SWIs Golden Sun reaches, whether it reads the protected BIOS region after
handoff, which IRQ sources and nesting patterns matter, whether any accepted
state starts in BIOS code, and whether normal Golden Sun execution ever needs
the BIOS image for a self-heal. Until those are recorded, the plan does not
choose a synthetic read value, cycle allowance, or IRQ shortcut.

## Work sequence

### 1. Inventory the Golden Sun dependency

Use the current BIOS-backed run as the oracle and record, without recording
payload bytes:

- every SWI number and caller reached during boot, title, field, battle, room
  transitions, save activity, and savestate/replay paths;
- every guest PC that executes in `0x00000000..0x00003FFF`, including IRQ and
  SWI entry/return paths;
- every data read from the protected BIOS region after cartridge handoff,
  including the open-bus/prefetch value used for it;
- whether the BIOS snapshot in `overlay_loader` is ever needed by Golden Sun’s
  normal path, or only by the generic healing machinery.

The inventory must distinguish a handled HLE SWI from an LLE fallback and a
real BIOS instruction fetch. It is complete only when the existing acceptance
routes have been exercised; static searches alone are not evidence.

### 2. Add an explicit no-BIOS runtime tier

Implement the tier in `gbarecomp`, beside the existing HLE backend, because
asset loading, ARM exception behavior, bus protection, snapshots, and generic
IRQ/SWI semantics are shared by games. Keep the current LLE path and existing
HLE-with-LLE-fallback path unchanged. The new no-BIOS tier starts default-off.

The no-BIOS tier should:

- skip BIOS asset resolution and pass an explicit “no BIOS” state through bus,
  runtime, and overlay-loader initialization;
- synthesize the post-reset state using the existing boot-skip code as the
  starting point, then enter the verified cartridge entry without dispatching
  a BIOS PC;
- make BIOS-region reads and open-bus behavior explicit. Use the inventory to
  decide whether Golden Sun needs a faithful generic policy or only a guarded
  synthetic response; never silently return zero because a pointer is null;
- disable BIOS-backed self-heal and fail loudly if a no-BIOS run attempts a
  BIOS dispatch, BIOS fetch, or unclassified protected read.

This is a runtime capability, not a Golden Sun replacement for the GBA
hardware model. PPU, DMA, timers, IRQ scheduling, sound, input, and saves stay
as they are unless the dependency inventory proves that a BIOS replacement
needs a targeted generic change.

### 3. Replace the remaining SWI paths

Extend the existing HLE dispatcher to cover every SWI the inventory shows in
Golden Sun. The machine-state services currently delegated to LLE are the
likely work: `Halt`, `Stop`, `IntrWait`, `VBlankIntrWait`, `SoftReset`,
`RegisterRamReset`, sound-driver services, and any other reached default case.

Each replacement must preserve the shared CPU, IME/IE/IF, halt, DMA/timer,
memory, and cycle-scheduling behavior. An unsupported SWI is a loud no-BIOS
failure, never an implicit jump to the recompiled BIOS or interpreter. Keep
the existing HLE arithmetic and decompression code as-is until the oracle
comparison identifies a real mismatch.

### 4. Replace BIOS IRQ entry

Add a generic runtime IRQ path that performs the behavior the guest depends on:
exception register banking and return state, the user handler pointer at
`0x03007FFC`, interrupt acknowledgement, nested/wait behavior, and return to
the interrupted guest. Compare it against the current BIOS-backed run at the
IRQ boundary and across VBlank, timer, and save-related activity.

Do not infer this from Golden Sun’s generated functions. The vector and CPU
exception machinery belong in `gbarecomp`; Golden Sun supplies only the
measured acceptance route and any game-specific handler expectations.

### 5. Make launcher, config, and state behavior coherent

During development, expose the mode through the existing launcher “Test
variables” child-environment plumbing. The normal launch remains BIOS-backed,
and the experimental run must work with no BIOS path in `config/local.json` and
without appending `--bios`.

Keep the ROM picker, exact ROM hash, save-file handling, and normal launcher
logging. Add a startup label that makes the selected backend and any guarded
BIOS access visible without printing payloads. Do not make a missing BIOS look
like a corrupted ROM.

Savestate rules:

- retain the existing ROM SHA-1 gate and keep BIOS bytes out of the state;
- load gameplay states directly in no-BIOS mode without resolving a BIOS;
- reject a state whose serialized CPU PC is in the BIOS range, with an explicit
  explanation that the state requires the BIOS-backed boot path;
- also check for an in-flight BIOS call or return into BIOS code, even when
  the saved PC is inside the game's interrupt handler. Establish a reliable
  backend/continuation compatibility check before accepting these states;
- verify input-record/replay startup states use the same backend and do not
  reopen the BIOS picker.

### 6. Validate before changing the default

Run the same route in both backends and compare the first synchronized
divergence, CPU/IO state at handoff, IRQ delivery, SWI results, framebuffer,
save bytes, and state/replay behavior. The no-BIOS route must cover:

- fresh boot through title and gameplay;
- field, battle, and a field-to-room transition;
- save write, reload, and continued play;
- savestate save/load and paired input replay;
- the user’s existing expanded-view route, to show that BIOS removal did not
  alter the unrelated presentation work.

Acceptance requires no BIOS file access, no BIOS PC execution, no BIOS fallback
dispatch, and no unexplained protected BIOS read in the no-BIOS run. The
BIOS-backed path must still pass its existing handoff/oracle checks. Do not
declare success from a boot screen alone; that would miss IRQ, wait, and
save-state dependencies.

### 7. Roll out deliberately

After the acceptance routes pass, make the no-BIOS tier the normal Golden Sun
profile. Keep an explicit
BIOS-backed launch option for regression comparison and retain the current
default-off enhancement rule in the shared runtime for other games.

Make the normal no-BIOS target build and run without the generated BIOS
corpus or BIOS-dependent caches. Gate any BIOS dispatch-table references
behind the optional comparison backend. This verifies that the replacement
does not merely hide a runtime dependency while retaining a build dependency.

## Ownership and limits

`gbarecomp` owns the no-BIOS backend, generic SWI/IRQ semantics, bus policy,
asset resolution, and snapshot lifecycle. This repository owns the Golden Sun
opt-in/default selection, launcher presentation, route coverage, and any
Golden Sun-specific BIOS call inventory.

This plan does not remove RAM self-healing, rewrite the room buffer or sprite
placement work, replace the PPU/DMA/timer/sound models, or begin the shelved
audio work. It also does not delete the BIOS-backed generated corpus before
the no-BIOS path has proved that it can build and run independently. The
optional comparison backend may retain its locally supplied BIOS dependency.

## Source pointers

- `gbarecomp/ENHANCEMENTS.md` — current HLE behavior and deferred no-BIOS
  requirements.
- `gbarecomp/src/runtime/runtime.cpp` — asset resolution, backend selection,
  boot skip, overlay initialization, and savestate order.
- `gbarecomp/src/runtime/bios_hle.{h,cpp}` — current HLE coverage and boot
  state synthesis.
- `gbarecomp/src/armv4t/runtime_arm.cpp` — SWI, IRQ, and BIOS dispatch seams.
- `gbarecomp/src/gba/gba_bus.{h,cpp}` and
  `gbarecomp/src/runtime/runtime_bus_bridge.cpp` — BIOS protection and
  open-bus behavior.
- `src/launcher_main.cpp` — current BIOS-required launcher flow.
- `gbarecomp/src/debug/snapshot.cpp` — ROM-gated state format and load order.
