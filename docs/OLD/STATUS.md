# Current status

Updated 2026-08-30. Faithful 240x160 output and original timing remain the
default; enhancements are opt-in.

## Milestone

Golden Sun is playable through the early game. The current baseline still has
dynamic RAM marked **NOT_STATIC**. The battle/menu compile-stutter regression
is fixed; residual small hitches are **PERF-08**. **CORE-01** is parked; see
[`PARKED.md`](PARKED.md).

## Build and launch

- Playable build: `build/gs011_opt`, selected by the repository-root
  `GoldenSunLauncher.exe`.
- Launch only `GoldenSunLauncher.exe`; the user performs gameplay tests.
- Normal non-LTO builds are preferred. Strict-static and capture runs force
  Native 240x160.
- Latest serial playable build passed. Latest validated-ROM replay
  `logs/session_20260825_221446.log` crossed the former Bilibin crash window
  through frame `522170`; this does not prove a behavior fix.

Session `20260829_211953` isolated the little-girl NPC bottom jump to slot 15's
exact raw-Y crossing `162/-94 → 161/-95 → 160/-96 → 159/+159`. Strict
Expanded's per-slot fail-closed final-render-Y latch now ignores identical
same-frame scanline calls and clears on same-frame raw/identity/activity,
target, epoch, or exact-provenance changes. Native and 288x160 remain
unchanged. A native presentation mirror remains future enhancement work; this
correction is Expanded-only. Rebuild and root-launcher manual acceptance are
pending.

The opt-in Expanded Experimental Fixes culler now requires exact same-frame
source/placement provenance, uses top-left full-sprite bounds, and fails open
for ambiguous coordinates, affine sprites, and unpaired body/shadow entries.
Focused widescreen tests pass `3/3`. The non-LTO playable build was rebuilt
successfully; manual acceptance remains pending.

Session `20260830_134250` recorded no experimental culls; all 164 observed
objects were kept. The premature bottom disappearance instead occurs at a
parentless B328 producer cutoff before the object reaches the final culler.
The 14:15 non-LTO build adds bounded B328-to-writer tracing without changing
rendering. Focused widescreen tests pass `3/3`; one root-launcher recapture is
required before that route can be widened safely.

Session `20260830_145317` found a reused OAM slot retaining the previous
sprite's signed placement across an ATTR identity change. The 15:04 non-LTO
build clears all derived placement and alias state immediately on that exact
mismatch. Parentless candidates remain rejected. Focused widescreen tests pass
`3/3`; manual acceptance regressed to sprite bottom wrapping. Treat the 15:04
reconciliation as suspect; see `issues/WIDE-01_CULLING_HANDOFF.md`.

Session `20260830` removed the 15:04
`reconcile_golden_sun_obj_slot_commit_identity()` change and its two helper
functions and sole call site from `src/runner_main.cpp`, restoring the prior
top-left/full-size bounds culler as the active state. Rebuild and manual
acceptance are pending.

Session `20260830` evening also recovered the toolchain after a Windows
reinstall: Git 2.55, CMake 4.4.3, Ninja 1.13.2, Python 3.12.10, and MSYS2/
MinGW GCC 16.2 were installed, `C:\msys64\mingw64\bin` was added to the user
PATH, and the stale `build/gs011_opt` CMake cache was regenerated. Runtime
DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) were
copied next to the repository-root `GoldenSunLauncher.exe` and into
`build/gs011_opt`. `config/local.json` `rom`/`bios` paths were repaired and
both checksum-verified (BIOS is `gbarecomp/bios/gba_bios.bin`).

A new BIOS-free feasibility investigation also began this session, tracked as
informational analysis only, not an active fix:
[`BIOS_FREE_CENSUS.md`](issues/BIOS_FREE_CENSUS.md) and
[`BIOS_FREE_IRQ_PATH.md`](issues/BIOS_FREE_IRQ_PATH.md), the latter including a
§6 observed-behavior black-box capture. This falls under the already-parked
`BIOS-01` in [`PARKED.md`](PARKED.md); behavior is derived by black-box
observation, not BIOS-byte disassembly, and the project's BIOS policy is
unchanged.

## Acceptance boundary

