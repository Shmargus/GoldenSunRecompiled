# Cheats

## Player walk/run speed

Status: implemented and opt-in. F1 -> Cheats -> Player -> Walking speed offers
Normal / 2x / 3x and persists as `PlayerWalkRunSpeedMultiplier=1|2|3`.
Normal is faithful; legacy `PlayerWalkRun2x=true` migrates to 2x.

Evidence: exact writes at `0x0800F33C`, `0x0800F32A`, `0x0800ECA0`, and
`0x0800EC8E` scale only the proven player speed limits before collision and
integration. Synchronized replays measured 2x walking/running deltas; the user
manually confirmed 3x. Gap-jump writers remain excluded.

Keep this feature default Normal. Re-test only through the root
`GoldenSunLauncher.exe` when changing movement code.
