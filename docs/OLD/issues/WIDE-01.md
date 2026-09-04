# WIDE-01 — Widescreen, culling, and scene acceptance

Status: open; fixed modes and the strict expanded viewport-branch hook are
implemented, and manual acceptance remains pending.

## Experimental culling safety update (2026-08-30)

The opt-in Expanded culler now hides an object only when the same frame
provides an exact source record and signed placement. It no longer guesses raw
Y `128..255` or ambiguous X values. Bounds use the OAM top-left coordinate and
the full sprite width/height, so sprite bottoms are not cut off. Affine or
double-size entries and unknown or stale identities remain visible. Body and
shadow entries use the measured record-base / `base+0x0C` relationship and
share the body's decision; if that link or placement is not exact, both fail
open. Native, 288x160, and toggle-off behavior are unchanged.

WIDE diagnostics now emit bounded first/change
`[wide-obj-experimental-cull]` records with slot, source, raw/resolved
coordinates, shape, size, pixel dimensions, decision, and reason. Focused
policy tests and focused widescreen CTest pass. The non-LTO playable build was
rebuilt successfully; manual root-launcher acceptance remains pending.

Session `20260830_134250` contains 164 experimental-culler observations and no
actual culls, so increasing its margin would not address the reported bottom
disappearance. The earlier B328 producer rejected an unpaired route while the
32x32 body was still visible. The current build adds bounded candidate-to-
writer tracing only; a safe behavior change requires one matching recapture.

Session `20260830_145317` identified an earlier slot-identity collision: slot 0
changed from ATTR identity `219e/8078/0924` to `2150/8d00/05a4` while stale
signed-placement state remained. Experimental Expanded now clears pending and
visible placement, signed-Y, and edge-alias state immediately when a committed
slot's full ATTR identity changes. Exact matches remain valid; parentless B328
candidates still cannot affect rendering. Focused widescreen tests pass `3/3`.

## Session `20260829_211953` edge-alias evidence

The little-girl NPC uses slot 15 / OAM target `0x030034F4` with stable
non-Y ATTR0, ATTR1, and ATTR2. Its raw Y crossed `162/-94`, `161/-95`,
`160/-96`, then `159/+159` (frames `519975`, `519976`, `519977`, `519978`),
making only the final sample wrap to the bottom. Strict Expanded now has a
per-slot, fail-closed latch for this exact sequence. Because the final provider
runs per scanline, identical same-frame samples are no-ops; same-frame
raw/identity/activity, target/epoch, or exact-provenance changes clear it. It
applies only at final render-Y resolution. Native and 288x160 are unchanged.
A native presentation mirror remains future enhancement work; this correction
is Expanded-only.

## Session `20260828_004514` handoff

Build under test: `build/gs011_opt/GoldenSunRecomp.exe`, timestamp
2026-08-28 00:41:31; worktree uncommitted; focused tests passed 3/3 before the
manual test. User observed that Expanded view lost its extra background space;
horizontal object culling appeared good, but vertical culling remained wrong.
Logs still report 360x240, so the regression is provider-source failure, not
mode/geometry failure.

Authored ownership reset on Palace split-scroll epoch 3 to `auth_cells=0`, then
had no new writes. BG1-3 each reported `replace=0`,
`unavailable=54,720,000`; margins became backdrop. The all-provider authored
gate is too strict for restored/unchanged Palace tables. B27E/B328 bypassed, but
OAM-Y provenance still keys old PCs B27C/B326, so widened slots are rejected.
B324 had zero bypasses; horizontal improvement is user-observed only, not
fully proven.

This handoff was resolved by the fix/build evidence below. Next: manual root-launcher capture. The prior requested instrument/fix branch-hook → OAM-Y provenance for B27E/B328 with fresh
epoch/frame/slot/address evidence. Separately design safe current-scene table
authorization for unchanged/restored Palace tables without accepting residual or
out-of-room cells. Reproduce first, make the smallest root-cause changes,
rebuild, then user retests. No screenshot or protected data added.

## Session `20260828` fix/build evidence

