# MP2K native-audio handoff

Status: **PAUSED — architecture review found no benefit even on success. Do
not resume without re-scoping first.** Last investigated: 2026-08-12.

## Read this first: native MP2K work is paused

Points 5-19 below chased sample-level accumulator/seed arithmetic to explain
the remaining producer-block rejections. Point 20 (below) finally isolated
that residual to sub-sample interpolation phase, not seed or gain arithmetic.
But a strategic review the same day found that closing it would not have been
worth doing: as built, everything published to `host_stream_` is quantized
back down to `int8_t` (`mp2k_decode_producer_word` /
`mp2k_extract_dry_sample`, `/128.0f`) and bit-diffed against the guest's own
8-bit ~13.4 kHz output before it can substitute. The native path is therefore
a bit-replica of the driver's requantized output, not a higher-quality
re-render — see "Strategic status" below. The `mp2k_shadow.h` header comment
claiming otherwise was corrected as part of this pass (comment only, no
behavior change).

**Do not "recapture sequence 8" or continue the points-5-19 line of work.**
If native MP2K audio is revisited, first re-scope the verification gate to
compare voice-state identity and structural output rather than 8-bit sample
equality (see "Strategic status"), then re-evaluate whether the phase bug in
point 20 is even still the relevant next step under that gate. Current
project focus has moved to emulator performance — see
`TECHNICAL_HANDOFF.md` "Current audio handoff".

## Current result (historical — measurement, not a live target)

Latest state-5/600 result after the measured `0x03000828` ARM snapshot boundary
and fresh-note cursor fix: `bad_waves=0`, producer underruns `0`, blocks
`290/27/129` judged/rejected/incomplete, first reject `r=.871` / ratio `1.070`
/ MAE `.021`, and global host candidate `r=.646` / ratio `.169`. Native remains
fail-closed and both launchers keep it disabled.

Block-loss breakdown for that trace: 263 published, 27 rejected (per-sample
arithmetic — what points 5-20 investigated), 129 incomplete (native never
produced enough samples before the block was judged). Points 5-20 together
targeted only the 27-block (6%) slice; the 129-block (31%) incomplete-block
slice was never investigated and is untouched by any of this work.

State 5/worldmap, 600 frames:

- native dry producer: correlation `0.9952`, level ratio `0.9956`;
- native post-reverb producer: correlation `0.9837`, level ratio `1.0170`;
- public host-rate candidate: correlation `0.660`, level ratio `0.171`;
- stale hooks, bad waves, and producer underruns: zero;
- native does not remain LIVE; canonical fallback is retained;
- both launchers still set `GBARECOMP_AUDIO_NATIVE=0`.

This means Golden Sun's native MP2K waveform generation is nearly matched for
the measured state-5 block. The remaining failure is end-to-end producer-block
availability/association in the circular DMA timeline. It is not a reason to
tune gain, weaken verification, or rework the mixer again.

The bounded `audio_state` timeline counters now identify the first divergence.
In the 600-frame state-5 TCP replay, 427 producer blocks were judged: block 0
was the first incomplete block and block 4 the first rejected block. Only 264
blocks were published and released, with no overwrites; the host resampler saw
78 sequence gaps, performed 107 resets, and recorded 105,547 underruns. The
canonical/native verifier counts were zero `640`/`6,566`, held/repeated
`180,676`/`57,543` respectively. DMA queue occupancy peaked at four, so this
is producer availability/rejection followed by host epoch resets, not circular
address overwrite. No safe host-rate mismatch fix is proven yet.

The next trace records the predicate: sequence 4 was rejected with signed
correlation `0.956`, level ratio `1.650`, and MAE `0.056`; this is the
level-ratio gate, not polarity or address association. Its producer completion
cursor was `0x00000FB0` and the C70 producer address was `0x03000DF8`. The
earliest incomplete block was sequence 0 at cursor `504`, with 352 guest
samples but only 76 native samples (expected 352), at dry routes
`0x02003660`/`0x02003C90`. This is an initial producer/native epoch-alignment
gap, not a silent guest block. Do not relax the signed gate or force-fill the
native block until the first aligned producer epoch is defined.

