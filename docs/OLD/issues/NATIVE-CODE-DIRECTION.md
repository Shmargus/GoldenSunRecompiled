# What is recompiled, what could be native — survey and options

Written 2026-09-03. A survey of the current state plus two competing directions
raised the same night. **No decision has been made.** Nothing here has been
attempted.

## Part 1 — the current state (surveyed 2026-09-03, file:line evidence)

### Everything the game does is recompiled. None of it is decompiled.

- **26,935 canonical translated functions** in the main corpus
  (`local/gs011/main/recompiled_*.cpp`, 34 files, 187 MB), emitted by
  `gbarecomp/tools/gba_recompile` via `gbarecomp/src/recompile/function_finder.cpp`.
- **95 overlay banks** — Golden Sun's own EWRAM code overlays for battle, menu
  and cutscene code — are each **fully AOT-recompiled ahead of time** into
  `local/gs011/overlay_rom_<addr>/`, registered in
  `config/usa/overlay-registry.inc`, and selected at runtime by SHA-1 identity
  match against live EWRAM. They are not compiled on demand.
- Every translated function is `void fn(void)` operating on a global `g_cpu`
  register struct (`local/gs011/main/recompiled_000.cpp:14-40`).

### The interpreter is a fallback, not a tier

`runtime_dispatch_miss` (`gbarecomp/src/runtime/runtime_arm_default_aborts.cpp:15-23`)
fires only when a guest PC has no generated function. It bridges through the
reference interpreter and records the miss for a reviewed TOML proposal.
A coverage snapshot from 2026-09-03 recorded 1 distinct miss, 380 healed native,
186,079 native calls.

Stage 2 self-heal (`gbarecomp/src/runtime/overlay_loader.cpp`) genuinely
compiles native x86-64 at runtime for repeated misses, shelling out to gcc/tcc
and caching the result as a DLL keyed by content hash. `GBARECOMP_SELFHEAL_RAM`
gates whether this is allowed for mutable RAM code; off by default, because
RAM-backed code can change under the game and every dispatch would need a CRC
recheck.

### The hardware is ALREADY hand-written native C++

This is the most important fact for the renderer question. The PPU, and the
hardware model generally, **was never derived from ARM machine code**. It is a
hand-written native model. The widescreen provider hooks
(`gbarecomp/src/gba/gba_ppu.h:354-410`) are typed callbacks operating on decoded
hardware fields, not on `g_cpu`.

So the emulator half of this project is already native code. Replacing the
renderer does not require decompiling anything.

### What is missing for any decompilation work

Two hard blockers, both verified:

1. **There is no facility to replace a guest function with a native one.** The
   function-entry hook is documented observe-only — *"Observers are called in
   registration order and cannot replace each other"*
   (`gbarecomp/src/armv4t/runtime_arm.h:815-816`). No dispatch-table
   substitution API exists anywhere in `gbarecomp/src/runtime`. The single
   wholesale-replacement path in the codebase is BIOS HLE
   (`gbarecomp/src/runtime/bios_hle.cpp`), and that substitutes at the SWI
   vector, not at arbitrary guest functions. **This primitive would have to be
   built first.**
2. **No type information exists anywhere.** `symbols/schema-v1.json` carries
   name, address, size, mode, section, confidence — and no field for argument
   count, return type, or calling convention. `config/usa/main.toml` has 4,186
   `[[extra_func]]` entries carrying only address, mode, name, note. Every
   function is `void fn(void)`. Decompiled C would need every signature
   reconstructed from scratch.

### The project has already ruled decompilation out — with a caveat

`gbarecomp/docs/ARCHITECTURE.md:1-40` states the boundary explicitly: *"This is
a static recompiler... This is not: A decompilation port. We do not lift the
game to high-level C and then re-implement gameplay against a host engine."* It
permits borrowing symbol maps and function boundaries from decomp resources, but
never decompiled C as an execution oracle: *"the emulator is the oracle; the
decomp is at best a hypothesis."*

Against that, `docs/history/TECHNICAL_HANDOFF.md:679-690` records a sequencing
decision attributed to Jimmy on 2026-08-08: **"fast recomp → native subsystems →
decompilation"**, with decompilation as step 7 and not yet reached.

These are reconcilable — "native subsystems" comes before decompilation in that
roadmap, and a native renderer is a native subsystem, not a decompilation. But
anyone proposing decompilation of game logic should read the architecture
boundary first and be explicit about overriding it.

## Part 2 — the two directions on the table

### Option A — native OpenGL renderer

Raised 2026-09-03. The argument made then was performance; the stronger argument
is correctness.

**Why it is cheaper than it sounds.** The renderer is already ours — hand-written
native C++, not recompiled. This is replacing one native component with another.
No decompilation, no function-replacement primitive, no signature recovery.
mgba's `gl.c` (`gbarecomp/third_party/mgba/src/gba/renderers/gl.c`, 1,971 lines)
is a working hardware renderer in-tree to reference, and it already solves
per-scanline state properly.

**Why it would help widescreen specifically.** Today widescreen is *subtractive*:
the game produces a 240-pixel scanline, we intercept per-pixel lookups, invent
the margins, then try to subtract whatever garbage arrives with them. Three culls
failed that way on 2026-09-03 alone (see `WIDE-MARGIN-ATTEMPTS.md`).

Drawing the field directly from the game's own map data inverts it. We now know
that format — the metatile table at 0x02010000, the tile-entry table at
0x02020000, the room bounds struct, and the camera clamp — all established and
recorded. Draw the room, at any width, bounded by the room's own extents, and
the garbage problem **stops existing** rather than needing to be culled.

