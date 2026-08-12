# GS-011 Transient RAM Code Images

## Status

**A non-LTO build (140,228,928 bytes, 2026-08-07 10:46) now completes
FULLY_STATIC at 10,800 frames on the `campaign` track**, strict-static,
self-heal off, cache load off — the milestone the 10,800-frame track had been
blocked on since the `Func_a2680` wall:

| track | 1,800 frames | 5,400 frames | 10,800 frames | 21,600 frames |
|---|---|---|---|---|
| no input | FULLY_STATIC | FULLY_STATIC | not run | not run |
| `campaign` | FULLY_STATIC | FULLY_STATIC | **FULLY_STATIC** (`trace_events=84942401`) | aborts at THUMB `0x08094944`, trace event #96,264,422 |

Every previous revision of this document had to qualify the headline as
no-input only, then as bounded at 5,400 frames. It no longer is, at 10,800.
**It is still frame-bounded and track-bounded**: `campaign` is one
deterministic route, the 21,600-frame extension of that same route is not
static yet (see "Measured results after the veneer derivation" below), and a
route that goes somewhere else will find its own gaps. Quote the frame count
and the track, always.

The three failure classes driving the game exposed:

1. **Interior IRQ resumes** - solved, by `[[resume_range]]`. See "Resume ranges".
2. **The relocatable RAM code pool** - **solved**, by position-independent RAM
   images in the recompiler. See "The relocatable pool". No run stops at a
   moving RAM base any more.
3. The overlay identity bug, a genuine defect in this project's own model -
   fixed. See "Hashing data as if it were code".

### Boundary history

With the pool closed, every remaining boundary has been a discovery gap, not a
structural one: an interior entry inside an ELF-sized `STT_FUNC`, one new pooled
routine, or one new EWRAM overlay.

| after | trace events | boundary |
|---|---|---|
| (start) | 9,553,286 | `0x03003f9c` - Func_1dc8 at an unregistered base |
| PIC images for Func_2cf4/6abc/1dc8 | 9,814,457 | THUMB `0x08096a74` = `Func_96960+0x114` |
| seeding Func_96960 | 9,845,087 | THUMB `0x080908b0` = `Func_9088c+0x24` |
| seeding Func_9088c | 10,385,332 | ARM `0x0300061c` = `Func_d30+0x5c` |
| seeding Func_d30 | **1,800-frame campaign completes** | - |
| extending to 5,400 frames | 12,327,361 | unknown identity at `0x0300387c` - a new ROUTINE |
| registering Func_1b70 | 13,491,210 | THUMB `0x0800fa5a` = `Func_f9f4+0x66` |
| seeding Func_f9f4 | 13,945,002 | THUMB `0x080104d4` = `Func_10424+0xb0` |
| seeding Func_10424 | 13,969,654 | ARM `0x03000390` = `Func_af0+0x10` |
| seeding Func_af0 | 17,648,403 | unknown identity at `0x02008104` - a new OVERLAY |
| registering rom_77dd1c | 17,654,155 | THUMB `0x08003f22` = `Func_3f04+0x1e` |
| seeding Func_3f04 | 31,054,452 | THUMB `0x08003eb8` = `Func_3e58+0x60` |
| seeding Func_3e58 | **5,400-frame campaign completes** | - |
| extending to 10,800 frames | 73,339,070 | THUMB `0x0801c2a8` = `Func_1c244+0x64` |
| seeding Func_1c244 | 73,352,416 | THUMB `0x080a26f0` = `Func_a2680+0x70` - **blocked, see below** |

Two of those fifteen steps needed real investigation (`Func_1b70`, `rom_77dd1c`);
the rest are one reviewed line in `REVIEWED_RESUME_FUNCTIONS` plus a
regenerate/rebuild. `REVIEWED_RESUME_FUNCTIONS` grew from 20 to 29 functions
(79 -> 90 derived ranges) and the main corpus from 23,180 to 24,480 functions.