Point 3 adds a fail-closed startup alignment gate: partial blocks are recorded
as incomplete but neither compared nor published; the gate opens only after a
full 352-sample guest/native pair. The synthetic mid-block attach regression
covers `352/76 -> 352/352`. On state 5/600 the gate opened, with 413 complete
blocks judged, 187 incomplete observations, 156 rejected observations, and
257 published/released blocks; DMA overwrites remained 0. Host gaps/resets/
underruns were `25/142/51,328`, and correlation/ratio remained `0.661/0.165`.
Alignment alone therefore does not solve the host-rate mismatch; do not make a
follow-on threshold, gain, DMA, or reset-policy change speculatively.

Point 4 traces sequence 4 through the first differing lane. Before the fix,
index 0 route A was guest `5` versus native `10`: the packed dry accumulator
was `0x0452FFAB`, but native old reverb words were
`0x585F666C/0x3F4B5254` instead of canonical guest
`0x3D3A3B3A/0x322F302F` at EWRAM routes `0x02003660/0x02003C90`.
The smallest fix re-anchors an observed native ring slot to its canonical old
word on mismatch before post-reverb finalization. Afterward sequence 4 is
`r=.9954`, ratio `.9921`, MAE `.0012`; the first residual is index 3 route A,
guest `6` versus native `5` (route B `8=8`), with raw guest `(1546,2204)`,
native accumulator `0x0398FEDE`, guest dry `(-2,7)`, native dry `(-3,7)`,
and equal old words. All seven voices contribute at that sample; the trace
records cursors/gains: v0 `998/0x7fb680 (35,47)`, v1 `1244/0x448180
(45,44)`, v2 `15213/0x5d3f20 (26,26)`, v3 FIX `1591/0x1aa000 (26,26)`,
v4 `12938/0x2da700 (6,8)`, v5 FIX `1411/0x1aa000 (19,18)`, v6
`5439/0x327220 (41,33)`. This remaining one-LSB dry-lane difference does
not justify gain or packed-arithmetic changes yet. The full post-fix state-5/
600 replay remains fail-closed (`native_enabled=false`): 290 complete blocks
judged, 27 rejected, 129 incomplete, 263/262 published/released, host
gaps/resets/underruns `78/105/105,546`, and global correlation/ratio
`0.646/0.169`; the first rejected sequence moves to 8.

> **SUPERSEDED (see "Read this first" above).** Points 5, 6, and 7 treat the
> captured `before=0x00000000` value in point 7 as evidence that the
> canonical accumulator starts from zero, and build the "guest intermediate
> accumulator is not observable" conclusion in point 6 on that reading. Both
> are wrong: `before` is read from the probe's own zero-initialized shadow
> array (`probe.previous_voice`, `gbarecomp/src/gba/gba_audio.cpp` ~1331/
> ~1341), so it is zero by construction on a block's first store — it says
> nothing about the guest's actual accumulator. The true prior value IS
> recoverable by inverting the accumulate (`actual_before = value -
> operand_gain * operand_sample`; the kernel is plain 32-bit modular
> arithmetic with no per-voice saturation, so the inversion is exact). This
> is now a real per-pass field. Measured for sequence 8 pass 0: `actual_before
> = 0x07C908B1` — equal to neither zero nor native's rolling seed
> `0x0459061D`. The historical text below is kept for the record; do not
> reuse its conclusion.

Point 5 adds one bounded `producer_diff` object to `audio_state` for the first
rejected complete block. Sequence 8 first differs at cursor `8404` (`0x20D4`),
index 0 route A: guest `-1` versus native `0`; producer address is
`0x03000DF8`. Guest dry is `(-17,-16)`, native dry is `(-9,-7)`, native post
is `(0,-1)`, guest raw is `(-223,-468)`, and the native packed accumulator is
`0xFC92FBFD`. Reverb history is equal, not stale: EWRAM starts are
`0x02003660/0x02003C90`, guest old words are
`0xFEF9FDFF/0x07020608`, and native old words are identical. Seven voices
contribute to the dry accumulator: `(cursor,frac,step; sample; gain_q9)` are
`(1993,5709952,1905024;-8;35,47)`,
`(2483,5830528,2373248;77;18,18)`,
`(16452,7451936,2373248;-8;26,26)`,
`(2996,2691072,2691072;12;26,26)`,
`(13992,1570560,2018304;6;2,3)`,
`(2816,2691072,2691072;2;13,13)`, and
`(5882,3635232,848512;-161;41,33)`. The trace proves the remaining error is
dry packed accumulation/seed arithmetic, not reverb history, DMA association,
or host policy. No safe producer fix is proven; do not tune gain or thresholds.