Current manual boundary is widescreen/Bilibin. Session `20260826_152239`
provided two user-identified Palace entries and the measured split-scroll
evidence now implemented as a separate
`AuthorizedMode0SplitScroll` Palace map class; runtime authorization still
requires a complete clean frame and fails closed on invalid/mixed rows. The
Palace table provider now additionally requires the exact map/raw CRC pair
`0aef4a71/11020c66`, authenticates once per auth epoch, invalidates on CPU/DMA
table writes, and fails closed on CRC mismatch. It rejects measured fill ID
`0x026`, raw `0xFFFF`, and finite out-of-bounds coordinates. Other residual IDs
are unmeasured and remain a limitation.
latest `gs011_opt` rearms bounded VRAM metadata per authentication epoch,
separates CPU/DMA EWRAM-table provenance, reports culls per epoch, and now
emits a bounded payload-free `[wide-field-producer]` PC/range summary so raw
producers are not hidden by the 64-record detail cap. Generated fast-path
EWRAM stores reach this diagnostic
seam only when the launcher toggle is enabled; Expanded provider runs currently
keep authored-cell tracking diagnostic-only for equal-scroll fields. Session
`20260828_111007` proved the attempted runtime gate regressed restored Bilibin:
its valid `7ac260c4/060ecd2c` table had `auth_cells=0`, rejecting every lookup.
The uncommitted root/upstream source now replaces the old immediate/literal
cull hooks with ten exact conditional-branch routes enabled only for strict
Expanded 360x240 geometry; Native and 288x160 preserve guest decisions.
B27E/B324/B328 overrides are limited to the newly visible coordinate bands; the
other seven reviewed routes retain their measured decisions. OBJ presentation
providers, final PPU clipping, and the 128-slot OAM
limit remain unchanged. B27E/B328 now create fresh OAM-Y provenance using
R7+16/R7+4 and emit bounded payload-free `[wide-obj-y]` gate records.
Per-layer/region replacement IDs are aggregated in bounded
`[wide-field-map-id]` records. Equal-scroll field lookups now again use restored
table contents while retaining measured sentinels and finite bounds; the user
confirmed Bilibin expansion restored in session `20260828_143521`. That log
then proved the seven earlier object-cull branches admitted coordinates
unconditionally. They are now operand-bounded to the newly visible padded
360x240 bands; original decisions are preserved elsewhere. Python tests
previously passed `86/86`; the updated focused CTest passes `1/1`.

Session `20260828_150103` still showed premature left-edge object loss and
Palace residual texture leakage. C6FA's signed 16.16 `x+32` lower band is now
admitted through `-60px` only in Expanded mode. Palace now additionally uses
a per-layer connected active-region mask seeded from the authentic canvas;
disconnected non-fill atlas islands fail closed as backdrop. Bilibin, Native,
and 288x160 are unchanged.

Session `20260828_153341` confirmed horizontal culling but still showed
premature vertical loss and connected Palace residual leakage. In authenticated
Expanded fields, unambiguous OBJ Y `160..199` now renders without writer
provenance; exact `192` remains provenance-gated as the hidden-sprite sentinel.
Palace reachability is now bounded to four horizontal and three vertical
metatile cells from the authentic canvas instead of flooding the full table.

Session `20260828_155411` proved adjacent Palace atlas fragments still pass
that mask and that remaining vertical loss occurs after the widened branches.
Payload-free `[wide-palace-margin*]` coordinate/ownership records and
steady-state `[wide-obj-y-*]` per-slot outcome counters are now implemented;
rendering behavior is unchanged pending the next evidence capture.

Session `20260828_163525` reproduced the same behavior. It proves Palace
leakage is accepted connected provider data, but the first-sample-only records
cannot locate the visible cells. OBJ-Y records are also multiplied by renderer
invocations rather than deduplicated slot/frame events. The next task is
diagnostic refinement only; see `docs/issues/WIDE-01_HANDOFF.md`.

The object path has since been simplified: strict Expanded mode widens the
measured guest viewport branches and records signed logical X/Y per OAM slot
before GBA OAM truncation. The wide renderer consumes only matching records
latched with the exact OAM identity and otherwise uses canonical wrapping. This is independent of field or
Palace classification; Native and 288x160 are unchanged. Manual culling
acceptance is pending, then Palace background leakage is next.