This table stops at the `Func_a2680` wall. The crawl continues in the dated
sections below: the wall is resolved as an upstream finder defect (next
section), the 10,800-frame boundary is walked past `Func_1bc34` and the
flash-driver pool family to `Func_a3ef0+0x7c`, a live player session finds and
fixes the runtime-patched-literal-pool defect and a fifth EWRAM overlay, and a
derivation of 705 interworking-veneer resume points ("Interworking veneers:
705 linker-emitted stubs derived, not listed") is what finally clears the
10,800-frame track and moves the live boundary out to 21,600 frames. See
"Measured results after the veneer derivation" at the end of this document for
the current numbers.

### The 10,800-frame boundary is resolved as an upstream finder defect, not an ELF contradiction

The dilemma this section previously recorded was wrong on both horns. It
claimed that either `[0x080a29fa, 0x080a2a30)` is not really `$d` inside
`Func_a2680`, or `Func_a2680`'s `STT_FUNC` size is wrong. Measured evidence says
both are correct:

- `goldensun.elf` has `Func_a2680` as `STT_FUNC` at `0x080a2680`, size `0xc38`,
  ending exactly at `0x080a32b8`, which is exactly where the next `STT_FUNC`
  (`Func_a32b8`) begins. No gap, no overlap.
- `0x080a29fa` is a genuine `$d` mapping symbol: a two-byte alignment pad,
  followed by a second `$d` at `0x080a29fc` starting a 13-word literal pool,
  with `$t` resuming at `0x080a2a30`. `objdump` renders `0x080a29fa` as a bare
  `.short 0x0000` and the pool as 13 `.word`s - two pointer constants
  (`0x03001f2c`, `0x080a26bc`) and eleven small integers under `0xc00`. It is a
  literal pool, not a jump table.
- `config/usa/main.toml` already declares `data_range [0x080a29fa, 0x080a2a30)`
  matching the ELF exactly, and `derive_resume_ranges` correctly subtracts all
  eight of the function's own interior `$d` runs, producing eight clean resume
  runs.
- The ROM bytes and the ELF bytes over `[0x080a26f0, 0x080a2a30)` hash
  identically.

The actual root cause is the already-filed `BL`-as-long-branch defect in the
recompiler's function finder (see "ELF-proven seeding of whole sections", below).
`[0x080a26f0, 0x080a29fa)` is 778 bytes / 334 instructions of ordinary compiled
THUMB containing **no walk-terminating instruction at all** - no `BX`, no
`POP`/`LDM` with `pc`, no `MOV pc`, and no unconditional `B`. The full mnemonic
tally is 55 `bl` plus only three conditional branches (`bne.n`, `beq.n`,
`bhi.n`), each targeting an address still inside the same stretch. The final
instruction is a 4-byte `BL` at `0x080a29f6` targeting `0x080a3252` - an interior
address of the caller's own extent (`Func_a2680+0xbd2`), not any `STT_FUNC`
entry. There is a second such intra-function `BL` at `0x080a274c` targeting
`0x080a31f6`.

These are long intra-function branches the compiler emitted as `BL` because the
target is out of conditional-branch range. The finder treats every `BL` as a
returning call, so it enqueues the continuation at `pc + 4` - which for the `BL`
at `0x080a29f6` is `0x080a29fa`, the literal pool. Because the stretch has no
terminator, every one of the 389 aliased THUMB halfword entry points that the
resume range opens between `0x080a26f0` and `0x080a29fa` walks the whole block
and falls into that same pool: 389 entry points, all entering at exactly
`0x080a29fa` and never at an interior pool address, reported 777 times (388
twice plus the last once). That reproduces the 777 count exactly, and
`[0x080a29fa, 0x080a2a30)` is the ONLY data range hit - the other seven interior
pools of the same function are untouched.

This is the same defect already filed under the `rom_a1000`/`rom_c9000`
exclusion, whose recorded instances are `0x080ac342`, `0x080e578a` and
`0x080e5ac6`. `Func_a2680` lies in `rom_a1000` - the very section already
excluded for this reason. The 10,800-frame boundary and the
`rom_a1000`/`rom_c9000` seeding exclusion are one root cause, not two open
items, and one upstream fix clears both. Upstream already half-acknowledges the
case: the comment on the THUMB `BL`-pair continuation seed in `gbarecomp`'s
`src/recompile/function_finder.cpp` notes that the `BL_prefix` half's
intermediate target "can point into data for long BLs".

The proposed fix lives in the `gbarecomp` checkout, not here: in the finder's
THUMB `BL`-pair handling, a `BL` whose continuation address `pc + 4` lies inside
a declared `data_range` is a non-returning long branch, because a returning call
cannot have a literal pool as its return address. Do not enqueue that
continuation, and terminate the walk cleanly instead of recording a data-range
collision. The real target is already enqueued separately, so control flow is
not lost. This is self-evidencing from the TOML's own data ranges and needs no
extra ELF metadata. The seed stays commented out in
`REVIEWED_RESUME_FUNCTIONS`: the question is answered, but the fix has not been
implemented upstream, so seeding `Func_a2680` today would still decode the
literal pool as code.

## The mechanism

Golden Sun DMA-copies short ROM routines into RAM and executes them there. The
same runtime address hosts different routines at different times, so a runtime
PC alone is never a valid dispatch key — the identity of the installed bytes is
part of the key. This is the concrete instance of D-005 in `DECISIONS.md`.

Each image is generated as its own corpus with `gba_recompile --symbol-prefix`,
and registered in `kTransientCodeImages` in `src/runner_main.cpp`. Before
dispatching, `verified_ram_dispatch` reads the live bytes over the bus, SHA-1s
them, and only dispatches through an image whose hash matches. An unknown
identity aborts loudly with a byte-level diff rather than executing a
translation of code that is no longer resident.

The registry is the game runner's own adapter installed on the generic
`g_runtime_ram_dispatch_hook` seam; the hook is consulted only for PCs in
`0x02000000..0x04000000` (see `docs/OVERLAY_RUNTIME_SPIKE.md`).

## Image inventory

**Eighteen** fixed-address images are registered - seventeen after the pool
conversions, plus the fourth EWRAM overlay found this session - down from
thirty-two, plus **nine** position-independent ones. The relocatable images have no runtime range
to tabulate - that is the point of them:

**Update (2026-08-06/07 session).** The counts above are the state as of this
section's original writing and are superseded. `src/runner_main.cpp` now
registers **19** fixed transient images (`kTransientCodeImages`, the fifth
EWRAM overlay `rom_78603c` added) and **15** position-independent images
(`kRelocatableCodeImages`, `Func_155d0`, `Func_15d74`, and two further
`Func_9bb8` identities - `..._short_pic` and `..._code_pic` - added on top of
the nine below). See "one routine, two copy lengths" and the runtime-patched
literal pool section, both later in this document, for what the two new
`Func_9bb8` identities are for.

| image | ROM source | size | mode | identity |
|---|---|---|---|---|
| `Func_2cf4` | `0x08002CF4` | `0x68` | arm | `87b60945...9c05` |
| `Func_6abc` | `0x08006ABC` | `0x4` | thumb | `bbfc4c62...af36` |
| `Func_1dc8` | `0x08001DC8` | `0xE0` | arm | `c2e9ace3...640b` |
| `Func_9bb8` | `0x08009BB8` | `0x2C4` | arm | `38da8bb7...529c` |
| `Func_15430` | `0x08015430` | `0x140` | arm | `d49cef21...109a` |
| `Func_15570` | `0x08015570` | `0x60` | arm | `20f9190c...33eb` |
| `Func_158e8` | `0x080158E8` | `0x214` | arm | `03e57cee...fca3` |
| `Func_2544` | `0x08002544` | `0x2C4` | arm | `b8967c9f...96e6` |
| `Func_1b70` | `0x08001B70` | `0x258` | arm | `7e06447b...cdbe` |

Every extent above was re-derived from `goldensun.elf` `STT_FUNC` sizes by
`tools/build_transient_image_config.py`, and every SHA-1 matched what the
live-verified fixed rows already carried - an independent check that the ELF
extents and the observed DMA extents agree. `Func_1b70` had no fixed row: it was
discovered as an unknown identity and registered position-independent directly.

One 5,400-frame campaign run verified **26 distinct (image, base) pairs** across
those nine corpora.

The IWRAM images came from observed DMA/store events; the EWRAM overlays were
matched against all 96 pinned decompressed builds, now from a live RAM dump
rather than a TCP capture session.

The fixed table below is the observed subset, not a closed set. Overlay extents
are `.text` only; see "Hashing data as if it were code". Rows marked *(now
relocatable)* were deleted from the runner and are kept here only as the record
of what the position-independent images replaced - fifteen rows in all.

| Runtime range | ROM source | Size | Image SHA-1 | AOT table |
|---|---|---|---|---|
| `0x03002000..0x0300207C` | `0x08002D5C` | `0x7C` | `a0e2414c…be84` | yes |
| `0x03002000..0x0300209C` | `0x0800A37C` | `0x9C` | `77bec232…0048` | yes |
| `0x03002000..0x03002140` | `0x08015430` | `0x140` | `d49cef21…109a` | no |
| `0x03002140..0x030021A0` | `0x08015570` | `0x60` | `20f9190c…33eb` | no |
| `0x03002000..0x030022C4` | `0x08002544` | `0x2C4` | `b8967c9f…96e6` |*(now relocatable)* |
| `0x03002400..0x030024E0` | `0x08001DC8` | `0xE0` | `c2e9ace3…640b` | no |
| `0x03006000..0x030062C4` | `0x08002544` | `0x2C4` | `b8967c9f…96e6` |*(now relocatable)* |
| `0x03006000..0x030064EC` | `0x08002808` | `0x4EC` | `5fd23904…0363` | yes (new) |

| `0x02008000..0x02008652` | compressed `rom_779188` | `0x652` | `84663bed...0c28` | yes |
| `0x02008000..0x0200971A` | compressed `rom_7795e8` | `0x171A` | `3f92e09c...8a6d` | yes |
| `0x0300387C..0x0300395C` | `0x08001DC8` | `0xE0` | `c2e9ace3...640b` | no |
| `0x0300347C..0x03003740` | `0x08002544` | `0x2C4` | `b8967c9f...96e6` | no |
| `0x03007DF8..0x03007DFC` | `0x08006ABC` | `0x4` | `bbfc4c62...af36` | *(now relocatable)* |
| `0x03000828..0x030008CC` | template `0x08001044` | `0xA4` | `6e801af8...2fc3` | yes |
| `0x03000828..0x030008CC` | template `0x0800105C` | `0xA4` | `e9437b0a...e2e` | yes |
| `0x03007BA4..0x03007C0C` | `0x08002CF4` | `0x68` | `87b60945...9c05` | *(now relocatable)* |
| `0x03000828..0x030008CC` | template `0x08001074` | `0xA4` | `d8d1bcfc...4fcca` | yes |
| `0x03007BC0..0x03007BC4` | `0x08006ABC` | `0x4` | `bbfc4c62...af36` | *(now relocatable)* |
| `0x0300347C..0x030035BC` | `0x08015430` | `0x140` | `d49cef21...109a` |*(now relocatable)* |
| `0x030035BC..0x0300361C` | `0x08015570` | `0x60` | `20f9190c...33eb` |*(now relocatable)* |
| `0x03003A84..0x03003CFC` | `0x08015AFC` | `0x278` | `f6ab01b5...0cb0` | yes |
| `0x03003A84..0x03003B00` | `0x08015E10` | `0x7C` | `50cf8d2e...af57` | yes |
| `0x02008000..0x02009AB4` | overlay `rom_787e04` `.text` | `0x1AB4` | `6ee2f3a2...76fb` | yes |
| `0x02008000..0x0200C8BC` | overlay `rom_77dd1c` `.text` | `0x48BC` | `b58745ad...7f1e` | yes |
| `0x03003400..0x0300347C` | `0x0800A418` | `0x7C` | `d3261b6f...27d1` | yes |
| `0x0300347C..0x03003740` | `0x08009BB8` | `0x2C4` | `38da8bb7...529c` |*(now relocatable)* |
| `0x0300347C..0x03003690` | `0x080158E8` | `0x214` | `03e57cee...fca3` |*(now relocatable)* |
| `0x03003B9C..0x03003CDC` | `0x08015430` | `0x140` | `d49cef21...109a` |*(now relocatable)* |
| `0x03003B9C..0x03003E60` | `0x08009BB8` | `0x2C4` | `38da8bb7...529c` |*(now relocatable)* |
| `0x03003CDC..0x03003D3C` | `0x08015570` | `0x60` | `20f9190c...33eb` |*(now relocatable)* |
| `0x03007DBC..0x03007E24` | `0x08002CF4` | `0x68` | `87b60945...9c05` | *(now relocatable)* |
| `0x03007DC4..0x03007E2C` | `0x08002CF4` | `0x68` | `87b60945...9c05` | *(now relocatable)* |
| `0x03007DC8..0x03007E30` | `0x08002CF4` | `0x68` | `87b60945...9c05` | *(now relocatable)* |

Fifteen rows are marked *(now relocatable)*, and read together they are the
pool's whole shape: six stack depths for `Func_2cf4`/`Func_6abc`, three bases
each for `Func_15430`/`Func_15570`, two for `Func_9bb8`, three for `Func_2544`.
Every one of them cost a discovery run, a config, a corpus, a registration and a
build. They are nine corpora now, and a new base costs nothing.

Two rows deliberately survive as *fixed* even though their routine is also
relocatable: `Func_1dc8` at `0x03002400`/`0x0300387c` and `Func_2544` at
`0x0300347c`. Those addresses are dispatched statically by the main corpus, so
the fixed rows are the identity gate that makes an unknown image there abort
instead of falling through to a stale static entry.

Entries without an AOT table are recorded identities whose dispatch is owned by
a fixed static entry; `verified_ram_dispatch` returns 0 for them.

## Stack thunk: Func_6abc at 0x03007DF8

The live bytes at the `_call_via_r5` target are `00 78 70 47`, decoded in
THUMB mode as `LDRB r0,[r0]; BX lr`. The target state is
`r5=0x03007DF9`, `sp=0x03007DF8`, CPSR.T set.

The always-on store trace attributes both halfword writes to THUMB
`0x08006AE6` in `Func_6ac0`:

```
addr=0x03007DF8 value=0x7800 width=2 source_cursor=0x08006ABC
addr=0x03007DFA value=0x4770 width=2 source_cursor=0x08006ABE
```

The pinned disassembly bounds THUMB `Func_6abc` exactly at
`0x08006ABC..0x08006AC0`. `Func_6878` allocates `0x44` bytes, calls
`Func_6ac0(sp)`, calls the installed thunk twice through `r5=sp|1`, and then
releases the frame. This proves source, size, mode, and lifetime. It is stack
code copied from a ROM template, not an invalid callback or a persistent DMA
image.

Config `config/usa/transient-func-6abc-03007df8.toml` generates one THUMB
function with zero undefined decodes. Dispatch requires the complete four-byte
SHA-1 `bbfc4c623d6e47e6d00a0c13ca4a9bbf58f6af36`, so reused stack bytes cannot
select stale code.

Local artifact fingerprints:

```
config TOML sha256   c935a51613c61eb0d84b99a201d8d28b58c24c88d0aa1cff88dfea661f42af9b
corpus aggregate    c3c5f610508c104d6aa63c5f086876855a686b04b0bc08298efe1b9d966df918
runner sha256       7f939b5be59e121261e1dc518f04312234a11126a5f3dab6926e5edd5dc41d41
```

Strict-static advances from 2,479,809 to 2,493,197 trace events and stops at
THUMB `0x080068FE`.

## Synthesized Func_dc8 image at 0x03000828

The first writer is ARM `0x03000800` at always-on store-ring event 3,295,783,
VBlank 664. It writes `0xE1A09B49` to `0x03000828`; the immediately preceding
register record contains the selected template words. ELF disassembly shows
`0x030007D0` selecting one of three 24-byte templates and `0x03000800..18`
expanding it over four iterations while preserving fixed instruction slots.

This proves the classification and metadata:

- source: templates at runtime `0x030008D4`, `0x030008EC`, or `0x03000904`,
  backed by ROM `0x08001044`, `0x0800105C`, or `0x08001074`;
- size: complete executable identity `0x03000828..0x030008CC` (`0xA4` bytes);
- mode: ARM, from ELF mapping symbols and live CPSR state;
- lifetime: after the writer loop completes until its next pass or IWRAM reset.

It is synthesized code, not a verbatim ROM copy and not an invalid callback.
The observed `0x08001044` and `0x0800105C` variants have complete-image SHA-1s
`6e801af81bd285752b5efffccc815c7c9dc2cfc3` and
`e9437b0a1a071ea8a136fab4ce4c47db3bac2e2e`. The unobserved third variant is
not registered. `tools/build_synthesized_dc8_variant.py` derives private input
from the user's hash-verified ROM without embedding ROM bytes publicly.

The pinned upstream checkout documents reverse-debug commands but does not
contain their implementation or CLI switch. Consequently this attribution
uses the runtime's always-on structured store ring; it does not claim evidence
from a nonexistent reverse-debug write ring.

## New image: Func_2808 at 0x03006000

The previously registered set was incomplete. A strict run aborted with
`unknown transient code identity at 0x03006000`, expected `b8967c9f…` (the
`Func_2544` image), actual `e4f6732c…`, 656 differing bytes.

Two independent sources establish the source and extent, and they agree:

1. **Live DMA3 trace.** With `GBARECOMP_DMA_WATCH_ADDR=0x03006000`, three
   channel-3 events touch the address, in order:

   ```
   ch=3 dad=0x03000000 (hit 0x03006000) src=0x03007EF4 word=6144/7680 cnt_h=0x8500
   ch=3 dad=0x03006000 (hit 0x03006000) src=0x08002544 word=0/177    cnt_h=0x8400
   ch=3 dad=0x03006000 (hit 0x03006000) src=0x08002808 word=0/315    cnt_h=0x8400
   ```

   The third copies 315 words = `0x4EC` bytes from `0x08002808`, overwriting the
   `Func_2544` image installed at the same address by the second.

2. **`goldensun.elf` `.symtab`.** `Func_2808` at `0x08002808`, size 1260
   (`0x4EC`), mode `arm`, section `rom_1b70`. The next symbol `Func_2cf4`
   begins exactly at `0x08002CF4`, so the image is all code with no trailing
   data.

`0x08002544 + 0x2C4 = 0x08002808`: the new image is the function immediately
following the already-registered one in the same ROM cluster.

Config is `config/usa/transient-func-2808-03006000.toml`, following the
established `[[code_copy]]` + `[[resume_range]]` pattern so every aligned
instruction in the image is an addressable entry point. Generation reports 312
functions (arm=312, thumb=0, indirect=114, **undefined=2**, branch_targets=910).
The two undefined decodes are not yet classified and are recorded here as a
known gap.

Local artifact fingerprints (ignored-build evidence, not release artifacts or
upstream pins):

```
config TOML sha256   58a25a73d8278f21b2e6cc0d44eecddc64db37da3f62091c9b9a2ccb409bffb2
corpus  aggregate    82fdc4b9ab06dd8dd7b8b2c4d43f4d65f6a4505ad2ef985453d316922ec62a84
runner  sha256       06c1b32a9fce710c218bcfc583eb300d8e29e32a987d8c57ea5178cc5cb4023e (20,337,506 bytes)
```

The corpus aggregate feeds each `.cpp` file's raw bytes to SHA-256 in lexical
filename order, matching `docs/MAIN_TOML_BASELINE.md`.

Registering the image advanced the strict-static run from 1,491,016 to
1,882,605 recorded trace events (+26%). Trace events are mem-write/branch/
dispatch records, not an instruction count.

## Enumerated miss set

A single self-heal run (`GBARECOMP_SELFHEAL_RECOMPILE=1`, 1800 frames) records
**41 distinct miss PCs**. Self-heal is used here strictly as a discovery
mechanism; the run is explicitly NOT static and its results are not an oracle.

### Group 1 — 32 ROM-space THUMB interior entries (resolved AOT)

```
0x0801011C 0x08010128 0x080102C8 0x080102D8 0x08011CE0 0x080160FC 0x0808B28C
0x080F9A50 0x080F9A6E 0x080F9AE0 0x080F9B60 0x080F9B66 0x080F9B74 0x080F9B8E
0x080F9B96 0x080F9B9E 0x080F9BA4 0x080F9BAA 0x080F9BF4 0x080F9BFA 0x080F9F6C
0x080F9FB0 0x080F9FB2 0x080FA0B2 0x080FA0C0 0x080FA0DA 0x080FA100 0x080FA10C
0x080FA144 0x080FA1D4 0x080FA1E8 0x080FACF8
```

Every one compiled successfully on demand, so the finder can reach them once
seeded; they are discovery gaps, not codegen defects. 26 of the 32 fall in
`0x080F9A50..0x080FACF8` — the M4A sound driver, which is what the game is
initializing at this point. These are now reviewed `[[extra_func]]` seeds
(`mode = "thumb"`) merged into `config/usa/main.toml` via
`tools/build_main_toml.py`.

Regeneration found one honest ELF mapping contradiction: the observed entry at
`0x080102C8` sequentially reaches raw halfword `0x0000` at `0x080102D2`, while
the ELF marks that halfword `$d`. Forced-THUMB ELF disassembly decodes it as
`MOVS r0,r0`, so the reviewed exception list now exposes only that halfword.

The regenerated corpus emits 12,983 functions and its dispatch table contains
all 32 exact PCs. The rebuilt runner SHA-256 is
`de6337331a4dae2253420f15aad97340d068b1d8cdbb8ebe791b32a4a2ba6424`
(20,486,349 bytes). A strict-static 1,800-frame attempt reaches 1,884,713 trace
events and aborts at THUMB `0x0200804C`; none of the 32 ROM PCs misses.

### Group 2 — 9 RAM-space misses with no code-copy metadata (structural)

```
EWRAM  0x02008030 0x0200803C 0x02008044 0x0200804C 0x02008054 0x020083CC  (thumb)
IWRAM  0x03000828 0x03000954 0x03007BAC                                    (arm)
```

All fail with `function finder found no entry at the miss PC` — there is no
mapping from these runtime addresses back to a ROM source, so nothing can be
generated. Each needs the Func_2808 treatment: DMA-watch the address, confirm
source and extent against the ELF, add a `[[code_copy]]`/`[[resume_range]]`
config, generate a prefixed corpus, and register the identity.

`0x02008xxx` is a THUMB EWRAM region not previously seen; the earlier images
are all ARM in IWRAM. `0x03000828` is the self-modifying region already
probed by `local/decode_probe.cpp` and carried as the single
`[[runtime_code_entry]]` in `main.toml`.

### Group 3 — run termination

```
SELF-HEAL bridge for 0x020083CC exceeded 200000000 instructions without
returning to stop_pc=0x0808C6D0 (current pc=0x00000180). Aborting rather
than spinning silently.
```

Interpreting unmapped EWRAM code diverged into BIOS space. This is a
consequence of Group 2, not an independent defect, and should be re-evaluated
only after the EWRAM images are registered.

## ELF-proven seeding of whole sections

`0x0800d7e8` was reached through `Data_13624` at `0x08013624`, an ELF-bounded
188-byte object whose 47 words are all odd ROM pointers landing on exact
STT_FUNC entries with `$t` mapping symbols; `Data_136e0` (164 bytes, 41 words)
is the same, and is loaded by three functions that are themselves entries in the
first table. 49 of the 74 distinct targets were absent from the corpus proven
set. Seeding them moved the boundary to `0x0800cc94`, then `0x0800c62c`, then
`0x0801789c` - each one another ELF-proven entry reached only through a pointer.

Those symbols are withheld by `import_main_symbols.py` solely because their
declared extents contain further mapping-symbol transitions
(`confidence = "unresolved"`); their entry address and mode carry exactly the
same evidence as the proven set. `build_main_toml.py` now seeds every such
corpus entry in `REVIEWED_SEED_SECTIONS` - `rom_c0`, `rom_1b70`, `rom_9000`,
`rom_15000`, `rom_77000`, `rom_8a000` - which is 1,723 additional seeds, and
`build_overlay_toml.py` applies the same rule to overlay corpora (rom_7795e8:
6 seeds to 79).

The later sections are held back by a finder question, not a metadata gap.
Widening discovery into `rom_a1000` and `rom_c9000` makes the walk fall through
the long intra-function branches at `0x080ac342`, `0x080e578a` and `0x080e5ac6`
into the literal pools that follow: the compiler emits those long branches as
`BL`, the finder treats a `BL` as a returning call, and decoding resumes on
bytes that are correctly marked `$d`. The real fix belongs in the recompiler.

Note that the fix as implemented is NARROWER than the one sketched in this
paragraph's original wording, which proposed keying on a `BL` whose *target*
lies inside the caller's own ELF extent. What upstream actually implements is a
`BL` whose *continuation* address `pc + 4` lands inside a declared
`data_range`. That test is self-evidencing from the TOML the finder is already
given - it needs no ELF extent knowledge in the recompiler, and it cannot fire
on a call whose return address is ordinary code. Descriptions of this fix
elsewhere should match the implementation, not the earlier sketch.

## Interworking veneers

THUMB code calls an ARM callee through `MOV ip,pc; BX rN`. `ip` reads as
site+4, the ARM helper (`Func_888`, runtime `0x03000118`) returns there with the
T bit set, and execution continues at site+4 - straight-line THUMB code that is
only ever entered by an inter-mode dispatch. `0x0800cc94` (`Func_cacc+0x1c8`,
`r12 = 0x0800cc94` on the exchange into `Func_888`) was one of these.

`build_main_toml.py` scans the seeded sections for the two-halfword pattern and
derives both the continuation entries (185) and the `0x0000` alignment pads that
precede odd-aligned veneers (98 data exceptions). Deriving rather than listing
means a seeded function can never arrive without the entries its own
interworking calls require. Three pads found before the scan existed stay
hand-reviewed in `config/usa/main-data-exceptions.json` (`0x0800dbfa`,
`0x0800dc52`, `0x0800dfa6`), each verified as ROM and ELF `0000` ahead of
`MOV ip,pc; BX rN`.

## New image: Func_2cf4 on the stack at 0x03007ba4

Strict-static trace events #3756279..#3756281 show `Func_3a7c` at PC
`0x08003aaa` writing DMA3 `SAD=0x08002cf4`, `DAD=0x03007ba4`,
`CNT=0x8400001a` - 26 words, `0x68` bytes, channel 3 enabled. Event #3756283
calls through `_call_via_sp` with `sp = 0x03007ba4`, and #3756284 exchanges to
`0x03007ba4` with CPSR.T clear.

`goldensun.elf` bounds `Func_2cf4` at exactly `0x08002cf4` size `0x68`, with an
`$a` mapping symbol at the entry and the next `$a` at `0x08002d5c`, so the DMA
extent and the ELF extent agree and the image is all ARM code. Dispatch requires
image SHA-1 `87b609455a1aa6cd4f6251a73f8e15f4b6bc9c05`.

The run now stops one step further on, with a **mode mismatch at `0x03007bc0`**:
a THUMB PC inside that registered ARM extent. The stack address is hosting
something else by then, which is the D-005 same-address problem in its sharpest
form. It needs its own writer attribution before anything is registered for it.
The registry checks mode before it checks identity, so the abort is a mode
mismatch rather than an unknown-identity byte diff.

## The 0x03003a84 staging slot

`0x03003a84` is a reusable IWRAM staging slot for the flash driver: the game
DMA-copies whichever routine it needs next into it. Two identities are now
registered there, and they overlap:

| DMA3 program | ROM source | Words | Bytes | ELF |
|---|---|---|---|---|
| `#4697289..91` at PC `0x0801a5c8` | `0x08015afc` | `0x9e` | `0x278` | `Func_15afc`, size 632 |
| `#4699483` at PC `0x0801a614` | `0x08015e10` | `0x1f` | `0x7c` | `Func_15e10`, size 124 |

In both cases the DMA extent and the ELF `STT_FUNC` size agree exactly, an `$a`
mapping symbol sits at the entry, and the observed entry is an ARM exchange
(`_call_via_r3` and `_call_via_r4` respectively, CPSR.T clear). The descriptor
the game itself keeps at `0x03001e50` independently records the `Func_15afc`
bounds `0x03003a84..0x03003cfc`.

The same pattern moved the `Func_15430`/`Func_15570` flash-driver pair from
`0x03002000`/`0x03002140` to `0x0300347c`/`0x030035bc`, contiguously, over the
region the `Func_2544` copy previously occupied. That pair could not be seeded in
`main.toml`, because `0x0300347c` is already an `[[extra_func]]` for `Func_2544`
and one runtime address cannot carry two ROM sources in one corpus. It gets its
own prefixed corpus, which is precisely what the transient-image mechanism is
for.

## The 0x03007bc0 stack thunk

The previous document ended on a mode mismatch at `0x03007bc0`: a THUMB PC inside
the extent of the ARM `Func_2cf4` stack image. The abort path did not dump the
trace ring, so there was nothing to attribute it with. `verified_ram_dispatch`
now calls `runtime_trace_dump_recent` on all three of its abort paths, which is
what made the rest of this session's attributions cheap.

With the ring dumped, the answer was immediate and identical in shape to the
already-registered `0x03007df8` thunk — same ROM template, different stack depth:

```
#3839470 Func_6878 (0x08006878) calls Func_6ac0 with r0 = sp = 0x03007bc0
#3839472 Func_6ac0 publishes r0|1 = 0x03007bc1 to 0x02004c1c
#3839473 PC 0x08006ae6 stores 0x7800 -> 0x03007bc0, cursor r3 = 0x08006abc, count 2
#3839475 PC 0x08006ae6 stores 0x4770 -> 0x03007bc2, cursor r3 = 0x08006abe, count 1
#3882937 _call_via_r5 exchanges to 0x03007bc0 with r5 = 0x03007bc1, CPSR.T set
```

`Func_6878` was entered one frame shallower here (through `Func_6910` from
`0x08006924`), so `Func_6ac0` installed the four bytes at `0x03007bc0` instead.
Hash-verified ROM `0x08006abc` is `00 78 70 47`, SHA-1 `bbfc4c62…af36` — byte
identical to the `0x03007df8` image. The registry checks mode before identity,
which is why the stale `Func_2cf4` registration reported a mode mismatch rather
than a byte diff.

## Third synthesized Func_dc8 template

`tools/build_synthesized_dc8_variant.py` always derived three 24-byte templates,
at ROM `0x08001044`, `0x0800105c` and `0x08001074`, but only the first two had
ever been observed live and the third was deliberately left unregistered. A
self-heal discovery run reached `0x03000828` holding image SHA-1
`d8d1bcfcf2734deaca85791720f6180f4da4fcca`, which is exactly what expanding the
`0x08001074` template produces. It is now an observed identity like the other
two.

## Reproduction

Paths are placeholders for private and separately built inputs.

```powershell
# 1. identify an unknown image
$env:GBARECOMP_STRICT_STATIC = '1'
$env:GBARECOMP_DMA_WATCH_ADDR = '0x03006000'
<local-GoldenSunRecomp.exe> --bios <private-BIOS> --rom <private-ROM> `
  --no-window --frames 600

# 2. generate its corpus
<gba_recompile.exe> --rom <private-ROM> `
  --config config/usa/transient-func-2808-03006000.toml `
  --out local/gs011/transient_func2808_03006000 `
  --symbol-prefix gsr_func2808_03006000_ --no-symbol-map --max-functions 20000

# 2b. …or, for a routine the game copies to more than one base, generate it
#     POSITION-INDEPENDENT and register it in kRelocatableCodeImages instead.
#     ORIGIN is only where the corpus was generated.
<gba_recompile.exe> --rom <private-ROM> `
  --config config/usa/transient-func-2cf4-relocatable.toml `
  --out local/gs011/transient_func2cf4_pic `
  --symbol-prefix gsr_func2cf4_pic_ --no-symbol-map --max-functions 20000 `
  --relocatable-image 0x03007ba4:0x68

# 3. register in src/runner_main.cpp + CMakeLists.txt, then rebuild
cmake --build build/gs011 --target GoldenSunRecomp

# 3b. see which bases a relocatable image actually served
$env:GBARECOMP_RELOCATABLE_LOG = '1'

# 4. enumerate the remaining misses
$env:GBARECOMP_STRICT_STATIC = ''
$env:GBARECOMP_SELFHEAL_RECOMPILE = '1'
$env:GBARECOMP_MISS_FRAG = '<local>/misses.toml.frag'
<local-GoldenSunRecomp.exe> --bios <private-BIOS> --rom <private-ROM> `
  --no-window --frames 1800
```

Note: an abort skips the normal exit-time miss-report and `.frag` write, so the
miss set above was recovered from stderr rather than the fragment file.

## Hashing data as if it were code

An input-driven run aborted with `unknown transient code identity at
0x020081fc`. Dumping the live EWRAM window and matching it against the 96 pinned
decompressed overlays showed it *was* `rom_7795e8` - differing in exactly two
bytes, at `0x020096b8` and `0x020096bc`.

Both are inside the overlay's `.data` section. The registered extents had been
the whole decompressed image:

| overlay | `.text` | `.data` | `.bss` |
|---|---|---|---|
| `rom_779188` | `0x02008000` + `0x5f8` | `0x020085f8` + `0x5a` | `0x02008658` + `0x54` |
| `rom_7795e8` | `0x02008000` + `0x14d4` | `0x020094d4` + `0x246` | - |

so the code identity depended on mutable game state and broke the moment the
overlay wrote to its own variables. It only survived this long because the
no-input run never got far enough to touch them.

Both extents are now the `.text` section only: `rom_779188` = `dbead77b...5e01`,
`rom_7795e8` = `c728ff67...26bc`. A code identity must cover code. The same audit
applies to any future overlay.

## Resume ranges

A VBlank yield re-enters the guest at whatever instruction it was executing, so
a function that yielded once yields at other instructions on other runs. Three
consecutive builds stopped at three adjacent halfwords of one delay loop.

`build_main_toml.py` now carries `REVIEWED_RESUME_FUNCTIONS` - ELF `STT_FUNC`
entries with an *observed* interior resume - and `derive_resume_ranges` expands
each into `[[resume_range]]` blocks:

- the extent is the ELF `STT_FUNC` size, never a guess;
- the function's own `$d` literal pools are subtracted;
- reviewed `[[jump_table]]` bytes are subtracted too. They are carved out of
  `[[data_range]]` by design, so deriving from the post-subtraction set would
  have offered the five absolute words at `0x080779c4` as THUMB code. Resume
  ranges are derived from the pre-subtraction set instead;
- runs are split on aligned boundaries at the schema's `0x1000` cap.

Twenty functions are listed, producing 79 ranges. The payoff is clearest at
`Func_8f52c`, where a single discovery run reported **eight** distinct interior
PCs between `0x0808fc32` and `0x0808fcda` - eight builds under the old method,
one line under this one.

Two RAM images cannot use it: `Func_1dc8` at `0x0300387c` and `Func_2544` at
`0x0300347c` are reached through an `[[extra_func]]` `source_addr` rather than a
declared `[[code_copy]]` span, and `[[resume_range]]` requires one. Their code
extents are aliased word by word instead, derived from the verbatim copy bias
and stopping at each function's trailing literal pool.

## The relocatable pool

**Solved.** The mechanism is position-independent RAM images in the recompiler,
which is what the previous revision of this document filed as the real fix
rather than working around.

### The problem

Golden Sun keeps a flash-driver working area in IWRAM and copies routines into
it at whatever base its allocator hands out. A DMA3 watch on `0x0300347c` across
one campaign run counts:

```
219 x src=0x08009bb8  177 words (0x2c4)   Func_9bb8
 12 x src=0x08015430   80 words (0x140)   Func_15430
  4 x src=0x08002544  177 words (0x2c4)   Func_2544
  1 x src=0x080158e8  133 words (0x214)   Func_158e8
```

and the same routines also appear at other bases - `Func_15430` at `0x03002000`,
`0x0300347c` and `0x03003b9c`; `Func_15570` at `0x03002140`, `0x030035bc` and
`0x03003cdc`; `Func_9bb8` at both `0x0300347c` and `0x03003b9c`. Routine and base
vary independently.

The stack images are the same problem in miniature. `Func_3a7c` DMAs `Func_2cf4`
to whatever its SP happens to be; four depths are registered (`0x03007ba4`,
`0x03007dbc`, `0x03007dc4`, `0x03007dc8`) and the set is bounded only by the call
graph. `Func_6abc` behaves identically (`0x03007df8`, `0x03007bc0`).

**Per-(routine, base) registration is combinatorial and does not converge.** It
was worth pushing far enough to establish that clearly, but continuing to
enumerate would be dishonest work: each build advances the boundary and none of
them approaches an end.

### The fix

`gba_recompile --relocatable-image ORIGIN:SIZE` (upstream, see
`gbarecomp/docs/TOML_SCHEMA.md`) generates ONE corpus per ROM routine. Every
guest address the image derives from its own PC is emitted as
`g_runtime_image_base + (addr - ORIGIN)` rather than a constant, so nothing in
the emitted code depends on where the routine was copied. `ORIGIN` survives only
in the dispatch table, the `goto` labels and the resume tokens - none of which
reach the guest.

The identity gate already did the safety half of this correctly; only the
generated code was pinned to an address.

The runner's `kRelocatableCodeImages` resolves a base instead of matching a
range. For a PC no fixed-address image explains, it derives a candidate base
from each of the image's own entry offsets (`base = pc - offset`), rejects any
that is outside RAM or misaligned for the image's mode, screens on the first
word against the ROM source, and only then hashes `[base, base + SIZE)` against
the registered identity. The base each image was last verified at is tried
first, so a routine entered repeatedly at one base costs one hash rather than a
scan. `g_runtime_image_base` is set to the verified base immediately before the
call. **An unverified base still dispatches nothing**: resolution runs as the
last step before the abort paths, so a genuinely unknown image is as loud as it
ever was.

### What it replaced

Nine routines are now position-independent. `GBARECOMP_RELOCATABLE_LOG=1`
reports each `(image, base)` pair the first time it is verified, which is the
only honest way to see a base set that is deliberately not enumerated. One
5,400-frame campaign run verified 26 pairs:

| image | bases verified in one run |
|---|---|
| `Func_2cf4` | `0x03007ba4`, `0x03007dbc`, `0x03007dc4`, `0x03007dc8`, **`0x03007d78`**, **`0x03007da8`** |
| `Func_6abc` | `0x03007df8`, `0x03007bc0` |
| `Func_1dc8` | **`0x03003f9c`** |
| `Func_9bb8` | `0x0300347c`, `0x03003b9c` |
| `Func_15430` | `0x0300347c`, `0x03003b9c` |
| `Func_15570` | `0x030035bc`, `0x03003cdc` |
| `Func_158e8` | `0x0300347c` |
| `Func_2544` | `0x03002000`, `0x03006000` |
| `Func_1b70` | `0x0300387c` |

The bold entries are bases that had never been registered. Under the old scheme
each one was a separate discovery run, config, corpus, registration and build;
here they cost nothing. Fifteen fixed rows were deleted outright, taking the
fixed registry from 32 entries to 17, and fourteen per-base corpora became nine.

Two routines keep a fixed row alongside their relocatable image, deliberately.
`Func_1dc8` at `0x03002400`/`0x0300387c` and `Func_2544` at `0x0300347c` are
addresses the main corpus dispatches statically; those rows are the identity
gate. Removing them would let an unknown image at one of those addresses fall
through to a stale static entry instead of aborting. The relocatable images
cover every other base - which is the set that was growing.

### What it does not solve

Relocation makes a routine's translation base-independent. It does not discover
routines, and this session produced a clean demonstration of the difference.

Extending the campaign track from 1,800 to 5,400 frames aborted with an unknown
identity at `0x0300387c` - 208 bytes different from the `Func_1dc8` image that
address had been hosting, and matching none of the eight registered identities.
That is a genuinely new routine in the pool, not a new base. It needed the same
writer attribution as every image before it: a DMA3 watch on `0x0300387c`
recorded `ch=3 src=0x08001b70 word=0/150 cnt_h=0x8400` as the last writer, the
live first word `0xE92D40E2` matches ROM `0x08001b70`, and `goldensun.elf` sizes
`Func_1b70` at exactly 600 (`0x258`) bytes with an `$a` at the entry - so the
DMA extent and the ELF extent agree.

It was then registered position-independent from the start, so its base set will
never have to be enumerated either. Expect more of these as the input track gets
longer: relocation removed one unbounded axis, not the discovery work.

## Tooling added this session

- `tools/resolve_miss_functions.py` - reads a runner log, extracts every miss PC
  and prints the containing ELF function plus a ready `REVIEWED_RESUME_FUNCTIONS`
  entry. A PC with no containing sized `STT_FUNC` is reported as such rather than
  guessed at; those are the ones needing writer attribution.
- `tools/build_transient_image_config.py` - emits a transient-image TOML for a
  verbatim ROM function copied to a runtime address, deriving the extent from the
  ELF `STT_FUNC` size and the resume/data split from that function's own mapping
  symbols. This removes the hand literal-pool arithmetic that was the most
  error-prone step. Cross-checked against two hand-written configs (`Func_2cf4`,
  and `Func_158e8` with its two interior pools); it reproduces their boundaries
  and SHA-1s exactly.
- `verified_ram_dispatch` dumps the runtime trace ring on all three abort paths,
  and writes the live RAM window to `GBARECOMP_TRANSIENT_DUMP` on an unknown
  identity. Compressed overlays have no linear ROM source to diff against, so
  before this an unknown overlay could only be identified through a live TCP
  capture session.

## Limitations

- **The FULLY_STATIC claim is now frame-bounded, not input-bounded.** Both the
  no-input and campaign tracks are fully static at 1,800 frames, and no-input is
  fully static at 5,400; the 5,400-frame campaign track is not. Quote the frame
  count and the track, always.
- `campaign` remains ONE deterministic route. A route that goes somewhere else
  will find its own gaps, and nothing here claims otherwise.
- Position-independence removes the need to enumerate BASES. It does not
  discover ROUTINES or OVERLAYS: the 5,400-frame track found one of each
  (`Func_1b70`, `rom_77dd1c`), and each needed the same writer attribution as
  every image before it. Longer tracks should be expected to find more.
- The 10,800-frame campaign boundary that was blocked on the upstream finder
  `BL`-as-long-branch defect inside `Func_a2680` is resolved (see "The
  10,800-frame boundary is resolved as an upstream finder defect, not an ELF
  contradiction"); the boundary has since moved twice more, past `Func_1bc34`
  and is currently at IWRAM `0x030044F4` with no containing ELF `STT_FUNC` -
  see "2026-08-06: `rom_a1000`/`rom_c9000` un-excluded and `Func_1bc34`
  seeded" below.
- The base resolver's screen is the image's first word plus a full-extent SHA-1.
  For the four-byte `Func_6abc` thunk that is only 32 bits of identity, which is
  the same evidence the two fixed registrations carried, but it is thin - mode
  and the fact that candidate bases are derived from the failing PC itself are
  what keep it honest.
- **Resolved (2026-08-06).** `rom_a1000` and `rom_c9000` are no longer
  excluded from section seeding: the upstream finder `BL`-as-long-branch fix
  now handles the same defect that blocked them (long intra-function branches
  at `0x080ac342`, `0x080e578a` and `0x080e5ac6` emitted as `BL`), and
  regenerating `main.toml` with both sections un-excluded produced zero
  `data_range` collisions. See "2026-08-06: `rom_a1000`/`rom_c9000` un-excluded
  and `Func_1bc34` seeded" below.
- `[[resume_range]]` cannot describe an image reached through an `[[extra_func]]`
  `source_addr`. Either the schema should accept `source_addr`-mapped ranges or
  those images should be declared as `[[code_copy]]` in `main.toml`.
- Two undefined decodes inside the `Func_2808` image remain unclassified, and the
  main corpus still reports one undefined decode.
- Several images share a start address and are separated only by SHA-1. That is
  correct, but an unregistered further image at any of them aborts rather than
  degrading.
- Coverage is only ever as good as the input track. `campaign` is one
  deterministic route; other routes will find more.
- No oracle comparison has been run against this state. The GS-010 `DISPSTAT`
  cycle-drift finding has not been retested.
- The generic `gbarecomp` deltas that GS-007 and GS-008 depend on remain
  uncommitted in the local core checkout; the pin cannot advance until they are
  reviewed.

## Local artifact fingerprints

Ignored-build evidence, not release artifacts or upstream pins. The corpus
aggregate feeds each `.cpp` file's raw bytes to SHA-256 in lexical filename
order, matching `docs/MAIN_TOML_BASELINE.md`.

```
main.toml sha256          911d9c4f994a946bf7eba95df88c08bba92f04f51502b6c2f5ba527be6d89b1d
main corpus aggregate     e1ce9c606e1515578faf288a187ce842fb5d41e59d28a07a0d3749c4a09e048b
rom_787e04 overlay toml   82cf1032e1c8805a5dbd07731d1078e77bbbea4403fceb336308232125607050
runner sha256             5814961c584617a197a3f76200dca9d7f30c92c55ba70853827ce637dceccc98 (45,500,785 bytes)
```

## Next

1. **Done (2026-08-06).** The upstream finder `BL`-as-long-branch fix is
   implemented in the local `gbarecomp` working tree (uncommitted) as
   `direct_call_continuation` / `stats_.long_branch_calls` in
   `src/recompile/function_finder.cpp`, with THUMB, ARM, and negative
   plain-fall-through coverage in `tests/recompile/function_finder_test.cpp`.
   All 17 upstream CTest targets pass, including `function_finder_tests`.
   `Func_a2680` is seeded in `tools/build_main_toml.py`'s
   `REVIEWED_RESUME_FUNCTIONS`, `config/usa/main.toml` was regenerated against
   it with zero `data_range` collisions, and the main corpus rebuilt and
   linked into the local runner. See "2026-08-06: `Func_a2680` seeded, new
   10,800-frame boundary" below for the full measurement.
2. **Done (2026-08-06).** Walked the 10,800-frame boundary at THUMB
   `0x0801BC84` (`Func_1bc34+0x50`) by seeding `Func_1bc34` in
   `REVIEWED_RESUME_FUNCTIONS`, and separately un-excluded `rom_a1000`/
   `rom_c9000` from `REVIEWED_SEED_SECTIONS` now that the finder fix covers
   their recorded `BL`-as-long-branch instances too. Both regenerations
   passed the zero-collision gate. See "2026-08-06:
   `rom_a1000`/`rom_c9000` un-excluded and `Func_1bc34` seeded" below.
2b. Walk the new 10,800-frame boundary at IWRAM `0x030044F4` (ARM, trace
    event #73,511,830). `tools/resolve_miss_functions.py` reports no
    containing ELF `STT_FUNC` - this needs writer attribution (which code
    copy installs it and when), the same class of investigation as the
    GS-011 transient images above, not a `REVIEWED_RESUME_FUNCTIONS` seed.
    Not attempted in this pass, per the planner's scope limit.
3. Register new pooled ROUTINES position-independent from the start, as
   `Func_1b70` was. There is no longer any reason to add a per-base row.
4. Re-open the oracle comparison and the GS-010 `DISPSTAT` question.
5. Audit any future overlay's `.text`/`.data` split before registering it.
   `rom_77dd1c` was registered `.text`-only (`0x48bc`, not its `0x57f8`
   decompressed size) for exactly this reason.

## 2026-08-06: `Func_a2680` seeded, new 10,800-frame boundary

The upstream finder fix landed as described above (item 1). Acting on it
first hit a false blocker: `tools/build_main_toml.py` gates on an ELF whose
producing-checkout revision matches `local/symbols/main-symbols.json`'s
`evidence_revision` (the old pin, `0fa7b312199c10b96544e825be86cfc476493eb7`),
and the `goldensun-disasm` checkout here is on local branch
`data/symbolize-pointer-tables`, several commits past that pin. Building the
ELF fresh at the exact pinned commit hit unrelated Makefile/MSYS2 toolchain
breakage in that third-party repo. This was resolved, not worked around: the
branch commits above the pin are the user's own symbolization work plus one
Makefile portability fix (not third-party drift), and decisively, a clean
`make` rebuild of `goldensun.gba` from the branch HEAD
(`84a80693003439acdbb78dd84538b0532461ad4b`) hashes to
`5c4695205413df7db52b9a184815a07783999971` - the exact supported ROM
(`sha1sum -c goldensun.sha1` -> `goldensun.gba: OK`). A byte-identical ROM
rebuild is strong identity evidence per AGENTS.md Section 4. Re-running
`tools/import_main_symbols.py` against this ELF reproduced the existing
2,977-symbol corpus byte-for-byte (aside from the revision stamp), so
`local/symbols/main-symbols.json`, `local/symbols/main-symbols-unresolved.json`,
and `config/usa/main-data-exceptions.json` were corrected to record
`evidence_revision = 84a80693003439acdbb78dd84538b0532461ad4b` - a provenance
correction, not a re-import. See `UPSTREAM.md`.

Two `main.toml` generations isolated the ELF-revision change from the
`Func_a2680` seed:

- **(a) HEAD-ELF baseline, `Func_a2680` still excluded** - sha256
  `9409938833b6dd4627b2579a4ebd57ec65acdecc8d98c018b62145fd783d8423`. Diffed
  against the prior on-disk `main.toml` (sha256
  `81883e0cc35031da1d1ddb94903d293925fc0979a575dbdfbe01a0e3209f5795`,
  built from the old pin): only the three header comment lines (revision,
  corpus sha256, data-exception sha256) differ - zero functional lines
  changed. `data_range` stayed at 3537, `reviewed resume ranges` stayed at
  104 from 37 functions. The branch delta made no difference to the main
  image proposal.
- **(b) `Func_a2680` seeded on top of (a)** - sha256
  `a8c01660ed7da69d14d8d84db4d7358e4b642a1563b66beb965cd8c449760fad`. Diffed
  against (a): `data_range` unchanged at 3537 (**zero collisions**, the hard
  acceptance gate), `reviewed resume ranges` 104 -> 112 from 37 -> 38
  functions - exactly 8 new `[[resume_range]]` blocks spanning
  `Func_a2680`'s extent around its 8 interior literal pools, all carrying the
  note `GS-011 strict-static IRQ resumes inside ELF-bounded Func_a2680`. No
  other line in the file moved.

`config/usa/main.toml` was regenerated to (b)'s content (sha256
`a8c01660ed7da69d14d8d84db4d7358e4b642a1563b66beb965cd8c449760fad`).
`gba_recompile` reported zero `ERROR: ... control-flow entries into
[[data_range]]` lines and exit code 0 for both `local/gs007/main` and
`local/gs011/main` (the directory the runner build actually consumes):
`TOTAL emitted` **25,178 -> 26,348 functions** (+1,170; `arm=609` in both,
`thumb` 24,569 -> 25,739), `long_branch_calls` **0 -> 334** (the fix now
correctly recognizing BLs whose continuation lands in a `data_range` as
non-returning, across the whole corpus, not only inside `Func_a2680`),
`midfn_aliases` 2,637 -> 3,287 entries / 113 -> 121 hosts. The jump from a
778-byte increment to +1,170 functions reflects newly call-graph-reachable
code beyond `Func_a2680` itself, not a discovery-heuristic change.

The rebuilt runner (`build/gs011/GoldenSunRecomp.exe`) ran the 10,800-frame
campaign strict-static track (`GBARECOMP_STRICT_STATIC=1
GBARECOMP_DEMO_INPUT=campaign --frames 10800 --no-window`, real process exit
code 3, not FULLY_STATIC): **the old 777-collision `Func_a2680` wall is gone**,
and the run now advances to trace event **#73,511,740** before a genuine
`STRICT_STATIC dispatch miss` at **THUMB `0x0801BC84`**, inside
`Func_1bc34` (`goldensun.elf` `STT_FUNC` at `0x0801bc34`, size `0xa0`, offset
`+0x50`) - confirmed independently by `tools/resolve_miss_functions.py`. This
is an ordinary unreviewed interior entry, not a data-range contradiction; per
the planner's scope limit, walking it is left for a follow-up pass.

Regression checks at the previously-proven lengths held: 5,400-frame campaign
- `self_heal_coverage=FULLY_STATIC dispatch_misses=0 interpreted_insns=0
healed_native=0 trace_events=36282668`; 5,400-frame no-input with
`GBARECOMP_DEMO_INPUT` genuinely unset (confirmed via `env | grep` before the
run, not merely set empty) - `self_heal_coverage=FULLY_STATIC
dispatch_misses=0 interpreted_insns=0 healed_native=0 trace_events=33936510`.
Both real process exit code 0.

`rom_a1000`/`rom_c9000` remained excluded from `REVIEWED_SEED_SECTIONS` at the
end of this pass; the `long_branch_calls: 334` figure above showed the fix
was already engaging broadly, so un-excluding them was expected to be a
similarly clean follow-up. It was: see below.

## 2026-08-06: `rom_a1000`/`rom_c9000` un-excluded and `Func_1bc34` seeded

Two independent changes, kept strictly separate, on top of the corpus above.

**Step A - `rom_a1000`/`rom_c9000` un-excluded from `REVIEWED_SEED_SECTIONS`.**
The exclusion existed for exactly one recorded reason (see "The 10,800-frame
boundary is resolved as an upstream finder defect, not an ELF contradiction"
above): the compiler emits long intra-function branches at `0x080ac342`
(`rom_a1000`) and `0x080e578a`/`0x080e5ac6` (`rom_c9000`) as `BL`, and the
finder resumed decoding on `$d`-marked literal-pool bytes as a result. That
defect is now fixed in the local `gbarecomp` checkout (the same fix that
resolved the `Func_a2680` wall).

Regenerating `main.toml` from the unmodified script reproduced the prior
sha256 `a8c01660...` exactly - confirming no other input had moved - before
the sections were un-excluded. With them un-excluded, `main.toml` regenerated
to sha256 `b604998545bb8af7d5e9a7dcaacbc740bee58f52e51fc90b7d623b28aa5bec22`:
reviewed section seeds 1,723 -> 2,080, reviewed interworking resumes
185 -> 190, `data_range` 3,537 -> 3,534 (fewer excluded gaps become code, not
a collision), `resume_range` unchanged at 112 from 38 functions. Running the
local `gba_recompile` (patched finder) against both configs with
`--max-functions 40000` exited 0 for both, with no
`ERROR: ... control-flow entries into [[data_range]]` line in either log: the
corpus went 26,348 -> 32,310 functions (+5,962; `arm=609` unchanged,
`thumb` 25,739 -> 31,701), and `long_branch_calls` went 334 -> 363.
No collision appeared, so the gate passed cleanly on the first try.

**Step B - `Func_1bc34` seeded in `REVIEWED_RESUME_FUNCTIONS`.** Verified
against `goldensun.elf` first: `STT_FUNC` `Func_1bc34` at `0x0801bc34`, size
`0xa0` (extent `0x0801bc34`..`0x0801bcd4`), a `$t` mapping symbol at the
entry (THUMB), and an interior `$d`/`$t` split at `0x0801bc4e`/`0x0801bc74`
(a literal pool followed by resumed THUMB code) - all inside the declared
extent, and the observed resume `0x0801bc84` falls inside it too. Adding the
seed on top of Step A moved `main.toml` to sha256
`f6b16a4ab628dc8ccdf5b905657a43ae35340575e00deecfb0698dfb484d981f`:
`resume_range` 112 -> 114 from 38 -> 39 functions (exactly two new blocks
splitting `Func_1bc34` around its one interior literal pool), `data_range`
unchanged at 3,534. `gba_recompile` exited 0, corpus 32,310 -> 32,343
functions (+33).

The rebuilt runner (`build/gs011/GoldenSunRecomp.exe`, regenerated
`local/gs011/main`) ran three tracks:

- **10,800-frame campaign, strict-static** (`GBARECOMP_STRICT_STATIC=1
  GBARECOMP_DEMO_INPUT=campaign --frames 10800 --no-window`): real process
  exit code 3, not `FULLY_STATIC`. The trace confirms dispatch through the
  old boundary - `#73511736 dispatch pc=0x0801BC34 <gf_Func_1bc34+0x0>` and
  `#73511740 dispatch pc=0x0801BC84 <gf_tfunc_0801BC84+0x0>` both execute
  cleanly now - and the run advances to a new boundary at trace event
  **#73,511,830**: `STRICT_STATIC dispatch miss for pc=0x030044F4 (arm)`.
  `tools/resolve_miss_functions.py` reports `0x030044F4 (arm) NO CONTAINING
  STT_FUNC - needs writer attribution, not a resume entry`: the address is
  IWRAM, inside the range the runtime already labels `gf_resume_03003948`
  from a prior code copy, but has no ELF-declared function of its own at
  that PC. This is a writer-attribution question (which code copy installs
  the bytes actually executing there, and when), the same class of work as
  the transient-image sections above - not a `REVIEWED_RESUME_FUNCTIONS`
  seed, and not attempted in this pass.
- **5,400-frame campaign, strict-static**: `self_heal_coverage=FULLY_STATIC
  dispatch_misses=0 interpreted_insns=0 healed_native=0
  trace_events=36282668`, real exit code 0 - identical to the pre-change
  figure, no regression.
- **5,400-frame no-input, strict-static**, `GBARECOMP_DEMO_INPUT` verified
  genuinely unset (`Get-ChildItem Env:` filtered to the name returned no
  row, not merely an empty value) before the run:
  `self_heal_coverage=FULLY_STATIC dispatch_misses=0 interpreted_insns=0
  healed_native=0 trace_events=33936510`, real exit code 0 - identical to the
  pre-change figure, no regression.

Per the planner's scope limit, the new `0x030044F4` boundary is reported and
not walked.

## 2026-08-06: ARM `0x030044F4` attributed — `Func_15afc` at a new base

The 10,800-frame campaign boundary at ARM `0x030044F4` had no containing ELF
`STT_FUNC`, which is normally the signature of an unattributed transient image.
It was not a new routine. It was a routine already registered here, at a base
its fixed registration did not cover.

### Attribution

| Source | Value |
|---|---|
| DMA3 watch | `ch=3 dad=0x030044F4 src=0x08015AFC word=0/158 cnt_h=0x8400` |
| DMA extent | 158 words = 632 bytes (`0x278`) |
| First installed word | `0xE92D0060` — ARM `STMDB sp!, {r5, r6}` prologue |
| `goldensun.elf` | `Func_15afc` `STT_FUNC` at `0x08015afc`, size **632** |
| Mapping symbol at entry | `$a` — ARM, matching the aborting PC's mode |
| Interior `$d` | `0x08015b30..0x08015b38` (one 8-byte literal pool) |
| Entry offset | 0 — the abort PC *is* the copy destination |
| Image SHA-1 | `f6ab01b56876f8d1b72a4bf5239bd5cbac620cb0` |

The DMA extent and the ELF `STT_FUNC` size agree exactly. The image SHA-1,
computed over the ROM source bytes at `0x08015afc`, is byte for byte the
identity the already-registered fixed row at `0x03003a84` was carrying — so the
routine is confirmed by an identity that was live-verified long before this
boundary appeared.

### Why it aborted, and the fix

`0x03003a84` was treated as a fixed staging slot. It is not a slot; it is one
base in the same relocatable pool as the rest of the flash driver. `Func_15afc`
and `Func_15e10` were the last two routines still pinned to a single base after
the earlier position-independence work converted their siblings. Both are now
registered position-independent, taking `kRelocatableCodeImages` from 9 to 11.

`Func_15e10` was converted **alongside** `Func_15afc` rather than after its own
abort. It shares the slot, so the pool would relocate it for the same reason.
This is not speculative: conversion removes an address assumption instead of
adding one, and the resolver hashes the full extent before dispatching, so a
wrong base aborts loudly rather than executing. The first run confirmed it —
`Func_15e10` verified at `0x030044F4` too.

The fixed `0x03003a84` rows are kept, following the `Func_1dc8` precedent. Here
they are a redundant identity gate rather than a necessary one, but a redundant
gate is safe and a missing gate is not. Removing them is a separate change that
should carry its own verified run.

### Result

One 10,800-frame campaign run verifies **41 distinct (image, base) pairs**. The
boundary moved from trace event #73,511,830 to #73,548,537 and became an
ordinary ROM-space interior resume — THUMB `0x0801BD42` = `Func_1bcd4+0x6e`
(ELF `STT_FUNC` at `0x0801bcd4`, size `0xc4`), the function immediately after
the `Func_1bc34` seeded in the previous pass. That is a one-line
`REVIEWED_RESUME_FUNCTIONS` seed.

Both 5,400-frame regressions are unchanged at byte-identical trace-event totals
(`campaign` 36,282,668; no-input 33,936,510, the variable proven unset rather
than empty). A larger dispatch registry changed resolution, not execution.

## 2026-08-06 (later): one routine, two copy lengths

This is the most important finding of the session, and it is a defect in this
project's identity model rather than a missing registration.

After the flash-driver pool family was closed, the campaign track aborted at ARM
`0x03003EEC`. A DMA3 watch on that address records the same source copied at two
different lengths **within one run**:

```
ch=3 dad=0x03003EEC src=0x08009BB8 word=0/177   (0x2c4)
ch=3 dad=0x03003EEC src=0x08009BB8 word=0/121   (0x1e4)
```

`0x2c4` is `Func_9bb8`'s full ELF `STT_FUNC` size and is what was registered.
`goldensun.elf` places a trailing `$d` literal pool at `0x08009d90` that runs to
the next function, `Func_9e7c`, at `0x08009e7c`. The short copy ends at
`0x08009d9c` — every instruction (the code ends at `0x08009d90`) plus the first
three literal words, and nothing more.

So the short copy is a complete, correct image of the routine's code. It simply
has a different identity, because a SHA-1 over the full `0x2c4` extent covers
224 bytes of RAM the short copy never wrote.

### The general rule

> An image identity must cover the bytes the **writer actually wrote**, not the
> bytes a symbol table says the routine spans.

This is the same defect class as the earlier bug that hashed a compressed
overlay's mutable `.data` section: in both cases the identity was derived from a
static description of the routine rather than from the observed write.

The consequence is that **an ELF `STT_FUNC` size is a hypothesis about an
image's extent, not proof of it.** It is excellent corroboration when the DMA
extent agrees — and every other image registered so far did agree, which is why
this went unnoticed. When they disagree, the DMA extent wins, and the routine
may legitimately need more than one registered image.

Any existing registered image could have an unregistered shorter-copy variant.
They will surface the same way this one did: an abort at a base the routine is
already known to use.

### Closing the pool family

Relocating `Func_15afc`/`Func_15e10` moved the boundary but hit `0x030044f4`
again with a *different* routine. The slot cycles between `0x08015AFC`,
`0x08015E10` and `0x08015D74`. Rather than continue one abort at a time, the
family was enumerated from the ELF: the ARM `STT_FUNC`s in the contiguous run
`0x08015430..0x08015e10` are exactly

`15430, 15570, 155d0, 158e8, 15afc, 15d74, 15e10`

(the interleaved odd-valued symbols have bit 0 set and are THUMB, not pool
members). Five were registered, so the remainder was a bounded set of two.
`Func_15d74` is directly DMA-attributed and verified live. **`Func_155d0` was
registered pre-emptively and has not verified in any run yet** — it is inert
until it does, and is recorded as such rather than claimed as confirmed.

### Verifying "no regression" properly

The 5,400-frame campaign track's `trace_events` changed (36,282,668 ->
36,277,140) while the no-input track stayed byte-identical. Rather than assume
the difference was benign instrumentation, it was checked against the recorded
baseline fingerprints:

| fingerprint | baseline | after |
|---|---|---|
| `cycles` | 1042707251 | 1042707251 |
| `steps` | 20362 | 20362 |
| `final_pc` | 0x000001b4 | 0x000001b4 |
| `ppu_frames` / `ppu_vcount` | 5400 / 49 | 5400 / 49 |
| `pal_nonzero` | 931/1024 | 931/1024 |
| `vram_nonzero` | 58877/98304 | 58877/98304 |
| `oam_nonzero` | 542/1024 | 542/1024 |

Every semantic invariant is identical; only the trace-ring counter moved, which
is what adding interior resume aliases changes. This is the same
cumulative-cycle method used to validate the LTO change, and it is the standard
any future "no regression" claim here should meet — a FULLY_STATIC headline
alone does not establish it.

## 2026-08-06 (evening): the Mt. Aleph play session — discovery only, not a static-coverage measurement

A player session (~21:00-00:22) drove a hard crash into a sequence of fixes.
**Every run in this arc had self-heal enabled and none reached FULLY_STATIC.
It is discovery evidence only and must never be presented as proof of static
coverage.**

The session opened with a hard abort, `unknown transient code identity at
0x03003B9C` (`repro.log`/`repro2.log`, 21:51-22:01). A DMA watch
(`GBARECOMP_DMA_WATCH_ADDR=0x03003B9C`) showed a full `0x2c4`-byte copy of
`Func_9bb8` (`word=0/177`, matching the already-registered full extent), and a
store watch (`GBARECOMP_STORE_WATCH`) then caught THUMB `pc=0x03000198`
writing `0x0E0E0E0E` into the copy immediately after the DMA completed. That
combination — a correctly-identified full copy, followed by a write into it
before the code ever ran — is the runtime-patched literal pool fixed below.

