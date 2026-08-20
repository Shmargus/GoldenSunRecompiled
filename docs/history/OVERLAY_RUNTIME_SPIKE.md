# GS-008 Overlay Runtime Spike

## Status

The synthetic same-PC dispatch gate passes locally in the pinned `gbarecomp`
checkout with an uncommitted generic runtime addition. Two immutable translated
image definitions share one guest RAM PC and THUMB mode. Activating image B
evicts overlapping image A before dispatch; overwriting part of the active
range invalidates B and falls through to the existing dispatch-miss path.

This proves the reusable dispatch primitive only. It does not yet prove a
Golden Sun loader event, decompressed byte identity, or overlay lifetime. The
public `gbarecomp` pin remains unchanged until the generic addition is reviewed
upstream.

No ROM, BIOS, disassembly output, or generated game code is used by this test.

## Existing integration seam

The pinned runtime already exposes `g_runtime_ram_dispatch_hook`. For guest PCs
in RAM, `runtime_dispatch` calls this hook before consulting the address-only
static dispatch table. A hook return of zero preserves the normal static,
self-heal, and miss tiers.

GS-008 adds a game-thread-only `RamOverlayRegistry` behind that seam. It:

- registers immutable image identities, half-open runtime ranges, and sorted
  `(pc, mode, native function)` entries;
- rejects duplicate identities, invalid ranges, out-of-range or misaligned
  entries, duplicate keys, invalid modes, and null native functions;
- activates one identity while removing every active image with an overlapping
  runtime range;
- invalidates active identities touched by a memory-overwrite range while
  retaining their immutable definitions for a later verified reload;
- returns false for an unknown PC or mode so strict/static miss policy remains
  authoritative.

The generic local upstream files are:

- `src/runtime/ram_overlay_registry.h`
- `src/runtime/ram_overlay_registry.cpp`
- `tests/ram_overlay/registry_test.cpp`
- build wiring in `CMakeLists.txt`
- an expanded hook contract comment in `src/armv4t/runtime_arm.h`

The three new-file SHA-256 values are, respectively:

```text
0cdb7c04ae0062a01ddbbc3f6cbdb53bd729d69e3bc7e8212e0ff534a12e58ef
cef5cf3bb3e067b9cfe06edbf87957bb97fb0c048e40d86d75224449936abc6d
87240279f9c9169d1ad8d2d264de2471ed4907bba4138799880cfb8fe92e5a73
```

These hashes identify the local spike; they are not a substitute upstream pin.

## Automated proof

The test installs a registry trampoline in `g_runtime_ram_dispatch_hook` and
calls the real `runtime_dispatch` entry point at guest PC `0x02001000`:

1. Image A is activated; its translated function writes the A sentinel.
2. Image B, with the identical runtime PC and THUMB mode, is activated.
3. The registry reports only B active; dispatch writes the B sentinel and A's
   call count remains unchanged.
4. A two-byte overwrite inside B's declared range invalidates it.
5. Dispatch at the shared PC records the normal runtime miss and calls neither
   stale function.
6. Reactivating B but dispatching in ARM mode also records a miss, proving mode
   remains part of the key.

Run the focused and complete generic suites with:

```powershell
cmake --build <gbarecomp-build> --target ram_overlay_registry_tests
ctest --test-dir <gbarecomp-build> -R ram_overlay_registry_tests `
  --output-on-failure
ctest --test-dir <gbarecomp-build> --output-on-failure
```

Local result with MSYS2 GCC 16.1.0: the focused test passes and all 15 upstream
CTest targets pass.

## Golden Sun integration boundary

The later game-owned adapter must use the reviewed GS-005 overlay manifest to
register generated overlay tables, then activate an identity only after an
observed load/decompression event verifies the installed bytes. Every write or
DMA/decompression operation that can replace an executable runtime range must
invalidate it before indirect execution can resume.

That adapter belongs in this repository. The identity registry and dispatch
semantics belong in generic `gbarecomp`; they contain no Golden Sun addresses or
special cases.

## Remaining limitations

- No Golden Sun overlay corpus is generated or linked yet.
- Loader/decompressor call sites and activation timing are not synchronized
  against mGBA.
- Decompressed bytes are not yet matched to a manifest identity at runtime.
- The registry is intentionally game-thread-only; a future caller must not
  mutate it concurrently with dispatch.
- Strict-mode rejection is reached through the existing dispatch-miss policy;
  a Golden Sun integration test still must prove its configured behavior.
- The generic addition and the separate GS-007 finder fix both remain local
  upstream deltas pending review and a future exact pin update.