B27E/B328 now create fresh OAM-Y provenance after successful branch overrides,
using B27E→R7+16 and B328→R7+4. Bounded payload-free `[wide-obj-y]` records
include frame, auth epoch, branch PC, R7, address, and slot. Palace table
authorization now requires a complete-clean `AuthorizedMode0SplitScroll` frame
and exact map/raw CRC `0aef4a71/11020c66`; CPU/DMA writes or CRC mismatch
invalidate it. The provider rejects measured fill ID `0x026`, raw `0xFFFF`, and
finite out-of-bounds coordinates without using `auth_cells`. Other residual IDs
inside the exact fingerprint remain unmeasured and are a known limitation.

The non-LTO `build/gs011_opt/GoldenSunRecomp.exe` was rebuilt at
2026-08-28 09:41:15 (878649847 bytes). Python tests passed `86/86`, focused
CTest passed `2/2`, and the public audit passed. No gameplay success or static
status change is claimed.

## Session `20260828_102032` diagnostic rebuild

The OAM trace now records latest ATTR0 provenance for CPU/fast-IWRAM, DMA, and
unseen slots. The measured 708-byte producer establishes slots 0..88; the
final 1024-byte shadow-to-OAM copy scans all 128 slots and groups raw-Y
candidates, including exact 192 versus other values. CRASH-03 now has a
non-evicting first-transition latch with restore/arm classification. These are
diagnostic-only; no visual or crash fix is claimed. The rebuilt
`build/gs011_opt/GoldenSunRecomp.exe` timestamp is 2026-08-28 11:05:29
(879753978 bytes); focused CTest passed 4/4, Python 86/86, and audit passed.
Session `20260828_111007` proved the authored-cell runtime gate regressed
restored Bilibin: valid CRCs `7ac260c4/060ecd2c` had `auth_cells=0`, so all
BG1-BG3 lookups failed. Equal-scroll providers now again use restored table
contents; authored tracking remains diagnostic-only. Manual retest is pending.

The user confirmed Bilibin expansion restored in session `20260828_143521`.
That capture then showed C6FA/C702/C708 were overridden on every call despite
operands outside Expanded's padded view; the dormant B388 routes had the same
unconditional policy. All seven routes are now operand-bounded: B388 X
`-92..332`, Y `-92..248`; C62C retains its unsigned X lower behavior and widens
only through its measured upper and Y bands. Native/288 and decisions outside
the added bands remain unchanged. Manual object acceptance is pending.

Session `20260828_150103` proved C6FA still rejected visible left-margin
objects because signed 16.16 `x+32` was interpreted as unsigned. Expanded now
admits only the added signed lower band `[-60px,-1]`; the existing upper band
is unchanged. The same capture attributed Palace leakage to successful
provider replacements rather than compositor wrap. Palace now applies a
per-layer four-neighbor active-region mask seeded from authentic-canvas cells,
with `0x026` as a barrier. Disconnected residual islands fail closed. This is
a conservative enhancement policy, not guest-map ownership proof.

Session `20260828_153341` showed that unbounded reachability still connected
BG3 residual art to the Palace seed. The mask is now limited to four horizontal
and three vertical metatile cells from authentic-canvas seeds. The same capture
confirmed remaining vertical loss at presentation: unambiguous bottom-margin
OBJ Y `160..199` no longer requires provenance, while sentinel `192` still does.

The later simplified object path supersedes that raw-Y policy. Strict Expanded
mode now captures signed logical X/Y from the measured `Func_b168` branch
operands before OAM truncation and associates them with the destination slot.
The PPU uses only fresh records whose truncated bits match the current OAM
entry; otherwise it preserves canonical GBA wrapping. Object placement no
longer depends on field/Palace authentication and does not reinterpret raw
Y=192 globally. Focused CTest passes 1/1; manual acceptance is pending.

Follow-up review found that widened `B27E` rejects forced to fall through to
the direct `R7+16` OAM writer were not recording signed Y. Their raw
`160..199` values consequently used canonical negative wrapping. Provenance is
now recorded for both original and forced fall-through at `B27E`/`B328`. The
final expanded cutoff stays `199`: `208..248` is earlier actor-box slack, not
the OAM sprite-origin visibility boundary.

