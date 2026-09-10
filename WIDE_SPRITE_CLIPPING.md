# Sprites cut off in the expanded view — findings, 2026-09-09

Written for Jimmy at the end of the 2026-09-09 session. Records what was fixed,
what was measured, what is ruled out, and what the next step is. Addresses and
names are here so they can be passed on, not as the explanation.

## Follow-up correction — the missing source is not yet identified

The counts below remain observations, but `context=none` does **not** prove the
capture point never ran. The code discards sources it cannot read as IWRAM
records before inserting a context. The old census cannot distinguish that
rejection from a missed entry, and the coordinate-capture hook is separate.
The suspected second structure therefore remains unproven.

A recorder-only source observation now records the pointer before those filters
and joins it to the actual sprite destination at the copy. The new `entry_*`
columns in `lifetime.csv` also say whether existing full-precision coordinates
were found for that source. This changes no rendering behavior. Rebuild and
make a marked `boulder_partial` capture with sprite recording, VRAM map write
trace and Function tracer enabled; the current steps are at the top of
`ROADMAP.md`. The next fix must follow that source evidence, not further relax
placement checks. This diagnostic has not yet been built or exercised.

## The problem in one line

In the expanded view, a sprite standing above the top of the original GBA screen
is drawn only where it overlaps the original screen area, so it appears cut in
half.

## Why it happens

The Game Boy Advance stores a sprite's vertical position in a single byte, which
cannot express "above the screen". The game computes a real, full-precision
position first and then truncates it. So the project captures that real position
at the moment the game computes it, and files it under the actor it belongs to.

If a sprite's real position is available, the renderer can draw it anywhere in
the expanded view. If it is not, the renderer refuses to guess and keeps only
the part inside the original screen rectangle — which is exactly the half-drawn
sprite you see.

So every one of these bugs is the same bug: **we did not manage to trace that
sprite back to the record the game staged its position in.**

## What was fixed this session

**1. Garet and the chest — fixed and confirmed by you.**

The check that decided "is this pointer one of the game's actor records?" was a
hand-written list of addresses that stopped partway through the array. Garet's
record sat past the end of that list, so his position was thrown away. A
previous session had pinned Garet's single address by hand, which fixed him and
nothing else.

That list is gone. The check is now the shape of the actor array — base
`0x03002000`, stride `0x38`, body at `+0x00`, its shadow at `+0x0C`, record
inside IWRAM — instead of an audited address range. Nothing about trust changed:
a record still has to carry the same artwork attributes as the sprite that was
drawn, its stored position still has to match the on-screen byte, and it still
has to belong to the same room. After this change the identity check rejects
nothing at all, which is the correct end state for it.

`golden_sun_obj_record_identity`, `src/runner_main.cpp`.

**2. The once-per-frame wipe — fixed, and it was worth 13 points of coverage.**

The table of captured positions is emptied at the start of each frame. Most
sprites are drawn *before* that frame's first capture arrives, so they were
being compared against the previous frame's timestamp and refused — while their
record was still sitting in the table untouched.

The proof was ordering, not theory: in all 647 frames that contained both a
success and a failure, every failure came before every success, with zero
interleaving in either direction. Same drawing routines on both sides of the
split, so it was about *when* in the frame a sprite was drawn, never *which*
sprite.

A record may now be up to one frame old. One frame is the measured bound, not a
chosen tolerance — the table is cleared exactly once per frame, so nothing
resident can be older than that.

`golden_sun_obj_record_frame_current`, `src/runner_main.cpp`.

## What the runs measured

All runs are the prologue through to the boulder cutscene, Enhanced Options on.

| Run | Change under test | Sprites drawn | With a real position |
|---|---|---|---|
| `session_20260909_190746` | production seam armed | 2,702 | 882 (32.6%) |
| `session_20260909_203127` | address list removed | 5,364 | 1,215 (22.7%) |
| `session_20260909_203451` | recorder capture of the above | 4,339 | 860 (19.8%) |
| `session_20260909_211709` | one-frame window | 6,319 | 2,112 (33.4%) |
| `session_20260909_213843` | near-miss census added | 6,890 | 1,817 (26.4%) |