The first simplified build missed provenance on `B27E` when Expanded forced
its taken reject into the direct OAM-writer fall-through. Those admitted
bottom-margin objects therefore wrapped as negative Y. Both original and
forced fall-through paths now retain signed Y; the numeric boundary remains
the correct Expanded `199` rather than unsafe actor-pre-cull slack `248`.

The attempted authenticated-field fallback for unprovenanced raw Y `160..199`
was unsafe. The user's `Stillwrapping.png` proved a north-of-screen NPC at raw
Y `191` was reinterpreted as positive `191` and appeared at the bottom. The
fallback is removed: only matching signed slot provenance may override
canonical GBA wrapping.

The follow-up statue screenshot proved the earlier `Func_b388` lower-bound
hook also mixed axes: vertical padding used the 60px horizontal margin, making
Y `-92` instead of `-72`. Lower X remains `-92`; lower Y is now the stock
`-32` expanded only by the 40px top margin to `-72`.

The next screenshot still showed the north statue at the bottom. The generic
PPU already decodes unprovided raw Y `>=160` canonically, proving the rich
slot provider accepted false positive signed provenance. A bounded
`[wide-obj-y-alias]` diagnostic now records writer/slot/frame/generation and
rendered OAM identity; rendering is unchanged pending one enabled capture.

Session `20260828_222157` reproduced the statue but had WIDE diagnostics off.
The tightly bounded/deduplicated alias record is now automatic in strict
Expanded mode; broad WIDE diagnostics remain opt-in. Rendering is unchanged.

Session `20260828_223034` reproduced a later wrap but emitted no positive alias,
indicating the final writer may already receive a plausible positive Y below
160. Automatic `[wide-obj-y-jump]` records now capture accepted per-slot
logical-Y changes of 64+ pixels with writer/OAM identity. Rendering is unchanged.

Session `20260829_101634` proved the visible OAM and its signed placement
provenance were one frame apart while moving: the same slot and object identity
alternated between `accepted-positive` and `canonical-provenance-y-mismatch`.
Placement provenance is now split into pending shadow state and visible state,
latched only by the exact 1024-byte `0x0300347C -> 0x07000000` OAM DMA. Native
and 288x160 remain unchanged. Focused CTest passes `1/1`; manual retest is
pending.

Session `20260829_104121` showed the latched record consistently two producer
frames behind rendered OAM. The visible path now keeps signed coordinates while
ATTR0/1/2 remain identical to the 12-byte staging record latched by the exact
shadow-to-OAM DMA; changed/reused slots fail closed. Raw-Y, epoch, and DMA
gates remain. Manual retest is pending.

Session `20260829_110157` improved culling but did not close WIDE-01. It showed
old signed records after slot changes, so changed identities now fail closed
while unchanged entries may remain valid. The exact remaining visible statue
event is still unproven.

Session `20260829_111355` still showed wrong NPC/shadow culling and bottom
wrap. Exact matching B328 provenance carries ambiguous positive Y `160..199`,
so the remaining sign loss is before B328 rather than in the PPU or OAM latch.
Session `20260829_112427` captured 16 such writes, but none matched the earlier
B3E6/B3EC/C702/C708 routes. The trace now carries B328's own R1/R3 inputs and
uses staging identity to preserve useful samples. Rendering is unchanged.

Session `20260829_113022` shows B328's ambiguous values are direct arithmetic:
for example `193 - 14 = 179`. The trace now links each B328 child to its B27E
parent route and records the fixed-point source inputs. Rendering is unchanged.

Session `20260829_114447` captured the visible statue twice at Y `179`. Both
records had no B27E parent authorization; the fixed-point inputs also proved
there was no arithmetic wrap. B328 now keeps the guest's cull decision instead
of widening this unauthenticated alternate route. B27E/B324 remain widened.
The user reports the 11:49 build is better but still not fixed; no later session
log was supplied. Continue from
[`WIDE-01_CULLING_HANDOFF.md`](issues/WIDE-01_CULLING_HANDOFF.md).

Session `20260829_160156` showed parentless statue records alongside legitimate
B27E-correlated B328 records. The current strict Expanded policy reopens B328
only for an exact same staging/frame/call-depth/return-PC parent admission;
parentless and stale routes remain closed. Focused policy tests pass `1/1` and
the non-LTO `gs011_opt` rebuild completed at 16:17:15. Manual acceptance is
pending through the root launcher.

