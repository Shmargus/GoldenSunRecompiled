# Changelog

## Unreleased

- Proactively registered all 96 pinned overlays as 95 unique immutable `.text`
  identities; one code-identical pair shares a dispatch corpus. Added the
  deterministic bulk generator/compiler, dynamic CMake/X-macro registration,
  complete ELF function/resume coverage, and 12 exact source-proven
  `.call_via` executed-alignment exceptions. Writable overlay data remains
  excluded. Fixed the expanded registry's host-stack diagnostic allocation.
  Binary size is 198.2 MB; the 14,000-frame route improved from 55.6 s to
  53.0 s. State8 and all acceptance tracks remain FULLY_STATIC with unchanged
  semantic invariants.
- Fixed the reproducible post-title roof-transition freeze from state8. Exact
  live hashes and healed-range CRCs identify the seventh registered overlay,
  `rom_780898`; its immutable THUMB `.text` is now hash-gated and AOT compiled.
  Added whole-function, ELF-code-run IRQ resume coverage for `Func_f2ebc` and
  `Func_f26ec`. The 6,000-frame state8 replay is now FULLY_STATIC with zero
  dispatch misses or interpreted instructions. All three acceptance tracks
  remain FULLY_STATIC with semantic invariants unchanged.
- Verified windowed steady-state pacing after the relocatable-RAM resolver fix:
  a 600-frame replay on the 120 Hz reference display averaged 16.7404 ms per
  guest frame (59.74 fps), with a 16.773 ms maximum and no overlay compilation.
  Whole-process timings include about 2.5 seconds of startup/cache loading and
  are not valid frame-rate measurements.
- Added the first optional enhancement: experimental 2x scene interpolation
  in the F1 Enhancements tab. Guest logic/audio/input remain at 59.7275 Hz;
  presentation targets 119.455 Hz on displays reporting at least 119 Hz.
  Scroll and affine BG state interpolate while sprites remain canonical. Every
  candidate endpoint is re-rendered and compared with the faithful scanline
  latch; mismatches fall back loudly to duplicated canonical frames. A
  120-frame gameplay smoke produced 118 midpoints and 2 safe fallbacks. The
  three strict-static acceptance tracks still pass with semantic invariants
  unchanged when the default-off enhancement is disabled.
- Replaced the relocatable-RAM resolver's exhaustive all-entry search with a
  live-opcode index plus full image-identity verification. On the deterministic
  14,000-frame route, scan candidates fell from 9,273,844,530 to 385,568 and
  wall time from 208.726 s to 55.583 s (67.1 to 251.9 fps); RAM dispatch fell
  from about 63% to 5% of host samples. Added opt-in
  `GBARECOMP_RELOCATABLE_PROFILE` counters. All strict-static acceptance tracks
  and semantic invariants remain unchanged.
- Created evidence-driven Golden Sun static-recompilation starter repository.
- Added agent rules, phased plan, overlay strategy, testing policy, legal boundaries, bootstrap checks, and a buildable non-ROM scaffold.
- Installed and validated the Windows, MSYS2, ARM, SDL2, and oracle prerequisites.
- Pinned the initial upstream revisions and recorded the gbarecomp baseline.
- Reproduced the Golden Sun disassembly and verified the main ROM plus all 96 overlays.
- Enabled public CI and extended protected-file auditing to cover BIOS `.bin` files.
- Verified the private BIOS identity and generated the ignored recompiled BIOS corpus.
- Added the GS-003 symbol-corpus schema, semantic validator, and synthetic fixtures.
- Added the hash-gated GS-004 main ELF symbol importer, synthetic ELF tests, and
  deterministic local inventory with explicit code/data and overlap reports.
- Added the GS-005 overlay manifest/importer, exact compressed-ROM checks, and
  collision validator for all 96 partitioned overlay identities.
- Added the GS-006 exact-ROM discovery comparison and deterministic discrepancy
  report workflow.
- Added the GS-007 evidence-derived main TOML generator, reviewed data
  exceptions, ELF-backed IWRAM code-copy mapping, and local x86-64 corpus
  compilation evidence. The required generic finder fix remains an uncommitted
  local upstream delta pending review and a future pin update.
- Added the GS-008 generic same-PC overlay registry spike and automated runtime
  dispatch regression. Overlapping activation evicts stale identity, range
  overwrite invalidates active code, and wrong-mode or unknown execution falls
  through to the existing miss policy; the generic delta remains local pending
  upstream review and a future pin update.
- Added the GS-009 opt-in local runner, corrected the main discovery entry to
  include the proven ARM reset vector at `0x08000000`, and reached early Golden
  Sun initialization through the strict-static generated corpus. The first
  remaining miss is recorded at THUMB PC `0x080047ae`.
- Hardened the local generic runtime so the final BIOS load enforces its
  configured SHA-1 instead of accepting a picker warning as authorization.
- Added the GS-010 TCP-only BIOS-handoff/oracle comparator and bounded mGBA
  executing-PC/fingerprint commands. CPU and writable memory synchronize at
  cartridge handoff; the corrected stream matches for 229 game instruction
  states before a live `DISPSTAT` phase difference at THUMB `0x080030b8`.
- Added GS-011 transient RAM code images: per-image AOT corpora generated with
  `--symbol-prefix`, registered in `kTransientCodeImages`, and dispatched only
  after a live SHA-1 of the installed bytes matches. Documented the previously
  undocumented GS-011 state in `docs/GS011_TRANSIENT_IMAGES.md`.
- Added hash-gated AOT integration for compressed EWRAM overlays `rom_779188`
  and `rom_7795e8`, using pinned decompressed identities and ELF metadata.
- Registered measured `Func_1dc8` and `Func_2544` DMA copies at `0x0300387C`
  and `0x0300347C`. Strict-static now reaches ARM `0x03003714` after 1,932,446
  trace events.
- Registered the eighth transient image, `Func_2808` at `0x03006000`
  (ROM `0x08002808`, `0x4ec` bytes), after a strict run aborted on an unknown
  identity where `Func_2544` had previously been installed at the same address.
  DMA3 trace and `goldensun.elf` independently agree on source and extent. The
  strict run advanced 26% further in recorded trace events.
- Enumerated the current miss set: 41 distinct PCs — 32 ROM-space THUMB
  interior entries (all compile on demand; 26 in the M4A sound driver) and 9
  RAM-space PCs lacking code-copy provenance (6 THUMB in EWRAM, 3 ARM in
  IWRAM). The build is not fully static.
- Corrected the `README.md` status paragraph, which still presented the
  GS-010 `DISPSTAT` divergence as the current boundary.
- Reclassified `0x080047ae` from missing code to an oracle-proven headless
  VBlank resume point and emitted one explicit `resume = true` alias inside the
  existing `0x080047a4` THUMB host body.
- Added all 32 observed ROM-space THUMB misses as exact reviewed AOT seeds.
  Regeneration exposed and resolved one ELF `$d` mapping contradiction at
  `0x080102d2`; strict-static now reaches the unresolved EWRAM THUMB image at
  `0x0200804c` after 1,884,713 trace events.
- Added the proven ARM resume alias at runtime `0x03003714`, offset `0x298`
  inside the SHA-1-gated `Func_2544` copy at `0x0300347C`, backed by ROM
  `0x080027DC`. Strict-static now reaches THUMB `0x080F2C06` after 1,971,301
  trace events.