The attempted unprovenanced field raw-Y fallback was removed after
`Stillwrapping.png` proved raw Y `191` could be a north-of-screen NPC and was
incorrectly placed at positive `191` near the bottom. Only fresh matching
signed slot provenance may now override canonical GBA wrapping. Non-field
scenes, Native, and 288x160 remain canonical.

The next statue screenshot proved `Func_b388` lower X/Y shared the larger
horizontal margin. The routes are now axis-specific: B3D2 lower X is `-92`
(`-32-60`) and B3E6 lower Y is `-72` (`-32-40`). This prevents far-north
actors from entering OAM and wrapping into the bottom while preserving the
intended expanded top padding.

The following screenshot disproved that as the last cause. Upstream PPU
fallback already maps raw Y `>=160` to `raw-256`, so the remaining bottom copy
requires a false positive rich-provider match. `[wide-obj-y-alias]` now emits
bounded slot/raw/logical Y, writer branch/frame/generation, and ATTR0/1/2
metadata when positive ambiguous provenance is accepted or rejected. One
WIDE-diagnostics reproduction is required before changing provenance lifetime.

Session `20260828_222157` again reproduced the statue with the WIDE toggle off,
so it contained no alias records. Only `[wide-obj-y-alias]` is now automatic
under strict Expanded geometry, still capped and deduplicated; broad WIDE
diagnostics remain opt-in and rendering remains unchanged.

Session `20260829_101634` isolated the remaining displacement: for the same OAM
slot and unchanged object identity, moving coordinates alternated between
`accepted-positive` and `canonical-provenance-y-mismatch`. Shadow placement
provenance was being overwritten before the matching OAM image became visible.
The runner now keeps pending and visible provenance arrays and publishes the
pending array only on the exact 1024-byte `0x0300347C -> 0x07000000` DMA.
Focused CTest passes `1/1`; manual Bilibin acceptance is pending.

Follow-up evidence shows that latched visible provenance can be exactly two
producer frames behind rendered OAM. The visible gate now uses the exact
ATTR0/1/2 identity read from the measured 12-byte staging record at the
shadow-to-OAM handoff, so an unchanged slot remains valid beyond that cadence;
changed or reused slots fail closed. Raw-Y, epoch, DMA, and Native/288x160
gates are unchanged.

Session `20260829_110157` improved but did not close the reported culling and
wrapping. Its later stale negative record had a different raw Y and OAM
identity, so it is rejected rather than reused. The capture does not identify
the exact remaining visible statue event; no new raw-Y rule or branch widening
is justified from it.

Session `20260829_111355` proves the remaining NPC/shadow wrap is earlier than
the exact OAM identity gate: B328 itself supplies ambiguous positive Y
`160..199`. A strict-Expanded-only bounded automatic
`[wide-obj-y-cull-correlation]` trace carried same-call B3E6/B3EC/C702/C708
signed operands through the staging handoff to the final slot and ATTR identity.
Session `20260829_112427` captured 16 B328 writes at Y `160..165` or `179`, but
none matched those earlier routes. The trace now captures B328 R1/R3 directly
and deduplicates by staging identity. It is diagnostic-only; no global Y
reinterpretation is justified yet.

Session `20260829_113022` shows those B328 operands are direct positive
arithmetic, including `193 - 14 = 179`; R1/R3 alone cannot distinguish a true
bottom object from a wrapped parent. The trace now correlates the same-object
B27E parent decision and B328 fixed-point inputs through final OAM identity.
It remains diagnostic-only.

Session `20260829_114447` captured the reported statue twice at B328 Y `179`.
Both were parentless alternate-route writes; `R11=0x00d10000`, `[SP+4]=0`,
and `[SP+0x18]=16` prove the value was not an arithmetic wrap. B328 now fails
closed to the guest BGT instead of widening an unauthenticated writer. B27E
and B324 expansion remain; Native and 288x160 are unchanged.
The user reports this is better but still not fixed. Continue from
[`WIDE-01_CULLING_HANDOFF.md`](WIDE-01_CULLING_HANDOFF.md); no later capture
currently proves the remaining route.

Session `20260829_160156` then distinguished the routes: parentless statue
records stayed closed, while legitimate B328 records matched a same-staging,
same-frame/call-depth/return-PC B27E admission. The runtime now applies that
exact fail-closed correlation independent of diagnostics; manual acceptance is
still pending.