Point 6 reconstructs sequence 8 index 0 from the bounded trace. The rolling
seed source is producer sequence 7, and guest/native seeds are identical:
`0x0459061D`. Applying the seven contributions in guest order—packed gains
`0x00180012,0x00090009,0x000D000D,0x001A001A,0x00020001,0x000D000D,
0x00110015` with samples `-8,77,-8,12,6,2,-161`—reproduces native
accumulator `0xFC92FBFD` exactly. The synthetic regression covers this replay.
Therefore rolling-seed identity/order and native signed lane carry are not
the proven cause; the canonical guest intermediate accumulator is not exposed
by the final dry-ring bytes. No root fix is justified without that guest-side
intermediate evidence.

## Earlier investigation record

Point 7 adds bounded `canonical_mix` state to the same TCP `audio_state`
response, latched for the first rejected complete block. State 5/600 captures
sequence 8 at `0x03000DF8`: eight canonical index-0 writes, with PCs
`0x030008B4`/`0x03000A8C`. The first is `before=0x00000000`,
`after=0x03B905A5`, sample `-52`, gain `0x0014000F`; native starts from
rolling seed `0x0459061D` with sample `-8`. Canonical samples are
`-52,42,-4,7,5,-1,-70,-11`, versus native `-8,77,-8,12,6,2,-161`.
The first divergence is canonical step 0, before reverb or DMA (hook mode is
not carried). This is a different eight-voice invocation, so no behavior fix
is proven; native remains fail-closed.

Point 8 adds invocation identity fields. For sequence 8, the native
pre-snapshot is block 8 at cursor `8404`, generation 9, SoundInfo
`0x02003050`, 7 active voices, and vblank fallback PC `0x03007000` at cycle
`128764`; canonical writes for
that label begin at cycle `2387762` and end at `2428683`, completing at cursor
`9544` / cycle `2443667`. The first divergence is block association: the
retained vblank snapshot is from an older mixer invocation, confirmed by the
7-versus-8 voice count. Rebinding at the first canonical store was tested but
produced 600 incomplete blocks, 78 bad waves, and zero judged blocks; it was
reverted. No safe fix is proven.

Point 9 adds bounded channel identity snapshots. At the vblank snapshot the
packed status/ctype words are `[17,81,1,2049,81,2113,17,2048]`; at the first
canonical accumulator write they are `[81,2176,1,2049,81,128,17,128]`.
Several channel fields therefore changed between the last observed stable
boundary and cycle `2387762`, while the SoundInfo pointer stayed
`0x02003050`. No sanctioned hook currently timestamps those channel/status
writes, so the exact latest stable pre-mix boundary and its PC/mode cannot be
proven. Behavior is unchanged. The next atomic step is a bounded channel-field
write ring (PC/mode/cycle/address/value) covering SoundInfo channel status,
ctype, cursor, and wave fields; do not retry first-write rebinding.

Point 10 adds the requested 64-entry channel-field ring with PC, ARM/Thumb
mode, cycle, address, channel, field, before, and value, exposed in TCP
`audio_state`. It captured 44 writes for sequence 8, but the earliest captured
event is cycle `2393751`, after the first accumulator write at `2387762`; the
final pre-completion write is channel 7 wave pointer at cycle `2434669` (ARM).
The channel mutations preceding `2387762` are not observed by the current
after-write callback, and `before` is zero on this path. The latest stable
boundary and triggering PC/mode remain unproven; no association fix is safe.
Next step: pre-write observation at the generated bus store boundary.

Point 11 wires a phase-tagged pre-write callback through the generic ARM bus
adapters, carrying the true value read immediately before mutation. The state5
static generated path does not traverse those adapters for the SoundInfo field
writes: sequence 8 still reports `channel_write_total=0` and no pre-write
events, while canonical first write remains cycle `2387762`. Therefore the
latest stable boundary is still not observable; no timing association change
was made. Next step is to attach the same pre-write callback to the generated
store observer itself (before its RAM write), preserving PC/mode/cycle.