- Added the proven THUMB resume alias at `0x080F2C06`, offset `0x96` inside
  ELF-bounded `Func_f2b70`. Strict-static now reaches ARM `0x030034CC` after
  2,218,205 trace events.
- Added the proven ARM resume alias at runtime `0x030034CC`, offset `0x50`
  inside the SHA-1-gated `Func_2544` copy and backed by ROM `0x08002594`.
  Strict-static now reaches unresolved THUMB IWRAM target `0x03007DF8` after
  2,479,809 trace events.
- Resolved `0x03007DF8` as the complete four-byte THUMB `Func_6abc` thunk.
  Live bytes, two halfword stores from `0x08006AE6`, ROM source
  `0x08006ABC..0x08006AC0`, and `Func_6878`'s stack-frame lifetime agree.
  The identity-gated AOT corpus advances strict-static to THUMB ROM resume
  `0x080068FE` after 2,493,197 trace events.
- Classified `0x03000828..0x030008CC` as synthesized ARM code. The first write
  is ARM `0x03000800` at store-ring event 3,295,783; the writer expands one of
  three 24-byte ROM templates into a 164-byte executable image. Added a
  hash-gated private-variant builder and AOT registrations for the two observed
  identities, sourced at ROM `0x08001044` and `0x0800105C`.
- Added exact ELF- and trace-backed resumes at ARM `0x0300139C`, ARM
  `0x03000954`, THUMB `0x080103A6`, and callbacks `0x08000290` and
  `0x080001A8`. Added observed entries for `rom_7795e8` at `0x02008030`,
  `0x02008044`, and `0x020083CC`. A 600-frame strict-static run completes; the
  1,800-frame run now reaches ARM `0x030035C0` after 3,722,023 trace events.
- Proved ARM `0x030035C0` as the VBlank scheduler resume at `Func_2544+0x144`,
  mapping to ROM `0x08002688` (`STRB`). Added the analogous verified-overlay
  scheduler resume at THUMB `0x020080C2` inside `OvlFunc_54`.
- Added only the three directly observed verified-overlay callback veneers:
  THUMB `_Func_92054` at `0x0808A080`, `_Func_41d8` at `0x080000D0`, and
  `_Func_91dc8` at `0x0808A360`. Strict-static now reaches unresolved computed
  THUMB target `0x0808FF38` after 3,749,120 trace events.
- Proved `0x0808FF24` as the hash-matched five-entry absolute jump table in
  `Func_8fefc`; its bounded selector and ELF `$d`/`$t` transitions establish all
  five THUMB targets. Added observed function-pointer entry `Func_908e0`.
  Strict-static now reaches `Func_cacc` at THUMB `0x0800CACC` after 3,749,181
  trace events. Seeding it is intentionally blocked because discovery exposes
  11 control-flow entries into five ELF `$d` halfwords inside its extent.
- Resolved all five `Func_cacc` contradictions as real THUMB code. The
  hash-matched ROM and ELF both contain `0x0000`; forced THUMB decodes each as
  `MOVS r0,r0` in a straight-line interworking sequence. Added five narrow
  two-byte data exceptions and the reviewed `Func_cacc` seed. Strict-static
  now reaches THUMB `0x0800D7E8` after 3,749,198 trace events.
- Resolved `0x0800D7E8` as a THUMB entry in `Data_13624`, an ELF-bounded
  188-byte table whose 47 words are all odd pointers to exact STT_FUNC/`$t`
  entries; `Data_136e0` (164 bytes, 41 words) is its sibling. 49 of the 74
  distinct targets were missing from the corpus. Seeding them exposed
  `0x0800CC94`, a THUMB continuation after a `MOV ip,pc; BX rN` veneer, so
  `build_main_toml.py` now derives every veneer continuation and its `0x0000`
  alignment pad by decoding the seeded sections out of the hash-verified ROM
  (185 resumes, 98 derived data exceptions) instead of listing them by hand.
  Three earlier pads stay hand-reviewed: `0x0800dbfa`, `0x0800dc52`,
  `0x0800dfa6`.
- Generalised the remaining crawl. Corpus symbols marked
  `confidence = "unresolved"` are withheld only because their extents contain
  literal pools, not because their entries are in doubt, so `main.toml` now
  seeds the whole ELF-proven set for `rom_c0`, `rom_1b70`, `rom_9000`,
  `rom_15000`, `rom_77000` and `rom_8a000` (1,723 extra seeds; corpus 12,983 ->
  19,496 functions) and `build_overlay_toml.py` applies the same rule to
  overlays (rom_7795e8: 6 -> 79 seeds, 389 functions). `rom_a1000` and
  `rom_c9000` stay out: widening discovery there falls through the long
  intra-function branches at `0x080ac342`, `0x080e578a` and `0x080e5ac6`, which
  the compiler emits as `BL` and the finder treats as returning calls. That is a
  recompiler question, filed rather than papered over.
- Registered the sixteenth transient image, ARM `Func_2cf4` at `0x03007BA4`.
  `Func_3a7c` programs DMA3 `SAD=0x08002cf4 DAD=0x03007ba4 CNT=0x8400001a`
  (26 words, `0x68` bytes) and calls the copy through `_call_via_sp` with
  CPSR.T clear; `goldensun.elf` bounds `Func_2cf4` at exactly `0x68` ARM bytes.
  Image SHA-1 `87b609455a1aa6cd4f6251a73f8e15f4b6bc9c05`.
- Strict-static now clears `0x0800D7E8`, `0x0800CC94`, `0x0800C62C`,
  `0x0801789C` (3,749,807 trace events), overlay entry `0x020081FC` and
  `0x03007BA4` (3,756,285 trace events). It stops on a transient-code mode
  mismatch at `0x03007BC0` - a THUMB PC inside the ARM stack image's extent -
  which needs its own writer attribution. The build is still not fully static.
- Made `verified_ram_dispatch` dump the runtime trace ring on all three of its
  abort paths. The GS-011 boundary was a mode mismatch at `0x03007bc0` with no
  recorded evidence; every attribution below came from that ring.
- Attributed and registered the `0x03007bc0` stack thunk: the same four-byte
  THUMB `Func_6abc` template as `0x03007df8`, installed one frame shallower.
  Store trace `#3839473`/`#3839475` proves both halfwords from cursors
  `0x08006abc`/`0x08006abe`; hash-verified ROM gives the identical image SHA-1.
- Added `[[resume_range]]` support to `build_main_toml.py`. Three consecutive
  builds had stopped at three adjacent halfwords of the same flash-timing delay
  loop, which is a crawl that cannot converge — a VBlank yield re-enters the
  guest wherever it was. `REVIEWED_RESUME_FUNCTIONS` lists ELF STT_FUNC entries
  with an observed interior resume, and `derive_resume_ranges` expands each into
  ranges split around its own `$d` literal pools and around reviewed jump-table
  bytes, so no alias can decode data as code.
- Registered the relocated `Func_15430`/`Func_15570` flash-driver pair at
  `0x0300347c`/`0x030035bc`, the `Func_15afc` image at `0x03003a84`, and the
  `Func_15e10` image that later reuses that same staging slot. In every case the
  DMA3 extent and the ELF `STT_FUNC` size agree exactly and the observed entry
  is an ARM exchange.
