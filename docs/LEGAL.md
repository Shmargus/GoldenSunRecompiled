# Legal and Distribution Boundaries

This document is operational guidance, not legal advice.

## Never redistribute game or BIOS data

Do not publish or commit:

- Golden Sun ROM images or patches containing substantial original bytes.
- GBA BIOS images.
- Extracted graphics, text, audio, maps, or other assets.
- Save states or memory dumps that contain substantial protected data.
- Pre-generated source/binaries that embed protected ROM data without a reviewed distribution design.

The application should request a user-owned, legally obtained ROM and BIOS and verify their hashes locally.

## Upstream licensing matters

At the time this starter was prepared:

- `mstan/gbarecomp` uses the PolyForm Noncommercial License 1.0.0. That restricts commercial use. An edited copy of it is committed in `gbarecomp/`, which that licence permits for noncommercial use as long as the licence text travels with the copy: keep `gbarecomp/LICENSE` and `gbarecomp/THIRD_PARTY_ATTRIBUTION.md` in place, and keep this project noncommercial.
- `gsret/goldensun` does not visibly provide a repository license in its root. Absence of a license is not permission to copy or redistribute its source.

Therefore:

- Treat `gbarecomp` as a noncommercial dependency unless separately licensed.
- Do not vendor or copy `gsret/goldensun` source into this repository.
- Prefer an importer that reads a separately cloned local checkout and emits only the minimal factual metadata needed.
- Before publishing imported symbol metadata or generated code, review whether its form and quantity are appropriate for distribution.

## Project license

The root `LICENSE` in this starter is intentionally conservative. Select a real license only after deciding:

- Whether the repository will distribute generated translated code.
- How the `gbarecomp` noncommercial terms affect the combined work.
- Whether imported metadata from the unlicensed disassembly will be committed.
- Which third-party UI/runtime components are included.

## Release design

A safer release model is:

1. Distribute only original project source and permitted dependencies.
2. On first run or via a local build step, ask for the user's ROM and BIOS.
3. Verify exact hashes.
4. Derive required private assets/generated code locally.
5. Cache outputs locally.
6. Never upload those private derived outputs automatically.

## Naming and trademarks

Golden Sun, Nintendo, Camelot, and related names/assets belong to their respective owners. Make clear that the project is unofficial and unaffiliated. Avoid official logos and box art in repository branding or release packages.

## Security and privacy

- Never log full ROM/BIOS contents.
- Avoid absolute private paths in committed reports.
- Redact save filenames/usernames from issue templates.
- Ensure crash bundles exclude memory regions likely to contain game data unless the user explicitly exports them privately for debugging.