Point 12 completes generated-store plumbing in the generic runtime helper.
After the final `-j8` rebuild, state5/600 returned the baseline: 290 judged,
27 rejected, 129 incomplete, first reject `0.871/1.070/.021`, global
`0.646/0.169`, native fail-closed. The ring contains 70 writes (64 retained).
The latest event before canonical's first accumulator write at cycle `2387762`
is channel 0 field `0x09` at `0x020030A9`, PC `0x030006E4`, Thumb mode, cycle
`2387207`, before `255`, value `215`. No writes occur between that event and
the accumulator boundary, making it the latest observed stable boundary. This
does not prove invocation transition, so no snapshot association change was
made.

Instrumentation, `producer_startup_aligned`, and bounded `producer_diff` are available through the
existing `audio_state` TCP response. The next atomic step is to identify why
the canonical hook's eight-voice invocation is not the seven-voice native
render invocation; do not change
thresholds, gain, DMA, or timeline reset policy.

Point 13 adds per-pass canonical channel identity to that same bounded response:
channel base, status/type, count/phase/sample cursor, wave pointer and loop
header, frequency, and gains, plus PC/mode/cycle for each accumulator pass. The
final state5/600 trace is unchanged (`290/27/129`, first reject
`0.871/1.070/.021`, global `0.646/.169`, native fail-closed). Sequence 8's
canonical passes are channels 0..7 at cycles
`2387762,2394016,2398461,2404596,2408992,2415758,2422327,2428683`.

The native seven-voice snapshot was taken at cycle `128764`, with channels 0..6
active and channel 7 inactive (`pre_channel_status` ends in `0x0800`). The
earliest identity transition after that snapshot is channel 7 activation at
cycle `2380713`, PC `0x080FA148` Thumb, status `0 -> 0x80`; its canonical pass
is therefore the extra eighth voice. Channel 5 changes type/wave at
`2382205/2382210` (PC `0x080F9F6E/0x080FA0F2`) and channel 1 changes type/wave
at `2384407/2384420` (PC `0x080F9F6E/0x080FA0F2`), before their passes. Thus
the native snapshot is stale, not a lost canonical pass: the first canonical
pass using a changed instrument is channel 1 at cycle `2394016`. No safe
snapshot-association fix is proven; changing it would require a new render
boundary or policy. Instrumentation and synthetic identity/timing coverage are
the only changes in this point.

Point 14 adds a 16-entry bounded canonical-boundary ring. It records the last
SoundInfo preparation write, the intervening runtime control event from the
existing TCP trace ring, and the first accumulator store, together with all
per-pass identities. State5/600 remains unchanged (`290/27/129`, first reject
`0.871/1.070/.021`, global `0.646/.169`, native fail-closed).

The boundary is stable across sequences 0..11: the last preparation write is
PC `0x030006E4` Thumb, field `0x09` of channel 0; the control transition is
dispatch PC `0x03000828` (ARM) 52 cycles before the first store; and the first
store is PC `0x030008B4` ARM. The final-write-to-first-store gap is 552--555
cycles. Sequences 8..11 contain complete channel identities 0..7 with stable
wave mapping; sequences 0..7 correctly contain only 7 voices and channel 7 is
zero/inactive. This proves a safe observation boundary, but no snapshot
association change was made in this point. A synthetic ordering regression
covers preparation < control transition < first store.

Point 16 adds bounded `bad_waves` evidence: sequence/channel, boundary cycle,
status/type, raw and resolved wave/data addresses, archive header range,
count/fraction/cursor, cursor-in-range, and the last status/type/wave writes
with PC/mode/cycle. The two observations are sequence 0 channels 1 and 5,
both reason 4 (`cp` outside the resolved wave data range). Their wave headers
resolve validly in ROM (`0x08100600` and `0x0811EB2C`), but live cursors are
`0x0810C6ED` and `0x08102460`, respectively, outside data ranges
`[0x08100610,0x0810334B]` and `[0x0811EB3C,0x0812067F]`. Preceding channel
writes show normal note-on/type/wave preparation; this is a transient or
unsupported live cursor state, not archive-relative resolution failure. The
predicate remains fail-closed and no decoder/timing relaxation is safe.

Restored final replay remains at the point-15 behavior: bad waves `2`, judged/
rejected/incomplete `290/27/129`, first reject `0.871/1.070/.021`, global
`.646/.169`, producer underruns `0`, native fail-closed. A deliberately
boundary-only experiment produced 48 bad waves, 6763 judged blocks, and global
`.471/.162`; it was reverted as unsafe. Synthetic coverage asserts valid
archive-relative resolution alongside rejected out-of-range cursors.

