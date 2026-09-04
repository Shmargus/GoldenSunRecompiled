> **SUPERSEDED 2026-08-20.** The primary suspicion below (identity drift / the
> tail-dispatch refactor) was wrong. The real cause was a 128-deep dispatch
> recursion cap added by `59c1858` returning `0`, which callers read as "no
> resident code". Read `docs/history/HANDOFF_2026-08-20.md` instead. Kept for its
> ruled-out list, which is still accurate.

# Handoff — battle effect recompilation stutter (2026-08-19)

## Symptom

Every attack and Psynergy animation stalls when it loads, **including repeat
uses of the same effect in the same battle**. The user reports this is a
**regression**: previously only a genuinely new effect lagged once, then was
smooth.

Repeat casts being slow *every* time is the key fact. It means we are not
reusing a compiled body we already have.

## Ruled out

- **CpuFastSet round-up** (added and then made optional today). User tested the
  launcher toggle both ON and OFF — slow either way. The masking was also
  verified correct (`bios_hle.cpp:236-238`, count masked to bits 0-20 before
  rounding, mode flag read separately). Exonerated.
- **Cache key mismatch from a wide snapshot.** Dispatch-time CRC
  (`heal_entry_crc_ok`, `overlay_loader.cpp:494-503`) covers only the function's
  own extent, so two byte-identical bodies produce the same CRC by construction.
- **Cold cache.** Plausible for first-use lag, but cannot explain a repeat cast
  in the same battle being slow. The cache is also demonstrably growing
  (561 → 784 files over one session).
- **Disk lookup being gated to startup.** This was real and was fixed today
  (`overlay_loader.cpp:1398` — the on-disk RAM-variant lookup now also runs
  after warm load). It did **not** fix the stutter, which is itself informative.

## Why the disk-lookup fix not helping matters

That fix turns "evicted variant" into a cheap `LoadLibrary` instead of a gcc
run. It changed nothing. So either:

1. we never reach that lookup, or
2. we reach it and the CRC we look up **is different every time**.

(2) is the strong reading, because the user's symptom is "it feels like it is
compiling it every time". If the same effect produced the same CRC, the on-disk
file from thirty seconds earlier would hit.

## Primary suspicion

**The compiled body's identity differs between identical uses of the same
effect**, so every cast looks like new code and every cast pays a fresh
compile.

Contributing structure, in order of suspicion:

1. **The uncommitted CORE-01 tail-dispatch refactor + emitter image-base
   resync** (`arm_codegen.cpp`, `emit_function.cpp`, `overlay_loader.cpp`
   `overlay_try_dispatch` → `overlay_resolve`). This is the newest broad change,
   it touches every dispatch/BL call site, and it wraps RAM-heal entry/exit
   exactly where self-heal snapshots are taken. It also lands in the right
   timeframe for a regression the user noticed today. Note this codegen is
   linked into the game itself for live Stage-2 self-heal compiles, not just the
   offline tool.
2. **Variant eviction** — `kMaxVariantsPerKey = 8` per address
   (`overlay_loader.cpp:86`), LRU (`heal_slot_promote`, `:521-550`). A single
   animation plausibly cycles more than 8 distinct bodies through the shared
   slot, evicting its own earlier variants before a repeat use. This is a real
   inefficiency but is **longstanding**, so it explains slowness, not the
   regression.

## Why the revert experiment was abandoned

Backing out the emitter resync alone would silently reintroduce CRASH-04: the
`overlay_resolve` change deliberately does not restore `g_runtime_image_base`
(comment at `overlay_loader.cpp:1639`) precisely because the codegen side now
handles it. They are one change, not two.

Worse, the restore-removal sits inside the same hunk as the tail-dispatch
restructure itself — `overlay_try_dispatch` (called the function directly, so
there was an "after" moment to restore in) became `overlay_resolve` (returns a
pointer for the caller to tail-call, so no such moment exists). Reverting it
means reverting the whole refactor and its call sites. Too large for an
experiment, and likely will not build.

Backups exist and nothing was reverted:
`scratchpad/gbarecomp_uncommitted_full_20260819.patch` (whole uncommitted
gbarecomp tree) and `codegen_revert_target_20260819.patch` (the two codegen
files).

## Next step — measure, do not reason

**This is now built and in `build/gs011_opt`.** Line format, emitted once per
RAM-code compile that lands (`overlay_loader.cpp:1507-1531`, inside
`overlay_drain_ready`'s `r.ok` branch), unconditional stderr, not gated behind
any tracing flag:

```
[ram-compile] seq=<N> pc=0x<addr> crc=0x<crc> addr=0x<addr> end=0x<end> len=<bytes> reason=<no-variant|cap-full|content-miss>
```

The plan it implements: emit **one line per RAM-code compile request**
(not per instruction, not per dispatch), carrying:

- guest pc / slot address
- the content CRC used as the cache key
- the body extent (start/end or length)
- the reason: no in-memory variant / evicted / disk lookup missed
- a monotonic counter

Cheap enough to leave on during normal play, deliberately **not** gated behind
crash tracing, so it can ride along with a normal session.

Then cast the same Psynergy three times and read the lines:

- **Same CRC each time** → this is a lookup/eviction problem. Fix is on the
  eviction and disk-lookup side (start by raising `kMaxVariantsPerKey`).
- **Different CRC each time** → the bytes in the slot genuinely differ, and the
  question becomes *what varies*: the resync work is the first place to look,
  then whether the body extent we hash is being computed differently per
  sighting.

Do not ship another fix before this line has been read. Two fixes were already
shipped today on reasoning alone (CpuFastSet round-up, disk-lookup gate) and
neither moved the symptom.

## Related open item

The Mercury Lighthouse crash (CRASH-06) stopped reproducing at every overclock
setting once the cache warmed. That is consistent with the crash window being
open only while an async compile is in flight — i.e. **the same machinery as
this stutter**. Fixing the constant recompiling may close the crash for real,
rather than by luck. See `overclock-breaks-decompressor` note: at 8x the user
crashed, at 1x he did not, before the cache warmed.