The percentages are not comparable across runs — each is a different length of
play with different scenes on screen, so the sprite mix differs. The comparison
that *is* valid is run 3 against run 4, which cover the same scene with the
recorder on: the one-frame fix moved bodies from 211 of 3,690 to 1,271 of 5,478,
and moved the failure reason from "wrong frame" (3,479 down to 219) to
"no matching record" (3,988).

Shadows have been at 100% throughout — 649 of 649, then 841 of 841. They are
placed from their paired body and were never the problem.

## Where it actually breaks now — the answer from the last run

The last run added a census that asks, for each sprite we failed to place,
whether we captured *anything at all* for that call. `session_20260909_213843`,
lines tagged `[wide-obj-nearmiss]`. 5,073 failures, and they split cleanly:

**3,432 (68%) — nothing was captured for that call at all.**

The seam where we grab a sprite's record never runs for these. The largest
single group is 2,264 sprites, all from one place in the drawing routine
(`writer 0x030038f0` returning to `0x030038b4`, seen at call depths 3 through
12), and all one kind of sprite that never appears among the successes: tall,
semi-transparent, palette 13 (`ATTR0` shape `2`, OBJ mode `1`; `ATTR2` `0xd524`
and `0xd538`). Two smaller groups behave the same way at returns `0x0300389c`
(558) and `0x030038a4` (568).

Everything that succeeds is a different shape entirely: square, 256-colour
sprites staged in the `0x03002000` actor array (`ATTR0` `0x20xx`, `ATTR2`
`0x093c` / `0x095c` / `0x0904` / `0x0800`).

**1,641 (32%) — something was captured at that call, but not for this sprite.**

These show a real staging pointer (`0x03002348`, `0x03002310`, `0x03002000`,
`0x0300200c`) with attributes that do not resemble the sprite being drawn — for
example `ctx 2042/808c/093c` against `oam 006b/4046/0044`. Note the census picks
the first record from that call site, so this only proves the correct one was
absent, not that the one it found is related.

## The conclusion

**The remaining sprites are not being missed by a check we can loosen. They come
from a source we have never captured.** Two thirds of them are invisible to the
seam that captures positions, and they are a distinct sprite class drawn from a
distinct place in the routine. That is consistent with the boulder and the four
pushers: large multi-part objects that are almost certainly not entries in the
`0x03002000` actor array at all.

Tuning the existing capture cannot reach them. The next step is to find where
that second class gets its position — which structure, and which instruction
stages it — the same way the `0x03002000` array and its `B324`/`B328` writers
were found. That is a new investigation, not a continuation of this one.

## Ruled out

- **The actor identity check.** It rejects nothing now; zero `record-identity`
  failures in the last two captures.
- **The provenance checks.** Every sprite that reaches them passes: `f0_placed`
  equals `f0_identified` in every run. Nothing is lost to the attribute,
  coordinate or room checks.
- **Frame timing.** Down from 3,479 failures to 219.
- **Diagnostic buffers overflowing.** Call sites, actor records and unsourced
  rows all report zero overflow in every capture.
- **A specific call site being unhooked.** The same call sites appear on both
  sides of the split, so no site is structurally invisible; it is the sprite
  class that differs.
- **Shadows.** 100% throughout.

## Not verified

- Whether the boulder and the four pushers are literally in the 2,264-sprite
  group. The signature and the scene match, but no capture joins an OAM row to a
  named actor.
- Whether the one-frame window ever places a sprite in a visibly wrong spot. No
  such report so far, and the exact attribute and on-screen-byte checks should
  prevent it, but it has not been looked for deliberately.

## Where the code is

- `src/runner_main.cpp` — `golden_sun_obj_record_identity` (actor identity),
  `golden_sun_obj_record_frame_current` (one-frame window),
  `note_golden_sun_obj_context_near_miss` (the census above),
  `golden_sun_obj_resolve_placement` (the checks that decide trust).
- `[wide-obj-hooks]` at the end of any Enhanced run reports the coverage
  counters; `[wide-obj-nearmiss]` reports the split above.
- Recorder captures live in `logs/objrec_*`; `tools/decode_obj.py <dir>`
  summarises one.