Point 15 applies the measured association: native shadow snapshots for the
Camelot path are deferred from VBlank and taken once at dispatch
`0x03000828` ARM, immediately before the first accumulator store. Direct
SoundMainRAM snapshot behavior and canonical execution remain unchanged.
The final replay records seq8 snapshot cycle `2387710`, source `3` (boundary),
then first store cycle `2387762`. Judged/rejected/incomplete remain
`290/27/129`; published/released remain `263/262`; DMA gaps/resets/underruns
remain `0/0/0`; host gaps/resets/underruns remain `78/105/105546`; global
correlation/ratio remain `.646/.169`, and native stays fail-closed/off.
Diagnostic changes are `bad_waves 0 -> 2`, first incomplete cursor `504 -> 548`
with native samples `76 -> 62`, and DMA unmatched `123058 -> 123030`; no
zero-judged or canonical-output regression occurred. Synthetic coverage now
checks one boundary snapshot for seq7 (seven voices) and seq8 (eight voices).

Point 17 extends the bounded channel-write evidence with width and the full
identity fields (count, fraction, frequency, gains, wave, and live cursor).
For the first bad-wave pair, channel 5 installs type/wave at cycles 136399/
136412 and channel 1 at 134184/134189; their status writes follow at 136823
and 134600. No pre-boundary cursor (`0x28`) write is observed for either bad
record. The complete seq8 ring shows the same ordering: channel 1 cursor is
written at cycle 2393812 and channel 5 at 2415069, both after the first
canonical accumulator store at 2387762. Their bad cursors therefore belong to
the previous wave epoch while note-on installs the new wave; cursor
initialization is performed later inside mixing. This proves the snapshot
boundary cannot be moved earlier or made to accept the cursor, and no
association/decoder fix is safe. The fail-closed predicate and metrics remain
unchanged; synthetic coverage asserts bounded width/field ordering.

Point 18 applies the proven fresh-note rule. A successful note-on now keeps
the cursor/phase initialized by `note_on` for that render and does not consult
the stale pre-mix `cp`; existing voices still validate `cp` against the wave
range. State5/600 now reports `bad_waves=0` (from 2), while judged/rejected/
incomplete remains `290/27/129`, first reject `.871/1.070/.021`, global
`.646/.169`, producer underruns `0`, and native remains fail-closed/off. The
seq8 cursor writes remain after the first accumulator store, confirming the
next authoritative snapshot can use the live cursor. Synthetic coverage tests
fresh-note bypass versus existing-voice fail-closed policy.

> **SUPERSEDED (see "Read this first" above).** Point 19 recaptured sequence
> 8 under the corrected eight-voice snapshot/fresh-note semantics, still
> proceeding from the wrong points-5-7 reading of `before=0x00000000`. That
> framing is corrected in point 20 below.

Point 19 recaptures sequence 8's `producer_diff` under the point-15/point-18
boundary and fresh-note fixes. The bounded trace is unchanged in shape but
its byte content moves with the corrected snapshot/cursor semantics; the
predicate stays fail-closed and no threshold/gain/DMA/host-policy change was
made. This recapture is what points 5-19 were building toward, and it is
this data that point 20 re-examined with the corrected `actual_before` field
and per-pass identity comparison below.

Point 20 adds real per-pass `actual_before` (see the corrected reading above)
and compares canonical against native pass-by-pass for sequence 8 index 0.
Passes 2, 3, and 6 have **identical** wave pointer, ctype, packed gain, and
whole-sample cursor on both sides, yet decode different samples. The
difference is the **fractional cursor phase**: canonical reads the live
fractional cursor from guest memory on every pass, while native re-steps its
own phase forward from the boundary snapshot instead of re-reading it.
Measured phase deltas: pass 2 canonical `0.605` vs native `0.888`; pass 3
canonical `0.000` vs native `0.321`; pass 6 canonical `0.332` vs native
`0.433`. So the residual divergence this whole investigation was chasing is
sub-sample interpolation phase drift — not seed arithmetic, not gain, not
voice identity, not reverb, not DMA association. No fix was applied in this
point (see "Strategic status" below for why resuming it is not the next
step); this is the terminal finding of the points-5-20 investigation.

## Strategic status (2026-08-12) — why this line of work is paused

