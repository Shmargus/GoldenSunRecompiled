# Release Checklist

Baseline releases are blocked until relevant items are satisfied.

## Provenance

- [ ] Source commit tagged.
- [ ] gbarecomp commit pinned and license bundled.
- [ ] Other third-party licenses/attributions bundled.
- [ ] Supported ROM/BIOS hashes documented.
- [ ] Reproducible build instructions tested on clean Windows x86-64.

## Accuracy

- [ ] Acceptance scenario matrix passed.
- [ ] No unexplained major oracle divergence.
- [ ] Save compatibility passed both directions.
- [ ] Overlay replacement/stale-dispatch tests passed.
- [ ] Strict-static report shows zero fallback/healing.

## Distribution safety

- [ ] Package contains no ROM or BIOS.
- [ ] Package contains no extracted assets/box art.
- [ ] Generated artifacts reviewed for embedded protected data.
- [ ] First-run flow asks for user-owned inputs and hash-verifies them.
- [ ] Crash/log export excludes private memory by default.
- [ ] Unofficial/non-affiliation notice included.

## Product quality

- [ ] Keyboard/gamepad controls documented.
- [ ] Save/config locations documented.
- [ ] Clean uninstall/removal documented.
- [ ] Known issues and coverage status honest.
- [ ] Faithful baseline mode remains available and default.