Once that fix verified, the session moved on to the overlay resume gaps
covered in the next section (`v3.log`-`v6.log`, 22:22-22:51: aborts of the
form `verified transient image overlay_rom_77dd1c has no AOT entry for
0x0200C51C` / `0x0200C448` / `0x0200C356`), then to a fifth EWRAM overlay
(`fix9.log`, 23:53: `rom_78603c`).

Two cold-cache (`recomp_cache` cleared) self-heal runs bound the discovery
work:

| run | dispatch_misses | interpreted_insns | healed_native | failed | trace_events | ppu_frames |
|---|---|---|---|---|---|---|
| `cold.log` (00:08) | 36 | 413 | 35 | 1 | 44,226,547 | 11,781 |
| `cold2.log` (00:22, post-seed-merge) | 2 | 2 | 2 | 0 | 43,550,054 | (same replay) |

(The `cold.log` figure is **36**, not 42 — an earlier "42 misses" count given
for this session was checked against the log and was wrong; 42 does not
appear anywhere in `cold.log`'s summary line. Recorded here as the correction,
per the standing rule that a figure that cannot be verified against evidence
does not get copied forward.)

`REVIEWED_RESUME_FUNCTIONS` grew from 42 to 79 entries across four labeled
batches merged from this session's self-heal proposals.