An architecture review found that the native path, as built, would deliver
**no benefit even if points 5-20 had fully succeeded**:

- Everything published to `host_stream_` goes through
  `mp2k_decode_producer_word` / `mp2k_extract_dry_sample` into `int8_t`, then
  `/128.0f`. The native candidate is therefore a bit-replica of the guest's
  8-bit, ~13.4 kHz quantized output, not a higher-quality re-render.
- The header comment at `gbarecomp/src/gba/mp2k_shadow.h:1-12` (now
  corrected as part of this documentation pass) used to claim the shadow
  re-renders "free of the driver's 8-bit requantization and low mix-rate
  ceiling" — false under the design as built.
- Cause: the bit-exact diff gate (`compare_signed_stereo_block`, `r >= 0.90`
  / ratio `0.70-1.30` / MAE `<= 0.18` against 8-bit guest bytes) forces
  native to reproduce the very quantization it exists to remove.
- Under the `AGENTS.md` canonical-oracle rule the guest mixer always runs
  regardless of native's state, so native can never remove per-frame work —
  it only adds. The roadmap's justification (native audio both raises
  quality and removes work) does not hold for this design.
- Block-loss breakdown from the 600-frame state-5 trace: 263 published, 27
  rejected (per-sample arithmetic — what points 5-20 attacked), 129
  incomplete (native never produced enough samples before the block was
  judged). Points 5-20 targeted 6% of the loss and left the 31%
  incomplete-block slice untouched.
- Headline metrics (`290/27/129`, `.646/.169`) were byte-identical across
  points 5 through 18 — twelve consecutive instrumentation-only points with
  no behavior movement.

**Native MP2K work is paused.** Resuming it requires first re-scoping it as
a quality feature with a verification gate that compares voice-state
identity and structural output, rather than 8-bit sample equality against
the guest's own quantized bytes. Worth keeping regardless of direction: the
tested integer kernels, `audio_shadow.{h,cpp}`, the DMA-consumer
timestamping, and the measured Camelot boundary evidence (dispatch
`0x03000828` ARM, 52 cycles before the first accumulator store at
`0x030008B4`).

The counters cover:

- canonical and native nonzero/held/zero/repeated sample counts;
- producer sequence, EWRAM address, and completion cursor;
- DMA source address and actual FIFO-consumer cursor;
- block age, queue occupancy, and resampler phase;
- circular-ring wrap/overwrite/unmatched-block events.

Do not change output gain or verifier thresholds. After state 5 remains LIVE
for 600 frames, replay states 1, 2, 4, and 8 before enabling either launcher.

## Reproduction and validation

The probe correctly loads state 5 through TCP. A CLI `--load-state` is skipped
when TCP starts paused, so do not replace this route casually.

```powershell
# Strict interpreter fallback for changed RAM code
.\local\probe_fifo.ps1 -Frames 600 -SelfHealRam 0

# Measure the opt-in self-heal tier separately
.\local\probe_fifo.ps1 -Frames 600 -SelfHealRam 1

cmake --build build/gs011 --target `
  ram_overlay_registry_tests ram_heal_tests codegen_tests `
  mp2k_shadow_tests audio_drc_tests GoldenSunRecomp -j 8

$env:GBARECOMP_SELFHEAL_RAM='1'
ctest --test-dir build/gs011 -R `
  "^(ram_overlay_registry_tests|ram_heal_tests|codegen_tests|mp2k_shadow_tests|audio_drc_tests)$" `
  --output-on-failure

python -m unittest discover -s tests -p "test_*.py"
```

The last focused audio safety check passed `mp2k_shadow_tests` and
`audio_drc_tests`. The five runtime/audio suites above passed after the overlay
ABI/self-heal integration. All 86 public Python tests passed earlier in the
same investigation. Re-run them after the next behavior change.

MinGW can fail the final parallel link with `file truncated`; retry the link
using `cmake --build build/gs011 --target GoldenSunRecomp -j 1`. Do not rebuild
the whole corpus serially unless necessary.

## Proven fixes already present

- `0x39D0` is an archive-relative wave offset, not an invalid live pointer.
- Live `cp` plus fractional `fw` controls the PCM cursor.
- Ordinary PCM uses Golden Sun's exact doubled Q22 interpolation and refreshed
  sample hold at each authoritative pre-mix snapshot.