## Current evidence

Native 240x160, 288x160, and Expanded 360x240 modes exist. Measured world-map
and Mode-0 field-layer expansion is active. In current uncommitted source, the
old immediate/literal threshold-widening hooks are removed and the supported
ROM config declares ten
exact conditional branches: B27E/B324/B328, B3D2/B3DC/B3E6/B3EC, and
C6FA/C702/C708. The runner applies range-bounded overrides to B27E/B324/B328
only in strict 360x240 geometry; the other seven reviewed routes retain their
guest semantics; B328 additionally requires the exact current B27E parent
correlation above. Native and 288x160 preserve guest decisions. The unchanged
PPU still performs final pixel clipping and
retains its 128-slot OAM limit. McCoy Palace retains its measured background
provider/classifier, but it is not an authentication gate for branch bypasses.
Manual Palace/Bilibin acceptance remains pending; this change did not launch
gameplay.

The bounded diagnostic trace now also emits a payload-free per-epoch writer-PC
histogram and address range, because the 64 detailed CPU-store records were
consumed by map writes before raw-table producers appeared. Generated fast-path
EWRAM stores feed that seam only when the launcher diagnostics toggle is on;
ordinary/native runs keep both observers null. Palace tables use the exact
map/raw fingerprint gate and invalidate on CPU/DMA writes or mismatch;
equal-scroll field providers do not use the authored bitmap as a runtime gate.
The
producer summary initializes `last_frame` from its first write instead of
retaining the `UINT64_MAX` sentinel. B27E/B324/B328 are range-bounded to the
new Expanded bands; the remaining seven reviewed routes retain their measured
decisions.
OBJ-X/Y presentation providers remain separate, narrow hooks with their
existing provenance rules. Full evidence is in
[`features/WIDESCREEN.md`](../features/WIDESCREEN.md).

The runner now emits payload-free `[wide-field-provider]` totals per auth epoch
and BG layer, separating successful margin replacements from precondition,
cross-layer boundary, raw-sentinel, and other lookup failures. This is
diagnostic-only and is intended to distinguish atlas-source loss from a
presentation/compositor defect in the next manual Palace capture.

The expanded compositor now emits bounded `[wide-margin]` samples and a
`[wide-margin-summary]` containing final layer/source counts for left/right,
top/bottom, provider replacement, explicit keep-wrapped, object, backdrop,
pillarbox, and forced-blank outcomes. The exact branch routes separately report
calls, successful bypasses, and inactive-mode rejects. The
opt-in OAM trace reports shadow slot writes/overwrites and post-DMA OAM
`used_slots`/visibility totals without retaining attribute payloads. It now
also tracks latest per-slot ATTR0 provenance across CPU/fast-IWRAM, DMA, and
unseen slots, with bounded raw-Y candidate groups after the exact 1024-byte
`0x0300347C -> 0x07000000` handoff; the measured 708-byte producer DMA
establishes provenance for slots 0..88. These
markers are observational only; no behavior change is justified from them
without a new capture.

The two requested non-OAM diagnostic requirements are implemented and covered
by focused tests. `WsMarginDiagnostics::provider_results[bg][result]` counts
provider `replace`, `keep-wrapped`, and `unavailable` results; its
`final_selected[layer][source]` matrix counts the actual final margin source,
and `horizontal_final_selected[left|right][layer][source]` separates the two
horizontal sides. `ppu_smoke_tests` exercises replace, keep-wrapped,
unavailable-to-backdrop, and the native no-callback path. The upstream codegen
test covers null, accepting, rejecting, and unknown-PC conditional-branch
callbacks. The root policy test covers all ten reviewed routes, both original
decisions, strict 360x240 gating, Native/288x160 preservation, unknown PCs,
and null output handling.

Build verification: Python tests pass `86/86`, focused CTest passes `2/2`, and
the public audit passes. The non-LTO
`build/gs011_opt/GoldenSunRecomp.exe` was rebuilt at 2026-08-28 09:41:15
(878649847 bytes). No gameplay or static-status change is claimed.

