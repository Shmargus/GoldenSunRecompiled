# Active issue index

Updated 2026-08-20. Open items only. Faithful behavior remains the default, and
every enhancement below is opt-in.

Current state in prose: `docs/STATUS.md`. Closed records: `docs/history/`.

## Open

### SESSION-2026-08-16 — Recap: CRASH-04 folded into gs011_opt, world-map speed explained, new crash found

Full findings from today's session, so the next session can resume cold
without re-deriving any of this.

**CRASH-04 fix is now in the build users actually run.** The emitter fix
described under CRASH-04 in "Recently resolved" below was folded into
`build/gs011_opt` (rebuilt 2026-08-16 12:14). The 86-test Python suite and
the `tail_dispatch_tests` ctest target pass against it. `docs/STATUS.md`
previously said this fix was "Not yet folded into `build/gs011_opt`" —
corrected below.

**The "very slow" report was measured and is NOT a code regression.**
Headless, state1 (world map) ran at **0.63x real-time**, with **~94% of
wall time in the interpreter bridge**. Three hot IWRAM PCs dominate the
interpreted instruction count:

| PC | Interpreted instructions |
|---|---|
| `0x0300081C` | ~3.08M |
| `0x030057E0` | ~3.06M |
| `0x03000A58` | ~1.29M |

- `0x0300081C`, `0x03000A58`, and `0x03000994` **do have static dispatch
  entries**, but are rejected by the sticky, never-reset, 256-byte-window
  `runtime_ram_code_range_dirty` guard
  (`gbarecomp/src/armv4t/runtime_arm.cpp:115`) — once any byte in a
  256-byte window is ever seen to change, the whole window is treated as
  dirty forever, so a PC that shares a window with genuinely
  self-modifying code stays interpreted even though its own bytes are
  stable and it has a real static entry.
- `0x030057E0`, `0x03007DD8`, and `0x03007DE8` have **no static dispatch
  entry at all**.

**Self-heal RAM plus a warm cache fixes it.** With
`GBARECOMP_SELFHEAL_RAM=1` and a warm `recomp_cache`, both state1 and
state3 reach **~2.0x real-time with zero interpreted instructions.** A
cold cache is temporarily worse (~0.37x) from first-compile stalls while
each RAM function gets JIT-compiled and cached for the first time.

**Self-heal is enabled only by the launcher, which explains the user's
~20 fps report.** `GoldenSunLauncher.exe` sets
`GBARECOMP_SELFHEAL_RAM=1` (`src/launcher_main.cpp:401`); launching
`GoldenSunRecomp.exe` directly does not, so it runs the ~3x-slower
always-interpret path above. Always launch through the launcher.

**Settings are per-exe-directory, not shared.** `config.ini` and
`keybinds.ini` are read from the running exe's own folder
(`gbarecomp/src/runtime/host_window.cpp:1554-1706`), so the launcher's
folder and each `build/*` folder carry independent overclock/widescreen/
turbo settings. Changing a setting in one does not affect the others.

Two unimplemented candidate fixes carried forward, not yet done:

1. Narrow the `runtime_ram_code_range_dirty` guard from a sticky
   256-byte window to the actual function body, so `0x0300081C` /
   `0x03000A58` / `0x03000994` can use their existing static entries
   instead of interpreting.
2. Discover and register the real function containing `0x030057E0` (and
   `0x03007DD8` / `0x03007DE8`), which currently has no static entry.

Also carried forward as a shipping concern, not yet addressed: the cold-
cache first-launch stall (~0.37x) is a real problem for a first-time
player, since self-heal needs a compiler present at runtime (currently
`gcc` from the pinned toolchain) — a bundled `tcc`-style fallback compiler
was floated as an option but not scoped.

**And now, a new crash — see CRASH-05 immediately below.**

### QOL-01 — QoL menu (walk speed, text speed): both blocked on real RE (2026-08-19)

