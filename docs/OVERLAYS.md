# Golden Sun Overlay Strategy

## Why this is the critical path

Golden Sun stores map-specific executable code in compressed overlays and loads it into RAM. Different overlays may occupy the same runtime address range at different times. A static recompiler cannot safely dispatch by runtime PC alone unless current overlay identity is represented.

## Questions to answer with evidence

For every overlay:

- Stable overlay ID.
- Compressed ROM range.
- Decompressed bytes/checksum.
- Decompressed size.
- Runtime destination.
- ARM/THUMB mapping regions.
- Function entries and boundaries.
- Loader/decompressor function.
- Activation event.
- Unload/overwrite event.
- Shared/common code relationship.
- Whether code relocations or absolute addresses require special handling.

## Manifest proposal

```json
{
  "schema": 1,
  "rom_sha1": "5c4695205413df7db52b9a184815a07783999971",
  "overlays": [
    {
      "id": "TODO-EVIDENCE",
      "source": {
        "rom_start": "TODO-EVIDENCE",
        "compressed_size": "TODO-EVIDENCE",
        "decompressed_sha1": "TODO-EVIDENCE"
      },
      "runtime": {
        "start": "TODO-EVIDENCE",
        "size": "TODO-EVIDENCE"
      },
      "symbols": "overlays/TODO.tsv"
    }
  ]
}
```

Do not commit a filled example using guessed Golden Sun addresses.

The version-1 machine-readable schema is
`symbols/overlay-manifest-schema-v1.json`. The local GS-005 importer verifies
each compressed range against the exact ROM and partitions function metadata by
`overlay_id`; see `docs/history/OVERLAY_IMPORT_BASELINE.md`. This inventory does not
claim to prove loader activation or lifetime events.

## Runtime requirements

1. Observe or intercept the generic memory/load event that installs overlay code.
2. Verify the loaded/decompressed bytes or identity.
3. Register the matching translated section.
4. Invalidate any translated section overwritten at that runtime range.
5. Resolve indirect dispatch using active section identity.
6. Log unknown/ambiguous activation loudly.

## Synthetic proof before game integration

Build a test with two tiny synthetic ARM/THUMB binaries that both map a function at the same runtime address but return different values. Activate A, call, activate B, call, and prove stale A cannot be selected.

This test should live in the generic core if the capability is reusable; Golden Sun's manifest parser remains in this repository.

GS-008 now passes this gate locally through the pinned runtime's
`g_runtime_ram_dispatch_hook` and a generic identity-aware registry. Activation
of an overlapping image evicts the old identity, a range overwrite invalidates
the active image, and mode mismatches fall through to the existing loud miss
path. See `docs/history/OVERLAY_RUNTIME_SPIKE.md` for the automated evidence and the
remaining Golden Sun integration boundary. The generic change is still an
uncommitted local upstream delta, not part of the public pin.

## Acceptance evidence

- Overlay load trace aligned with the oracle.
- Memory checksum after decompression matches expected local build output.
- Dispatch table reports active overlay ID.
- Replacement at the same RAM range invalidates previous entries.
- Strict mode rejects execution from unregistered bytes.