Session `20260826_182912` records BG1 `3,802,475` replacements and
`9,637,525` raw-unavailable samples, and BG2 `5,415,276` replacements and
`8,024,724` raw-unavailable samples. The upstream PPU contract is explicit:
`kWsTilemapUnavailable` (0) is not a wrapped-entry fallback; the wide regular-BG
compositor skips the sample unless the provider returns
`kWsTilemapKeepWrapped` (2). A focused synthetic PPU case with an opaque
wrapped tile 0 confirms that an unavailable horizontal margin shows only the
backdrop and leaves the native center unchanged. The session totals therefore
do not prove that the reported yellow pixels came from those unavailable
samples. No safe local or upstream behavior change is justified until a
payload-free final-margin source diagnostic identifies the selected layer and
whether it was `replace`, `keep-wrapped`, or backdrop; suppressing all BG1/BG2
replacements could hide valid authored terrain.

Generated `gs011_opt` disassembly shows that the B322/B326 block first computes
`r6 = r1 - r3`, then tests `r4` at B322 and the derived `r6` against `159` at
B326. The reviewed PCs are the following conditional instructions, not the
preceding compare/immediate PCs: B27E/B324/B328. B388's four bound decisions
are B3D2/B3DC/B3E6/B3EC. Func_c62c's direct reject routes are C6FA, C702, and
C708, associated with the measured C6F2/C6FE literal-load block.

## Session `20260826_212025` evidence

The capture completed normally without a crash at frame `521045`, with
`frames_presented=1244` and `failed=0`. It remains **NOT_STATIC**:
`dispatch_misses=10`, `interpreted_insns=447`, `dynamic_ram_pcs=19`, and
`dynamic_ram_dispatches=28`.

The earliest proven yellow/right leakage stage is provider-backed margin
replacement: the right margin is `14,400` pixels, selected as
`right_bg1=0/3850/0`, `right_bg2=0/4178/0`, and `right_bg3=0/6372/0`, with no
wrapped/keep-wrapped selection. Restore starts with `auth_cells=16384`, but
ownership is boot/restored rather than current-epoch ownership and the provider
passes `authored=nullptr`. The smallest candidate fix is to gate `Replace` on
fresh current-auth-epoch ownership and return `Unavailable` until established.

Object evidence: the previous cull/literal route counters are historical and
are no longer used by the runtime. OAM was populated (`used_slots=128`, `visible_slots=128`,
`nonzero_slots=128`), but shadow provenance covered only slots `0..88`; slots
`89..127` remain unproven. The missing NPC cause therefore remains unproven,
and no OAM behavior change is authorized.

## Next action

The stale gate is fixed, the verified-ROM corpus regenerated, all ten branch
hooks passed, and non-LTO `gs011_opt` rebuilt. Use the root launcher diagnostics
toggle with cheats/mods disabled, enter/walk/exit/re-enter Palace once, and
return that session log. The capture should verify the split-scroll policy,
per-layer final margin sources, exact branch decisions, and object behavior;
no manual success is claimed.

The next diagnostics capture must also retain the bounded Palace records:
`[wide-palace-margin]` aggregates by BG, edge, and outcome, while
`[wide-palace-margin-sample]` reports payload-free `hw_x`, `screen_y`,
`map_x`, `map_y`, and `map_id`. `accepted-native-seed` identifies a cell
observed in the authentic canvas; `accepted-connected` identifies a cell
admitted only by the bounded active-region envelope. Include the summary and
any dropped-sample count. For object culling, include `[wide-obj-y-summary]`
and each `[wide-obj-y-slot]`: these steady-state-only counters cover raw Y
160..191, exact 192, 193..199, and 200..255 (including wrapped top-edge
coordinates), with per-slot outcome totals. Transition and scene-disabled
records are intentionally excluded from these counters.

Session `20260828_163525` completed that capture and reproduced unchanged
vertical culling plus Palace leakage. Palace's accepted-connected counts prove
the leak is provider-backed, but the diagnostic retains only the first
coordinate per BG/edge/outcome and cannot locate the visible fragments. OBJ-Y
counts are renderer invocations rather than deduplicated slot/frame events;
epoch 3 is dominated by 14,423,760 exact-192 observations. No behavior change
is justified yet. The next pass must refine these diagnostics as specified in
[`WIDE-01_HANDOFF.md`](WIDE-01_HANDOFF.md).