**Walk speed: PARKED (2026-08-19).** A runtime hook scaling `r6` at the
`0x0800DC7A` accumulate site was built and shipped, gated behind a new F1 →
Enhancements → QoL → Walk Speed control. It had no in-game effect. Instrumented
stderr diagnostics (routed through the launcher's existing session log) proved
why: in the real windowed build the hooked write site never fires and the
accumulator at `0x0203155C` never changes while walking — every original
measurement above (the accumulate/init/recommit pcs, the ramp values) came
from headless runs on `gs011_rel`/an older build and does not transfer to the
current windowed `gs011_opt`. Root cause of *why* the site differs is
unresolved (not investigated further — parked, not fixed). The F1 control is
kept visible but disabled (greyed out) and forced to 1x/off; the underlying
scaling and pending-override code in `runtime_arm.cpp`/`runtime_arm.h` is left
in place, unused while the multiplier is pinned to 1. Do not re-attempt
without new evidence gathered from the actual windowed build.

Planned: a **QoL** section in the F1 menu — Walk Speed 1x/2x/3x/4x, and faster /
instant text. Nothing has been added to the menu yet; this entry records what
the investigation found so the work does not restart from scratch.

**Walk speed — traced to the end, no value to poke.**

- Actor struct base `0x02030CCC`; X at `+0xE4` (`0x02030DB0`), Y at `+0xE8`
  (`0x02030DB4`), both 16.16 fixed. Steady walking approaches 1.0 px/frame after
  a ~20-frame ramp. The community RAM map's `0x0200043C` fields are wrong for
  this ROM.
- Only `0x0801010A` (`str r6,[r2]`) writes Y, once per frame, and `r6` is
  already the final absolute position.
- The accumulator is `0x0800DC7A` (`str r3,[r4,#16]` into `0x0203155C`, r3 =
  old + r6); `0x0800D0BA` re-commits per frame, `0x0800DC3A` initialises on the
  first frame of a move. **`r6` is the per-frame increment** (ramp 0xFDF,
  0x25A2, 0x408D, 0x5895, 0x6D9C, 0x8002 …).
- `r6` is the return value of a subroutine called through a function-pointer
  trampoline (`bx fp`, target from a ROM literal at `0x800DCD8`) — an easing
  function fed a phase counter and a scale. Not a stored number.
- `actor+0x8` (`0x02030CD4`) is NOT speed: it is 0 while walking straight, the
  `adds r6,r6,r0` at `0x080100EA` is gated on it being non-zero, and poking it
  produced erratic motion including a backwards frame.
- **Only remaining route:** a runtime hook that scales `r6` at `0x0800DC7A`.
  That is a code change, needs a play-test for collision/tile-snapping and
  cutscene side effects, and has not been approved.

**Text speed — the game's own setting exists, its RAM mirror was not found.**

- The in-game menu (Start → Change settings → "Message speed", Slow/Normal/Fast)
  is reachable headlessly and toggles live with Left/Right.
- Diffing EWRAM *and* IWRAM across the three states found nothing: the game runs
  an RNG that churns both regions every frame, so value-diff search drowns.
  `0x03000EAB` matched 0/1/2 exactly but proved to be RNG noise on an idle
  stability check. The web-sourced `0x0200046C` is wrong (writing 0 there opens
  the party menu).
- Glyphs are not drawn through the BG0-3 tilemap (`0x06002000-0x06004000` shows
  only one-shot box-border fills), so they are OBJ/sprite or another VRAM
  region.
- **Next route:** breakpoint the code that READS the setting during text reveal
  or during the menu's redraw-on-change, instead of more diffing.

Tooling added during this work: `write_mem <addr> <value> <width>` in the
headless debug server (`gbarecomp/src/debug/tcp_debug_server.cpp:1247`). The
server still has no SRAM/save-RAM read command.

### CRASH-06 — Mercury Lighthouse: the LZ decompressor never terminates (2026-08-19)

Reproducible from savestate 5 on `build/gs011_opt` via the launcher. Abort is
always `SELF-HEAL bridge hit interpreter Undefined at pc=0x030020A8`.

**Root finding (2026-08-19): guest LZ decompressor `Func_2808` does not stop.**
Its own copy loop (`strb r7,[r1,#1]!`, guest pcs `0x030061E8`, `0x030061F0`,
`0x03006200`) walks the destination pointer from `0x03002000` all the way up
through `0x03006000` — the code it is itself executing from — and on into the
stack. Evidence, `logs/session_20260819_123512.log`:

```
[c06-entry] pc=0x03006010 r0=0x08613835 r1=0x03002000 r14=0x080053C5 frame=2569
[c06-lr] entry pc=0x03006010 addr=0x03007E60 value=0x080053C5 sp=0x03007E64
[c06-codewrite] pc=0x030061E8 addr=0x03006000 value=0x000000BA frame=2569
[c06-codewrite] pc=0x030061F0 addr=0x03006001 value=0x00000077 frame=2569
... sequential single-byte stores climbing through 0x0300600A ...
[c06-lr] exit pc=0x030062F8 addr=0x03007E58 value=0x03002000 sp=0x03007E3C
```

Entry parameters are correct and come from ROM callers
(`0x0800536C` → frame 2574-style header probe that early-returns;
`0x080053C0` in `gf_Func_5394` → the call that actually decompresses):
`r0=0x08613834/35` source, `r1=0x03002000` destination.

**Everything previously chased is downstream of that one overrun.** Retired:

- *Destination pointer miscomputed / 0x20 too low* — dead. `[c06-entry]` shows
  the caller passes `r1=0x03002000` deliberately. The `r9 = r1+0x20` at entry is
  dead code; the `0x1f` mask is the format's 32-byte ring window, not an address.
- *Another transient occupant reused the slot mid-call* — dead. `[c06-codewrite]`
  shows the ONLY writer into `0x03006000-0x030063FF` is the decompressor's own
  copy loop.
- *In-frame stack corruption of the saved r14* — dead. `[c06-lr]` shows entry and
  exit read **different** addresses (`0x03007E60` vs `0x03007E58`), i.e. an 8-byte
  stack imbalance, not a clobbered slot — and that imbalance is itself fallout
  from the overrun reaching the stack.
- *Zero prefix at `0x03002000-0x0300201F`* — fallout, not a cause.

**Translation checked and found faithful** (no mistranslation located):
the 7-bit end-of-stream sentinel (`ands r5,r2,#0x7f` at `0x0300616C`,
`beq 0x030062F0` at `0x03006170`) including its carry-preserve rule; the
bit-refill at `0x030062E0`/`0x030062DC` including barrel-shift carry-out for the
`<32 / ==32 / >32` cases and the `R[4] & 0xFF` truncation; the PC+8 bias; the
condition-code encodings; the jump-table dispatch spacing (`add r15,r9,r8,lsl#5`,
entries 0x20 apart). Termination is a pure in-stream sentinel — there is **no
counted length** — so a desynced stream runs forever by construction.

**Two theories still open:**

- **(a)** the source blob or pointer is wrong/misaligned upstream of `Func_2808`,
  so the stream never contains its terminator. Not yet checked.
- **(b)** a mistranslation in one of the ~35 unrolled jump-table copy routines
  (`0x03006300`-`0x03006440`-ish). These were **not** individually verified.

**Next steps, in priority order:**

1. Settle (a) vs (b). Cheapest route: bound or dump the compressed blob at
   `0x08613834` and decode the stream statically. No length field was found in
   the header (one type byte, must equal 2, then alignment/priming bits), and no
   neighbouring object was found in `local/gs011/main/symbol_map.cpp` to bound it
   from above — so this needs a real look, not a grep.
2. If that is inconclusive, walk all ~35 jump-table routines for a field/count
   mistranslation.

**Diagnostics currently in the tree — all TEMPORARY, remove when CRASH-06 closes.**
All live in `gbarecomp/src/armv4t/runtime_arm.cpp`, behind the existing
`GBARECOMP_TRACE` gate (launcher "Crash tracing" checkbox) **and** a
`g_c06_armed` boolean that stays false until the first `[c06-entry]` fires:

| tag | fields | note |
|---|---|---|
| `[c06-entry]` | pc r0 r1 r14 frame | armed from boot; it is the trigger |
| `[c06-lr]` | pc addr value sp frame | entry `stm` and exit `ldm` of the r14 slot |
| `[c06-codewrite]` | pc addr value frame | writes into `0x03006000-0x030063FF` |
| `[c06-term]` | r5 r2 r0 r1 frame | **BROKEN — produced zero lines** |

Arming everything (including the pre-existing `[slot-watch]` store/dispatch
logging and its read ring) only at the first `[c06-entry]` is what made crash
tracing usable again — before that it ran at under 1 FPS and the user could not
reach the crash.

`[c06-term]` is broken: it hangs off `g_runtime_fn_entry_hook`, which never fires
for `0x0300616C`. A working version is awkward — that pc is a pure ALU op with no
bus access, so there is no natural hook, and the per-instruction fingerprint ring
(`runtime_insn_fp`, full 16-register snapshot every instruction) is far too slow
to be an option. Needs a different idea.

**Still established from earlier passes** (unchanged): the savestate is fine and
matches the state file byte for byte; `0x03002000` is a shared IWRAM code slot;
DMA channel 3 refill logic was checked against GBATEK and is correct; our runtime
substitutes nothing for the guest-computed address; `RamOverlayRegistry` is dead
code. Also unrelated and unfixed: `do_cpu_fast_set`
(`gbarecomp/src/runtime/bios_hle.cpp:234-241`) skips the hardware's round-up of
the transfer count to a multiple of 8.

Also added while chasing this: `overlay_request_compile` now declines to heal a
dispatch-miss pc that falls strictly inside an already-healed function's range
(`gbarecomp/src/runtime/overlay_loader.cpp:1358-1382`). That stopped one class of
bogus heal but did not fix this crash.

**Observation, not yet fixed:** `runtime_ram_code_dirty_reset()` is only called
from `reset_recomp_cpu()` (`gbarecomp/src/runtime/runtime.cpp:797`), so a RAM
word's "dirty" bit, once set by any write, stays set for the rest of the session —
`runtime_ram_code_range_dirty`
(`gbarecomp/src/armv4t/runtime_arm.cpp:115-129`) then keeps routing every later
dispatch through that address down the interpreter/self-heal tier even with no
write anywhere near it in time. Likely a real performance defect independent of
CRASH-06; not investigated.

### CRASH-05 — SIGABRT: self-heal bridge hit an interpreter gap while healing `0x03002000`

User report 2026-08-16: launched via `GoldenSunLauncher.exe`
(`GBARECOMP_SELFHEAL_RAM=1`, `GBARECOMP_PRESENT_IN_PLACE=0`, persistent
`recomp_cache`), game crashed. Artifacts confirmed fresh (13:23:18 local,
not the stale Aug 15 21:41 `crash_dump.dmp`):
`build/gs011_opt/crash_dump.dmp`, `build/gs011_opt/crash_report.txt`,
`logs/session_20260816_132238.log` (all three timestamped 13:23 today).

**Failure signature:** `SIGABRT` (`abort()` called), 39.078 s uptime.
`crash_report.txt`'s 64-frame backtrace has no symbols for our own frames
(RelWithDebInfo strips this path), but shows a tight 4-frame cycle
repeating ~15 times before the abort — consistent with the known
guest-jumps-become-host-calls pattern (CORE-01) driving deep call
nesting, though this abort's own message (below) points at a different,
more specific cause.

**The session log's last lines are the real signature**, not a wild jump
this time — a genuine interpreter gap, hit while self-healing:

```
[ERR] runtime_arm: SELF-HEAL dispatch miss for pc=0x03002000 <near gf_copied_Func_15430_03002000+0x0> (arm) — no generated function; bridging through the interpreter and recording for a TOML proposal (Stage-1 self-healing; NOT fully static). Merge the proposal into game.toml and regenerate.
[ERR] runtime_arm: SELF-HEAL bridge hit interpreter Undefined at pc=0x030020A8 while bridging dispatch miss 0x03002000. The reference interpreter cannot execute this op; add its lowering in src/armv4t/interpreter.cpp.
[ERR] runtime_trace: last 0 event(s)
[ERR] self_heal: HEALED 0x03002000 (arm) -> native via gcc (crc=D2E71116, [0x03002000,0x03002098)); the interpreter bridge stops for this PC.
```

The "HEALED" line appearing *after* the abort message is stdio
interleaving/buffering on the way down, not evidence the process
survived — `crash_report.txt` and the SIGABRT are unambiguous that the
process died. `runtime_trace: last 0 event(s)` means the trace ring was
not armed, so there is no instruction history leading up to
`0x030020A8` — same gap CRASH-03 hit.

This is the loud, by-design abort documented in `gbarecomp/CLAUDE.md`
("Genuine interpreter gaps still abort loudly") — the reference
interpreter reached an ARM opcode at `pc=0x030020A8` it has no lowering
for, while bridging a dispatch miss for the RAM function at `0x03002000`
(previously seen and healed under **CRASH-04**, same address, different
failure — that one was a stale `g_runtime_image_base`, this one is a
missing interpreter opcode). No further investigation was done this pass
(budget-limited capture-only task) — the actual bytes at `0x030020A8`
were not read (they live in mutable RAM, not ROM, so they are only
knowable from a live/traced session, not static disassembly) and no fix
was attempted.

**First clearly abnormal lines in the log** (for context, not the crash
itself — self-heal running loudly is expected/by-design with
`GBARECOMP_SELFHEAL_RAM=1`): lines 8-21, a burst of ordinary `SELF-HEAL
dispatch miss` records starting immediately after boot
(`pc=0x030020A4` etc.), followed at lines 27-91 by repeated `unknown
transient code identity` / `DYNAMIC RAM CODE` notices for addresses in
`0x02008xxx`-`0x0200Cxxx` and `0x03006xxx` — all expected self-heal
chatter, not the failure. The genuine abnormality is the single
"interpreter Undefined" line above, near the very end of the log.

**Self-heal activity in this session, per item 3 of this task:** yes,
extensive — `[HEALED->native]`-style lines appear (`self_heal: HEALED
0x0200BA14`, `0x03005CF4`, `0x03002000`, all via gcc), no `self_heal:
compile FAILED` lines, no `ram_crc_mismatches` lines logged this run. The
crash follows self-heal activity directly (the very next dispatch-miss
bridge after two successful heals hit the interpreter gap), but self-heal
itself did not fail — the interpreter it bridges to simply does not
implement whatever opcode is at `0x030020A8`. Self-heal RAM being newly
default-on via the launcher is worth keeping in mind as the newest
variable in this configuration, but the proximate cause here is a missing
interpreter opcode lowering, not a self-heal failure per se.

**Next step, not done this pass:** get the actual instruction bytes at
`0x030020A8` — either via a live TCP session reading RAM before the next
repro, or `GBARECOMP_TRACE`/`GBARECOMP_TRACE_ON_DISPATCH_MISS` armed
ahead of time (same gap CRASH-03 flagged) — before touching
`interpreter.cpp`. Do not guess the opcode from the surrounding context.

### BIOS-01 — Ship without requiring a GBA BIOS file

> **PAUSED 2026-08-15 at the user's request.** Resume when he is further into
> the playthrough and can stress the game harder. The next step is not code: it
> is arming `GBARECOMP_SWI_LOG` during a real play session (battles, menus, map
> transitions) to close the coverage gap left by the headless boot-only run.
> Command at the bottom of this entry.

**Goal (user, 2026-08-15): players supply only a ROM.** Neither the ROM nor the
BIOS can ever be distributed, so both must come from the player — but a player
who owns Golden Sun has the ROM as a matter of course, while the BIOS means
dumping a console or sourcing it from somewhere dubious. Removing that
requirement removes the one step that pushes players toward the murky path.

Today the BIOS is genuinely load-bearing. It is not embedded in the binary; it
is read from disk and SHA-1 checked at startup (`gbarecomp/src/gba/gba_bios.cpp:15`),
so without it the game does not start.

**Scope is far smaller than expected.** Golden Sun calls exactly five BIOS
services across every generated corpus (`grep runtime_swi` over `local/`):

| SWI | Service | Native version? |
|---|---|---|
| `0x0B` | CpuSet | yes — `bios_hle.cpp` |
| `0x19` | SoundBias | yes — `bios_hle.cpp` |
| `0x02` | Halt | no — small |
| `0x03` | Stop | no — small |
| `0x2A` | SoundGetJumpList | no — **but dead, see below** |

`SoundGetJumpList` was the feared case, because it hands the game pointers into
BIOS code that the game can then call — something a native replacement cannot
fake. **It is dead. Confirmed three independent ways 2026-08-15.**

1. *No direct call.* Its only call site is a two-instruction wrapper at
   `0x080FA674` (`local/gs011/main/recompiled_018.cpp:150648`). The only
   references to that address anywhere are generated bookkeeping (dispatch
   table, header, symbol map) — no `bl`, no jump-table entry.
2. *No stored pointer anywhere in the ROM.* Scanned all 8,388,608 bytes for
   both `0x080FA675` (THUMB pointer form) and `0x080FA674`: **zero
   occurrences.** Method validated with a positive control — 24 of 300 randomly
   sampled functions from `symbol_map.cpp` do have pointers stored in ROM, so
   the scan detects real ones and a zero result is meaningful.
3. *Never executed.* 18,000-frame cold boot with `GBARECOMP_SWI_LOG` armed:
   17,652 SWI records, and `0x2A` is not among them.

With no direct call and no pointer to it in the entire ROM, the function is
unreachable in practice. The only theoretical route left is an address computed
arithmetically at runtime, which would be very unusual for an SDK library stub.
Worth re-checking once against a full play session, but it should not gate the
work.

**Runtime SWI profile (18,000-frame cold boot, no input):**

| SWI | Calls | Note |
|---|---|---|
| `0x02` Halt | 17,648 | 99.98% of all SWI traffic |
| `0x0B` CpuSet | 4 | already implemented natively |
| everything else | 0 | Stop, SoundBias, SoundGetJumpList never fired |

**So the SWI half of this work is essentially just Halt.** Stop and SoundBias
have static call sites but never execute — same dead-SDK-code pattern as
`0x2A`. Coverage caveat: this run had no input, so it covers boot, intro and
title only. Re-run with the log armed during real play (battles, menus, map
transitions) to close the gap.

**Notably absent: `IntrWait` (`0x04`) and `VBlankIntrWait` (`0x05`).** Golden Sun
halts directly instead. Those two carry the nastiest BIOS edge cases, so the
hardest part of a general BIOS replacement does not apply here.

Already solved and reusable:

- boot handoff — `bios_hle_boot_skip()` synthesizes post-BIOS CPU state;
- BIOS-region data reads — `gba_bus.cpp:159-295` already models real bytes when
  readable and open-bus prefetch when not.

**The one real piece of work is the interrupt path.** `runtime_irq()`
(`gbarecomp/src/armv4t/runtime_arm.cpp:1540`) banks into IRQ mode, sets
`R15 = 0x18` and dispatches the recompiled BIOS vector, then drives it to
completion. The handler itself is short and documented (save registers, call the
game's handler from `0x03007FFC`, restore, return).

**Where the code lives.** The user's call: keep it in this repo, not upstream.
The SWI half needs no upstream change at all — `g_bios_hle_hook`
(`gbarecomp/src/armv4t/runtime_arm.h:763`) is a public hook we can install from
`src/`. The IRQ half has **no equivalent hook**; adding one is a ~3-line edit to
`runtime_irq()`, after which all real logic stays here. This repo already
carries local gbarecomp patches (see `ROADMAP.md`, pin update pending).

**Verification plan, and the reason this is safe.** The real BIOS stays on the
development machine as the reference. Run the same scene twice — real BIOS and
native BIOS — and diff with the existing fingerprint tooling. Equality is
measured, not assumed. Note upstream's rulebook treats the real BIOS as sacred;
the user has explicitly set that rule aside for this project, but the
engineering reason behind it (a fake BIOS hides bugs elsewhere) is exactly what
this comparison is for. A BIOS-free mode must stay opt-in and must never become
the verification path.

Order: ~~confirm `0x2A` is dead~~ **done 2026-08-15** → Halt through the
existing `g_bios_hle_hook` → IRQ path (needs a ~3-line hook added to
`runtime_irq()`) → bit-exact comparison against the real BIOS → arm the SWI log
during a real play session to close the coverage gap.

How to re-run the SWI measurement (no rebuild needed, the instrumentation is
already in the binary):

```powershell
$env:GBARECOMP_SWI_LOG = "<path>\swi.csv"   # presence arms it; value is the output path
build\gs011_opt\GoldenSunRecomp.exe --bios <bios> --rom <rom> `
  --frames 18000 --no-window --quiet
```

The CSV column `imm` is the SWI number in decimal (`2` = Halt, `42` = `0x2A`).
The ring holds 131,072 entries and a boot run produces ~17,600, so it does not
wrap. Note `--no-window` writes no framedumps — captures happen on the present
path only.

### WIDE-03 — A Golden Sun tile sidecar needs reverse engineering first

Researched 2026-08-15, to size the "draw the off-screen margins ourselves" path
that WIDE-01 identified as the only way to widen towns/dungeons/field.

**The blocker is upstream of any rendering work: Golden Sun has no semantic
symbol names.** Checked three sources and all are address-derived:

- `private/goldensun-disasm/goldensun.elf` — only `Func_XXXXXX`, `Data_XXXXXX`,
  `Exports_XXXXXX` labels. No DWARF, no `.debug_info`. Grepping the function
  symbols for `map|tile|metatile|camera|scroll|room|layer|chunk` returns **zero
  hits**.
- `local/gs011/main/symbol_map.cpp` — same scheme (`gf_Func_*`, `gf_afunc_*`).
- `config/usa/main.toml` — 4,186 `[[extra_func]]` and 3,525 `[[data_range]]`
  entries, none carrying a semantic name; discovery is purely by address/scan.

The upstream `gsret/goldensun` project uses the same scheme in its own `.s`
sources and its README states editing "is not meaningfully possible yet" — it
is a byte-matching disassembly, not a named decompilation.

**This is why Minish Cap's sidecar cannot be ported by analogy.**
`ws_sidecar.cpp` works because that game has a mature pret-style decomp where
`gBGTilemapBuffers1`, `gMapHeader`, `CB2_Overworld` and `DrawMetatileAt` already
exist as names. Golden Sun has no equivalent to look up — not locally, not
upstream, today.

So before a sidecar can even be designed, someone must identify by
instruction-level analysis or live tracing: the map-load routine, the map-header
struct, the camera/scroll struct, and whether the full map is resident or
streamed. Each is its own reverse-engineering task across ~4,200 unnamed
functions.

**Compression makes it worse.** Community tooling (`romhack/GoldenSunCompression`)
documents two *custom* LZ77/LZSS variants for Golden Sun graphics/tilesets —
bit-flag and byte-flag block schemes, **not** the stock GBA `SWI 0x11/0x12`
format — plus a separate Huffman scheme for text. If map tiles turn out to be
streamed rather than resident, the sidecar would have to call or replicate a
non-standard decompressor, not just redirect a buffer pointer the way Minish
Cap's does.

TODO-EVIDENCE, all still open:

- map/tilemap/camera/map-header symbol identities;
- full-map-resident vs. streamed (needs the load routine identified first, or a
  live trace);
- whether any specific routine is the tile decompressor. `Func_9bb8` is a
  *lead* only — it is the dominant occupant of the IWRAM staging slot
  `0x0300347C`, but `config/usa/transient-func-*-0300347c.toml` shows that slot
  is reused by at least four different routines, so calling it the decompressor
  would be a guess.

**Honest sizing: materially harder than Minish Cap, and the hard part is not
the rendering.** The wide-scanline renderer and OBJ path WIDE-01 validated are
still fine. The missing piece is knowing where Golden Sun keeps its map.

Separately noted: the disassembly revision pinned in `config/local.json`
(`84a80693…`) no longer resolves as a commit against `gsret/goldensun` via the
GitHub API — likely rebased upstream. Worth a look if that checkout's
reproducibility matters later.

### WIDE-04 — World-map widescreen: BUILT 2026-08-15, awaiting user play-test

Shipped as an opt-in enhancement, default OFF. Turn on in **F1 → Enhancements →
"Widescreen (World Map)"**; the choice persists in the config INI
(`[Enhancements] Widescreen=`).

Policy: per frame, read DISPCNT bits 0-2. **Mode 2 (world map) → render wide.
Anything else → `g_ws_pillarbox = 1`, black margins.** Implemented as
`gs_widescreen_scene_policy()` in `src/runner_main.cpp:1312`, hung off the
existing `blitter_function_entry` hook, so no new gbarecomp hook was needed.
Early-returns when `g_ws_active == 0`, so it costs nothing with the feature off.
No tile sidecar and no `g_ws_tilemap_provider` — the world map's data is already
resident, which is the whole reason this scene works.

Also raised `max_view_width` / `max_resize_view_width` from a hard 240 to
`GbaPpu::kMaxRenderWidth` (480, the engine ceiling) and flipped
`launcher_expose_widescreen` to true (`src/runner_main.cpp:2193-2201`). The F1
toggle plumbing follows the existing enhanced-timing pattern through
`host_config_ui.*`, `host_window.*` and `runtime.cpp`.

Verified headless:

- faithful default → 240×160, unchanged;
- `--view-width 480` on the world map → 480×160, both 120px margins full of
  real varied terrain (25/29 distinct colours), not repeats or garbage;
- `--view-width 480` on a mode-0 scene → margins 100% black, correctly
  pillarboxed;
- 86/86 Python, 4/4 ctest, full `gs011_opt` build clean;
- no coverage regression: 1,800 frames with `GBARECOMP_SELFHEAL_RAM=1` gives
  `native_calls=1,547,854 interpreted_insns=1,570 dispatch_misses=4
  ram_crc_mismatches=0`. (Beware comparing runs that omit that env var — without
  it RAM code is interpreted and the numbers look alarming for unrelated
  reasons.)

Useful by-product — savestate BG modes: **state2/3/8/9 = mode 0, state7 = mode
1, the rest = mode 2.** User-supplied labels for two of them (2026-08-15):
**`state1` = world map** (mode 2, the scene widescreen widens) and
**`state3` = Bilibin town** (mode 0, loaded by hotkey F4, offered as the
walk-around town test case). Handy for picking test scenes.

**WIDE-04a — black screen on the F1 toggle. FIXED 2026-08-15, awaiting
user play-test.**

Root cause was **not** the pillarbox theory first proposed here (that
`g_ws_active` was unset so the scene policy early-returned). That was checked
and disproven: `sync_widescreen_toggle()` does set `g_ws_active`,
`g_ws_pillarbox` defaults to 0, and pillarbox only blacks margin columns — it
can never black the native 240px centre, so it could not produce a fully black
screen. Recorded here because the wrong theory is worth remembering: a *fully*
black screen is a presentation-layer symptom, not a compositing one.

Actual cause, and it is a general engine bug rather than anything Golden Sun
specific: `HostWindow::set_surface_size()` resized the SDL texture and flipped
`expanded_view` but never updated the **SDL renderer's logical size**. `open()`
pins `SDL_RenderSetLogicalSize(240,160)` with integer scaling for any window
that starts native — which every config-driven widescreen session does, since it
opens 240×160 and resizes later. After expanding, `present()` switches to an
explicit destination rect in real drawable pixels, but SDL was still
reinterpreting that rect inside the stale 240×160 logical space, so the copy
landed outside the clipped viewport and only `SDL_RenderClear`'s black
survived. `--view-width 480` at startup never hit this because it opens
*already* expanded, so `open()` skips the logical-size call from frame 1 — which
is exactly why the earlier pass saw that path as working.

Fix: `gbarecomp/src/runtime/host_window.cpp:1157` now re-runs the same
logical-size/integer-scale branch `open()` uses on every live resize, so the
renderer's coordinate mode can never drift from `expanded_view`. One file, no
game-specific special-casing.

Verified with live renderer readback (`GBARECOMP_WINDOW_SHOT`), which is the
only capture that proves what was actually presented — note `--dump-png` runs
its own separate off-screen `ppu.render()` and bypasses presentation entirely,
so it showed a correct picture while the real screen was black. Worth
remembering for any future presentation bug.

- before fix, toggle ON, world map: 100% black — matches the user's report;
- after fix: real terrain, 1635 distinct colours in the content region;
- toggle OFF: returns cleanly to faithful, no stale surface;
- Bilibin (state3, mode 0) with toggle ON: normal picture, clean black margins;
- faithful default and the startup `--view-width` path both unaffected;
- 86/86 Python, 4/4 ctest, plus `ppu_smoke_tests` and
  `presentation_layout_tests` rebuilt and passing.

Still not covered by any harness: a literal mouse click on the ImGui checkbox.
The fix was driven through the identical downstream event, but the click itself
needs a human.

**USER-CONFIRMED WORKING 2026-08-15**: "It works on the world map! Thats super
cool." Towns correctly do not widen, as designed. The F1 checkbox click path is
therefore closed too.

**WIDE-04b — window does not fit the wide picture.** Same play-test: "image
doesnt neatly fill the window, if widescreen is on make window adjust in
horizontal size if it doesnt fit." Widescreen changes the view from 240×160
(3:2) to 480×160 (3:1) but the window keeps its old shape, leaving the picture
letterboxed with dead space. **DONE 2026-08-15, awaiting user look.**

`largest_fitting_scale()` plus a resize block in `HostWindow::set_surface_size()`
(`gbarecomp/src/runtime/host_window.cpp:1163,1220-1229`). On every config-driven
view-size change it queries the window's display work area
(`SDL_GetDisplayUsableBounds`), picks the largest integer scale that fits, and
resizes and recentres to exactly `base_w*scale × base_h*scale`. Measured on the
user's 3440×1440: widescreen ON → **7× = 3360×1120** (9× correctly rejected,
would need 4320 px); OFF → back to the launch scale, 720×480 at `--scale 3`.

Recomputed from scratch each time rather than storing a previous scale, so
repeated toggling cannot drift. Skipped entirely when `resize_driven_view` (the
user dragged the window themselves) or fullscreen is active.

**Deliberate behaviour worth knowing:** with widescreen on, the window stays
wide in towns and the *picture* is pillarboxed inside it. Window sizing follows
the requested view width; per-scene widening stays with the scene policy. The
alternative — resizing the window every time you walk between the world map and
a town — would be far worse. Flagged to the user in case he wants it changed.

Verified with live window readback on both savestates; 86/86 Python; ctest 29/31
with `ppu_smoke_tests` and `presentation_layout_tests` passing. The two
not-run targets (`heal_gate_tests`, `tail_dispatch_tests`) have no built `.exe`
in this configuration and predate this work.

Still needs a human: a genuine live F1 toggle and repeated toggling in one
session. Verification drove the identical code path via the saved `Widescreen=`
setting across two launches.

Still open:

- the F1 checkbox itself was never click-tested, only the underlying
  `set_view_margins` / `set_surface_size` path it drives — so the toggle could
  still have a snag;
- **affine edge wrap is unobserved.** Both layers have the wrap bit set, so
  walking to the 512px buffer boundary should wrap the far side in. Nobody has
  walked there yet. Decide clamp-vs-pillarbox once it is seen;
- faithful-default byte-identity is argued from the code path, not proven by a
  byte-diff against a pre-change binary.

### CRASH-03 — Wild jump into MMIO space, Bilibin ↔ McCoy's palace

User report 2026-08-15: loaded the Bilibin savestate, walked up near McCoy's
palace, came back down to Bilibin — hung briefly, then crashed. Log
`logs/session_20260815_190611.log`.

Fatal line (line 1061 of 1065):

```
SELF-HEAL bridge hit interpreter Undefined at pc=0x04000024
while bridging dispatch miss 0x0300394C
```

`0x04000024` is **not code** — it is the I/O register block (BG2 affine
params). The guest PC ran off into MMIO space, i.e. a wild jump, and the
interpreter then hit an undefined opcode there. The "add its lowering in
interpreter.cpp" suggestion in that message is a red herring; there is no
instruction to lower, the PC is simply wrong.

**Not caused by widescreen.** The same failure class appears in
`logs/session_20260815_005828.log` at **00:58**, before any widescreen work
existed (widescreen was built ~17:47), there landing at `pc=0x8E5E6D4C` — pure
garbage. Two occurrences out of 20 logged sessions spanning 2026-08-14 09:15 to
2026-08-15 19:06, both on the 15th. Also not clearly attributable to the
tail-dispatch WIP checkpoint `d36b253` (2026-08-15 11:07), since the 00:58
failure predates it.

**The preceding lines are the real lead.** Immediately before the wild jump the
runtime healed a run of tiny 4-8 byte "functions" in IWRAM around
`0x0300018C`-`0x030003F0`, with only two CRCs repeating over and over
(`3503EB07`, `A845F9DE`), interleaved with `compile FAILED … function finder
found no entry at the miss PC`. Identical short byte sequences at many
addresses, and a finder that cannot locate function boundaries, is the
signature of **the runtime dispatching into a data region and healing it as
code**. The wild jump is likely the consequence, not the cause.

Next time it happens, arm the trace ring — this run had
`runtime_trace: last 0 event(s)`, so there is no history of how the PC got
there. `GBARECOMP_TRACE` plus `GBARECOMP_TRACE_ON_DISPATCH_MISS` should capture
the chain. Intermittent, so it needs to be armed during ordinary play rather
than reproduced on demand.

### WIDE-06 — Distant world-map objects are culled by the game

User report 2026-08-15, with widescreen working: caves and similar map objects
that are now inside the widened view still pop in and out based on distance,
because **Golden Sun's own code culls them** — it decides what to put in OAM
based on the stock 240px window, so the extra margin has nothing to draw.

This is not a renderer limit. WIDE-01 established our OBJ path already decodes
the full 9-bit signed OAM X, so any sprite the game actually submits will render
correctly off-window. The cull happens upstream, in guest logic.

Fixing it means finding and widening the game's own visibility test — the same
class of reverse engineering as WIDE-03/WIDE-05, and it should be folded into
that effort rather than treated separately. Do not attempt it before the map
system is understood.

### WIDE-07 — MEASURED: Golden Sun does NOT stream map tiles as you walk

Trace 2026-08-15, `logs/mmio_map_20260815_191329.csv` (262,144 MMIO writes =
full ring, covering the last ~400 frames / 6.7 s of a session where the user
loaded state3, entered Bilibin, entered a house, left the house, left the town).
Captured with `GBARECOMP_MMIO_DUMP` via the `TRACE MAP.ps1` helper.

**Decoding note that cost one analysis pass:** Golden Sun arms DMA with a single
**32-bit** write to `DMACNT_L` covering count *and* control together (the
`stm r12,{r2,r3,r4}` idiom found in WIDE-05). Filtering on 16-bit writes to
`DMACNT_H` finds **zero** DMA3 transfers and looks like a null result. Decode
the 32-bit form: 4,575 armed transfers, 3,938 of them DMA3.

**Area load = one burst.** At the transition (frames 281-284) the game
decompresses several blocks into EWRAM, then bulk-copies **32 KB into VRAM in
four back-to-back 8 KB DMA transfers** — `0x06008000`, `0x0600A000`,
`0x0600C000`, `0x0600E000`, sourced from a contiguous
`0x02038000`-`0x0203FFFF` EWRAM staging block (`pc=0x08010EBC..0x08010F0A`).
Tilemap-sized writes land separately around `0x06002000`-`0x06002800`.

**Walking = no map traffic at all.** After the load, the only recurring VRAM
writes are at frames 303, 318, 338, 358, 378, 393 — every ~20 frames, always the
**same nine fixed destinations** (`0x06008C00`, `0x06009000`, `0x06009440`,
`0x06008240`, `0x06008640`, `0x06008A40`, `0x06008E40`, `0x06009240`,
`0x06009640`), sizes 128/128/96×7, all from `pc=0x08011860`, with sources
cycling through a small fixed EWRAM set. Fixed destinations with rotating
sources is **animation** (animated tiles), not streaming — streaming would write
*advancing* destinations as the camera moves. There is also a 16-unit copy every
6 frames, likewise periodic.

**Verdict: resident, not streamed. This is the easier road.** A sidecar would
not have to replicate Golden Sun's custom LZSS decompressor (WIDE-03) — the
decompressed tile data is already sitting in EWRAM, and the game itself copies
from there.

**Next question, and it is the one that decides the sidecar:** the 32 KB burst
is tile *graphics*. What we need for widescreen margins is the **tilemap** —
which tile goes where. Is the whole area's tilemap resident in EWRAM, or does
VRAM only ever hold the visible window plus a small border? The tilemap-sized
writes to `0x06002000`-`0x06002800` are small (320 and 384 units), consistent
with WIDE-01's finding that the VRAM tilemap is only a wrapping ring.

Settle it with a store watch on the EWRAM staging region while walking
(`GBARECOMP_STORE_WATCH=<lo>:<hi>`, which reports the PC responsible), or by
reading EWRAM over TCP at two camera positions in the same area and diffing. If
the full tilemap is resident, the sidecar is straightforward.

Useful anchors recovered: `pc=0x08010EBC/0x08010ED6/0x08010EF0/0x08010F0A` arm
the 8 KB tileset copies; `pc=0x08011860` drives the animated-tile updates;
`pc=0x0800BB6A/0x0800C038/0x08015F4C/0x0809115A` produce the EWRAM decompression
blocks. These are the first real function anchors for the map system —
everything before this was `TODO-EVIDENCE`.

### WIDE-05 — Hunting Golden Sun's map system: first bite

Static pass 2026-08-15, working backwards from fixed GBA hardware registers
(which need no symbol names) since WIDE-03 established there are none.

**Methodological finding, and it matters for every future pass: you cannot grep
the generated corpus for MMIO addresses.** Register writes lower to
`bus_write_u32(ea, value)` where `ea = base + offset`. The offset is usually a
visible instruction immediate, but the *base* is nearly always materialised by a
PC-relative literal-pool load (`ldr rX,[pc,#N]` — ARM's normal way to
build `=0x04000000`), and the recompiler does not fold those into compile-time
constants. The real address exists only as bytes in the ROM's literal pool,
invisible to a text search of `local/**`. Example lowering at
`local/gs011/main/recompiled_000.cpp:354-371`. A direct
`mov rX,#0x4000000` occurs just **4 times in the whole 4.5M-line corpus**.

So "grep the corpus for the register constant" is structurally a dead end. Use
literal-pool-aware disassembly of the ROM, or a runtime trace.

**Proven:** `gf_Func_2cf4` (`0x08002CF4`, 104 bytes, symbol confidence
`proven`, at `local/gs011/main/recompiled_012.cpp:1741-1786`) arms DMA3. It sets
`r12 = 0x04000000 + 0xD4` then `stm r12,{r2,r3,r4}`, writing **DMA3SAD,
DMA3DAD, DMA3CNT** in one go. It is a dual-mode routine: a preceding block
(`recompiled_008.cpp:2651-2729`) handles small byte/half/word tail fills and
falls through to the DMA path for bulk. This is the strongest lead for the
bulk map-copy path.

Also proven but not map-related: the IRQ prologue's `REG_IE`/`REG_IF`/`REG_IME`
reads in `gf_irq_handler_03000000` (`recompiled_025.cpp:19-27`).

**TODO-EVIDENCE, all of it:**

- Scroll (`0x04000010`-`0x1F`) and BG-control (`0x04000008`-`0x0F`) writers — no
  proven writer found, for the literal-pool reason above. One near-miss at
  `recompiled_014.cpp:6764,6910` (`gf_afunc_0800A1B4`) writes a signed 16-bit
  pair after a fixed-point multiply/shift, structurally camera-shaped, but `r7`
  is a caller-supplied pointer whose provenance is not statically traceable.
  Not claimed.
- Camera/scroll RAM address — none found.
- `gf_Func_2cf4`'s copy size and destination — it has **zero static call sites**
  (marked `indirect`, reached only via `runtime_dispatch`), so its arguments are
  not statically resolvable.
- Resident vs streamed — still open, and it is the question that decides the
  size of the whole sidecar job.

**The next measurement is a runtime trace, and it settles resident-vs-streamed
directly.** Record `DMA3DAD` (destination) and `DMA3CNT` (size) each time
`gf_Func_2cf4` arms a transfer while walking through a town or dungeon:

- repeated small transfers to shifting VRAM offsets → **streaming** (hard: the
  sidecar would have to replicate Golden Sun's custom decompressor);
- one large transfer sized to a whole map → **resident** (much easier: the
  sidecar just reads a buffer, like Minish Cap's does).

Existing env hooks that look right for this: `GBARECOMP_DMA_WATCH_ADDR`,
`GBARECOMP_MMIO_DUMP`, `GBARECOMP_STORE_WATCH`. Confirm their semantics before
use.

**This needs the player to actually walk around** — a parked savestate never
moves the camera, so nothing streams. Fold it into the same play session as
BIOS-01's SWI log; one session answers both.

### TOOL-02 — `--load-state` is a dead flag when `--tcp` is also passed

Found during the WIDE-01 measurement, 2026-08-15. In
`gbarecomp/src/runtime/runtime.cpp`, the `if (args.tcp_port > 0)` block (from
line 1707) calls the blocking `server.run(...)` (line 1856) and then returns
(line 1871). The `--load-state` handling sits further down at line 2683, in
code only reached when the TCP block is skipped. So passing both flags silently
ignores `--load-state` — no warning, no error.

Workaround that works today: load through the TCP `savestate_load` command
instead, which is correctly wired to `ctx.savestate_load` and does the same
restore under `park_and_wait`.

Low severity but it wastes time and lies silently, which is the bad kind of
bug. Fix by either honouring `--load-state` before entering the TCP server
loop, or refusing the combination loudly at argument-parse time.

### PERF-08 — Residual small stutter: menus, normal attacks, entering town (2026-08-20)

Open. After PERF-07 was fixed the user reports the build feels like his old
smooth builds, with an occasional small hitch remaining. He states it repeats on
the same action against the same enemy, which rules out one-off cold-path work.

Measured, `logs/session_20260820_110039.phase.csv`: averages guest 4765us,
render 746us, audio 9us, compile 0us; 36 frames over 20ms, 15 over 33ms. **Every
spike is entirely in `guest_us`** — render, audio and compile stay flat through
them. Not graphics, not audio, not compilation (compiles run off the game
thread; only 13 of 195 were real gcc runs, the rest warm-loaded from disk).

The worst spikes in that log are NOT the reported symptom. Frames 344519
(148ms) and 344595 (167ms) match single synchronous `bridge_us` values in
`events.csv` (141519, 162083), triggered by `SELF-HEAL bridge entered at top
level (empty call-return stack) for pc=0x03005CD4; LR=0xB7C00000 (fallback
stop)`. With no real return address to stop at, the bridge free-walks forward
through `gf_resume_03003908`'s state machine, healing ~40 first-time PCs in one
pass. All 10 worst frames belong to that single unbroken discovery run
(cumulative `dispatch_miss` 4 to 92) starting ~200 frames after
`savestate_loaded frame=344326`. That is cold-start cost; the session was about
one minute long.

**Blocked on evidence, deliberately.** Needs a multi-minute log with the same
attack repeated after warm-up and the menu opened several times. Flat `guest_us`
on the repeat means cold-load only; spikes of comparable size mean something is
re-evicting already-healed entries. Do not ship a fix first — three fixes were
shipped on reasoning alone across 2026-08-19/20 and none moved the symptom.

Lead: `src/runner_main.cpp` has its own separate same-PC recursion guard
(`SELF-DISPATCH RECURSION at 0x...`, evicting `g_verified_ram_cache`), unrelated
to the gbarecomp cap. At `0x03005EE0` one eviction produced 15 consecutive
`content-mismatch` rejections of the same crc `0x70652395` before a real 4-byte
compile landed — one cascade, not 15 events. Not counted in `events.csv`, so
there is no per-frame signal for it yet.

### CORE-01 — Guest jumps are emitted as host calls

The recompiler turns guest branches into host function calls, so a guest loop
that never returns grows the host stack without bound. This is the cause of the
Bilibin stack overflow. It is our architecture, not a game bug.

**Status 2026-08-20: the tail-dispatch refactor is PARKED, and it is materially
unfinished — further from done than its stashes suggest.** See
`docs/HANDOFF_2026-08-20.md` for the full reading of both stashes. Summary:
outer repo `stash@{2}` converts `verified_ram_dispatch`'s signature to the
resolve pattern but its internals still execute nested and un-tail-called
(`src/runner_main.cpp:1862-1865`) and then return a no-op placeholder
(`:1847`); `dispatch_dynamic_ram` (`:1774`) is untouched by either stash.
Retyping `RuntimeRamDispatchHook` (`runtime_arm.h:60`, call site
`runtime_arm.cpp:1056-1058`) would compile while tail-calling that no-op, with
the real recursion already done underneath. Finishing this is authoring work in
`src/runner_main.cpp`, not a merge.

Both stashes are intact and must not be dropped: `gbarecomp` `stash@{0}`, outer
`stash@{2}`. Backup of the pre-attempt tree:
`scratchpad/tail_refactor_prefix_20260820/`.

**Do not attempt to bound the recursion instead.** The 128-deep cap added by
`59c1858` is what caused PERF-07 below; raising it to 512 made trips go *up*
(8098 to 9902), which is direct evidence that the depth is unbounded. A large
host stack only delays the same failure.

`d36b253` remains the working checkpoint taken before the refactor started.

Related and still latent: **CRASH-02** below.

### CRASH-02 — Interpreter bridge does not save/restore call-return depth

`runtime_irq()` saves and restores `g_call_return_floor` around its
drive-to-completion loop (`gbarecomp/src/armv4t/runtime_arm.cpp:1549`, restored
`:1575`). The Stage-1 self-heal interpreter bridge `runtime_dispatch_miss`
(`gbarecomp/src/runtime/runtime_arm_default_aborts.cpp:1151`) has no analogous
protection. Separately the interpreter's own `IrOp::BL`
(`gbarecomp/src/armv4t/interpreter.cpp:616-621`) and `IrOp::BX` (`:637-645`)
set registers directly and never call `push_return`/`should_return`, so the
interpreter tier is asymmetric with the AOT tier by design. A
static→interpreter→static crossing could leak or corrupt frames.

Real but never observed in practice. Needs a repro before it is worth touching,
and should be re-read alongside CORE-01, which reworks the same machinery.

### BIN-01 — Binary is very large (881 MB)

`build/gs011_opt/GoldenSunRecomp.exe` is 881,861,146 bytes at RelWithDebInfo
(`-O2 -g -DNDEBUG`). The size is dominated by debug info, which optimization
inflates further through inlining.

The key fact that makes this cheap: the crash-ring symbol names this project
relies on come from the generated `local/gs011/main/symbol_map.cpp`, **not**
from DWARF. Past crashes were diagnosed entirely from those, so stripping or
splitting DWARF likely costs very little real debuggability.

Options in rough order of effort: build `Release` (`-O3 -DNDEBUG`, no `-g`);
keep `-g` but strip the shipped copy; or `-gsplit-dwarf` to keep symbols in a
side file. Measure size **and** speed for whichever is chosen — `-O3` vs `-O2`
may move the perf number. Do not enable LTO: two attempts produced 0-byte
binaries and drove this 16.7 GB machine to ~1.2 GB free.

### PERF-05 — Prologue boulder scene not full speed

Reported during a full intro playthrough. It stayed slow even at 4x guest CPU
overclock, and that is the informative part: the overclock scales only guest
CPU work, never halt/idle, DMA-stolen cycles or IRQ wake latency, and it does
not touch PPU render or host present cost. A scene that stays slow at 4x is
evidence the bottleneck is **not** guest CPU work.

Remaining candidates: our own first-touch compile cost, PPU render cost, or a
host-side present/pacing stall.

**Re-test on `gs011_opt` before investigating further** — the original report
predates the optimized build, which is ~2.9x faster. If it still repeats, ask
for a savestate just before the scene and compare frame times at 1x and 4x.

### PERF-02 — 2x interpolation coverage is uneven

Only 2,885/9,118 midpoint opportunities succeeded (31.6%); the safe duplicate
fallback covered the other 68.4%. PPU cost is small. Fallback logging is now
rate-limited with exact shutdown totals retained.

Next: measure battle and field coverage and cadence again. World map remains
unsupported.

### PERF-03 / AUD-01 — Unconfirmed audio suspicions

Two reports the user explicitly flagged as *not sure it is a bug*: audio
possibly missing at some events, and possible small glitches on item use or
shop buy/sell.

The loudness half of AUD-01 was investigated and produced a real fix (AUD-02,
below, now landed), so re-listen before anything else. The whole mix path was
cross-diffed line by line against the vendored mGBA reference and is otherwise
bit-exact; no unrequested gain exists between MP2K's output and the host
device.

**Do not instrument yet.** These need one specific reproducible moment named
first ("selling any item in the Vale shop clicks"). Then compare against a
known-good reference of the original at that same action.

### VFX-LINGER-01 — Sprites linger over black during the battle-exit fade

On the fade-out at the end of a battle the screen goes black but sprites stay
visible over it instead of fading with everything else. Small visual error.

Untested hypothesis: the fade is a brightness effect (BLDY/BLDCNT effect 2-3)
applied to the background layers but not to OBJ — i.e. OBJ is missing from the
blend target set, or our PPU applies brightness to BG only. That is a checkable
claim about our renderer.

Reachable at the end of any battle, so no special savestate is needed. Read
BLDCNT/BLDY and the per-layer blend targets across the fade frames through the
existing TCP debug server (`ppu_state`). Do not change rendering code until the
register evidence says which layer is being skipped.

### PRES-01 — Enhanced Timing A/V drift gate still open

Enhanced Timing fixes display and menu cadence, not CPU stalls. On the measured
120 Hz panel, faithful timing adds an uneven refresh about every 1.8 s; exact
60 Hz reduces that to about every 160 s. The choice persists between runs, and
first run remains OFF because exact 60 Hz deliberately speeds wall cadence by
0.456%.

Blocking gate before it could ever become a universal default: a one-hour A/V
drift measurement, not yet run.

### PRES-02 — Reported screen "tearing"

D3D11 flip presentation reports VSync active, and the evidence fits cadence
judder or shimmer rather than a torn swap. Note the host window has since moved
to OpenGL for ImGui multi-viewport support, so this needs a fresh look on the
current path.

Capture a photo or video only if a genuine horizontal tear remains.

### PRES-03 — Ivan / world-map shimmer

The Ivan-specific cause is still TODO-EVIDENCE. World-map interpolation and
native supersampling both fail their exact endpoint gates, so unsafe temporal
smoothing stays disabled there.

Two opt-in mitigations now exist: spatial `Reduce shimmer (soft filtering)`,
and the temporal blend shipped for VFX-FLICKER. Do not weaken endpoint
verification to widen either.

### UI-01 — Turbo ceiling

Turbo now omits intermediate render and VSync work while still pumping input
and audio and refreshing the display at least every 16.7 ms. Persisted mute and
a 32x slider exist, but current compute headroom is about 4x, not 32x, so the
slider promises more than the machine delivers.

`[turbo-present]` reports presented/skipped frames.

### UI-03 — F1 config menu sizing

Reported 2026-08-14: the menu kept resizing and responded badly. A tab wider
than the others makes an auto-sized ImGui window jump size when switching tabs,
which matches the resizing half. The "not responding" half may be separate:
analog triggers emit `SDL_CONTROLLERAXISMOTION` continuously, so a trigger
resting near `kTriggerPressThreshold`/`kTriggerReleaseThreshold`, or a
controller with drift, could flood `config_ui_capture_pad`.

**Needs re-confirmation** — the host window has since moved to OpenGL with
multi-viewport panels, which changes window sizing behavior. Re-test before
investigating.

If it does still repeat: fix sizing by pinning a size or sizing to the widest
tab once, rather than restructuring. Keep the controller column and the Turbo
Toggle row. Do not change binding semantics, the Held/Toggle rule, or the
persisted config format — those are user-confirmed working.

### COV-01 — New progression coverage misses

The latest post-bandit route completed but logged new ROM/overlay/RAM misses.
This is ordinary work, not a regression: any route nobody has run will find its
own gaps.

Next: prove the new immutable entries against pinned ELF data, regenerate, and
re-run from the state9 checkpoint.

### TOOL-01 — Trace symbolization mislabels overlay-selected PCs

The trace ring and log resolve every PC against the **primary** symbol table
(`local/gs011/main/symbol_map.cpp`) regardless of which registered candidate
`overlay_try_dispatch` (`gbarecomp/src/armv4t/runtime_arm.cpp:1044-1049`)
actually selected. When two images are registered at the same IWRAM address, as
at `0x03002000`, every crash-ring line carries the wrong function name.

This already cost a full evidence pass once: a worker disassembled the
labelled, inactive image, correctly found no `BL`, and reported a contradiction
that took a second pass to resolve.

Not urgent, but it will mislead every future RAM-code or overlay investigation
the same way. Fix: symbolize against the candidate actually dispatched, or at
minimum print which candidate is live alongside the label.

### CLEANUP-02 — Diagnostic cost probe left in `runtime_irq()`

`runtime_irq()` in `gbarecomp/src/armv4t/runtime_arm.cpp` unconditionally
references cost-probe globals (`g_cost_irq_handler_ns`,
`g_cost_irq_handler_calls`, `g_cost_probe_enabled()`) defined in
`runtime_bus_bridge.cpp`. Its own comment marks it "Diagnostic-only; intended
for removal after the report".

It has already cost something real: it broke the isolated `codegen_tests` link,
which needed three stub definitions added at
`gbarecomp/tests/codegen/stubs.cpp:149` to work around.

Low priority, but decide deliberately — remove it and drop the three stubs
again, or keep it and say so. Do not remove it mid-playtest; it touches the IRQ
path and forces a full rebuild.

### WIDE-01 — Widescreen: mostly not feasible, one contingent maybe

Studied 2026-08-14. The 240 px clamp is an active opt-out, not an oversight:
`src/runner_main.cpp:2094,2098` hard-set `max_view_width` /
`max_resize_view_width` to 240 and `:2102-2103` set
`launcher_expose_widescreen=false` against an upstream default of `true`.
Enforced generically in `gbarecomp/src/runtime/view_config.h:26-42`; the engine
ceiling is `kMaxRenderWidth=480` (`gba_ppu.h:121`).

A real wide-scanline renderer already exists and is per-scanline-correct:
`render_scanline_wide` (`gba_ppu.cpp:924-1440`) runs at true HBlank with that
scanline's live IO and captured affine reference, and the OBJ loop decodes the
full 9-bit signed OAM X, so off-window sprites already render correctly. **But
it was built and proven for The Minish Cap, not Golden Sun**: the only
off-screen tile provider, `gbarecomp/src/debug/ws_sidecar.cpp`, is hardcoded to
Minish symbols, and nothing in `src/` or `config/usa/main.toml` references it.

Per scene type:

- **Regular tiled BG (towns, dungeons, field) — not feasible.** The tilemap is
  only a 256/512 px ring and wraps immediately, so margins show repeats, and
  Golden Sun has no equivalent map-streaming hook.
- **WIN0/WIN1-heavy scenes (menus, dialogue) — correctly pillarboxed to black
  by design** (`black_nonuniform_window_margins`,
  `gba_ppu.cpp:1006-1008,1409-1416`). These should not be widened.
- **World map (affine mode 2) — the only plausible win**, since an affine layer
  is a flat `128<<size_code` px buffer that may already hold real world data
  beyond 240.

**MEASURED 2026-08-15 — the world map passes.** Loaded the world-map savestate
(`Golden Sun.state5`) and read live PPU state and VRAM over the TCP debug
server:

- DISPCNT `0x1F42` → BG mode **2** (affine), as expected for the world map.
- BG2CNT `0xAA0E` and BG3CNT `0xA80A` → both size_code **2** = a **512×512 px**
  buffer, more than double the 240 px window. Wrap bit set on both, but wrap
  applies at the 512 px buffer edge, not at the viewport edge.
- **Real terrain data exists well past 240 px.** BG3's screen map (base
  `0x06004000`) is 4096/4096 bytes non-zero with 176 distinct tile indices,
  populated across columns 30-63 — i.e. the region beyond the visible 30-tile
  width is full, varied map data, not padding or stale garbage. BG2 (base
  `0x06005000`) is sparser, consistent with a marker/feature overlay above
  BG3's terrain base, but still carries real varied data past column 30.

So widening the world map would sample genuine adjacent terrain. This is the
"close to plug-and-play" case the measurement was meant to find.

Remaining care for the world-map implementation: the wrap bit means scrolling
to the map's own boundary would show the opposite edge wrapping in. Decide a
policy there (clamp, or pillarbox at the true map edge) rather than letting it
wrap visibly.

Not yet hard-confirmed: the savestate was identified as the world map by BG
mode 2 alone, which is strong but indirect. Worth one visual confirmation
before building.

Also noted: mosaic is unimplemented repo-wide — a pre-existing PPU gap, not a
widescreen one.

### WIDE-02 — Camera zoom-out slider, as an alternative to widescreen

User request, offered explicitly as an alternative to WIDE-01: a percentage
slider that zooms the camera out so more of the current map is visible, scaled
down to fit the normal 240x160 frame. Aspect ratio and frame stay stock; only
the scale changes. The user's framing: "camera can not show more than base
game, but can render more of the current map at once."

The honest caveat is the one WIDE-01 established: zooming out needs the same
off-screen picture data widescreen needed, so it may hit the same wall. But it
is **not** obviously the same answer. A regular tiled BG map is 256 or 512 px
wide against a 240 px window (`gba_ppu.cpp:1086-1088`), so a modest zoom — up
to roughly 2x on a 512 px map — could be sampling real tile data rather than
the wrapped seam.

Determine first whether Golden Sun keeps the full map ring populated or streams
tiles in just ahead of the visible window, leaving stale tiles in the
off-window part. Affine scenes are a separate and probably easier case, since
the affine transform already scales natively. Sprites are fine either way: the
OBJ path already decodes the full 9-bit signed X
(`gba_ppu.cpp:1259-1398`). Also check what a non-integer scale does to the
existing integer-scaling and supersampling paths, which assume the stock pixel
lattice.

## Recently resolved

Kept as one-liners so a returning reader does not re-open them. Full diagnostic
narratives are in git history and `docs/history/`.

| ID | Resolution |
|---|---|
| PERF-07 | Battle/menu stutter, 2026-08-20, a regression. `gbarecomp` `59c1858` (a WIP checkpoint, marked not-a-working-state in its own message) added a 128-deep dispatch recursion guard `kOverlayDispatchRecursionCap`; when it tripped, `overlay_try_dispatch` returned `0`, which every caller reads as "no resident code for this address". The code was resident and byte-identical, so the caller re-requested a compile it already had and dropped to the interpreter meanwhile. With guest tail jumps emitted as host calls (**CORE-01**), tail-looping routines hit 128 constantly: the same 24-byte body at `0x03006100` (crc `C4790BAA`) was recompiled **338 times** in one battle, 2581 RAM compiles total. Fix: the guard is removed entirely, restoring pre-`59c1858` behaviour (verified against `add0815` — none of those symbols existed before the checkpoint). **The `g_runtime_image_base` save/restore around `e.fn()` (`overlay_loader.cpp:1688-1691`) landed in the same commit but is independent relocatable-overlay functionality; deleting it alongside the guard caused an immediate crash on load at `pc=0x00000000`. It stays.** After: ~195 compiles per battle, worst body 16x, no cap trips. User-confirmed. Diagnosed by instrumentation now left permanently on (`[ram-compile]`, `[dispatch-miss]`), not by reasoning. Full record: `docs/HANDOFF_2026-08-20.md`. Residual hitch tracked as PERF-08. |
| CRASH-04 | Mercury Lighthouse cutscene aborted (`SELF-HEAL bridge hit interpreter Undefined at pc=0x03002038 while bridging dispatch miss 0x03002000`, `logs/session_20260815_192102.log`). Cause: the tail-dispatch refactor's `overlay_resolve()` sets `g_runtime_image_base` for a relocatable RAM-heal entry but nothing restores it after the call returns, unlike the pre-refactor `overlay_try_dispatch`; a later relocatable body reached through the fixed static dispatch table (the one tier that never sets the base) could read a stale value left by an unrelated earlier dispatch. Fix, in the emitter rather than a caller-side wrapper (keeps the sibcall/tail-jump property `CORE-01` exists for): `arm_codegen.cpp`/`emit_function.cpp` now resync `g_runtime_image_base` to the current function's own base immediately before every `runtime_dispatch`/`runtime_dispatch_with_exchange` call site (a store before a tail transfer, free — costs nothing structurally) and again right after a `BL`/link call returns (already a non-tail path). Verified via `tail_dispatch_overlay_tests` (net host-stack drift stays negative over 2000 laps, well inside the 262144-byte ceiling) and a controlled A/B: the emitter fix and the old crude caller-side save/restore probe, run from matched-fresh caches, produced equivalent results (no abort either side, ~88 vs 87 `ram_crc_mismatches`, ~2.98M interpreted instructions both). User-confirmed 2026-08-15: manual playthrough on `build/gs011_test` clears the Mercury Lighthouse cutscene. **Folded into `build/gs011_opt` 2026-08-16 (rebuilt 12:14); 86-test Python suite and `tail_dispatch_tests` pass against it.** Two adjacent findings that turned out NOT to be bugs: (1) the ~88-91 `ram_crc_mismatches` logged at `pc=0x03000820` during investigation are a genuine self-modifying-code detection working as designed — `GSR_IMGBASE_DEBUG=1` (new permanent debug env var, `overlay_loader.cpp`) shows the live CRC actually changing between occurrences while `g_runtime_image_base` stays correct and stable; (2) `local/gs011`'s 17 relocatable `transient_*_pic` corpora were stale relative to the current `gba_recompile` (predating a resume-trampoline dedup that collapses per-resume-point duplicate functions into goto-labeled switch cases inside one shared decode) — regenerating from the exact command already documented in `docs/GS011_TRANSIENT_IMAGES.md` reproduces identical dispatch/alias coverage; verify a regen with more than a line-count glance (function/case counts plus an instruction-level diff of one function), since the dedup shrinks line count ~9x while covering the same PCs. |
| CRASH-01 | Hard abort leaving Vale for the overworld. Our shadow return stack leaked on a legitimate shared-trampoline idiom: `runtime_call_cancel_return` popped only when the frame was exactly on top, stranding one frame per loop iteration until the 1024 ceiling aborted. It now scans down to `g_call_return_floor` and truncates, mirroring `should_return`. User-confirmed 2026-08-14. The overflow `abort()` stays as a deliberate tripwire. |
| PERF-01 | Battle attack/Psynergy stalls, two separate causes. Our compile-storm bug is fixed in `overlay_loader.cpp` (identity-window plus warm-lookup): interpreted_insns 870,336 → 34,221, worst frame 136.8 ms → a 49.8 ms one-time hitch. The rest was the real GBA's own 1x cost, now addressable with the guest CPU overclock. |
| PERF-04 | Name-entry screen lag on fresh boot. Fixed by PERF-06; user-confirmed. |
| PERF-06 | `build/gs011` was an unoptimized Debug build with no `-O` flag anywhere. `build/gs011_opt` at RelWithDebInfo measured ~2.9x faster over 600 headless frames. Always check build type before trusting a perf number. |
| VFX-FLICKER-01 | Kolima blue barrier opaque and on one character. The game fakes translucency by drawing on alternating frames; a real GBA's slow LCD smears them, our instant-pixel display shows the alternation as shimmer. Addressed by the opt-in temporal blend ("LCD ghosting"), default off. |
| AUD-02 | `gba_audio.cpp` added ch3 (SOUND3, wave RAM) without the `<<3` the other three PSG channels got, making SOUND3 ~8x too quiet and everything else sound too loud by comparison. `mix_psg_samples()` now applies the shared x8 to all four channels equally, matching mGBA. |
| UI-02 | Controller-bindable hotkeys, plus Turbo Held and Turbo Toggle as separately bindable modes. Shipped. |