## 2026-08-06 (evening): Func_9bb8's runtime-patched literal pool — the third case of the identity rule

This extends "one routine, two copy lengths" above with a fix and a third
instance of the same defect class, found on the same routine, `Func_9bb8`,
independently of the short/full copy-length split.

**The finding.** The game bump-allocates an IWRAM block, DMA3-copies
`Func_9bb8` into it, and then — after the DMA, before the copy is ever called
through — patches the tail of the copy at runtime. A byte-level diff of the
live window against the registered `0x2c4`-byte identity showed all 224
differing bytes at or after offset `0x1d8`, exactly where `goldensun.elf`
ends `Func_9bb8`'s code (the `$d` literal pool begins at `0x08009d90`, offset
`0x1d8` from `0x08009bb8`). Bytes `[0, 0x1d8)` matched the ROM byte for byte.
**Neither of the two already-registered identities — the full `0x2c4` extent
nor the short `0x1e4` copy-length variant — can ever match a live image at
this base**, because both extents include bytes the game overwrites after the
copy completes but before dispatch.

**The fix.** Hash the CODE extent only, `[0, 0x1d8)` — `Func_9bb8_code_relocatable`,
SHA-1 `36f6cde400bd706d9418bf212705dcfda0a02d94`, corpus
`local/gs011/transient_func9bb8_code_pic` (`fix2.log`, 22:09; confirmed live
at `0x03003B9C` by `verify.log`, 22:10). Registered as a third
position-independent `Func_9bb8` identity in `kRelocatableCodeImages`
(`gsr_func9bb8code_pic_*`), alongside the existing full-extent and
short-copy-length identities — not a replacement for either, because the game
still legitimately installs the other two shapes elsewhere.

