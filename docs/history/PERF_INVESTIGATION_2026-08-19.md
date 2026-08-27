# Investigation — why effects stutter, and what else is wrong (2026-08-19)

> Historical, 2026-08-19. Its stutter conclusions were superseded by
> `docs/history/HANDOFF_2026-08-20.md`; kept for its measurements.

Investigation only. **Nothing in this document has been changed or fixed.**
Everything below is either read out of the source tree or measured from the
files the game itself left on disk.

Companion to `docs/HANDOFF_BATTLE_COMPILE_STUTTER.md`. Where the two disagree,
this one has the evidence.

---

## 1. What actually happens when an effect plays

In plain words:

Golden Sun builds parts of its battle-effect code in RAM while it runs. We
cannot know that code ahead of time, so the first time we see it we **write out
a C file, run a real compiler (`g++`) on it, and load the result as a DLL**.
Until that finishes, that code runs on the fallback interpreter instead.

So an effect is slow for exactly as long as it takes to compile the code it
needs — and the fallback we run in the meantime is very slow.

Three separate things make that window long. All three are real and all three
are measured.

---

## 2. Finding A — each new piece of effect code costs a real compiler run, and they run one at a time

Measured, on this machine, with warm disk cache, using the exact command the
game builds (`overlay_compile.cpp:331-338`):

| cached body | lines of C | compile time |
|---|---|---|
| `03000820_9C01DE4E_a` | 819 | **401 ms** |
| `0300589C_DD7C3BCB_a` | 966 | **436 ms** |
| `020085FA_3379F841_t` | 273 | **294 ms** |

Roughly **0.3–0.45 s each**, and there is exactly **one** compile worker thread
(`overlay_loader.cpp:1270`). Nothing runs in parallel.

Now the volume. Timestamps on the `.c` files the game wrote today, in
`recomp_cache/<image>/gcc/windows-x64/`:

```
14 files at 21:52     4 at 22:12     61 at 22:19     46 at 22:40
82 files at 21:53     3 at 22:13      9 at 22:20     20 at 22:41
```

That is **96 compiles inside two minutes**, then 70 inside two minutes, then 66.
At ~0.4 s each on one thread, a burst like that keeps the compiler saturated for
**30–40 seconds of continuous wall time**. The animation is long over before its
code exists. This is the stutter.

**This alone explains the symptom.** It does not require the code to be
"different every cast" (see Finding D, which retires that theory).

## 3. Finding B — the fallback we run while waiting is much slower than a normal interpreter

`runtime_bridge_interpret` (`runtime_arm_default_aborts.cpp:686-780`) is what
executes the effect while its compile is queued. **Per guest instruction** it
does:

1. `std::getenv("GBARECOMP_AUDIO_PROBE")` — line 694. An environment-block
   lookup, every single instruction, unconditionally, in the shipping build.
   This is a leftover debug probe.
2. Three full CPU-state copies (`store_interp_into_arm_cpu` twice,
   `load_arm_cpu_into_interp` once) around the tick.
3. `bus->audio().mp2k_frame_hook(...)` and `bus->set_bios_access_enabled(...)`.
4. A fresh decode — there is no decode cache, so a tight guest loop re-decodes
   the same instruction on every iteration.

Item 1 is free to remove and is pure waste. Items 2–4 are structural.

Net effect: the window in Finding A is not just long, it is spent in the
slowest execution mode we have.

## 4. Finding C — every call into already-compiled RAM code re-hashes the whole function

`overlay_try_dispatch` (`overlay_loader.cpp:1690-1706`) calls
`heal_entry_crc_ok`, which does an **unconditional CRC32 over the entire
function body** (`overlay_loader.cpp:480-495`) before every single entry into
that native code. The CRC is the byte-at-a-time table implementation
(`gbarecomp/src/gba/crc32.cpp:29-36`) — about one byte per cycle.

Typical healed bodies here are 0xAC–0x200 bytes. A blitter called thousands of
times a frame therefore pays hundreds of thousands of extra byte-hash
operations per frame **after** it has been compiled.

There is already a cheaper mechanism in the tree that does the same job:
`runtime_ram_code_guard` (`runtime_arm_default_aborts.cpp:442+`) tracks dirty
4 KB pages and skips work when nothing wrote to the code. The dispatch-time
check does not use it.

This is the most likely reason effects can still feel heavy **after** the
compile finishes.

## 5. Finding D — the "different content every cast" theory is not supported

The previous handoff's primary suspicion was that the same effect produces
different bytes (and therefore a different cache key) on every use. The on-disk
record says otherwise.

Distinct content hashes per RAM address, across the **whole day**:

```
03006380 -> 6    0300607C -> 4    03002120 -> 3    03000820 -> 3
03006404 -> 2    03006220 -> 2    03000828 -> 2    030065A0 -> 2
```

Two to six variants per address, over hours of play. The per-address variant
cap is 8 (`kMaxVariantsPerKey`, `overlay_loader.cpp:86`). **The cap is not being
hit, and content is not churning per cast.** Raising the cap would change
nothing.

## 6. Finding E — the disk cache does not survive between sessions

This is the biggest single lever, and it looks like a defect, not a design cost.

In the cache directory there are **239 `.c` files and only 65 `.dll` files.**

- 108 of the missing ones still have their `.pic` sidecar. `.pic` is written
  *only after* a compile succeeded **and** the DLL loaded
  (`overlay_compile.cpp:383-390`). So those DLLs existed and worked.
- Every surviving `.dll` is timestamped 22:40–22:41 — the last session. Every
  DLL from 21:52, 22:12 and 22:19 is gone.

The only code that deletes a finished DLL is the ABI check in
`load_and_resolve` (`overlay_compile.cpp`, `DeleteFileA` on ABI mismatch). The
startup warm-loader runs that check over the whole cache
(`warm_scan_rom_bios` / `warm_scan_ram`, `overlay_loader.cpp:848-965`) with
`compile_if_missing = false`, so on rejection it **deletes and does not
rebuild**.

Consequence: after most rebuilds of the exe, the next play session starts with
an effectively empty cache and pays the full 30–40 s of live compiling again,
during gameplay. That matches the report that it feels like it recompiles every
time, and it explains why the effect is worst right after a new build is handed
over.

**This needs to be confirmed rather than assumed** — see §9 step 1. The
alternative explanation is that the cache was cleared by hand during debugging
(the 2026-08-19 handoff records `recomp_cache` being moved aside once).

There is also an infinite-loop hazard in the same code: on ABI rejection
`overlay_compile_one` deletes the DLL, then re-enters itself because the file is
now missing (`overlay_compile.cpp:365-375`). If a freshly built DLL could ever
be rejected, that is an unbounded compile loop on the worker thread.

## 7. Finding F — a header/emitter mismatch silently disables healing entirely

Seven `.log` files from 22:12–22:13 contain:

```
error: 'g_runtime_image_base' was not declared in this scope
```

The emitter was writing code that referenced a symbol the overlay shim header
did not declare at that moment. The current header does declare it
(`overlay_runtime_arm.h:31`), so this was transient — but the failure mode
matters:

- Every RAM heal fails.
- Each failed PC is added to `s_failed` and **never retried for the rest of the
  session**.
- The game then runs all of that code on the slow bridge (Finding B) forever,
  with nothing on screen to say anything is wrong.

The ABI version number does not cover this class of drift, because the mismatch
is between the *emitted C* and the *header*, not between the DLL and the
runtime.

## 8. Finding G — smaller things found along the way

1. **Compiling stack garbage.** ~50 distinct addresses in `0x03007AB0`–
   `0x03007DF4` were compiled, nearly all sharing just two content hashes
   (`66F864FE`, `3B88AD7A`). That address range is the IWRAM **stack**. None of
   them produced a `.pic`, i.e. none of them loaded successfully. That is
   roughly **20 seconds of compiler time per session spent on bodies that were
   never usable**, competing with the real effect code in the same one-thread
   queue.

2. **Game-thread O(N) scan on every RAM miss.** `try_bind_completed_relocatable`
   (`overlay_loader.cpp:1366-1408`) walks *every* healed entry and every
   alternate, doing a CRC32 plus a `memcmp` per candidate, on the game thread.
   Cost grows as the cache warms — so the game gets slower per miss the longer
   you play.

3. **Source tree and shipped binary disagree.** `gbarecomp`'s working tree is
   clean at `59c1858` ("checkpoint *before* tail-dispatch refactor"), but all of
   today's work — tail dispatch, walk-speed QoL, `CpuFastSet`, the disk-lookup
   fix, and the `[ram-compile]` instrumentation — sits in `git stash@{0}`
   (1575 added lines, 21 files). `build/gs011_opt/GoldenSunRecomp.exe` is dated
   22:35 today, i.e. built from the stashed state. **Anyone rebuilding right now
   gets a different game than the one on disk.**

4. **The instrumentation the last handoff says to read does not exist.**
   `grep -rn "ram-compile" gbarecomp/src` returns nothing. It is only in the
   stash. The handoff's "next step — measure, do not reason" cannot be carried
   out until the stash is restored.

