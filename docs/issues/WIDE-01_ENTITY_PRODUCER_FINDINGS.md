# WIDE-01 entity-producer census — findings (2026-08-30)

This is the findings report for the investigation defined by
`WIDE-01_ENTITY_PRODUCER_CENSUS.md`. Read that doc first for the objective,
boundary, and evidence rules — this file is the result, not the playbook.
It does not authorize or propose any culling/mirroring implementation; see
"What remains before implementation" at the end.

## Reproduction

Faithful replays go through the root `GoldenSunLauncher.exe` only, via
`scripts/gs-replay.ps1`:

```powershell
.\scripts\gs-replay.ps1 -LoadState "<state file>" -ReplayFrames <N> `
  -InputReplay "logs/session_<id>.input" -VramMapTrace "1" -Mute
```

`gs-replay.ps1` sets the `GBARECOMP_*` replay env vars, starts the launcher,
and places the window on the secondary monitor without stealing focus. It
never invokes `build/gs011_opt/GoldenSunRecomp.exe` directly.

**Why bypassing the launcher breaks this investigation silently:** the
launcher is what turns on RAM self-heal for the child process. Launching the
child directly (or otherwise bypassing the launcher) leaves self-heal off;
the game runs and *looks* fine, but the field NPC/shadow staging DMA that
this whole census depends on — `pc=0x0800C674`, `[oam-shadow-dma]`,
`0x08009BB8 -> 0x0300347C` — never fires. A broken run reports **0** calls to
that DMA instead of the expected count. This is exactly the class of
"comes back fast but wrong" failure the census doc warns about.

**Fidelity anchor, per canonical (non-Expanded) run:** the first
Bilibin/Palace session (`20260829_234316`) recorded `[oam-shadow-dma]` count
`512` and `pc=0x0800C674` count `504`, matching the live log at capture time.
That anchor holds for a **canonical 240x160** run. It does **not** transfer
directly to an Expanded run of the same input: Expanded mode drives
`auth_epoch` transitions (`begin_golden_sun_field_auth_epoch()` in
`src/runner_main.cpp`) that call `gba::vram_trace::reset_oam_trace_window()`
at every scene boundary, so the `[oam-shadow]`/`[oam-shadow-dma]` counters
legitimately reset mid-session and never accumulate to a single fixed total.
Use **event-anchor matching** instead for Expanded runs: `frames_presented`
must equal the input length, and `[wide-field-table-summary]`'s
`map_first_frame`/`map_last_frame` (and `[wide-auth-epoch]` frame numbers)
must match across repeated runs of the same state+input. Every session below
was checked this way and matched.

**Expanded must be active for any of the transition/producer diagnostics to
say anything.** All `[wide-scene]`, `[wide-obj-stage]`, `[wide-obj-handoff]`,
`[wide-field-producer]`, `[wide-obj-record-writer]`,
`[wide-obj-record-value]`, etc. are gated on `g_ws_active`
(`src/runner_main.cpp:1036`, `:2929`, `:4135`) which is only true when the
host's fixed view mode is Expanded 360x240. This is controlled by
`build/gs011_opt/config.ini`: `[Enhancements] Widescreen=` and `ViewMode=`
(`gbarecomp/src/runtime/host_window.cpp:1719`, `:2463`). `ViewMode=2` is
Expanded (`ViewMode=0` Native, `1` Widescreen 288x160, per
`src/runner_main.cpp:179-183`). A canonical-mode replay of the same
state+input produces **zero** lines for any `wide-*` tag — that is expected,
not a bug.

**Config-restore race — read before editing `config.ini` for a replay.** The
game process persists `ViewMode`/`Widescreen` back into `config.ini` on exit
(`gbarecomp/src/runtime/host_window.cpp:2463`, `:3156`). A `cp`-based restore
issued immediately after the process-exit wait loop can race that write and
silently fail to restore the file. Verify with `Get-Process -Name
GoldenSunRecomp,GoldenSunLauncher` (expect none running) before trusting the
restored `config.ini`, and re-check the file content after restoring, not
just after copying.

**Warm-cache cost profile:** with the compiled-code cache warm, a ~1200-frame
replay with Expanded diagnostics on and the new record-value census active
takes well under a minute end to end. See "Performance" below for per-frame
cost at scene transitions specifically — that is not part of this warm-cache
total time, it is a per-transition spike inside it.

## Sessions used

| Session id | Savestate (real path from the log, not the hotkey number) | Restore frame/PC | Replay frames used | Route |
|---|---|---|---|---|
| `20260829_234316` | `<local rom dir>\Golden Sun.state2` | frame in the low 521000s, `pc=0x000001b4` | 1183 | world map -> Bilibin -> McCoy Palace -> Bilibin -> world map |
| `20260830_015113` | `<local rom dir>\Golden Sun.state3` | `frame=314852`, `pc=0x000001b4` (from `savestate_loaded` in the log) | 1273 (`frames_presented` from the log) | world map -> field town -> one indoor room -> field town -> world map |

**Hotkey numbers are not savestate file numbers in this project.** The F4
hotkey ("state3" in user language) loaded `Golden Sun.state3` in this case,
but do not assume that mapping holds generally — always read the actual
`savestate_loaded slot=... path="..."` line out of the session log before
reusing a savestate reference.

Diagnostic-instrumented Expanded replays derived from those two recordings,
in the order they were produced this session (frame counts/anchors all
verified to match, see each entry's context above):

- `20260830_011210` — first Expanded pass over the Bilibin/Palace input, no
  new instrumentation yet.
- `20260830_012504`, `20260830_013222` — after `[wide-obj-record-writer]`
  was added (cap 64, then raised to 4096).
- `20260830_014054`, `20260830_014247` — after `[wide-obj-record-value]` was
  added; `014054` had a bounds-check bug (see Diagnostics section) and its
  data was discarded; `014247` is the corrected, trustworthy run.
- `20260830_015252` — the second-town cross-validation, Expanded, with both
  diagnostics active and the bounds-check fix in place.

Scene-transition frames observed (`[wide-scene]` dispcnt/scroll changes,
`[wide-auth-epoch]`), Bilibin/Palace session:

- restore ~frame 521564, world map (dispcnt mode 2)
- Bilibin entry: dispcnt flips to mode 0 at frame 521695, settles ~521703
- Palace entry: mode-0 field->room transition at frame 522077, settles
  ~522095 (triple `auth_epoch` bump at the same frame — a load boundary, not
  three visual changes)
- return to world map: frame 522567-522580

Second-town session:

- restore frame 314852, world map (dispcnt mode 2, same as Bilibin's)
- field/town entry: dispcnt mode 0 at frame 314936, settles ~314943
- indoor-room entry: frame 315408, with an unusual `dispcnt` sequence
  (`0x6140`, `0x7f40`) at frames 315421-315423 never seen in the first
  session at all
- return to world map: frame 315975-315988

## Town identity — second session is a different town

The `[wide-field-map-id]` tile-ID sets for the second session are almost
entirely disjoint from the Bilibin/Palace session's sets (a handful of
generic/reused tile IDs overlap, as expected for shared tileset pieces, but
the bulk of each set differs). The `dispcnt` values `0x6140`/`0x7f40` at the
second session's indoor-room entry never appear anywhere in the first
session. This is a genuinely different map, not a re-visit of Bilibin or
McCoy Palace. An old project note labelled the F4/"state3" savestate as
Bilibin; that label does not match what the replay actually shows and should
not be trusted. The exact town/room name is **TODO-EVIDENCE** — no map-name
lookup tool was available to this investigation.

## The record

**Base `0x03002000`, IWRAM, stride `0x38` (56 bytes), 14 slots (indices
0-13).** Confirmed identical in both towns: same base address, same stride,
same slot count, in every scene type (world map, field/town, indoor room).
This is an engine-level layout, not town-specific.

**Active-slot count is per-scene, not fixed.** In the Bilibin/Palace session,
`[wide-obj-stage]` staging addresses (bounded by that tag's own sample cap)
showed 6 unique slots active in the Bilibin field window and 12 in the
Palace window (union across the whole capped session: 14). In the second
town, the record-writer census (uncapped for that session, see below) showed
14/14/14/13/12 unique slots active across its five auth epochs. **The
7-vs-14 discrepancy from earlier in this investigation is reconciled: "7"
was one town's per-scene active count sampled through `[wide-obj-stage]`'s
own cap, not the array's total capacity; the array itself has 14 physical
slots and different scenes populate different numbers of them.**

### Proven field-offset map (both towns)

Correlation method: join `[wide-obj-record-value]` events to
`[wide-obj-handoff]` samples by exact `(frame, record_base_address)`, then
test the decoded byte/halfword value against the reported `logical_x`/
`logical_y` for that same slot/frame.

| Offset | Size | Field | Evidence |
|---|---|---|---|
| `+0x00` | 4 bytes | candidate allocator/link pointer (see below) | not X/Y; see below |
| `+0x04` | 1 byte | Y | Bilibin/Palace: 77/77 exact. Second town: 70/70 exact. Raw byte equals `logical_y` directly (unsigned 0-255) when non-negative, or via signed-8 (`value-256`) when `logical_y` is negative. Same rule both towns, no exceptions in the second-town sample. |
| `+0x05` | 1 byte | not X/Y (tested, no match) | proven negative |
| `+0x06` | 2 bytes | X | Bilibin/Palace: 71/77 exact with plain 9-bit two's-complement (`value & 0x1FF`, `-512` if `>=256`); all 6 remaining mismatches were the single boundary value `logical_x=256`, which resolves with the unsigned 9-bit reading (`value & 0x1FF` with no sign flip) instead — so 77/77 once the boundary case is read unsigned. Second town: 70/70 exact, no boundary case landed in that sample. Bit layout matches the GBA hardware ATTR1 halfword shape (9-bit X plus a constant high byte, e.g. `0x80xx`, consistent with OBJ size/flag bits). |
| `+0x07` | 1 byte | not X/Y (tested, no match) | proven negative |
| `+0x0c` | 4 bytes | not X/Y (tested, no match) | proven negative |
| `+0x10`-`+0x13` | 1/1/2/1 bytes | not X/Y (tested, no match) | proven negative |

**This directly answers the census doc's item 4: the coordinate is in this
record, at proven byte offsets, not sourced elsewhere by `Func_b168` at
render time.** A negative result was on the table and did not happen.

Byte-level semantics of `+0x00`, `+0x05`, `+0x07`, `+0x0c`, `+0x10`-`+0x13`
beyond "not X/Y" are TODO-EVIDENCE.

### `+0x00` — candidate allocator/pool link, not proven to be body/shadow ownership

`+0x00` (4 bytes, written by `Func_3dec`, `0x08003dec`) mostly holds `0`.
When non-zero, in the second-town session it held exactly slot-base-aligned
addresses into the same 14-slot table (`0x03002150 -> 0x03002000` =
slot6->slot0; `0x030022a0 -> 0x03002188` = slot12->slot7; `0x03002118 ->
0x03002000` = slot4->slot0; 8 non-zero samples total, all captured near
frames 315806-315815). That pattern — several different slots pointing at a
shared "current head" slot at different times — is consistent with a
free-list "next" pointer in an object-pool allocator. It is not consistent
evidence of a body/shadow owner link. In the first (Bilibin/Palace) session,
`+0x00`'s non-zero values were not all stride-aligned (some fell mid-record,
e.g. `0x0300204C`, `0x030022B4`), which weakens the free-list reading
somewhat — the field may hold other data at other times, or the
alignment-breaking samples were captured mid-update by a different writer.
Per the census doc's rule, an address/pointer alone never proves ownership;
this is reported as the allocator-link candidate it is, nothing stronger.
TODO-EVIDENCE: exact mechanism (free-list vs. something else), and whether
the mid-record first-session values are a different regime or noise.

## Lifecycle: persistent, not rebuilt every frame

**Verdict: persistent, incrementally updated.** Evidence:

- In the world-map epoch (uncapped, clean census — see Diagnostics), only a
  subset of each slot's `0x38` bytes are touched at all; most of the stride
  is quiet.
- In field/town scenes, each touched field is written by one fixed
  instruction at roughly 1 write per elapsed frame (e.g. `writes=143` over a
  142-frame span) — many small 1/2/4-byte writers each owning one field, not
  one writer rewriting the whole `0x38`-byte stride every frame.
- Room entry (see next section) is a genuine one-shot bulk-fill event,
  clearly distinct in character (single frame, `writes=1` each byte) from
  the ongoing per-frame update pattern that follows it — exactly the
  "create-time writes plus small per-frame updates" signature, not a
  per-frame rebuild.

### Room-entry creation event — same in both towns

At the exact frame a field->indoor-room transition completes (frame 522084
Palace, frame 315408-onward second town), a small family of relocatable
IWRAM writer PCs performs a strict sequential single-byte fill across the
full `0x38`-byte stride of every active slot, `writes=1` each, all in the
same single frame:

- Palace: `0x03006030, 0x03006100, 0x0300613c, 0x03006144, 0x030061e8,
  0x030061f0, 0x03006200`
- second town: the same seven addresses, plus one new one, `0x03006134`,
  not seen in the Palace session.

After that one-shot fill, a stable set of fixed-ROM functions takes over
per-frame updates on individual offsets (same functions both towns, see next
section).

## Route to screen

- `Func_b168` (`0x0800b168`, size `0x220`) — the X/Y cull/placement
  function. Branches `0x0800B324` (X) and `0x0800B328` (Y) are where the
  widescreen policy code observes signed logical coordinates before OAM
  truncation. Also writes several derived 1/2-byte fields into the record
  (`+0x04`-`+0x07`, `+0x10`-`+0x13`) immediately after `Func_3dec` seeds
  `+0x00`/`+0x0c`.
- `Func_c62c` (`0x0800c62c`, size `0x250`) — the OAM-shadow-commit function.
  Contains `pc=0x0800C674`, the `[oam-shadow-dma]` staging DMA
  (`0x08009BB8 -> 0x0300347C`), and writes several more 1-byte fields into
  the record (`+0x09`, `+0x15`, `+0x25`).
- `Func_3dec` (`0x08003dec`, size `0x24`) — writes `+0x00` (see above) and
  `+0x0c`.
- `Func_aa0c` (`0x0800aa0c`, size `0x668`) and `Func_ba30` (`0x0800ba30`,
  size `0x9c`) — additional per-field writers, `+0x08`, `+0x22`-`+0x25`.
- Per-object staging addresses (`[wide-obj-stage]`) and their handoff
  destinations (`[wide-obj-handoff]`) confirmed: staging address equals the
  record's own base (`r7` in the culling code equals the record pointer,
  proven by `[wide-obj-y-cull-decision]`/`[wide-obj-y-cull-correlation]`
  showing `r7` equal to `staging` at every sample) -> destination
  `0x0300347C + slot*8` (8 bytes/slot, standard GBA OAM attribute size) ->
  final DMA, exactly 1024 bytes, `0x0300347C -> 0x07000000` (128 OAM slots),
  observed at `pc=0x0800C674`.
- `Func_1dc8` relocatable image (ROM key `0x08001DC8`, ELF-confirmed size
  `0xE0` bytes, `src/runner_main.cpp:5278`): writer route offsets verified
  relative to whatever base a given copy is resident at — `D4 = base+0x58`,
  `EC = base+0x70`, `F0 = base+0x74`. One verified instance this session:
  `base=0x03005ee0`, `pc=0x03005f38` (`=base+0x58`, route D4),
  `image_key=0x00001dc8`. Always resolve by this offset math and a fresh
  hash check, never by a fixed RAM PC — bases move between generations (see
  Cautions).

## Cross-validation result — second, different town

Everything above marked "both towns" was independently re-derived on the
second-town session using the unchanged correlation method. Summary of
match/mismatch:

- **Matched exactly:** record base/stride/slot-count; `+0x04`=Y and
  `+0x06`=X (70/70 both, no boundary exception this time); the room-entry
  bulk-fill signature and its relocatable writer family; almost the entire
  fixed-ROM writer set (`Func_3dec`, `Func_b168`, `Func_c62c`, `Func_aa0c`,
  `Func_ba30` — 18 of 19 field-epoch writer PCs identical to Bilibin's);
  persistent-not-rebuilt lifecycle; `+0x00`'s pointer character
  (stride-aligned this time, see above).
- **Did not match / new:** one of 19 field-epoch writer PCs was new and
  town-specific — `0x0200917e`, an EWRAM-resident address, not seen in
  Bilibin/Palace at all. The room-entry relocatable family gained one new
  member, `0x03006134`, alongside the same seven Palace addresses. Neither
  new PC's role was investigated further (out of scope for this pass). The
  second town's `+0x00` pointer samples were cleanly stride-aligned where
  the first town's were not (noted above as a reliability difference, not a
  contradiction).

**Conclusion: the record layout and its writer functions are general-engine
behavior, not specific to Bilibin/McCoy Palace.** Confirmed by an
independent, differently-tiled, differently-shaped map with no re-use of the
first town's dispcnt sequence.

## Graphics data availability (off-window BG content)

Two distinct questions, answered separately — do not blur them:

**Does real tile data exist beyond the visible 240x160 window?** Yes, for
the large majority of the expanded margin once a scene has been visited.
`[wide-field-provider]`, second-town indoor-room steady state
(`auth_epoch=5`, frame 315975): BG1 10,176,000 lookups, 8,586,960 succeeded
with real data (84.4%), 1,589,040 unavailable (15.6%); BG2 10,115,520/
10,176,000 succeeded (99.4%); BG3 8,526,480/10,176,000 succeeded (83.8%).

**Is it ever simply absent?** Yes, in two distinct, measured cases:

1. Right at fresh scene entry, before the authorization table is
   (re)built: `auth_epoch=6`, frame 316124 (immediately after leaving the
   room), all three BG layers show 0 replacements / 100% unavailable
   (`precondition=577800` each — the table has not been authorized yet at
   that instant).
2. Real map edges: the ~15-16% unavailable on BG1/BG3 in the steady-state
   number above is not noise — it is genuine absence of authored tile data
   at the true edge of what the game has ever populated for that map.

Sprite/OAM tile residency for currently-culled objects specifically was not
checked — the logs used here do not carry that signal. TODO-EVIDENCE if
that distinction matters for future work.

## Performance

**No leak found.** `ram-compile` counts (169-227 per ~1200-1300 frame
session) and `dispatch-miss` counts (9-18) are small and stable across all
five sessions checked, no PC recompiled more than 5 times, `ram_crc_
mismatches=0` and `ram_smc_fallbacks<=1` everywhere — no sign of the
project's known battle-scene repeated-recompile pattern. Caveat: no battle
was ever entered in any of these replays; this check covers field/town
scenes only and says nothing about battle-scene behavior.

**Real, measured cost, but self-inflicted by this session's own
diagnostics:** every session shows its largest single-frame stalls (up to
several million microseconds, `dispatch_count=0`) landing exactly on
scene-transition frames. Baseline (`20260829_234316`, diagnostics-lean):
max 290ms. With `[wide-obj-record-writer]`/`[wide-obj-record-value]` active
and the cap raised to 4096: up to 4.56s in one frame. This is the
synchronous `fprintf` reporting these new tags do at every
`begin_golden_sun_field_auth_epoch()` boundary, scaling with the number of
entries captured — it is diagnostics-gated (zero cost in a normal player
launch) but would freeze a real diagnostic session for seconds at every map
transition if left as-is. Not fixed here; flagged for whoever next touches
this instrumentation.

**Telemetry gap, not a finding:** `compile_us`, `ppu_render_us`, and
`halt_pump_us` in every session's `.phase.csv` are `0` on every row, in this
build. That field is not wired up for these runs — treat any "zero render
cost" reading as a gap, not a real measurement.

## Diagnostics added this session

Both are diagnostics-only, gated on `golden_sun_wide_diagnostics_enabled()`
(no output, no behavior change, and no cost in a normal user launch where
that gate is false), and reset at every `auth_epoch` boundary and at
process exit — same reset points as the existing `[wide-field-producer]`
census (`begin_golden_sun_field_auth_epoch()`, `install_golden_sun_
widescreen()`, and the exit report path).

- `[wide-obj-record-writer]` (`src/runner_main.cpp:2617`-`2714`). Bounded
  write census over IWRAM `0x03002000..0x030022E0`. Deduped by
  `(address, pc, size)`, cap 4096 entries/epoch
  (`kGoldenSunObjRecordCensusLimit`, `:2634`) — sized for the range's 736
  bytes times a handful of distinct `(pc,size)` writer identities per byte,
  with headroom; measured epochs stayed under 2500 entries, so this cap was
  exhaustive (0 overflow) in every session run after it was raised. An
  `[wide-obj-record-writer-overflow]` counter still fires if it ever
  truncates. Logs `addr`, `pc`, `size`, `writes` count, and `frames=first..
  last` per unique key.
- `[wide-obj-record-value]` (`src/runner_main.cpp:2716`-`2790`). Bounded
  per-event trace of the actual byte/halfword/word value written,
  restricted to relative offsets `0x00..0x13` of each slot (layout-relative,
  not town-specific — the window the X/Y correlation needed). Cap 4096
  events/epoch (`kGoldenSunObjRecordValueLimit`, `:2726`); one session saw
  a small overflow (3 dropped events) in its busiest epoch, noted but not
  chased. Values logged are decoded small integers (coordinates/pointers),
  never raw ROM/asset bytes.
- Bug found and fixed during this work: the value-event observer initially
  lacked the same `address >= kGoldenSunObjRecordCensusEnd` bound the
  write-census had, so it briefly also captured unrelated IWRAM stack
  writes near `0x03007fxx`. Fixed at `src/runner_main.cpp:2747`. Any data
  from session `20260830_014054` (the run between adding value-capture and
  fixing this) should be treated as unreliable; `20260830_014247` is the
  corrected re-run and is what this document's value-level findings are
  based on.
- Cost warning: see Performance above — these two tags are the direct cause
  of the multi-second transition stalls measured this session when active
  together at the raised cap.

## TODO-EVIDENCE (full list)

- Exact town/room identity of the second (`state3`) session's map — no
  map-name lookup tool was available.
- Byte-level semantics of record offsets `+0x00` (beyond "candidate
  allocator link"), `+0x05`, `+0x07`, `+0x0c`, `+0x10`-`+0x13`. Would be
  closed by the same value-correlation method used for X/Y, against a
  different known-observable (e.g. animation frame counter, facing
  direction, or a deliberately triggered despawn/respawn) rather than
  `logical_x`/`logical_y`.
- Exact mechanism behind `+0x00`'s pointer values (free-list vs. something
  else) — would need either watching it across an object's full spawn/
  despawn lifecycle, or correlating it against a known allocator routine's
  disassembly.
- The game's own exact sign-vs-raw decision rule for Y/X at the 128/256
  boundary — this session matched both regimes empirically but did not
  trace the guest branch that picks between them (almost certainly inside
  `Func_b168`, already identified, just not decoded further).
- Semantics of the two new, town-specific writer PCs from the second
  session: `0x0200917e` (EWRAM) and `0x03006134` (IWRAM, room-entry
  family). Would need the same census method run against that specific
  writer's touched offsets.
- Image identity (base/key/generation) of the room-entry relocatable writer
  family (`0x03006030` and siblings). The only existing verifier
  (`golden_sun_func1dc8_writer_route`, feeding `[wide-obj-writer]`) has a
  session-lifetime log cap (`g_golden_sun_func1dc8_writer_logs < 64`, not
  per-epoch) that was already exhausted before every room-entry transition
  observed this session. Closing this needs either that cap changed to
  per-epoch (an instrumentation decision for the user) or a differently-
  timed replay that reaches a room transition before the cap fills.
- Sprite/OAM tile residency for objects currently culled by view geometry
  (distinct from the BG/tilemap-provider hit-rate finding already reported)
  — not checked, would need a different trace than what exists in these
  logs.
- Body/shadow or other multipart grouping — no shared owner/group-ID field
  was found by this census; not proven to exist or not exist beyond what is
  stated above.

## Cautions carried forward

- The `0x03002000` IWRAM range has, at other times, hosted a copy of
  relocatable code (the flash-driver working-area routine), not this
  record — see the existing comment at `src/runner_main.cpp:5293`-`5296`
  ("the same routines also appear at 0x03002000, ..."). Every claim in this
  document is scoped to the specific `auth_epoch`/generation windows
  measured in the sessions listed above. Do not assume this address range's
  identity is fixed across the whole game.
- config.ini restore race — see Reproduction above. Verify no
  `GoldenSunRecomp`/`GoldenSunLauncher` process is running before trusting a
  restored `config.ini`.
- Hotkey numbers are not savestate file numbers. Always read the actual
  `savestate_loaded` line from the session log.

## What remains before implementation

Per `WIDE-01_ENTITY_PRODUCER_CENSUS.md`'s acceptance criteria, a centralized
Expanded cull/mirror seam may only be proposed once the census identifies a
stable record/producer contract, signed logical coordinates before
truncation, camera/map transform, creator/update/despawn lifecycle, overlay
identity, writer/DMA correlation, and body/shadow/multipart grouping, with
no unresolved alternate route to the same seam. This document establishes
the record contract, the proven X/Y offsets and their transform rules, the
creator/updater lifecycle, and the writer/DMA route. It does not establish
the camera/map coordinate transform, full overlay/generation identity for
the relocatable creation writers, or any grouping/ownership evidence beyond
the unproven `+0x00` pointer candidate. This document takes no position on
culling or mirroring behavior and proposes none.
