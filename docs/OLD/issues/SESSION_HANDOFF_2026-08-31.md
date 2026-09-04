# Session handoff — 2026-08-31

Checkpoints: `31cb78a` (startup + trace), `759af82` (widescreen terrain-only
margins). Branch `local/gsrecomp-checkpoint-2026-08-27`.

The `gbarecomp` submodule still has uncommitted work. It is intact on disk and
was deliberately left uncommitted — do not stash or reset it.

## Start here tomorrow

Retest widescreen in the three places that were wrong tonight (screenshots
taken 23:37–23:38):

1. Temple exterior — a column of repeating `A` glyphs in the right margin.
2. Cave interior — a large magenta field of repeating `E` glyphs, right and
   bottom margins.
3. Overworld — uniform teal crosshatch filling the margins.

Pass condition: every margin shows real terrain or black. Any glyphs, repeated
scenery, or crosshatch means it is still wrong.

Also confirm no regression in McCoy Palace and in rooms that already rendered
margins correctly (equal-scroll).

## Done and confirmed working

**Startup speed.** IWRAM-resident code (sound driver + IRQ handler) was being
recompiled at runtime on every launch. The AOT functions already existed; the
dirty-page check discarded them because the game's own DMA copy marks the page
dirty, and a co-located self-modifying slot keeps that page tracked. The runtime
now confirms content identity and defers to the static dispatch table. Session
`20260831_213629` logged `9 transient-RAM range(s) registered`. User confirmed
the game loads and runs at full speed.

**Debug symbols split.** Post-build `objcopy` in `CMakeLists.txt` (guarded to
Debug/RelWithDebInfo). Exe 883 MB → 205 MB, symbols in
`GoldenSunRecomp.exe.debug` beside it. Keep that file next to the exe or crash
and hang traces stop resolving. This does **not** speed up builds — the split
happens after linking.

## Widescreen — built, not visually verified

Commit `759af82`. Margins are now drawn from the terrain layer only.

- Terrain bg resolved live from DISPCNT via
  `golden_sun_field_terrain_bg` (`src/widescreen_policy.h:891`) — lowest enabled
  regular BG among BG1..BG3, not hardcoded.
- BG2/BG3 are refused for any margin coordinate in a scroll-mismatch scene
  (`src/runner_main.cpp:4800-4805`); the PPU renders them transparent.
- Removed: `golden_sun_signed_mod256`, `GoldenSunGenericSplitScrollFrame`,
  `golden_sun_reconstruct_generic_split_scroll`, and the override params on
  `golden_sun_field_tilemap_entry`. Those guessed per-layer parallax offsets and
  were the source of the leaked font/menu tiles.
- Palace's exact-fingerprint path and the equal-scroll path are untouched.
- Existing sentinel checks (`0xffff`, `0x017`/`0xF200` fill, `0x01a` leak tile)
  apply to this path automatically.

Expected trade-off: rooms with a distant backdrop layer show black behind the
terrain in the margins rather than sky. Consistent, not garbage. Needs a
judgement call on whether that reads acceptably.

`TODO-EVIDENCE`: "the lowest enabled BG carries terrain" is only proven for the
rooms we have measurements for. A room where a different layer carries terrain
would show margins that do not match the centre.

## Why the earlier approach failed

Worth not repeating. The generic path tried to extend *every* layer into the
margins at live-measured parallax offsets. The offsets were the guessed part,
and the guesses landed on unpopulated atlas cells — the same shape as the
documented Bilibin leak (`docs/features/WIDESCREEN.md:233-237`). Bilibin and
Palace work because their margin content is placed from a position known to be
correct. Drawing one trusted layer beats reconstructing three.

Evidence from the play session is in `wide_scroll_trace.csv` (repo root,
untracked, session `20260831_213629`): per-room layer offsets are stable mod 256
and settle within 0–2 frames of map load. Retained in case the per-layer
question is revisited.

## Open threads

- **NPC margin culling** — untouched all session, still the known open WIDE-01
  bug. See `WIDE-01_CULLING_HANDOFF.md` and `NEXT_TASK.md`.
- **Cycle parity, interpreted vs AOT** — never verified, and the startup change
  moves first-touch execution from interpreted to AOT. Matters for
  `scripts/gs-replay.ps1` and launcher `input_record` determinism. Treat old
  recordings as suspect until checked.
- **Build time** — unchanged tonight. Real levers are `-gsplit-dwarf` (keeps
  debug info out of the linker) and switching to LLD. The generated corpus
  dominates compile time; your own edits mostly pay link cost.
- **Hang watchdog false positive** — it reports a hang on the game's normal
  wait-for-scanline loop at boot (`0x080FA804`–`0x080FA808`, a legitimate
  frame sync waiting for VCOUNT 159). It only writes a dump, it does not stop
  the game, but the false alarm cost most of an investigation this session.
- `docs/features/WIDESCREEN.md` was not updated for the terrain-only change.

## Dead ends, do not revisit

- Loosening the equal-scroll atlas heuristic — causes the Bilibin font-tile
  leak, already regressed once.
- Merging `recomp_master_misses_AGSE.toml.frag` — stale, and coverage was never
  the problem.
- A static per-room parallax table in ROM — searched, not found; scroll MMIO is
  written through computed addresses so static search cannot resolve it.
- The data range at `0x080FA798`–`0x080FA816` is correctly classified as code.
  Not a misclassification.