5. **Checked-in CMake presets are Debug-only.** `CMakePresets.json` defines only
   `CMAKE_BUILD_TYPE=Debug`. `build/gs011_opt` is `RelWithDebInfo` and was
   configured by hand. Anyone (person or agent) who builds via a preset produces
   the ~3x slower binary. This has already caused a wrong measurement once
   before.

6. **Binary and build size.** `GoldenSunRecomp.exe` is **875 MB**;
   `build/gs011_opt` is **2.3 GB**. Generated sources are 186 MB across 35
   files. Most of the exe is `-g` debug info, which is not paged in at runtime,
   so this is probably not a frame-time cost — but it makes every rebuild and
   every link slow, and it is worth confirming rather than assuming.

7. **DLLs are never unloaded.** Modules stay loaded for the process lifetime by
   design. At today's volumes (65) that is fine; at the previously reported 784
   it is 70+ MB of loader state and hundreds of `LoadLibrary` calls at startup.
   Worth watching, not acting on.

---

## 9. How I would investigate and fix, in order

Ordered by expected gain per unit of risk. **None of this is done.**

### Step 1 — confirm Finding E, because it is the cheapest large win

Do not change code first. Do this:

1. Note the current DLL count in the cache.
2. Launch the game, play until effects are smooth, quit.
3. Note the count again (it should have gone up).
4. Launch again **without rebuilding**, quit immediately, count again.
5. Rebuild the exe, launch, quit immediately, count again.

If the count collapses at step 5 (or step 4), Finding E is confirmed. The fix is
then small and obvious: on ABI rejection during a warm scan, do **not** delete —
or delete and immediately rebuild — instead of silently throwing the session's
work away. Also bound the self-recursion in `overlay_compile_one`.

Expected effect: after the first playthrough of an area, effects never stutter
again on that machine.

### Step 2 — stop wasting the compile queue on stack addresses

Refuse to heal PCs inside the IWRAM stack region (`0x03007A00`–`0x03008000`),
or more precisely refuse anything that has already failed to load once with the
same content hash. That is ~20 s of the queue per session returned to real work.

Needs a decision first: is a dispatch miss into the stack a *symptom* of a
different bug (a wild jump) that should be investigated rather than filtered?
**That should be answered before anything is filtered.**

### Step 3 — make the wait cheaper (Finding B)

Delete the per-instruction `getenv` at `runtime_arm_default_aborts.cpp:694`, or
hoist it to a `static` read once. Near-zero risk, immediate benefit to every
interpreted instruction in the game.

The three state copies and the missing decode cache are a larger change and
should be a separate, deliberate piece of work — not folded into a stutter fix.

### Step 4 — make dispatch into healed RAM code cheap (Finding C)

Have `heal_entry_crc_ok` consult the existing dirty-page mask
(`g_ram_code_page_mask_iwram` / `_ewram_*`) and skip the CRC when no page under
`[addr, end)` has been written since the last verification. The mechanism
already exists and is already trusted inside the generated bodies; this is
reusing it, not inventing it.

This is the one that should make effects feel light *after* they compile.

### Step 5 — only then, revisit compile throughput

If effects still stall on genuinely first-ever content after steps 1–4, the
remaining levers are, in order:

- **More than one compile worker.** The cheapest real change; the queue is
  embarrassingly parallel and the machine has cores idle.
- **Lower optimisation for the first build** (`-O1` or `-O0`), then optionally
  recompile at `-O2` in the background and swap. Gets code running sooner.
- **`tcc` instead of `g++` for the live path.** The backend already exists
  (`HealBackend::Tcc`) and tcc is roughly an order of magnitude faster to
  compile, at the cost of slower generated code — which matters much less if
  the `-O2` version arrives moments later.

All three are design choices with trade-offs and should be chosen deliberately,
not picked by an agent.

### Step 6 — housekeeping that should not wait

- Decide what happens to `git stash@{0}` in `gbarecomp`. Right now the shipped
  exe cannot be reproduced from the tree. This should be resolved before any
  further perf work, or every measurement is untrustworthy.
- Add a `RelWithDebInfo` preset to `CMakePresets.json` so the fast build is the
  default anybody gets.
- Consider making an emitter/header drift (Finding F) loud: one banner line
  when the first N heals fail to compile, rather than silence.

---

## 10. One-line summary

Effects stutter because the game stops to run a real C++ compiler on the effect's
code, one function at a time, ~0.4 s each, up to a hundred of them per battle —
**and the results appear to be thrown away between sessions**, so it pays that
cost again and again. Confirming and fixing cache retention is the biggest and
cheapest win; making the dispatch-time hash conditional is the biggest win for
effects that are already compiled.