- Registered the third synthesized `Func_dc8` template variant (`0x08001074`),
  which a discovery run finally observed live at `0x03000828`.
- **A 5,400-frame headless strict-static run now completes FULLY_STATIC:
  `dispatch_misses=0 interpreted_insns=0 healed_native=0`, with no interpreter,
  self-heal bridge or cache load enabled.** The run feeds no input, so this
  covers the boot and attract path only; gameplay coverage is untested.
- Drove the runner with the deterministic `GBARECOMP_DEMO_INPUT=campaign` track,
  which clears the title and file-select and enters gameplay. The previous
  FULLY_STATIC result held only for the no-input idle path; under input the run
  aborted immediately, which is what the earlier limitation predicted.
- **Fixed a real defect in this project's overlay identity model.** Both
  registered EWRAM overlays hashed their whole decompressed image, which
  includes a writable `.data` section, so the code identity depended on mutable
  game state. An input-driven run aborted on `rom_7795e8` differing in exactly
  two bytes, both inside `.data`. Extents are now the ELF `.text` section only.
- Registered a third EWRAM overlay, `rom_787e04`, matched byte for byte against
  the pinned decompressed build; the abort PC `0x02008160` is that build's exact
  ELF `OvlFunc_160` entry.
- Grew `REVIEWED_RESUME_FUNCTIONS` to twenty functions (79 derived ranges). One
  discovery run reported eight distinct interior PCs inside `Func_8f52c` alone,
  which would have been eight builds under the old one-PC-at-a-time method.
- Replaced the three hand-listed interior resumes in the `Func_2544` copy with a
  derived alias over its whole ARM extent, matching the `Func_1dc8` treatment.
- Registered ten further RAM images (32 total): `rom_787e04`, `Func_a418`,
  `Func_9bb8` at two bases, `Func_158e8`, `Func_15430`/`Func_15570` at a third
  base each, and `Func_2cf4` at three more stack depths.
- Added `tools/resolve_miss_functions.py` (miss PC -> containing ELF function)
  and `tools/build_transient_image_config.py` (ELF-derived transient-image TOML,
  including the resume/data split around literal pools). The latter was
  cross-checked against two hand-written configs and reproduces them exactly.
- `verified_ram_dispatch` now dumps the trace ring on every abort path and
  writes the live RAM window to `GBARECOMP_TRANSIENT_DUMP` on an unknown
  identity, so a compressed overlay can be identified without a TCP capture.
- **Established that per-(routine, base) registration does not converge.** The
  flash-driver working area is a relocatable pool: routine and base vary
  independently, and stack images land wherever their caller's SP happens to be.
  Coverage under input moved from ~4.5M to ~9.55M trace events and is still
  moving. The real fix is position-independent RAM images in the recompiler;
  filed in `docs/GS011_TRANSIENT_IMAGES.md` rather than enumerated further.
- The no-input 5,400-frame strict-static run still completes FULLY_STATIC; that
  claim now carries its qualifier explicitly wherever it appears.

- **Solved the relocatable RAM code pool with position-independent RAM images.**
  The previous session established that per-(routine, base) registration is
  combinatorial and filed the real fix upstream rather than enumerating further;
  this session implemented it. `gba_recompile --relocatable-image ORIGIN:SIZE`
  emits every guest address an image derives from its own PC as
  `g_runtime_image_base + offset`, so ONE corpus dispatches at every base the
  game copies the routine to. ORIGIN survives only in the dispatch table, the
  `goto` labels and the resume tokens, none of which reach the guest.
- Added `kRelocatableCodeImages` to the runner. For a PC no fixed-address image
  explains, it derives a candidate base from each of the image's own entry
  offsets, rejects bases outside RAM or misaligned for the image's mode, screens
  on the first word against the ROM source, and only then hashes the whole
  extent against the registered identity. The last verified base is tried first.
  Resolution runs as the last step before the abort paths, so an unknown image
  is exactly as loud as it was.
- Converted `Func_2cf4`, `Func_6abc` and `Func_1dc8`. One 1,800-frame campaign
  run served **nine** (routine, base) pairs from three corpora, including three
  bases that had never been registered (`0x03007d78`, `0x03007da8`,
  `0x03003f9c`). The four `Func_2cf4` and two `Func_6abc` per-base corpora were
  deleted; the fixed registry went from 32 entries to 26.
  `GBARECOMP_RELOCATABLE_LOG=1` reports each pair the first time it verifies,
  which is the only honest way to see a base set that is deliberately not
  enumerated.
- `Func_1dc8` is additive rather than a replacement: its two fixed rows stay,
  because they are the identity gate for PCs the main corpus dispatches
  statically, and removing them would let an unknown image fall through to a
  stale static entry instead of aborting.
- **The campaign run no longer stops at a moving RAM base.** The boundary moved
  9,553,286 -> 9,814,457 trace events and became an ordinary ROM-space discovery
  gap: THUMB `0x08096a74`, an interior entry in the ELF-sized `Func_96960`.
  Seeding it through `REVIEWED_RESUME_FUNCTIONS` moved the boundary again to
  9,845,087 and THUMB `0x080908b0` (`Func_9088c+0x24`), which is resolved and
  ready as the next one-line seed.
- The no-input 1,800-frame strict-static run still completes FULLY_STATIC.
- Upstream (`gbarecomp`): threaded a `RelocatableImage` through
  `emit_function`/`ArmCodegen`, added `g_runtime_image_base`, and added
  `relocatable_image_tests`. The test asserts an exact equivalence rather than
  spot-checking strings: folding a base back into a relocatable body must
  reproduce the fixed body byte for byte (catching over-rewriting), and no
  literal guest address from the image's own range may survive in a relocatable
  body (catching an address the emitter missed). A corpus generated without the
  flag is byte-identical to before.
- Converted the remaining five pooled routines - `Func_9bb8`, `Func_15430`,
  `Func_15570`, `Func_158e8` and `Func_2544` - to position-independent images.
  Nine more fixed rows deleted; the fixed registry is 17 entries, down from 32,
  and fourteen per-base corpora became nine. Every extent was re-derived from
  `goldensun.elf` `STT_FUNC` sizes and all five SHA-1s matched what the
  live-verified rows already carried.
- **A 1,800-frame campaign (input-driven) strict-static run now completes
  FULLY_STATIC.** This is the first time the with-input path has reached the end
  of a run; every previous revision had to qualify the headline as no-input
  only. The 5,400-frame no-input run is FULLY_STATIC too, re-confirmed rather
  than inherited.
- Walked the boundary through four `REVIEWED_RESUME_FUNCTIONS` iterations
  (`Func_96960`, `Func_9088c`, `Func_d30`, `Func_f9f4`), 20 -> 24 reviewed
  functions and 79 -> 84 derived ranges. The main corpus emits 23,325 functions.
- **Found and registered a new pooled ROUTINE, `Func_1b70`** - the case
  relocation deliberately does not close. Extending the campaign track to 5,400
  frames aborted on an unknown identity at `0x0300387c`, 208 bytes different
  from the `Func_1dc8` image that address had been hosting and matching none of
  the eight registered identities. A DMA3 watch attributed it: `ch=3
  src=0x08001b70 word=0/150 cnt_h=0x8400`, live first word `0xE92D40E2` matching
  ROM `0x08001b70`, and `goldensun.elf` sizing `Func_1b70` at exactly 0x258 with
  an `$a` at the entry - DMA extent and ELF extent agree. Registered
  position-independent from the start, so its base set never has to be
  enumerated.