## Closure condition

Session `20260829_162157` reports shadow culling improved, while NPCs still
jump to the bottom edge or disappear early. This leaves the earliest B328
route unresolved; no rendering change is claimed.

The next diagnostics build classifies every strict-Expanded B328 operand
`160..199` as `exact-current`, `same-staging-mismatch`, or `no-parent`.
Aggregate counts are uncapped; bounded payload-free samples identify differing
frame/call-depth/return-PC fields. Accepted B328 candidates are correlated to
the existing `030038D4` staging and exact ATTR0/1/2 OAM handoff, with handoff
and no-handoff totals reported independently of detailed-log caps.

Session `20260829_164429` still reports NPC bottom jumps/early culls while
shadows and the statue are improved. The next diagnostic build now correlates
rejected B328 candidates with both measured OAM writers: D4 reports proven
staging/slot/ATTR0/1/2/frame-context identity; F0 reports committed
slot/ATTR0/1/2/frame-context identity and full-register staging matches as
`identity-unproven` because its register roles are TODO-EVIDENCE. Uncapped
rejected totals and bounded consumed/unrelated/identity-unproven/expired
samples are emitted by `[wide-obj-y-b328-writer-summary]` and
`[wide-obj-y-b328-writer-sample]`. Rendering policy is unchanged.

The follow-up diagnostics-only build captures F0 entry R6 at exact
`0x030038EC`, before the generated LDM overwrites it, through the existing
function-entry observer. F0 correlation now requires entry R6 equal the B328
candidate staging address, exact frame/depth/return context, and matching
committed ATTR0/1/2; context and attribute mismatches are separate outcomes.
Native/288x160 and rendering policy remain unchanged. Build artifact:
`build/gs011_opt/GoldenSunRecomp.exe`, 2026-08-29 17:25:28,
880205304 bytes. Focused CTest passes 1/1; root-launcher recapture is pending.

The 2026-08-29 17:57:43 diagnostics-only build adds a bounded final-provider transition trace
for raw Y `159..199`. It reports actual selected logical Y (or canonical
wrapping), signed/canonical resolution, reason split (provenance, frame, epoch,
target, raw, and ATTR0/1/2), top/native/bottom/offscreen region, and exact
provenance identity. Aggregates are uncapped by reason/region/resolution;
samples are first/change-per-slot. Rendering and Native/288x160 are unchanged.
Recapture with Expanded 360x240, Widescreen diagnostics ON, and Self-heal OFF.

The 2026-08-29 18:19:17 diagnostics-only build separates the final-provider
samples into independent 128-entry buckets for active-candidate canonical
fallback, signed-bottom entries, and outcome transitions for the same
slot/target. Identity deduplication uses slot, expected OAM target, and current
plus expected ATTR0/1/2; empty, disabled, and dormant ATTR0 entries do not
consume reserved samples. Uncapped aggregates and rendering policy are
unchanged. Focused widescreen CTest passes `1/1`; recapture with Expanded
360x240, Widescreen diagnostics ON, and Self-heal OFF.

The 2026-08-29 18:50:25 diagnostics-only build adds an independent accepted
B328-to-F0 ledger. It captures entry R6 at `0x030038EC` and correlates frame,
auth epoch, call context, committed target, and ATTR0/1/2 as consumed,
unrelated, context-mismatch, attr-mismatch, or expired. Its bounded samples
cannot be consumed by rejected/top traffic. Build artifact is
`build/gs011_opt/GoldenSunRecomp.exe` (880333327 bytes); focused CTest passes
`1/1`. Rendering policy is unchanged; root-launcher recapture remains pending.

The producer-census investigation playbook is
[`WIDE-01_ENTITY_PRODUCER_CENSUS.md`](WIDE-01_ENTITY_PRODUCER_CENSUS.md).

Native center equality, all fixed-mode scene policies, Bilibin transitions,
objects/nameplates, Palace viewport, and vertical OBJ behavior remain pending
manual acceptance; unreviewed branch PCs remain unchanged.
