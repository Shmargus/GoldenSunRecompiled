# Psynergy reverse-engineering — plan for 2026-09-04

Written 2026-09-03 at the end of the widescreen session. Nothing here has been
attempted yet. This is a plan, not a record of findings.

## Why this is shaped as a prediction

Every failure in the widescreen margin work came from the same mistake: forming
a theory that fit the code, implementing it, and only then discovering it was
wrong. Three room-bounds culls were shipped on an unmeasured assumption.

This plan is deliberately built the other way round. It commits to a **written,
numeric prediction before the confirming observation is made**. If the
prediction is wrong, that is a cheap and useful result. If it is right, the
model is earned rather than assumed.

**Rule for tomorrow: the predicted values for Flare go into this file, in
writing, BEFORE the user casts Flare in a battle.** No adjusting the prediction
after seeing the result. If it is wrong, record that it was wrong and why.

## The hypothesis

All Psynergy is data-driven: a table with one row per spell (power, element,
cost, target type, animation id, ...), executed by a single shared routine that
reads the row. Individual spells are not individual code paths.

Prediction that follows: Quake and Earthquake — same line, different strength —
execute the **same** functions, differing only in an argument value (the row
index). If true, the differing value points straight at the table.

This is a guess based on how GBA-era RPGs are usually built. It has NOT been
verified against this ROM.

## What exists already

- `golden_sun_function_entry_observer(uint32_t entry_pc)`,
  `src/runner_main.cpp:8380`, installed unconditionally at `:9523` via
  `options.function_entry_observer`. Every guest function entry passes through
  it. This is the hook a tracer needs; it does not have to be created.
- `private/goldensun-disasm/goldensun.elf` — 18,484 symbols, all address-named
  (`_Func_<hex>`). No semantic names anywhere; zero symbols contain "battle".
- `local/gs011/main/recompiled_*.cpp` — 34 files, ~187 MB of translated code.
- The launcher's "Test variables" master checkbox is the established place for
  a new diagnostic toggle. `src/launcher_test_policy.h` holds the defaults.
- Precedent for exactly this kind of memory-watching work: the room-bounds
  struct was located on 2026-09-02 by watching writes, and the writing
  instruction was pinned to a specific guest PC. That technique worked first
  time. See `docs/issues/WIDE-MARGIN-ATTEMPTS.md`.

## Step 1 — build the call tracer

A launcher toggle, off by default, under "Test variables".

Design, and the reason for it: do NOT write a log line per call. Function entry
fires millions of times a second; streaming it would destroy the frame rate and
produce an unreadable file. A per-pixel diagnostic already caused single-digit
FPS earlier in this project.

Instead:
- A counter array indexed by function entry PC. One increment per call, which is
  effectively free.
- A hotkey that snapshots the array and labels the snapshot.
- Snapshots written at exit, or on demand, to `logs/`.

**Also capture arguments.** Counting entries alone cannot distinguish "Quake"
from "Earthquake" if the hypothesis is right, because the difference would be an
argument rather than an address. Record the first few argument registers at
entry for functions of interest. Decide the mechanism when building it — a
watchlist of PCs is probably cheaper than capturing for everything.

## Step 1b — how the trace gets turned into labels

Raised 2026-09-03: if a full playthrough records every function encountered, how
do we know what any of it is? Raw capture answers nothing. Contrast does. Build
the tracer so all five of these are possible from day one — retrofitting any of
them means replaying the game.

1. **Marked windows.** A hotkey that opens and closes a *labelled* capture
   window. The player always knows the context — battle, town, menu, save,
   world map, shop, cutscene. Marking converts that knowledge into labels. Without
   it a 30-hour playthrough yields one undifferentiated set and is close to
   worthless.
2. **Set differencing.** Ran in battle but never in a town; ran when casting but
   not when attacking. Each contrast carves off a group.
3. **Call frequency signatures.** Free, and strong. ~60 calls/second is per-frame
   engine code; once per battle is battle setup; once per playthrough is init.
   This bands all ~6,000 functions before any manual labelling.
4. **Co-occurrence clustering.** Across hundreds of windows, functions that always
   appear together belong together — recovered statistically, without naming
   anything. This is what replaces the region-of-memory analysis that proved
   infeasible on the translated corpus (see `NATIVE-CODE-DIRECTION.md` Part 3).
5. **Overlay identity, logged per window.** 96 swappable banks, 777 KB, and the
   runtime already knows which is loaded (SHA-1 identity match against live
   EWRAM, `src/runner_main.cpp:7266-7280`). Logging it gives a coarse label for
   the largest single block of unidentified code immediately. Battle code is
   likely one or a few specific banks.

**Seeding.** A few functions can be identified with certainty by watching memory
— whoever writes enemy HP, party gold, the map tables. Two landmarks are already
confirmed (`Func_10230` camera clamp, `tfunc_080101CA` scroll writer). From a
seed, the call graph propagates the label outward.

**The limit, stated honestly.** All of this yields functional grouping, not
meaning. "These 40 functions run only when damage is applied" is not "this
computes damage". Closing that gap is per-target work using the memory-watch
technique, which is what steps 3 and 4 below are for.

## Step 2 — differential trace

Snapshot, perform one action, snapshot, subtract. Repeat each action several
times and keep only what recurs, so one-time setup inside the window is filtered
out.

Windows to capture, each varying one thing:

| Window | What it isolates |
|---|---|
| Standing still in the field | Baseline idle engine |
| Battle, no action taken | Battle idle, menus, animation loop |
| Physical attack | The action dispatcher |
| Quake | A Venus Psynergy |
| Earthquake | Same line, higher power — tests the table hypothesis directly |
| Frost | Different element — tests whether elements branch in code |

## Step 3 — find Quake specifically

Narrow from the Quake window to the routine that actually applies damage:

1. Set-difference the Quake window against battle-idle and physical-attack.
2. In the remaining set, watch the memory holding the target's HP and catch
   which instruction writes it when the hit lands. Same technique that found the
   room bounds.
3. Walk outward from that write to the routine that computed the value, and to
   whatever table it read.

Acceptance criterion: we can name the guest address of the routine, and the
guest address and row stride of the table it indexes.

## Step 4 — predict Flare, then verify

Flare is a Mars Psynergy; Quake is Venus. So this tests two things at once: that
the table generalises across spells, and that elements are data rather than
separate code.

1. From the table found in step 3, read Flare's row **without casting it**.
2. Write the predicted values into this file: power, element, cost, target
   type, and the damage the next cast will do against a specific named enemy,
   with the party member and their stats recorded so the number is reproducible.
3. The user casts Flare in a real battle against that enemy.
4. Compare. Record the outcome here either way.

If the number is right, we have a verified model of the Psynergy system rather
than a plausible one.

## Step 5 — only then, build something

If and only if step 4 verifies, a first reverse-engineered Psynergy system
becomes reasonable: a native, human-readable representation of the table and the
damage routine, with the guest behaviour as the reference to check against.

Do not start step 5 before step 4 passes. A native reimplementation built on an
unverified model is exactly the failure mode this plan exists to avoid.

## Open questions to settle tomorrow

- Whether argument capture at function entry is cheap enough to leave on for a
  whole battle, or needs a watchlist.
- Whether the animation path is separable from the damage path, or whether
  cutting a window around "casting Quake" unavoidably catches both.
- Whether battles use overlays, which would change how the code is reached and
  how the tracer sees it.