- FIX voices use their distinct packed four-byte cursor and unhalved gain path.
- Camelot synth controls are decoded from payload `wave + 16`.
- Pre-mix and post-mix hooks have distinct block identities and do not advance
  state twice.
- Packed stereo gain, lane-isolated wrapping accumulation, saturation, dry
  extraction, GS1 cross-feed/reverb, and output packing are integer kernels
  covered by synthetic captured-operand tests.
- Completed producer words seed the next rolling accumulator block.
- Direct Sound A/B routes are separated by `0x630` and captured by writer PC;
  the earlier ring-range heuristic silently discarded B.
- Producer blocks are timestamped at actual DMA/FIFO consumption, not at their
  creation or the restored global sample epoch.
- The audible candidate uses the proven integer producer stream; the obsolete
  independent float/Q15 reverb path is bypassed.
- Direct Sound routing, 50/100% volume, PSG contribution, SOUNDBIAS clipping,
  final scaling, polarity, and saturation share one tested output transform.

The first remaining producer difference observed after these fixes was one
signed-byte unit on state-5 sample 1 dry-B. Post-reverb quantization matched at
that sample. This is lower priority than the host timeline mismatch.

## Canonical-oracle correction and static status

Golden Sun patches ARM instructions at `0x03000A5C..0x03000A84`, then dispatches
through `0x03000A58` in the same mixer invocation. The previous AOT route kept
executing stale NOPs, making the canonical audio oracle wrong for FIX voices.

Generic dirty-word dispatch validation now checks existing AOT RAM function
extents. Changed code bypasses stale AOT and enters the explicit
interpreter/self-heal tier. Synthetic write-then-execute, registry, codegen,
and temporary-overlay compilation tests cover this path. No Golden Sun address
is hard-coded in the generic runtime.

With self-heal disabled, the measured state-5 route is `NOT_STATIC`:

- `ram_smc_fallbacks=1402`;
- approximately `6,160,561` interpreted instructions;
- six dispatch misses, including the three previously known mutable-IWRAM
  entries.

Do not claim this route is fully static. Measure `-SelfHealRam 1` separately if
optimizing the fallback cost.

## Main files

- `gbarecomp/src/gba/mp2k_shadow.{h,cpp}`: native producer, verifier candidate,
  integer kernels, producer queue/resampler.
- `gbarecomp/src/gba/gba_audio.{h,cpp}`: guest writer observation, DMA/FIFO
  timestamps, routing/output domain, fallback.
- `gbarecomp/src/gba/audio_shadow.{h,cpp}`: signed and host-rate verification,
  timeline switching.
- `gbarecomp/src/gba/gba_io.cpp`: DMA source tracking.
- `gbarecomp/src/armv4t/runtime_arm.{h,cpp}` and
  `gbarecomp/src/runtime/runtime*.cpp`: dirty RAM dispatch/fallback.
- `gbarecomp/src/runtime/overlay_{abi,runtime_arm}.h` and
  `overlay_loader.cpp`: self-healed overlay guard callbacks.
- `gbarecomp/tests/mp2k_shadow/test_main.cpp`: synthetic audio regressions.
- `gbarecomp/tests/selfheal/ram_heal_test.cpp` and
  `gbarecomp/tests/ram_overlay/registry_test.cpp`: mutable-code regressions.
- `local/probe_fifo.ps1`: correct state-5 TCP replay; accepts `-Frames`,
  `-StateDelay`, and `-SelfHealRam`.

Probe-only diagnostics are gated by `GBARECOMP_AUDIO_PROBE`. Remove noisy
temporary tracing after the timeline fix is proven. Never hand-edit
`generated/**`, and do not commit ROM/BIOS/state/capture bytes.

## Acceptance before launcher enablement

- state 5 remains LIVE for at least 600 frames with no fallback, bad waves,
  stale hooks, underruns, queue drops, or timeline discontinuities;
- native-on output is audibly coherent and the signed producer plus host-rate
  verifier gates pass without threshold changes;
- states 1, 2, 4, and 8 cover PCM, FIX, synth variants, reverb, transitions,
  menus, and silence;
- reversed/compressed voices fail closed unless explicitly proven;
- takeover/fallback preserves the canonical safety timeline without clicks;
- long windowed A/V drift and underrun/overflow measurements pass;
- launcher changes happen only after all of the above.