**The hard part.** It is a hybrid, not a replacement. Sprites, blending,
priority, windowing and effects still come from the emulated PPU, and
compositing a natively-drawn background with an emulated foreground correctly is
where this kind of project usually fails. The 2026-09-02 scoping also noted the
two scanline compositors (`render_scanline_internal` and `render_scanline_wide`,
~1,300 lines) are duplicated rather than shared.

**Recorded objection from 2026-09-02**: do not start until profiling shows
drawing is a dominant cost. That objection was argued on performance grounds and
remains valid *as a performance argument* — the user's own overclock result shows
the in-game slowdown is the game's CPU falling behind, which a renderer would not
help. It is not an argument against the correctness case, which is new.

### Option B — Psynergy reverse engineering

See `PSYNERGY-RE-PLAN.md`. Smaller, self-contained, and structured as a
falsifiable prediction. Needs no new architecture — the function-entry observer
already exists. Does not require the function-replacement primitive unless step 5
is reached.

### How they relate

They are not competing for the same foundation. The renderer is a native
subsystem and sits inside the existing architecture boundary. Psynergy work is
reverse engineering that would eventually push against it. Doing the renderer
first does not block the Psynergy work; doing the Psynergy work first does not
help the renderer.

The honest sequencing question is which one the user actually wants to be able to
*do* something with sooner.

## Part 3 — how big the game actually is (surveyed 2026-09-03)

### The 27,942 figure is not the decompilation scope

The corpus contains 27,942 generated C++ bodies, but those are **basic-block
dispatch targets inside real functions**, not subroutines. Cross-referencing
against the ELF proves it: `gf_Func_*` entries match a real named function 99.4%
of the time, while `gf_tfunc_*` and `gf_afunc_*` match a function start 0.0-0.7%
of the time. They exist so indirect branches always have a callable stub.

**The real scope:**

| | Functions | Code size |
|---|---:|---:|
| Main ROM, real named functions | 2,259 | 533 KB |
| Overlay banks (96, combined) | 3,731 | 777 KB |
| **Total** | **~6,000** | **~1.3 MB** |

For calibration, that is the same order as the well-known GBA decompilation
projects, which took large communities several years each.

**Function size distribution** (ELF bytes, so real guest code rather than
translated C++, which is inflated ~114x by scaffolding and is not a usable
complexity proxy): median 80 bytes, mean 242. 65% of functions are 256 bytes or
smaller. The tail carries the weight — about 115 functions over 1 KB account for
42% of all code bytes. Largest is 7,808 bytes.

### The 96 overlay banks are area and event code

Every overlay's entry point is identical (EWRAM 0x02008000), confirming they are
mutually exclusive swappable banks. Each re-links the whole base game plus a
small set of overlay-unique functions. Per-bank size 344 B to 32 KB, median
8.7 KB. Battle and menu code is likely to live here rather than in main ROM.

### Only 11% of the code can be labelled by static analysis, and that is a ceiling

Address locality works well — 2,231 of 2,258 gaps between functions are exactly
zero bytes, so functions pack contiguously and the ELF's own section boundaries
give 16 natural code regions. But naming those regions is another matter:

- `rom_9000` (239 functions, 37 KB) — field/camera/scroll, confidence medium,
  anchored by the two known landmarks (`Func_10230` camera clamp,
  `tfunc_080101CA` scroll shadow writer).
- `rom_770` (18 functions, IWRAM) — interrupt handlers and hot code, medium.
- **The other 1,999 functions, 89% of the code, are unidentified.**

**And static analysis cannot fix that.** Region-of-memory analysis was attempted
and is *infeasible*, not merely undone: the translated corpus never contains
literal addresses. Every bus access takes a computed effective address from a
runtime register expression. Grepping all 32 shards for the field map tables
(0x0201, 0x0202) and save memory (0x0E00) returns **zero hits**. Recovering which
memory a function touches would need constant propagation over guest register
state.

The call graph has the same problem from the other side: 7,951 inter-function
edges were recovered, but 1,809 call sites (12%) go through register veneers
whose target is only known at runtime, and one giant component holds 82% of all
functions — shared utility code gluing everything together, not a subsystem.

**Conclusion: the codebase cannot be carved into named systems by reading it.
Only running it can do that.** This is the strongest argument for the tracer,
and it applies regardless of which direction is chosen.

## Part 4 — the playthrough proposal (raised 2026-09-03)

The user proposed playing the entire game with self-healing enabled, and treating
an absence of hard crashes as the gate for moving toward decompilation.

**Build the tracer first.** Otherwise the playthrough has to happen twice. One
run with call-counting active yields both the coverage result and a behavioural
map of the whole codebase — which is the only way to solve the 89%-unidentified
problem above.

**"No hard crashes" proves less than it appears.** Self-healing is designed to
stop you noticing gaps: a missed function is healed and play continues. The
valuable artifact is the miss log, which the system already records as reviewable
TOML proposals rather than merging silently (`self_heal.h:9-14`).

**Three-phase protocol:**

1. Play through with healing on and the tracer active. Harvest misses and the
   behavioural map.
2. Merge the miss proposals into the config so those functions are compiled
   ahead of time.
3. Replay under `GBARECOMP_STRICT_STATIC=1`, which disables healing and aborts on
   any miss. A clean run here proves static coverage is genuinely complete rather
   than papered over at runtime.

**On whether this justifies shifting to decompilation:** it is necessary but not
sufficient. It does not remove either blocker in Part 1 — no function-replacement
primitive, no type information. What it does provide is a *trustworthy oracle*.
The architecture doc's rule is that the emulator is the oracle and decompiled C
is at best a hypothesis; a recompilation proven complete across a full playthrough
is exactly what makes a decompiled function checkable against real behaviour.

So the playthrough is the foundation for decompilation, not the trigger for it.
