# GS-010 Oracle Handoff Baseline

## Status

GS-010 is complete. The native runner and the independent mGBA oracle
synchronize exactly at the BIOS-to-cartridge handoff. A TCP-only fingerprint
comparison then identifies the first architectural divergence at THUMB PC
`0x080030ba`, caused by the preceding `LDRH` at `0x080030b8` reading `DISPSTAT`
(`0x04000004`) at different PPU phases.

No interpreter, JIT, cache healing, generated-code edit, guessed function
boundary, or game-value patch was used. Private reports and fingerprint files
remain under ignored `local/gs010/` paths.

## Inputs and synchronization

- ROM SHA-1: `5c4695205413df7db52b9a184815a07783999971`
- BIOS SHA-1: `300c20df6731a33952ded8c436f7f186d25d3492`
- base `gbarecomp`: `af51d0e48e847ae2f056fe01560fb9f087ea27f8`
- oracle: the pinned upstream mGBA 0.10.5 source built as
  `gbarecomp_oracle`
- native policy: strict-static, self-heal disabled, cache disabled, interpreter
  bridge aborting

`tools/compare_bios_handoff.py` launches both processes with the same real BIOS
and exact ROM. It advances native execution through the TCP breakpoint/step
seam and advances mGBA internally to the executing PC `0x08000000`. mGBA's
pipeline-visible R15 is normalized to the executing PC before comparison.

The oracle reached the handoff after 7,290,725 instructions. Native required
795 cooperative dispatch chunks because the recompiled BIOS yields at hardware
events. At the handoff, PC, CPSR, and R0-R14 were identical. In particular:

- PC `0x08000000`
- CPSR `0x0000001f`
- SP `0x03007f00`
- LR `0x08000000`

Full EWRAM, IWRAM, palette RAM, VRAM, and OAM were byte-identical. The diagnostic
IO-page views first differ at `0x04000010`: native exposes `0x00` while mGBA's
internal IO view exposes `0x24`. That address is BG0HOFS, whose stored debug
view is not an architectural readback checkpoint, so it is recorded but is not
used to declare CPU execution divergence.

## First measured resume boundary

The initial strict runner stopped at THUMB `0x080047ae`. Fingerprints proved
that native and mGBA were identical for all 132 cartridge instruction states
through `0x080047ac`. Inspection of the generated corpus then established that
`0x080047ae` was already decoded inside the function beginning at
`0x080047a4`; it was not missing code.

The `STM` at `0x080047ac` advances PC to `0x080047ae`, and its hardware tick
reaches VBlank. Headless/TCP execution yields before fingerprinting the next
instruction, unwinds the host function, and must re-enter at that interior PC.
The recompiler's reviewed mechanism for this case is an explicit
`resume = true` seed. `tools/build_main_toml.py` now emits that single
oracle-proven resume entry. Regeneration reports exactly one mid-function alias
rolled into one host, with the function count unchanged at 12,580.

The corrected dispatch table routes `0x080047ae` through a thin alias into the
`0x080047a4` host body's interior label. A second oracle run verifies that
`0x080047ae` executes with matching architectural state. This is a resume
correction, not a new function boundary.

## First architectural divergence

After the resume correction, native and mGBA match for 229 consecutive
cartridge instruction states. The first difference is:

- writer/event PC: THUMB `0x080030b8`
- instruction: `LDRH r3,[r5]`
- effective address: `r5 = 0x04000004` (`DISPSTAT`)
- comparison PC: `0x080030ba`, immediately after the read
- native result: `r3 = 0x00000001` (VBlank status)
- oracle result: `r3 = 0x00000002` (HBlank status)
- every other compared register and CPSR: equal

The cumulative cycle stamps at the comparison PC are 76,041,857 native and
76,035,266 oracle, a native lead of 6,591 cycles. The handoff skew was only
-14 cycles. This makes the current root-cause hypothesis a PPU/event timing
drift between handoff and the `DISPSTAT` read, not incorrect decoding of the
load or an invented game constant.

The corrected strict run later reaches an ARM dispatch miss at `0x03000000`,
the ELF-proven IWRAM code-copy destination. That later boundary is deliberately
not patched yet: the project must first localize and fix the earlier PPU phase
divergence.

## Local build evidence

The corrected TOML SHA-256 is:

`95037ca9c55dfb595b751eb47afb29c45d48be361d298741ac2f02809d9f88e6`

The 34 generated `.cpp` files hash to:

`40b1d5c5c7ec11c0ae7f6de3813aa4e850f19de944a56268c000ca22cf16e164`

The local corrected runner is 14,195,590 bytes with SHA-256:

`c1309e5b7e8f3c579510f147baa19e8ee80ca6fdb31c162ee36788e8a5425edf`

These are ignored local-build fingerprints, not release artifacts or upstream
pins.

Verification after the cumulative local changes passes 25 project Python unit
tests, the four built project CTest targets, all 15 upstream CTest targets, and
the public protected-asset audit.

## Oracle tooling delta

The local upstream oracle gained three bounded observation commands:

- `emu_run_to_pc`: run internally to an exact normalized executing PC;
- `emu_trace_to_pc`: capture pre-instruction architectural fingerprints to a
  bounded target;
- `emu_fp_save`: save those fingerprints through the TCP control path in the
  existing GFP1 format.

The project harness saves the native ring through TCP after every successful
cooperative dispatch chunk. This is necessary because a strict dispatch abort
does not run the normal shutdown-time fingerprint dump. Stale fingerprint files
are removed before launch, and both asset hashes are checked before either
process starts.

## Reproduction

Use placeholders for the private and separately built inputs:

```powershell
python tools/compare_bios_handoff.py `
  --native <local-GoldenSunRecomp.exe> `
  --oracle <local-gbarecomp_oracle.exe> `
  --bios <private-canonical-BIOS> `
  --rom <private-supported-ROM> `
  --output local/gs010/handoff_report.json
```

The detailed JSON, logs, and GFP1 files stay ignored. The public evidence above
contains no ROM, BIOS, extracted asset, or generated ROM bytes.

## Next hypothesis

Bracket the first PPU/cycle phase drift between `0x08000000` and
`0x080030b8`, using synchronized hardware-event anchors. Fix the earliest
generic scheduler, DMA, timer, BIOS, or instruction-cycle discrepancy and add a
regression before addressing the later IWRAM activation boundary.
