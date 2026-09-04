# CRASH-03 handoff — poisoned IWRAM stack word feeding the pool-LDM

Companion to [`CRASH-03.md`](CRASH-03.md). This file is durable technical
guidance for anyone resuming CRASH-03; it is not a session diary. Evidence
below was independently re-verified against `logs/session_20260826_122040.log`
and current code on 2026-08-26.

## Process state (where this stands)

1. **Investigation complete** (`session_20260826_122040`, Ox Alpha Max): the
   wild jump recurs as documented; every retained record is a carrier, so the
   origin writer of the `0xDCEF0210` poison is still unknown. No root fix is
   authorized yet.
2. **The approved bounded diagnostic below is now implemented and validated**:
   depth-8 per-slot write rings over `0x03007E00..0x03007E40` plus fixed slots
   `0x03007E24/28`, `g_runtime_state_epoch` tags on every record, one-time
   `[pool-ldm-crc] reason=arm` window CRC, and
   `runtime_note_pool_ldm_epoch_crc()` re-CRCs alongside each auth epoch. A
   restore boundary now clears stale host-only rings, arms before resumed guest
   execution, and emits `[pool-ldm-crc] reason=restore` without changing IWRAM.
    Synthetic coverage lives in `gbarecomp/tests/armv4t/pool_ldm_probe_test.cpp`
    (ctest name `pool_ldm_probe_tests`) and proves retention of a non-carrier
    poisoning PC across carrier pushes, depth bounding, epoch tagging, CRC
    sensitivity, and the direct stack-window bus seam through an IWRAM mirror.
    The seam now records every measured window cell, not only the two fixed
    slots. Focused CTest, the full public Python suite (86 tests), the playable
    build, and the public audit all pass.
3. **Next evidence step**: repeat the manual WIDE-01 capture (root launcher,
   diagnostics toggle on, Bilibin → Palace → exit → re-enter). Two outcomes:
   - If the crash recurs, the new `pool-ldm slot-ring[*]` lines must contain a
     record writing `0xDCEF0210` whose pc is neither the carrier `0x030060C0`
     nor absent, tagged with a `state_epoch`. That names the origin actor;
     scoped root-fix work may then begin.
   - If no such record exists anywhere in the rings, compare the restore CRC
     and first post-restore writes. This distinguishes poison already present
     in restored IWRAM from a later unobserved writer.
4. Do not attempt any guest-state patching or identity registration until
   outcome (a) or (b) above is measured.

## Milestone / hypothesis

Milestone context: Golden Sun playable through early game; dynamic RAM stays
**NOT_STATIC**; widescreen/Bilibin acceptance is in flight (`STATUS.md`,
`NEXT_TASK.md`).

Working hypothesis: the wild jump is caused by a **persistent poisoned IWRAM
stack word** at `0x03007E30` (`0xDCEF0210`, paired with `0x00000000` at
`0x03007E2C`). The pool dispatch at `0x03006100` executes an LDM whose loaded
words include that slot, injecting the poison into r9; derived dispatch then
targets garbage (`0xDCEF03D0` family), self-heal bridges data-as-code, and the
interpreter dies at an UND op after ~2.7M instructions. The **origin writer of
the poison is NOT identified** — every retained record so far is a carrier —
so no root fix is proven and none may be attempted until provenance lands.

## Earliest evidence chain (session_20260826_122040.log)

1. `L1192`/`L1215`: `[dispatch-miss] reason=not-healed pc=0xDCEF03D0 crc=0x00000000`.
2. `L1194`–`L1209` pool history (oldest→newest): `history[5]` enters
   `0x03006100` with `r9=0x03006300`; `history[6]` enters `0x03006108` with
   `r9=0xDCEF0210` — divergence happens inside the LDM block at `0x03006100`
   (`0x03006104` = `LDM SP!,{r5,r9}` per prior synchronized analysis).
