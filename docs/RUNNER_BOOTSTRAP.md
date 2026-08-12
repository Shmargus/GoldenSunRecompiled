# GS-009 Runner Bootstrap

## Status

The local, hash-gated `GoldenSunRecomp.exe` reaches and executes the first
cartridge instruction through the static generated corpus. The acceptance run
uses the real recompiled BIOS in LLE mode and enables upstream strict-static
policy: interpreter bridging, cache loading, and runtime healing are disabled.

The run then continues into early Golden Sun initialization and aborts loudly
at the first remaining static miss, THUMB PC `0x080047ae`. No fallback or
game-specific value patch was used. GS-009 is therefore complete; the miss is
the first GS-010/coverage investigation point.

GS-010 subsequently proved that this historical stop was a headless VBlank
resume seam, not missing decoded code. The instruction already existed inside
the `0x080047a4` host body; an evidence-backed `resume = true` alias now reaches
it. The current first architectural divergence is the `DISPSTAT` read at
`0x080030b8`. See `docs/ORACLE_HANDOFF_BASELINE.md`.

No ROM, BIOS, generated source, executable, save data, or trace is stored in the
public tree. All generated inputs and build products remain under ignored
`local/` and `build/` paths.

## Reset-vector correction

The initial GS-007 proposal used the decoded reset branch target
`0x080003c0` as `[program].entry_pc`. That generated the target body but omitted
the actual cartridge reset instruction at `0x08000000`. The GBA BIOS hands
control to `0x08000000`, so strict dispatch would have missed before the first
cartridge instruction.

`tools/build_main_toml.py` now keeps both facts separate:

- discovery entry and first cartridge instruction: `0x08000000`, ARM;
- hash-verified instruction at that address: unconditional ARM branch;
- decoded branch target: `0x080003c0`.

The generated dispatch table consequently contains static ARM entries for both
`0x08000000` and `0x080003c0`. The corrected TOML SHA-256 is:

`1f76fee49f0ae9059475fc63cd85ad6eadc57b5aa8b06de9642ad58ec534620a`

The corrected corpus emits 12,580 functions: 141 ARM and 12,439 THUMB. Its 34
`.cpp` files hash to
`5840606dbd69606cc99b1330340c9b083681616eb0b00a5780f90af611f586db`
when their raw bytes are fed to SHA-256 in lexical filename order.
The local patched `gba_recompile.exe` used for this generation hashes to
`f8d62e22ef07ba423942ef894864afc300020ea1ddc68e22ad1420702442fc6a`.

## Runner wiring

The public build still creates only the asset-free `gsr_bootstrap`. A new
`GSR_BUILD_LOCAL_RUNNER` option adds the private runner only when both a separate
`gbarecomp` checkout and a complete ignored generated directory are supplied.
The runner:

- links the generated cart shards, cart dispatch table, symbol map, recompiled
  BIOS, and pinned platform runtime;
- bakes in only the supported ROM SHA-1 and faithful 240x160 policy;
- accepts private paths through `--bios`, `--rom`, or an ignored `game.toml`;
- leaves BIOS HLE, widening, adaptive view, and audio shadows disabled;
- relies on the runtime's hard ROM and BIOS identity checks.

The local executable is 14,195,520 bytes with SHA-256:

`093698d05b06f0ed043afeca5559184347db76e686a380575ea770dba93325bf`

That hash is local build evidence, not a release artifact or public pin.

## BIOS identity prerequisite

The pinned runtime's asset picker warned on a BIOS hash mismatch, then
`run_game` passed an empty expected hash to `GbaBios::load_from_file`. This made
the final BIOS load size-only despite upstream's stated identity policy.

The local generic fix passes the configured BIOS SHA-1 into the authoritative
loader. A mismatch now stops execution. The canonical BIOS used by the
acceptance run reports SHA-1
`300c20df6731a33952ded8c436f7f186d25d3492` and size 16,384 bytes.
This one-line generic fix remains uncommitted pending upstream review.

## Strict-static acceptance evidence

The local runner reported:

- BIOS SHA-1 verified, 16,384 bytes;
- ROM SHA-1 verified, 8,388,608 bytes;
- game code `AGSE` and decoded header entry `0x080003c0`;
- BIOS backend LLE, real intro;
- strict static enabled;
- self-heal recompilation, cache loading, and interpreter bridge disabled.

The runtime trace then recorded, in order:

1. ARM exchange and static dispatch at `0x08000000` (`gf_start_vector`);
2. execution at `0x080003e0`, inside the body reached from the proven
   `0x080003c0` reset target;
3. subsequent static ARM/THUMB calls through early initialization;
4. the first strict-static miss at THUMB PC `0x080047ae`.

This proves the first cartridge instruction was selected from the static
generated table, not interpreted or healed.

## Reproduction

Use placeholders for private and separate-checkout paths:

```powershell
cmake -S . -B build/gs009 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DGSR_BUILD_LOCAL_RUNNER=ON `
  -DGBARECOMP_ROOT=<pinned-gbarecomp-checkout> `
  -DGSR_GENERATED_DIR=<ignored-generated-main-directory>
cmake --build build/gs009 --target GoldenSunRecomp

$env:GBARECOMP_STRICT_STATIC = '1'
$env:GBARECOMP_TRACE_ON_DISPATCH_MISS = '1'
build/gs009/GoldenSunRecomp.exe `
  --bios <private-canonical-bios> `
  --rom <private-supported-rom> `
  --frames 300 --no-window
```

## Remaining limitations

- `0x080047ae` is not a static dispatch entry and is the first current miss.
  Its function boundary/resume semantics must be proven before metadata changes.
- The corrected corpus and runner depend on the uncommitted GS-007 finder fix.
- The generic overlay registry and BIOS hard gate are also uncommitted upstream
  deltas; the public pin remains unchanged.
- The milestone build is headless because SDL2 was not discovered in this fresh
  local CMake configuration. Windowed presentation is not part of GS-009.
- No oracle synchronization claim is made yet; that is GS-010.