**The general rule now has a third case.** The rule from "one routine, two
copy lengths" was: an image identity must cover the bytes the writer wrote,
not the bytes a symbol table says the routine spans. This session's finding
sharpens it further: **an identity must cover the bytes the writer wrote AND
must not cover bytes the game later mutates.** A correct DMA-extent identity
can still be wrong if something writes into that extent after the copy and
before the code is dispatched through it. This is the same defect class as
the compressed-overlay `.data`-hashing bug (`Hashing data as if it were
code`, above) — mutable bytes inside a registered extent — but discovered on
a code copy rather than an overlay, and on a copy whose extent the ELF
`STT_FUNC` size does correctly describe. The literal pool is data the routine
itself would read if called, but by the time it runs, the pool has been
overwritten with runtime-computed values, and the identity gate must not care
what was originally there.

## 2026-08-06 (evening): a missing-dispatch-entry defect is a distinct class from an identity failure

The `v3.log`-`v6.log` aborts (22:22-22:51) took the form `verified transient
image overlay_rom_77dd1c has no AOT entry for 0x0200C51C`, then `0x0200C448`,
then `0x0200C356`. This is worth naming as its own failure class, separate
from everything else in this document: the live bytes **did** hash-match the
registered `rom_77dd1c` identity — the image was correctly identified — but
the corpus generated for that identity had no dispatch entry for the specific
interior PC execution actually reached. An identity failure means "I don't
know what this is." A missing-dispatch-entry failure inside an
already-identity-verified image means "I know exactly what this is, and I
still can't run this part of it," because the corpus generator did not emit
an entry point there.

