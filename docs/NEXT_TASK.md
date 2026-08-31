# Next task — WIDE-01 NPC vertical-culling acceptance

> **2026-08-31 session:** start from
> [`issues/SESSION_HANDOFF_2026-08-31.md`](issues/SESSION_HANDOFF_2026-08-31.md).
> Widescreen margins were reworked and need visual retest first.
> The NPC culling task below is still open and untouched.

Date: 2026-08-30. Keep dynamic RAM **NOT_STATIC**.

The opt-in Expanded culler now requires exact same-frame source/placement
evidence, uses top-left full-sprite bounds, and fails open for ambiguous raw
coordinates, affine entries, and unpaired body/shadow identities. Focused
policy tests pass and the non-LTO playable build is rebuilt.

Session `20260830_134250` proved the experimental culler made no cull
decisions. A parentless B328 producer route drops the bottom NPC earlier. The
current build adds bounded tracing for that route.

Session `20260830_145317` then proved a reused sprite slot retained the old
signed placement after its identity changed. The current build clears that
stale state immediately, but the user reports bottom wrapping regressed after
that build. Isolate or remove only the 15:04 reconciliation after proving its
first divergence; keep parentless routes closed. Follow
`issues/WIDE-01_CULLING_HANDOFF.md`.

Session `20260829_211953` confirmed the little-girl bottom jump as one exact
slot-15 raw-Y alias crossing. The latch now treats repeated identical
same-frame provider calls as no-ops, while same-frame raw/identity/activity,
target/epoch/provenance mutations reset fail-closed. Focused policy tests cover
the repeated `162/-94 → 161/-95 → 160/-96 → 159/+159` sequence and mutations.
Rebuild and manually retest through the root launcher; Native/288 and guest/OAM
state must remain unchanged. A native presentation mirror remains future
enhancement work; this correction is Expanded-only.

## First check

Use the current culling handoff:
[`WIDE-01_CULLING_HANDOFF.md`](issues/WIDE-01_CULLING_HANDOFF.md).

Retest Bilibin vertical culling through repository-root
`GoldenSunLauncher.exe`. Session `20260829_101634` proved placement provenance
could be ahead of visible OAM; the current build latches it on the exact
shadow-to-OAM DMA and checks exact ATTR0/1/2 identity, so unchanged entries
remain valid after the observed producer-frame cadence.
Walk NPCs and shadows across the top/bottom edges once. Session
`20260829_162157` reports shadows improved but NPCs still jump to the bottom
or cull early. B328 now reopens only
for an exact current B27E parent admission; parentless/stale routes remain
closed. B27E/B324 expansion remains. Confirm the statue and bottom pops are
gone; the WIDE toggle is not required.

## Engineering task

Use the new bounded B328 diagnostics. The current build captures F0 entry R6
at exact `0x030038EC` before the generated LDM and correlates it to the
subsequent F0 committed slot. Review
`[wide-obj-y-b328-summary]`, `[wide-obj-y-b328-sample]`, and the writer
correlation tags `[wide-obj-y-b328-writer-summary]` /
`[wide-obj-y-b328-writer-sample]`, plus the accepted-only
`[wide-obj-y-b328-accepted-f0-summary]` /
`[wide-obj-y-b328-accepted-f0-sample]`, for every `160..199` operand. Compare D4
`consumed` versus `unrelated`/`expired`, and F0 `consumed` versus
`unrelated`/`identity-unproven`/`expired`, including slot, ATTR0/1/2, frame,
call context, and route. For F0 require entry R6 == candidate staging, exact
frame/depth/return context, and matching committed ATTR0/1/2. Identify the
earliest route divergence before changing behavior; do not broaden B328 or
alter provenance gates.

Do not blacklist representative map IDs, edit `generated/**`, or change Native
and 288x160 behavior. Current playable build:
`build/gs011_opt/GoldenSunRecomp.exe`, built 2026-08-29 18:50:25,
880333327 bytes; focused widescreen policy test passed `1/1`. Its final
provider trace keeps uncapped reason/region/signed-vs-canonical aggregates and
three independent 128-sample buckets: active-candidate canonical fallback,
signed-bottom, and same-slot/target outcome transitions. Samples include
current/expected ATTR0/1/2, selected/canonical Y, reason/region, provenance,
and frame; empty/disabled/dormant entries are excluded. Recapture through the
root launcher with Expanded 360x240, Widescreen diagnostics ON, and Self-heal
OFF; do not launch a child executable directly.