- One 5,400-frame campaign run now verifies **26 distinct (image, base) pairs**
  across nine relocatable corpora. The 5,400-frame campaign boundary is
  13,945,002 trace events at THUMB `0x080104d4` (`Func_10424+0xb0`), an ordinary
  interior entry whose seed is resolved and ready.
- **A 5,400-frame campaign (input-driven) strict-static run now completes
  FULLY_STATIC**, as does the no-input run at the same length. Both tracks are
  also fully static at 1,800 frames. The claim is now frame- and track-bounded
  rather than input-bounded: `campaign` is one deterministic route and its
  10,800-frame extension is not static yet.
- Walked the boundary through nine more iterations. `REVIEWED_RESUME_FUNCTIONS`
  is 29 functions / 90 derived ranges, up from 20 / 79; the main corpus emits
  24,480 functions, up from 23,180.
- **Registered a fourth EWRAM overlay, `rom_77dd1c`.** The 5,400-frame track
  aborted on an unknown identity at `0x02008104`; the live EWRAM window matched
  the pinned decompressed `rom_77dd1c` build byte for byte over the whole 0x4000
  dump, and `0x02008105` is that build's exact ELF `OvlFunc_104` THUMB entry
  (a `$t` mapping symbol sits at `0x02008104`). Registered `.text`-only -
  `0x02008000 + 0x48bc`, not the `0x57f8` decompressed image - because `.data`
  at `0x0200c8bc` and `.bss` at `0x0200d7f8` would make the code identity depend
  on mutable game state. Registering it moved the boundary 17,654,155 ->
  31,054,452 trace events (+76%).
- **Did NOT seed `Func_a2680`, and that is the current 10,800-frame wall.**
  `0x080a26f0` is an observed interior entry in an ELF-sized `STT_FUNC`, so it
  looks like every other seed, but deriving a resume range over its extent puts
  777 control-flow entries into the declared `data_range [0x080a29fa,
  0x080a2a30)` - which lies inside the function's own ELF extent.
  `derive_resume_ranges` subtracts a function's own `$d` runs, so either that
  range is not `$d` inside `Func_a2680` or the `STT_FUNC` size is wrong. Seeding
  it would decode bytes the ELF marks as data. Recorded as a commented-out entry
  with the evidence rather than forced through.
- **Enabled a 40% throughput win: LTO.** Headless, no window/audio/pacing, the
  Release `-O3` build ran the campaign track at 64.1 fps (1.07x realtime) -
  about 5% of headroom over 60 Hz, which is why a windowed run stutters while
  the same frames "pass" headless. Every recompiled instruction calls out to
  `runtime_should_yield`, `runtime_mem_cycles` and `runtime_tick` (plus
  `bus_read_*`/`bus_write_*` and `runtime_trace_event` for memory ops), all
  out-of-line in the runtime libraries while guest bodies are in the generated
  corpus TUs, so none could inline. `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`
  gives **90.1 fps (1.50x realtime)**. Costs: 20m31s link, 48.7 -> 56.7 MB.
  Verified behaviour-preserving rather than assumed - strict-static at 1,800
  and 5,400 frames on both tracks is FULLY_STATIC from both binaries with
  identical cumulative cycle counts. See `gbarecomp/docs/DEBUGGING.md`.
- Measured and ruled out two suspects: the always-on trace ring costs ~2.7%,
  and idle-loop elision was already on.
- Added an in-window key rebind menu (default **F1**) and the RGB888 overlay
  primitives it needed - there was no text rendering in the host window at all.
  Save-state slots moved F1..F9 -> **F2..F10** to free the key; the menu key is
  itself rebindable via `config.ini [KeyMap] Menu=`. The menu writes
  `keybinds.ini` in the same generic format the launcher's rebind page reads.
- Corrected a benchmarking error of my own: `GBARECOMP_DEMO_INPUT=` with an
  empty value ENABLES a demo track, so an earlier "no-input vs campaign" sweep
  was really running a demo track twice. Re-run with the variable unset; the
  FULLY_STATIC claims were separately backed by genuinely-unset runs and still
  hold, but the sweep that appeared to confirm them did not.

- **Resolved the `Func_a2680` dilemma: it was misdiagnosed, on both horns, as
  an ELF contradiction.** `goldensun.elf` sizes `Func_a2680` `STT_FUNC` at
  `0xc38`, ending exactly at `0x080a32b8` where `Func_a32b8` begins - no gap, no
  overlap - and `0x080a29fa` is a genuine `$d` alignment pad followed by a real
  13-word literal pool at `0x080a29fc`, with `$t` resuming at `0x080a2a30`; the
  ROM and ELF bytes over `[0x080a26f0, 0x080a2a30)` hash identically. The real
  cause is the already-filed finder `BL`-as-long-branch defect: the 778-byte
  stretch `[0x080a26f0, 0x080a29fa)` has no walk-terminating instruction at all
  (no `BX`/`POP pc`/`MOV pc`/unconditional `B`), ending in a 4-byte `BL` at
  `0x080a29f6` targeting the interior address `0x080a3252` (`Func_a2680+0xbd2`),
  with a second intra-function `BL` at `0x080a274c` -> `0x080a31f6`. The finder
  enqueues `BL`'s return continuation at `pc + 4`, landing on the literal pool;
  389 aliased THUMB entry points in the terminator-less stretch all fall into
  that one pool, reported 777 times (388 twice, one once) - reproducing the
  previously recorded 777 exactly, and hitting no other data range in the
  function's other seven interior pools. `Func_a2680` sits in `rom_a1000`, the
  section already excluded for this same defect (recorded instances
  `0x080ac342`, `0x080e578a`, `0x080e5ac6`), so the 10,800-frame boundary and
  the `rom_a1000`/`rom_c9000` exclusion are one root cause, not two. The fix -
  treat a `BL` whose `pc + 4` continuation lands inside a declared `data_range`
  as non-returning, and skip enqueuing it - was designed but NOT implemented;
  it belongs in the `gbarecomp` checkout, not here. Nothing was regenerated, no
  strict-static claim changed, and the `Func_a2680` seed stays commented out in
  `REVIEWED_RESUME_FUNCTIONS`.
- **The upstream finder `BL`-as-long-branch fix is now implemented** (uncommitted,
  local `gbarecomp` checkout: `direct_call_continuation` /
  `stats_.long_branch_calls` in `src/recompile/function_finder.cpp`, THUMB/ARM/
  negative-case coverage in `tests/recompile/function_finder_test.cpp`). All 17
  upstream CTest targets pass, including `function_finder_tests`. The
  `Func_a2680` entry in `tools/build_main_toml.py`'s `REVIEWED_RESUME_FUNCTIONS`
  is uncommented on that basis - the "designed but NOT implemented" note above
  is now stale for the implementation itself.