3. `L1210`: `pool-ldm load frame=391 pc=0x03006100 sp=0x03007E2C pre_r9=0x03006300`.
4. `L1211`–`L1212`: live `[0x03007E2C]=0x00000000`, `[0x03007E30]=0xDCEF0210`;
   last-writer for both is `pc=0x030060C0 frame=391 r9=0xDCEF0210` — a
   **carrier**: r9 was already corrupt when that prologue pushed it.
5. `history[0]` (oldest retained) *already* has `r9=0xDCEF0210`; later entries
   show `r9` restored to `0x03006300` and re-poisoned — the bad word persists
   across pool invocations in the stack slot.
6. `L1213`–`L1214` fixed slots `0x03007E24/28`: writer `pc=0x08000716`,
   values `0x02003050` (EWRAM) / `0x0858517C` (ROM) — plausible, healthy,
   distinct from the carrier. Fixed-slot identity requested by NEXT_TASK.md is
   collected and clean.
7. `L1216` self-heal bridges the miss; `L1219`–`L1220` interpreter UND at
   `0xDD94C2F8` after `iters=2715594`. Prior sessions died at `0xDD94E128`
   (20260825_120847:L963), `0xDD94C538` (20260826_122442:L6236),
   `0xDD94D034` (CRASH-03.md) — same family, differing endpoints because the
   bridge executes data-as-code.
8. Timing: `frame=391` counts guest VBlank-starts since session start
   (`g_runtime_vblank_starts`, runtime_bus_bridge.cpp:56/:944);
   `ppu.frame_count()` ≈ 520229 is restored-from-savestate guest timeline
   (`[wide-policy-sample]` L1217–L1218) — Bilibin transition window.
   Both counters must be read separately; neither is reset by savestate load.
9. Fast-EWRAM diagnostics are exonerated for this crash: observer callbacks
   fire only for region `0x02` at both seams
   (`gbarecomp/src/armv4t/runtime_arm.h:445-482`,
   `gbarecomp/src/gba/gba_bus.cpp` write8/16/32 Ewram arms only), the
   runner callback mutates host state only
   (`src/runner_main.cpp:1183-1190`), and recurrence predates them
   (sessions `20260825_151756.log:3538-3554`, `20260825_154953.log:285+`).

## Where to look

Probe implementation (all in `gbarecomp/` unless noted):

- `src/armv4t/runtime_arm.cpp` — probe state (~:145-190),
  `pool_ldm_note_write` / `pool_ldm_snapshot_before_body` (~:191-258),
  arm point `runtime_pool_dispatch_history_record` (~:261-281),
  crash dump `runtime_pool_dispatch_history_dump` (~:283-345),
  bus seam `runtime_note_pool_ldm_bus_write` (~:396-411).
- `src/armv4t/runtime_arm.h` — `bus_fast_ram` (:359-368), EWRAM-gated fast
  store observers (:445-482), probe seam declarations (:160-179).
- `src/gba/gba_bus.cpp` — write8/16/32: Ewram arms carry the wide-diagnostic
  observer; Iwram arms call `note_ram_code_write_bus` +
  `runtime_note_ram_image_bus_write`, which feeds the pool-LDM probe.
- `../src/runner_main.cpp` — wide observer body (:1183-1190) and toggle-gated
  wiring (:1256-1263); `[wide-auth-epoch]` emission inside
  `begin_golden_sun_field_auth_epoch()` (:1045 area).
- `src/runtime/runtime.cpp:1757-1775` — savestate load resets cycles and bumps
  `g_runtime_state_epoch` but restores full IWRAM (including the stack)
  without scrubbing it.
- `src/runtime/runtime_bus_bridge.cpp:56,:944,:270` — `g_runtime_vblank_starts`
  definition/increment; `g_runtime_state_epoch` storage.
- Stack-image overlap context: `config/usa/transient-func-2cf4-03007ba4.toml`
  (image ends `0x03007C0C`; tracked slots sit ~0x220 above its tail).