The verified-ROM corpus was regenerated and the non-LTO
`build/gs011_opt/GoldenSunRecomp.exe` was rebuilt at 2026-08-29 16:17:15
(880088067 bytes); the worktree is uncommitted. Session `20260828_102032`
completed the diagnostic implementation/rebuild pass; no visual or crash fix
is claimed. The Bilibin regression fix has focused CTest `1/1`; Python/audit
could not be rerun because no Python command is available in this shell.
WIDE-01 remains open pending root-launcher manual capture. Follow
[`NEXT_TASK.md`](NEXT_TASK.md).

Session `20260829_162157` reports shadow culling improved but NPCs still jump
to the bottom edge or cull early. The latest non-LTO `gs011_opt` adds
diagnostics-only B328 `160..199` parent classification and uncapped accepted
candidate-to-OAM-handoff outcome totals; rendering policy is unchanged.
Built 2026-08-29 16:42:29 (880118555 bytes). Manual root-launcher retest is
pending.

Session `20260829_164429` still reports NPC jumps/early culls. The next
diagnostic build correlates rejected B328 candidates against both measured OAM
writer entries D4 and F0, retaining slot/ATTR0/1/2/frame/call context. The
follow-up diagnostics-only build captures F0 entry R6 at exact `0x030038EC`
before its LDM, then classifies F0 commits as consumed, unrelated,
identity-unproven, context-mismatch, attr-mismatch, or expired. Rendering is
unchanged. Non-LTO `gs011_opt` rebuilt at 17:25:28 (880205304 bytes); focused
widescreen CTest passes 1/1. Manual root-launcher recapture is pending.

The 2026-08-29 17:57:43 non-LTO build (880242809 bytes) adds a diagnostics-only final OBJ-Y provider transition
trace for raw Y `159..199`: uncapped reason/region/signed-vs-canonical
aggregates plus bounded first/change-per-slot samples carrying selected or
canonical Y, ATTR0/1/2, and provenance frame/epoch/target/writer identity.
Rendering policy is unchanged. Recapture through the root launcher with
Expanded 360x240, Widescreen diagnostics ON, and Self-heal OFF.

The 2026-08-29 18:19:17 non-LTO build (880290591 bytes) refines that trace into
three independent 128-sample buckets: active-candidate canonical fallback,
signed-bottom, and same-slot/target outcome transitions. Empty, disabled, and
dormant ATTR0 entries are excluded from reserved samples; uncapped aggregates
remain unchanged. Focused widescreen CTest passes `1/1`; rendering is unchanged.

The 2026-08-29 18:50:25 non-LTO build (880333327 bytes) adds a separate
accepted-B328-to-F0 diagnostics ledger. It captures F0 entry R6 at
`0x030038EC`, then correlates exact frame/epoch/call context and committed
OAM target/ATTR0/1/2 as consumed, unrelated, context-mismatch, attr-mismatch,
or expired. Accepted-F0 aggregates and bounded samples cannot be starved by
rejected/top traffic. Rendering policy is unchanged; focused widescreen CTest
passes `1/1`.

CRASH-03 provenance now arms at the savestate restore boundary, clears stale
host-only rings, and emits a payload-free `reason=restore` CRC before resumed
guest execution. Session `20260828_102032` adds first-transition poison and
OAM provenance diagnostics; this remains diagnostic-only and the poison writer
is unproven.

Open work is indexed in [`ACTIVE_ISSUES.md`](ACTIVE_ISSUES.md). Load exactly
the linked issue or feature file for the task. Do not use
[`history/`](history/) as current guidance.

## Next session starts here

1. User tests the 2026-08-30 culling revert in Expanded mode with
   Experimental Fixes on. This has not been tested yet.
2. If that test is good, consider committing. Git is now available in this
   environment, and nothing in the repository has been committed since
   2026-08-27.
3. The BIOS-free next step is a longer watchpoint capture to close the four
   `TODO-EVIDENCE` items in
   [`issues/BIOS_FREE_IRQ_PATH.md`](issues/BIOS_FREE_IRQ_PATH.md).

## Protected-data gate

ROM SHA-1 must be `5c4695205413df7db52b9a184815a07783999971`. Never commit or
expose ROM/BIOS bytes, saves, private traces, screenshots, or generated files
containing substantial protected data.