- **Seeded `Func_a2680` and regenerated the main corpus - zero `data_range`
  collisions.** The `goldensun-disasm` checkout here is on local branch
  `data/symbolize-pointer-tables`, past the pin `local/symbols/main-symbols.json`
  recorded; a clean `make` rebuild of `goldensun.gba` from that branch's HEAD
  (`84a80693003439acdbb78dd84538b0532461ad4b`) hashes to
  `5c4695205413df7db52b9a184815a07783999971` - the exact supported ROM - and
  re-running `tools/import_main_symbols.py` against that ELF reproduced the
  existing 2,977-symbol corpus byte-for-byte, so the `evidence_revision`
  fields were corrected to the actual commit used (provenance fix, not a
  re-import; see `UPSTREAM.md`). Generating `main.toml` with the HEAD ELF
  alone (before touching the seed) changed only header comment lines versus
  the prior `81883e0c...` file - the branch delta made no difference to the
  proposal. Adding the `Func_a2680` seed on top left `data_range` unchanged at
  3537 and added exactly 8 `[[resume_range]]` blocks (104 -> 112, 37 -> 38
  functions). `config/usa/main.toml` sha256 is now
  `a8c01660ed7da69d14d8d84db4d7358e4b642a1563b66beb965cd8c449760fad`. The
  regenerated main corpus emits 26,348 functions (up from 25,178 under the
  same HEAD ELF without the seed; arm=609 unchanged, thumb 24,569 -> 25,739),
  with `long_branch_calls` going 0 -> 334 corpus-wide - the fix engaging well
  beyond `Func_a2680` itself once reachable.
- **The 10,800-frame campaign boundary moved past `Func_a2680`, to a new,
  ordinary interior miss.** The rebuilt runner's strict-static 10,800-frame
  campaign run (real process exit code 3) now advances to trace event
  #73,511,740 before `STRICT_STATIC dispatch miss for pc=0x0801BC84 (thumb)`,
  inside ELF-sized `Func_1bc34` (`0x0801bc34`, size `0xa0`, offset `+0x50`) -
  confirmed by `tools/resolve_miss_functions.py`. The 5,400-frame campaign and
  5,400-frame no-input (env genuinely unset, checked via `env | grep` before
  the run) tracks both remain `FULLY_STATIC dispatch_misses=0
  interpreted_insns=0 healed_native=0` (trace_events=36,282,668 and
  33,936,510 respectively) - no regression from the seed change.
- **GS-011: un-excluded `rom_a1000`/`rom_c9000` from `REVIEWED_SEED_SECTIONS`
  now that the upstream finder's `BL`-as-long-branch fix is in place - zero
  `data_range` collisions.** The exclusion existed for exactly one recorded
  reason: the compiler emits long intra-function branches at `0x080ac342`
  (`rom_a1000`) and `0x080e578a`/`0x080e5ac6` (`rom_c9000`) as `BL`, and the
  finder treated a `BL` as a returning call, resuming decoding on
  `$d`-marked literal-pool bytes. That defect is fixed in the local
  `gbarecomp` checkout. Regenerating `main.toml` from the unchanged script
  reproduced the prior sha256 `a8c01660...` exactly, confirming no other
  input moved; regenerating with the two sections un-excluded produced
  sha256 `b604998545bb8af7d5e9a7dcaacbc740bee58f52e51fc90b7d623b28aa5bec22`
  with reviewed section seeds 1,723 -> 2,080, reviewed interworking resumes
  185 -> 190, and `data_range` 3,537 -> 3,534 (fewer excluded gaps become
  code, not a collision). Running the local `gba_recompile` (patched finder)
  against both configs exited 0 with no collision reports: the corpus went
  26,348 -> 32,310 functions (+5,962) and `long_branch_calls` 334 -> 363.