Log tags to grep: `[dispatch-miss]`, `pool-history[`, `pool-ldm load`,
`pool-ldm slot=`, `pool-ldm fixed-slot=`, `[wide-auth-epoch]`.

## Ranked hypotheses

1. **Persistent poisoned IWRAM stack word at `0x03007E30`** survives across
   pool invocations; LDM `0x03006100` injects it into r9; derived dispatch
   yields `0xDCEF03D0`. Strongly supported; origin unresolved.
2. **Origin actor outside current retention**: DMA, non-pool CPU store, or
   pre-probe/savestate-restored stale IWRAM (load restores full IWRAM incl.
   stack; runtime.cpp:1757-1775 does not scrub it). Supported by
   carriers-only evidence; needs ring + epoch instrumentation to discriminate.
3. New fast-EWRAM diagnostics caused it — disproven (gating proof above;
   recurrence exists in 20260825 sessions).
4. Generated code producing `0xDCEF0210` de novo — unsupported: identical
   value across ≥4 sessions behaving as stored-then-reloaded memory.

## Do-not-do safety boundaries

- Never patch guest state or savestate contents; never scrub the stack slot.
- No guessed RAM identities / overlay registrations beyond measured log
  evidence; no whitelist for invalid PCs; invalid-dispatch signals stay loud.
- Interpreter bridging is dynamic self-heal behavior, never static.
- No root-fix attempt until a non-carrier poisoning write is captured.
- Never edit `generated/**`; preserve unrelated dirty work.
- Gameplay runs go through the root launcher only; ROM gate SHA-1
  `5c4695205413df7db52b9a184815a07783999971`; strict-static/capture uses
  Native 240x160.

## Smallest approved change (bounded diagnostic, not a fix)

Extend the existing pool-LDM probe only (`gbarecomp/src/armv4t/
runtime_arm.{h,cpp}`):

1. Depth-8 write ring per tracked slot (same payload-free fields: addr,
   value, width, pc, frame, sp, r9) over both the window cells
   `0x03007E00..0x03007E40` and the fixed slots `0x03007E24/28`, so pushes no
   longer erase the poisoning write. Newest entry still backs the existing
   `last-write` line shape.
2. Tag every new record (ring entries + the `pool-ldm load` snapshot line)
   with `g_runtime_state_epoch` to separate pre/post-savestate-load writes.
3. On probe arm emit a one-time CRC-32 (IEEE, poly `0xEDB88320`) of guest
   bytes `0x03007E00..0x03007E40`; expose `runtime_note_pool_ldm_epoch_crc()`
   so the runner can re-CRC alongside each `[wide-auth-epoch]` emission.
   Payload-free: CRC only, no retained guest bytes.

No guest-visible behavior change; strict-static claims untouched; dump output
only fires on the existing invalid-dispatch path plus one arm-time CRC line.

## Acceptance

Synthetic (ctest `pool_ldm_probe_tests`, no ROM/BIOS):

- Arming via `runtime_pool_dispatch_history_record` emits exactly one
  `[pool-ldm-crc] reason=arm` line.
- A poisoning store into `0x03007E30` followed by ≥8 carrier pushes remains
  visible in the dump ring with its own PC (not overwritten), while the
  newest-entry `last-write` lines keep their existing shape.
- Ring records carry distinct `state_epoch` values after a simulated
  savestate-load epoch bump.
- `runtime_note_pool_ldm_epoch_crc()` reflects changed window contents.

Manual (single authorized capture per NEXT_TASK.md through the root launcher,
wide diagnostics enabled): if the crash recurs, the `pool-ldm` section must
contain a ring entry writing `0xDCEF0210` into `0x03007E30` whose pc is
neither the carrier `0x030060C0` nor absent, tagged with a `state_epoch`;
plus `[pool-ldm-crc]` lines bracketing auth epochs. That capture either names
the poisoning writer (root-fix work may begin, scoped to the proven writer)
or proves stale-since-load IWRAM (then investigate restore-path provenance).