This is the same underlying gap `build_overlay_toml.py`'s later
`[[resume_range]]` generalization closed (see `CHANGELOG.md`, "The Mt. Aleph
scene now completes"): the overlay generator originally validated each
observed entry as one exact THUMB symbol and had no equivalent of
`main.toml`'s `derive_resume_ranges` to split a seeded routine's extent around
its own literal pools. Recorded here because it is a distinct failure mode
from every other one this document documents, and worth keeping distinct in
any future triage: "unknown identity" and "missing dispatch entry" call for
different fixes.

## 2026-08-06 (late): fifth EWRAM overlay, `rom_78603c`

Live play past the boulder scene aborted with `unknown transient code
identity at 0x020080A4`; all four then-registered overlays mismatched.
Identified without a new capture session: the abort path hashes the live
bytes against every candidate's range and reports the mismatch, so hashing
the first `0x5F8` bytes of each of the 96 pinned decompressed overlay builds
against the live hash `864b61a47d6fad56a040afb44baffbeff533d605` named it
uniquely as `rom_78603c`. `overlay.elf` places a `$t` mapping symbol at
`0x020080a4` and `OvlFunc_a4` (56 bytes) at `0x020080a5` — an exact THUMB
function start, not an interior PC.

Registered `.text`-only, following the `rom_779188`/`rom_7795e8`/`rom_77dd1c`
precedent: `0x02008000` + `0x1bdc` (`0x02008000..0x02009bdc`), SHA-1
`eff53a832d743ae378ec9d07ff54e2e43b4c383c` (`fix9.log`, 23:53). `.data`
begins at `0x02009bdc` and is writable by the game, so it is deliberately
excluded from the identity, the same rule already applied to the other three
overlays.

**Attribution caveat.** Unlike the earlier overlays, this one was not
confirmed against a DMA trace — the live-window byte match against the pinned
decompressed build is the only evidence. That is weaker evidence than the
other four overlays carry, and is recorded as such rather than presented as
equally strong.

## 2026-08-07: interworking veneers — 705 linker-emitted stubs derived, not listed

`goldensun.elf` contains 705 linker-emitted interworking veneer stubs: sized-8
THUMB `STT_FUNC` symbols named `_Func_<target>`, in 15 contiguous stride-8
runs (`080000c0`, `08009000`, `08015000`, `08077000`, `0808a000`, `080a1000`,
`080b0000`, `080b5000`, `080c9000`, `080f0000`, `080f2000`, `080f4000`,
`080f6000`, `080f9000`, `08185000` — the heads of the already-seeded
sections). Every stub is byte-identical: `4c00` (`ldr r4,[pc,#0]`), `4720`
(`bx r4`), then a `$d` word holding the THUMB target (bit 0 set). Each has
exactly one resumable interior halfword, at `+0x2` (the `bx r4`), which is
only ever reached by falling out of `+0x0`, never by a discovery walk that
starts there — the same shape as the `MOV ip,pc; BX rN` continuations already
derived elsewhere in this file.

One of the 705 was found the expensive way: a self-heal miss during play
(`_Func_92054` at `0x0808a080`, resume `0x0808a082`), hand-seeded into
`REVIEWED_STATIC_SEEDS`/the old resume list. The other 704 are latent
boundaries of the identical class, never hit yet only because no observed
route had walked into them.

`tools/build_main_toml.py` now derives all 705 from the hash-verified
ELF/ROM (`scan_veneer_stubs` / `derive_veneer_resume_points`), requiring four
independent checks per stub, none of which is "the name looks right":

1. `STT_FUNC`, size exactly 8, THUMB (odd) address;
2. a `$t` mapping symbol at the entry and a `$d` mapping symbol at `+0x4`;
3. the actual ROM bytes at the entry are `4c00 4720`;
4. the word at `+0x4` has bit 0 set (a THUMB target).

All 705 pass, and the derived set is exactly equal to the linker's own
`_Func_<target>`-named symbol set — no more, no fewer. The hand seed for
`_Func_92054`'s `+0x2` resume was removed from the reviewed list as
redundant, now that it is one of the 705 the scan derives (the entry-point
seed for `0x0808a080` itself, used elsewhere as an observed verified-overlay
callback target, is a separate, unrelated seed and was not touched). This
follows the file's existing precedent for the `MOV ip,pc; BX rN` scan, which
already derives ~190 continuations rather than listing them by hand: it
replaces a crawl with a rule.

Measured against `local/gs011_task/main_before.toml` /
`main_after.toml` (before/after adding the 705 derived entries, same
`REVIEWED_RESUME_FUNCTIONS`/`REVIEWED_SEED_SECTIONS` otherwise): `[[extra_func]]`
entries go from 2,996 to 3,701 — exactly +705 — and the regenerated corpus
goes from 34,890 to 34,923 functions.

**A planner hypothesis was checked and disproved.** The hypothesis was that
the 705 veneers might explain `alias_seeds_dropped` (seeds proposed on a
`BL_suffix` halfword, which is not a legal entry point, and so silently
dropped rather than emitted). Measured: `alias_seeds_dropped` reads **712**,
not 705, in the post-derivation regeneration log
(`local/gs011_task/after_regen.log`), and adding all 705 veneer entries left
that counter unchanged from the pre-derivation regeneration
(`local/gs011_task/before/regen.log` reports `resume_range entries: 186`,
`TOTAL emitted: 34890` — the 712 figure is unaffected by the veneer addition
in either log). The near-miss (712 vs 705) is coincidence, not causation, and
the mechanism confirms it independently: `alias_seeds_dropped` only fires on
`BL_suffix` halfwords — the second halfword of a 4-byte `BL` instruction pair
— and a veneer's own bytes (`4c00`, `4720`, then a `$d` word) can never
produce one, because a veneer's `+0x2` halfword is `4720` (`bx r4`), not a
`BL` suffix.

## 2026-08-07: measured results after the veneer derivation

Non-LTO build, 140,228,928 bytes, built 2026-08-07 10:46. All runs
strict-static, self-heal off, cache load off.

| track | frames | result | trace_events | notes |
|---|---|---|---|---|
| `campaign` | 5,400 | FULLY_STATIC | 36,298,166 | `cycles=1042707251 steps=20362 final_pc=0x000001b4 ppu_vcount=49 pal=931/1024 vram=58877/98304 oam=542/1024` — every semantic invariant identical to the recorded baseline fingerprint |
| no input | 5,400 | FULLY_STATIC | 33,909,353 | `cycles=1040556882 steps=21071 ppu_vcount=214 pal=937/1024 vram=50205/98304 oam=515/1024` — identical to baseline |
| `campaign` | 10,800 | **FULLY_STATIC** | 84,942,401 | `dispatch_misses=0`, `cycles=1897809555 steps=39917 ppu_vcount=171 pal=959/1024 vram=59741/98304 oam=535/1024`. **This is a milestone**: the 10,800-frame track previously aborted at `Func_a3ef0+0x7c`; it now completes. |
| `campaign` | 21,600 | **not static** | aborts at trace event #96,264,422 | THUMB `0x08094944`, inside `Func_94820` |

(`build/gs011/step3_campaign.log`, `step3_noinput.log`, `step4_campaign10800.log`,
`step4_21600.log` are the source logs for the four rows; all four figures
were confirmed against the log text, not copied from a summary.)

The 21,600-frame boundary: `goldensun.elf` sizes `Func_94820` as `STT_FUNC` at
`0x08094821` (THUMB, bit 0 set), size 392 bytes, extent
`0x08094820..0x080949a8`, with a clean `$t` run `[0x08094928, 0x080949a4)`.
The aborting PC `0x08094944` falls inside that clean run, at offset `+0x1c`
from the run's start — an ordinary interior entry, not a data-range
contradiction and not a new routine or overlay. The trace ring confirms
dispatch reached it through an ordinary call chain (`gf_Func_4420+0x28` ->
`gf_tfunc_08003152` -> `gf_tfunc_080A4350` -> dispatch miss at
`gf_tfunc_08094928+0x1C`). **As of this writing, a worker is seeding it**;
record this boundary as current at time of writing, not as a stable resting
point.