- **Seeded `Func_1bc34` and closed the 10,800-frame campaign's prior
  boundary.** `goldensun.elf` confirms ELF `STT_FUNC` `Func_1bc34` at
  `0x0801bc34`, size `0xa0` (`0x0801bc34`..`0x0801bcd4`), with a `$t` mapping
  symbol at its entry (THUMB) and an interior `$d`/`$t` split at
  `0x0801bc4e`/`0x0801bc74`; the observed resume `0x0801bc84` falls inside
  that extent. Adding it to `REVIEWED_RESUME_FUNCTIONS` on top of the
  `rom_a1000`/`rom_c9000` change moved `main.toml` to sha256
  `f6b16a4ab628dc8ccdf5b905657a43ae35340575e00deecfb0698dfb484d981f`,
  `resume_range` 112 -> 114 from 38 -> 39 functions (`data_range` unchanged
  at 3,534). `gba_recompile` exited 0, corpus 32,310 -> 32,343 functions
  (+33). The rebuilt runner's strict-static 10,800-frame campaign (real
  process exit code 3) now advances past `0x0801bc84` and the old boundary
  entirely, to a new one at trace event #73,511,830:
  `STRICT_STATIC dispatch miss for pc=0x030044F4 (arm)`, an IWRAM address
  with no containing ELF `STT_FUNC` (`tools/resolve_miss_functions.py`
  reports "NO CONTAINING STT_FUNC - needs writer attribution, not a resume
  entry") - out of scope for this pass per plan. The 5,400-frame campaign and
  5,400-frame no-input (env genuinely unset, verified empty before the run)
  tracks remain `FULLY_STATIC dispatch_misses=0 interpreted_insns=0
  healed_native=0` at the same trace-event totals as before
  (36,282,668 and 33,936,510) - no regression.

- **Resolved the ARM `0x030044F4` boundary: it was a known routine at an
  unregistered base, not a new one.** A DMA3 watch attributes it exactly —
  `ch=3 dad=0x030044F4 src=0x08015AFC word=0/158 cnt_h=0x8400`, i.e. 158 words
  = 632 bytes (`0x278`), first word `0xE92D0060` (ARM `STMDB sp!` prologue).
  `goldensun.elf` sizes `Func_15afc` at exactly 632 bytes with an `$a` at the
  entry, so the DMA extent and the ELF extent agree, and the abort PC equals
  the copy destination, so the entry offset is 0. The SHA-1 of the ROM source
  bytes over that extent, `f6ab01b56876f8d1b72a4bf5239bd5cbac620cb0`, matches
  byte for byte the identity the already-registered fixed row at `0x03003a84`
  carried — independent confirmation that this is the same routine relocated,
  not a new image.
- `Func_15afc` and `Func_15e10` were the last two flash-driver routines still
  pinned to a single base (`0x03003a84`, a slot they share). Both are now
  **position-independent**, bringing `kRelocatableCodeImages` from 9 to 11.
  Converting `Func_15e10` alongside `Func_15afc` rather than waiting for its
  own abort was vindicated by the very first run: it verified at `0x030044F4`
  too. Conversion removes an address assumption rather than adding one, and the
  resolver hashes the full extent before dispatching, so a wrong base aborts
  loudly instead of running.
- The two fixed `0x03003a84` rows are deliberately KEPT, following the
  `Func_1dc8` precedent. They are a redundant identity gate here rather than a
  necessary one, but a redundant gate is safe and a missing one is not;
  deleting them is a separate change that wants its own verified run.
- One 10,800-frame campaign run now verifies **41 distinct (image, base)
  pairs**. The boundary moved from trace event #73,511,830 to #73,548,537 and
  became an ordinary ROM-space interior resume: THUMB `0x0801BD42` =
  `Func_1bcd4+0x6e` (ELF `STT_FUNC` at `0x0801bcd4`, size `0xc4`), the function
  immediately following the `Func_1bc34` seeded in the previous change. Its
  one-line seed is resolved and ready.
- Both 5,400-frame regressions re-ran with the eleven-image registry and are
  unchanged: `campaign` FULLY_STATIC at 36,282,668 trace events and no-input
  FULLY_STATIC at 33,936,510, `dispatch_misses=0 interpreted_insns=0
  healed_native=0`, with `GBARECOMP_DEMO_INPUT` proven unset (not empty) for
  the no-input run. Byte-identical totals: a larger dispatch registry changed
  resolution, not execution.
- Corrected the recorded description of the upstream finder fix in
  `tools/build_main_toml.py` and `docs/GS011_TRANSIENT_IMAGES.md`. Both said the
  finder recognises "a `BL` whose *target* lies inside the caller's own ELF
  extent". The implementation tests the **continuation** `pc + 4` against a
  declared **`data_range`** — narrower than the design originally sketched in
  the doc, needing no ELF knowledge in the recompiler, and unable to fire on a
  call whose return address is ordinary code. That distinction is what makes
  the suppression proof rather than heuristic.

- **Found a real defect in the RAM-image identity model: one routine, two copy
  lengths.** After the flash-driver pool family was closed, the campaign track
  aborted at ARM `0x03003EEC`. A DMA3 watch on that address records
  `src=0x08009BB8` copied at TWO different lengths in the same run —
  `word=0/177` (`0x2c4`, the full ELF `STT_FUNC` extent, which is what was
  registered) and `word=0/121` (`0x1e4`). `goldensun.elf` puts a trailing `$d`
  literal pool at `0x08009d90` running to `Func_9e7c` at `0x08009e7c`; the short
  copy ends at `0x08009d9c`, i.e. every instruction (code ends at `0x08009d90`)
  plus the three literal words it needs, and nothing else. A SHA-1 over the full
  `0x2c4` extent therefore covers 224 bytes the short copy never wrote and can
  only ever match the long copy.
  **The general rule: an image identity must cover the bytes the WRITER wrote,
  not the bytes a symbol table says the routine spans.** This is the same defect
  class as the earlier bug that hashed an overlay's mutable `.data` section.
  Registered the short copy as a second position-independent image
  (`49496bc9fb68cacac07613ccb2ead86bc0583ffc`); it verified at `0x03003EEC` and
  moved the boundary +348,654 trace events.
- **Closed the flash-driver pool FAMILY instead of discovering it one abort at a
  time.** Relocating `Func_15afc`/`Func_15e10` moved the boundary but hit
  `0x030044f4` again with a different routine — the slot cycles between
  `0x08015AFC`, `0x08015E10` and `0x08015D74`. In `goldensun.elf` the ARM
  `STT_FUNC`s in the contiguous run `0x08015430..0x08015e10` are exactly 15430,
  15570, 155d0, 158e8, 15afc, 15d74, 15e10 (the interleaved odd-valued symbols
  are THUMB and not pool members), so the remainder was a bounded set of two.
  `Func_15d74` is directly DMA-attributed (39 words = 156 bytes, ELF size 156).
  `Func_155d0` was registered pre-emptively on the `Func_15e10` precedent; it
  has NOT verified in any run so far and is inert until it does — recorded
  honestly rather than presented as confirmed.
- `kRelocatableCodeImages` is now 14 images, up from 9. One 10,800-frame
  campaign run verifies 43 distinct (image, base) pairs, up from 41.
- Walked the ROM-space boundary through four more `REVIEWED_RESUME_FUNCTIONS`
  iterations (`Func_1bcd4`, `Func_a112c`, `Func_1de5c`, plus `Func_1bc34`
  earlier): 39 -> 42 reviewed functions, 114 -> 121 derived ranges, main corpus
  32,343 -> 32,760 functions. The 10,800-frame campaign boundary is now THUMB
  `0x080A3F6C` = `Func_a3ef0+0x7c`, an ordinary interior resume.
- **Both 5,400-frame tracks remain FULLY_STATIC, verified semantically rather
  than by headline.** The no-input run is byte-identical to baseline
  (33,936,510 trace events). The campaign run's `trace_events` moved
  36,282,668 -> 36,277,140, so it was checked against the recorded baseline
  fingerprints instead of assumed: `cycles=1042707251`, `steps=20362`,
  `final_pc=0x000001b4`, `unmapped=0`, `io_unhandled=0`, `ppu_vcount=49`,
  `ppu_frames=5400`, `pal_nonzero=931/1024`, `vram_nonzero=58877/98304`,
  `oam_nonzero=542/1024` are ALL identical. Only the trace-ring instrumentation
  counter differs, which is what adding interior resume aliases changes. Same
  cumulative-cycle method used to validate LTO.

- **Performance: enabled LTO in the runner build, +52% throughput.** The build
  was running WITHOUT `CMAKE_INTERPROCEDURAL_OPTIMIZATION` despite an earlier
  session measuring it as a 40% win, so the shipped binary was running the
  campaign track at 42.5 fps = **0.71x realtime — slower than the hardware**.
  With LTO it reaches 64.8 fps (1.09x) on `campaign` and 56.5 fps (0.95x) on the
  no-input track. Verified behaviour-preserving rather than assumed: every
  semantic fingerprint is byte-identical across both binaries on both tracks
  (`cycles=1042707251`/`1040556882`, `steps=20362`/`21071`, all PAL/VRAM/OAM
  population counts, both FULLY_STATIC with zero misses). Cost: ~25 minute link
  (GCC runs LTO partitions serially by default), 75.4 MB binary.
- Measured and **rejected** `-march=native`: +1.2% on the campaign track, inside
  run-to-run noise, not worth tying the binary to one CPU. Reverted, and the
  verified generic binary restored rather than left in place.
- Recorded that static coverage costs throughput: ~24,480 corpus functions
  previously measured 1.50x realtime under LTO; at 32,760 functions (+34%) the
  same configuration yields 1.09x. Worth measuring whenever the corpus grows
  substantially rather than discovering later.
- Added `docs/CHECKPOINT_2026-08-06.md` — the measured state of static coverage,
  performance, and what is explicitly NOT claimed.
- Upstream (`gbarecomp`): fixed the inert "Integer scaling" checkbox. It was
  applied via `SDL_RenderSetIntegerScale`, which only affects the
  `SDL_RenderSetLogicalSize` path, while frames are presented through
  `compute_presentation_layout` + an explicit `SDL_RenderCopy` destination rect
  that bypasses it — so the setting defaulted ON and did nothing, and every
  non-multiple window size got unevenly duplicated pixels (e.g. 1920x1080 gave
  a 6.75x scale with source columns 6 and 7 pixels wide). Added a `ScalingMode`
  (`IntegerLetterbox` default, `AspectFill` = previous behaviour, `Stretch`) and
  wired the preference through. Tests assert uniform source-pixel width across a
  sweep of real window sizes rather than fixed numbers. Upstream suite 17/17.
- Upstream: recorded `docs/ENHANCEMENT_NOTES.md` — designs for logic-decoupled
  frame interpolation and affine-only internal upscaling. Neither is
  implemented; both are Rule 10 gated, and frame interpolation needs 2x the
  render budget when there is currently ~1x.

- **Windowed play crashes on the default path: STATUS_STACK_OVERFLOW.** The
  first genuine windowed run by a user died with exit code `-1073741571`
  (`0xC00000FD`) seconds after the GBA logo. Cause: when `args.window` is set the
  runtime defaults `present_in_place` to true, installing a frame-present hook
  that presents WITHOUT unwinding the guest call stack. Each frame nests deeper
  than the last and the host stack grows without bound. `main.toml`'s
  `0x080040E8` note already recorded that present-in-place "hid this class by
  never unwinding, at the cost of an unbounded host stack" - but that cost was
  never connected to windowed play being unusable.
  **This escaped every test because all automated verification runs
  `--no-window`, and the hook is only installed when windowed.** Headless
  coverage, however thorough, proves nothing about the windowed path.
  Workaround in the local launcher: `GBARECOMP_PRESENT_IN_PLACE=0` selects the
  unwind-and-redispatch path. A larger host stack is NOT a fix - the growth is
  unbounded, not merely deep.
  NOT verified by me: the fix cannot be reproduced or tested without a real
  display, and this environment has none. Awaiting user confirmation.
- Windows startup also required `SDL2.dll`, `libgcc_s_seh-1.dll`,
  `libstdc++-6.dll` and `libwinpthread-1.dll` beside the executable. They resolve
  from the MSYS2 shell's PATH but not from Explorer or PowerShell, so the program
  failed to load with no diagnostic at all. Copied next to the binary; the
  launcher now checks for them and restores them if missing.

- **Root-caused and fixed the Mt. Aleph crash: a relocated routine PATCHES its
  own literal pool after being copied.** A player hit `unknown transient code
  identity at 0x03003B9C` in the prologue. Reproduced headlessly and
  deterministically from the player's save state plus a recorded input track
  (`GBARECOMP_INPUT_REPLAY`), which turned a hand-hit bug into a seconds-long
  test loop.
  The game bump-allocates an IWRAM block (allocator at `0x03001e50`), DMA3-copies
  `Func_9bb8` in, records the address in a struct field, and later calls through
  it. A live window dump shows all 224 differing bytes at or after offset
  `0x1d8` — exactly where `goldensun.elf` ends the code (`$d` at `0x08009d90`).
  Bytes `[0, 0x1d8)` match ROM byte for byte. The routine's CODE is never
  modified; its trailing literal pool is patched with runtime values.
  Registered a **code-only** position-independent image, extent `0x1d8`, SHA-1
  `36f6cde400bd706d9418bf212705dcfda0a02d94`. It verifies live at
  `0x03003B9C` and the boundary moves on.
  **This is the third instance of one defect class**, after hashing an overlay's
  mutable `.data` and after assuming a routine has a single copy length: an
  image identity must cover only the bytes the WRITER leaves alone — not an ELF
  extent, not a copy length. Any remaining registration whose extent includes a
  trailing literal pool is suspect for the same reason.
- Three hypotheses were tested and **disproved** before the real cause was
  found, each recorded rather than quietly dropped: the setup routine failing to
  run (it runs 574 times), a corrupt save state (states capture EWRAM and IWRAM
  faithfully), and the earlier `Func_948` self-heal failure (fixed; crash
  unchanged). An offset error of mine — reading the dump at the candidate range
  base instead of `pc & ~0xFFF` — produced a wrong "450 of 708 bytes differ"
  intermediate result that was corrected before it reached a conclusion.
- Added `GBARECOMP_STORE_WATCH=<lo>[:<hi>]` upstream: reports every guest store
  into an address range with the PC responsible. The DMA watch cannot see
  ordinary stores, and the abort ring dump only carries 160 events — far too
  short to reach a corruption from many frames earlier.
- Added headless rasteriser accounting (`ppu_render_total_ms`): the PPU is
  **8.9%** of wall time, guest CPU 91.1%. Frame interpolation re-runs only the
  rasteriser, so 2x smoothness costs about **+9%**, not 2x — correcting an
  earlier claim in these notes.
- Parallel LTO (`-flto=16`): link 25m -> **5m04s**, for ~3% runtime. Measured
  and **rejected** three micro-optimisations as noise: the trace recorder,
  `-march=native`, and serial-vs-parallel LTO all sit inside a +/-3% run-to-run
  band (recorder on 85.6s mean vs off 85.7s over three runs each).

- **Player-confirmed: the boulder now spawns.** The code-only `Func_9bb8`
  identity cleared the Mt. Aleph abort in live play, not just in replay.
- **Fixed the psynergy-scene stutter: the game was running a C compiler
  mid-scene.** 14 functions were being self-healed (translated at runtime)
  during the sages' spell. Resolved them from the session log to 8 containing
  ELF functions, seeded all 8, and the reproduced scene now performs **zero**
  runtime compiles (14 -> 0). Corpus 34,071 -> 34,208 functions.
- **The remaining freeze needs a feature the overlay generator does not have.**
  After the above, the replay stops at `verified transient image
  overlay_rom_77dd1c has no AOT entry for 0x0200C306`, then `0x0200C51C`. Both
  are INTERIOR addresses, not function entries:
    * `0x0200C306` is `OvlFunc_4304+0x2` (`$t` at `0x0200c304`, STT_FUNC 72 bytes)
    * `0x0200C51C` lies in the THUMB run starting at `$t` `0x0200c4f0`, with no
      named STT_FUNC covering it
  `build_overlay_toml.py` validates each observed entry as "one exact thumb
  symbol" and correctly rejects both. `main.toml` solves exactly this with
  `REVIEWED_RESUME_FUNCTIONS` / `derive_resume_ranges`, splitting a function's
  extent around its own `$d` literal pools; the overlay path has no equivalent.
  Porting that mechanism to overlays is the next piece of work. The observed
  entry for `0x0200C306` is recorded in
  `config/usa/overlay-rom-77dd1c-observed-entries.json` with full ELF evidence
  so it is ready the moment the generator can express it; the validator refuses
  it today, which is the desired behaviour, not a bug to work around.

- **The Mt. Aleph scene now completes: replay exits 0 instead of aborting.**
  Generalised `build_overlay_toml.py` to emit `[[resume_range]]` blocks over
  every seeded routine's full extent, split around the overlay's own data
  ranges - the same rule `derive_resume_ranges` already applied to `main.toml`.
  Previously an overlay entered mid-body cost one regeneration/rebuild/run cycle
  per interior PC (0x0200C306, 0x0200C51C, 0x0200C448, 0x0200C356 ... which does
  not converge). rom_77dd1c: 169 resume ranges, corpus 1,782 -> 2,818 functions,
  zero data-range collisions.
  Correction: an earlier entry claimed the overlay path had no interior-resume
  support. It did - a per-entry `resume: true` flag - and the real gap was that
  it covered a single PC rather than a routine. The `resume: true` flag was
  simply not set on the observed entries.
- 68 ROM-space functions are still translated at runtime during the scene. They
  no longer freeze it, but they are the remaining stutter and the next seeds.

- **Registered a fifth EWRAM overlay, `rom_78603c`.** Live play past the boulder
  scene aborted with `unknown transient code identity at 0x020080A4`, all four
  registered overlays mismatching. Identified WITHOUT a new capture: the abort
  report prints the live hash over each candidate's range, so hashing the first
  0x5F8 bytes of every pinned decompressed overlay against
  `864b61a47d6fad56a040afb44baffbeff533d605` names it uniquely as `rom_78603c`.
  `overlay.elf` puts `$t` at `0x020080a4` and `OvlFunc_a4` (56 bytes) at
  `0x020080a5`, so the entry is an exact THUMB function start.
  Registered `.text`-only (`0x02008000` + `0x1bdc`, SHA-1
  `eff53a832d743ae378ec9d07ff54e2e43b4c383c`); `.data` at `0x02009bdc` is
  writable and excluded, the same rule already applied to `rom_779188` and
  `rom_7795e8`. 73 seeds, 1,773 functions, transient registry 18 -> 19.
- Player-confirmed: the Mt. Aleph boulder scene now completes in live play,
  "with MASSIVE slowdowns" - the remaining runtime translation. The replay exits
  0 with 0 runtime compiles, though that figure benefits from a warm compile
  cache and is not by itself proof of full static coverage.

- **Cut the boulder-scene stutter: 70 runtime translations -> 4.** Enumerated
  them by replaying the player's recorded input with the compile cache parked,
  so every heal re-reported instead of loading warm. 70 events resolved to 16
  containing ELF routines; 14 were new and are now seeded. Corpus 34,208 ->
  34,890 functions, `REVIEWED_RESUME_FUNCTIONS` 65 -> 79 functions / 184 ranges,
  zero data-range collisions, replay still exits 0.
  Method note: a warm cache makes a run report zero heals regardless of static
  coverage, so "0 runtime compiles" is only meaningful with the cache parked.
  The first measurement after the previous build showed 0 for exactly that
  reason and was not evidence of anything.
- Two PCs still translate at runtime (`0x08003D8E` in `Func_3d28`, `0x0808A082`
  in `_Func_92054`). Both containing routines are already seeded, so these need
  individual attention rather than another batch - left as the next item, since
  4 events will not be perceptible where 70 were.

- **Root-caused and fixed a runtime-patched literal pool: `Func_9bb8`'s third
  identity.** A hard abort, `unknown transient code identity at 0x03003B9C`,
  turned out to be neither of the two already-registered `Func_9bb8`
  identities (full `0x2c4` extent, short `0x1e4` copy length). A DMA watch
  showed the full copy landing intact; a store watch then caught THUMB
  `pc=0x03000198` writing `0x0E0E0E0E` into the copy immediately after the
  DMA, and a byte-level diff placed all 224 differing bytes at or after
  offset `0x1d8` — exactly where `goldensun.elf` ends `Func_9bb8`'s code.
  Registered a third position-independent identity covering the CODE extent
  only (`[0, 0x1d8)`), SHA-1 `36f6cde400bd706d9418bf212705dcfda0a02d94`; it
  verified live at `0x03003B9C`. This sharpens the "one routine, two copy
  lengths" rule to a third case: an identity must cover the bytes the writer
  wrote AND must not cover bytes the game later mutates. `kRelocatableCodeImages`
  is now 15 entries.
- Distinguished a **missing-dispatch-entry** defect from an **identity**
  defect. A separate sequence of aborts (`verified transient image
  overlay_rom_77dd1c has no AOT entry for 0x0200C51C` / `0x0200C448` /
  `0x0200C356`) hash-matched the already-registered `rom_77dd1c` identity
  correctly; the corpus generated for it simply had no dispatch entry at the
  specific interior PC reached. Recorded as its own failure class going
  forward, since "unknown identity" and "missing dispatch entry" call for
  different fixes.
- **Registered a fifth EWRAM overlay, `rom_78603c`.** Live play past the
  boulder scene aborted with `unknown transient code identity at
  0x020080A4`; all four registered overlays mismatched. Identified without a
  new capture: hashing the first `0x5F8` bytes of every pinned decompressed
  overlay build against the live hash `864b61a47d6fad56a040afb44baffbeff533d605`
  named it uniquely. `overlay.elf` puts a `$t` mapping symbol and
  `OvlFunc_a4` (56 bytes) exactly at `0x020080a4`/`0x020080a5`. Registered
  `.text`-only (`0x02008000` + `0x1bdc`, SHA-1
  `eff53a832d743ae378ec9d07ff54e2e43b4c383c`); `.data` at `0x02009bdc` is
  writable and excluded, the same rule as the other three overlays.
  Attribution is a live-window byte match rather than a DMA trace — weaker
  evidence than the other overlays, and recorded as such.
  `kTransientCodeImages` is now 19 entries.
- Two cold-cache (recomp cache cleared) self-heal discovery runs bound this
  session's work: `dispatch_misses=36 interpreted_insns=413 healed_native=35
  failed=1 trace_events=44226547` before the fixes above merged, and
  `dispatch_misses=2 healed_native=2 failed=0 trace_events=43550054` after.
  Every run in this arc had self-heal enabled and none reached FULLY_STATIC —
  discovery evidence only, not a static-coverage measurement.
  `REVIEWED_RESUME_FUNCTIONS` grew from 42 to 79 entries across four labeled
  batches merged this session.

- **Derived all 705 linker-emitted interworking veneer stubs instead of
  listing them.** `goldensun.elf` contains 705 sized-8 THUMB `STT_FUNC`
  symbols named `_Func_<target>` in 15 contiguous stride-8 runs, each
  byte-identical (`4c00 ldr r4,[pc,#0]; 4720 bx r4; $d target|1`), with
  exactly one resumable interior halfword at `+0x2`. One (`_Func_92054`) had
  been found and hand-seeded the expensive way, during a self-heal miss; the
  other 704 are latent boundaries of the identical class. `scan_veneer_stubs`
  / `derive_veneer_resume_points` in `tools/build_main_toml.py` now derive
  all 705 from the hash-verified ELF/ROM, requiring four independent checks
  per stub (sized THUMB `STT_FUNC`, `$t`/`$d` mapping symbols at entry/`+0x4`,
  the exact ROM bytes, and bit 0 set on the target word); the derived set is
  exactly the linker's own named-symbol set. The hand seed for `_Func_92054`'s
  `+0x2` resume was removed as redundant, now proven covered by the scan.
  `[[extra_func]]` entries went from 2,996 to 3,701 (+705), corpus 34,890 ->
  34,923 functions.
  A hypothesis that this might explain the `alias_seeds_dropped` counter
  (712) was checked and disproved: adding all 705 veneer entries left the
  counter unchanged, and the mechanism confirms why — the counter only fires
  on `BL_suffix` halfwords, which a veneer's own bytes can never produce.
- **Measured after the veneer derivation: the 10,800-frame `campaign` track
  is now FULLY_STATIC.** Non-LTO build, 140,228,928 bytes, 2026-08-07 10:46,
  all runs strict-static/self-heal off/cache-load off. 5,400-frame `campaign`
  and no-input both remain FULLY_STATIC with every semantic fingerprint
  identical to the recorded baseline. 10,800-frame `campaign`:
  `self_heal_coverage=FULLY_STATIC dispatch_misses=0 ... trace_events=84942401`
  — the track previously aborted at `Func_a3ef0+0x7c` and now completes.
  Extending to 21,600 frames is **not** static: it aborts at THUMB
  `0x08094944`, trace event #96,264,422, an ordinary interior entry inside
  the clean `$t` run `[0x08094928, 0x080949a4)` of ELF-sized `Func_94820`.
  Being walked as of this writing.
