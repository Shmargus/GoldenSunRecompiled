# Verified facts

Measured findings and their evidence. **Read this before investigating
anything** — it exists so nobody re-derives what is already known.

Only measured or directly observed things belong here. Never record a guess.
Add the date and how it was established.

## Map and room data

- **The room pointer is useless as an identity.** The value at `0x03001E70` is
  always `0x02030CCC`. Measured across a 55-minute session, 2026-09-04.
- **The room bounds beside it do track rooms.** The four bounds values took 23
  distinct combinations across that session, changing as the player moved
  between rooms. Use the bounds tuple, not the pointer.
- **Field map table layout, recovered 2026-09-04** from the archived widescreen
  work (`src/widescreen_policy.h:905-908`, `docs/OLD/WIDE-MARGIN-ATTEMPTS.md`):
  `0x02010000` is a 128x128 grid of u32 metatile IDs, low 12 bits significant —
  16,384 cells, 64 KB. `0x02020000` is a 4,096-entry atlas, each entry four u16
  8x8 tile entries forming a 2x2 metatile — 32 KB. **All BG layers read that
  one shared id grid** — confirmed 2026-09-05 — but ~~per cell, so layer
  choice cannot relocate content to different cells~~ **not at the same cell:
  each layer reads a different region of the grid, offset by its own scroll
  offset, so relocating a layer's content is exactly what that offset does.**
  See "Per-layer sourcing" below.
- **The map table is never cleared between rooms.** Measured across 15 rooms:
  essentially all 16,384 cells hold a nonzero id in every room (1 empty cell in
  two of them). The game loads a room into its rectangle and leaves the rest
  exactly as the previous room left it.
- **Everything outside the room rect is residue, not fill.** Present from the
  first frame, never changes (arrival-vs-final diff zero written, zero cleared),
  and it is not the current room in another form: exact and x-shifted metatile-id
  comparison against the in-rect region returned 0.0000 match at every offset in
  two rooms, with numerically disjoint id sets. **No function paints it** — do
  not go looking for one.
- **The id table is written purely by CPU stores** (0 DMA bytes across 12
  sessions); only the raw tile table at `0x02020000` is DMA-filled.
- **Room bounds, extended.** `min_x`/`min_y` were 0 in every room observed.
  **Between rooms the struct reads all zeros** — a clean and reliable
  room-transition marker. (The extents once recorded here as 512x256,
  800x512, 752x496 came from the deleted recorder and did not survive the
  offsets being re-measured; see the recovered table below for what the
  fields actually hold.)
- **The room-load EWRAM capture recorded nothing, in two sessions and for two
  different reasons.** All 12 `roomload_*.bin` from session_20260905_084755
  and all 11 from session_20260905_092954 hold 0 of 8,192 lines set in every
  frame. First cause: `map_recorder_on_ewram_write` was fed only by
  `golden_sun_wide_ewram_write_observer`, installed only inside
  `install_golden_sun_widescreen`, which the runtime calls only when the view
  is expanded — and widescreen was removed on 2026-09-04. Second cause, which
  defeated the first fix: **`run_game()` clears both EWRAM observer slots on
  entry** (`gbarecomp/src/runtime/runtime.cpp:889-890`, so a game's hooks
  cannot leak into a later faithful run), which happens after `main()` and
  before any guest instruction. An observer installed from `main()` is wiped
  every time. The recorder now installs its own from its per-frame path,
  inside `run_game()`, and only into a slot nobody else holds — with that in
  place session_20260905_100233 recorded 362–4,835 lines per load and the
  intersection did its job. Captures taken before that fix are worthless for
  this purpose.
- **The bounds field offsets are recovered.** Measured 2026-09-05 from
  session_20260905_100233 (16 room loads; town exits and re-entries, shops,
  houses, an upstairs), then cross-checked against sessions 20260905_084802
  and 20260905_093001. Four `u16` fields:

  | address | field |
  |---|---|
  | `0x02030DC0` | `min_x` — 0 in every room observed |
  | `0x02030DC2` | `max_x` — room width in pixels |
  | `0x02030DC4` | `min_y` — 0 in every room observed |
  | `0x02030DC6` | `max_y` — room height in pixels |

  Found by intersecting the room-load EWRAM write bitmaps across all 16 loads
  (356 of 8,192 lines survived) and then keeping the fields that hold still
  within a room and change at a load. Room sizes seen: 272x240, 256x512,
  400x776, 496x464, 512x512, 528x272, 800x800. The all-zero
  between-rooms marker reproduces exactly — the rect blanks for a frame or
  two at every load, then takes the new room's values.
  The field *order* is confirmed independently by the camera clamp below:
  the second field pairs with the 240 px screen width, the fourth with the
  160 px height. `gsr_map_record_identity`
  (`src/runner_main.cpp:8523-8544`) can be wired to these.
- **On the world map the same struct reads `(0, 1, 8, 256)`**, constant, in
  all three sessions — 103 snapshots. Not a room rect; a usable "on the world
  map" signal, and the only value that breaks the `min_x`/`min_y == 0` rule.
- **The camera lives at `0x02030DB0` (x) and `0x02030DB4` (y), 16.16 fixed
  point**, in the same pixel space as the room rect. Its integer part stayed
  inside `[0, max - screen]` in **103 of 103** well-formed Mode 0 snapshots
  across three sessions, and sat exactly at that maximum in 49 of them. So
  bounds and camera *do* share a coordinate space — the assumption the three
  failed culling attempts made and never measured was right, and the cause of
  those failures is elsewhere.
- **Character data does not stream with the camera. Settled 2026-09-05**, and
  this retires the stated main risk to milestone 2. Across 12 room visits in
  session_20260905_100233, comparing the first and last snapshot of each visit
  with the screenblocks excluded from the character range: four visits (all
  the 512x512 town) changed **zero** character bytes, and the rest changed
  186–880 bytes in 12–45 distinct 8x8 tiles. Every visit drew from **the same
  46 tile slots**, 45 of which recur in three or more visits, clustered in one
  high-numbered band. That is a fixed animation window (water, torches), not
  streaming. So a room buffer can snapshot character data once per room load
  and re-read only that band.
- **The room rect is anchored at the grid origin, 16 px per cell.** Measured
  2026-09-05: decode the resident BG3 screenblock back to metatile ids through
  the atlas, locate an 8x4 block of them in the 128x128 grid, and the located
  cell fell inside `[0, max_x/16) x [0, max_y/16)` in **49 of 49** snapshots
  that resolved uniquely, across seven different rooms. Ring cell always
  equalled grid cell mod 16, as a 16-cell wrapped ring requires. So the bounds
  rect converts to grid cells by dividing by 16, with no offset — answering
  what the three culling attempts assumed about the *grid*.
- **The field map format is confirmed end to end. 2026-09-05**, session
  20260905_134222: rebuilding every visible cell of all three layers from the
  EWRAM tables and comparing byte-for-byte against live VRAM gives **BG3
  98.77%, BG2 98.40%, BG1 99.18%** over 59,245 frames and 15.1 million cell
  comparisons.
  The residual is the game's own streaming lag, not reconstruction error: the
  rate is **100% while the camera is still**, drops to 70-80% during
  movement, and climbs back as soon as the player stops — the game writes each
  new strip of ring as it scrolls into view, so for a frame or two VRAM is
  behind the tables the reconstruction reads. A prebuilt room buffer is
  therefore *ahead* of VRAM rather than wrong, and holds the whole room where
  VRAM only ever holds a 16x16 window.
  Getting here took three corrections, each found by the check rather than by
  reasoning: the ring window is anchored on the camera, not aligned to a
  multiple of 16; the per-layer grid regions must be taken from the writers'
  arguments, not derived; and a layer's ring position comes from its own grid
  cell, not the ground layer's.
- **The per-layer regions, observed directly. 2026-09-05**, session
  20260905_125143, `writer_args.csv` — 1,088 distinct argument sets captured
  from the six field tilemap writers, tagged by room.

  The main writer `gf_Func_fec8` (`0x0800FEC8`, 279 calls; its sibling
  `0x0800FF54` took 792) has this signature, read from its recompiled body:

  ```
  destination screenblock = 0x06002800 + (r0 << 11)   // r0 = 0,1,2 = the layer
  grid cell               = col (r1 >> 1) & 0x7F, row (r2 >> 1) & 0x7F
  entry within the ring   = ((r2 & 0x1E) * 32 + (r1 & 0x1E)) * 2
  ```

  `r1` and `r2` are tile (8 px) coordinates, so the grid cell is simply half
  of each. **The grid address does not depend on `r0` at all** — the layer
  selects only the destination. The layers differ because the *caller* passes
  a different position for each. Grid cells each slot was told to read:

  | room | slot 0 -> slot 1 | slot 0 -> slot 2 |
  |---|---|---|
  | 512x512 | +32 cols | +32 rows |
  | 800x512 | +32 rows | +50 cols |
  | 752x496 | +32 rows | +50 cols |
  | 400x776 | +60 cols | +60 rows |
  | 592x544 | +60 cols | +60 rows |
  | 800x800 | +60 cols | +60 rows |

  Constant within a room, different between rooms, and **the axis changes
  too** — some rooms stack the layers horizontally, others vertically. Only
  512x512 matches its own room dimensions (32x32 cells), which is why the
  scroll-derived rule looked right there and failed elsewhere. The values fall
  into a few layout classes ((32,32), (32 rows/50 cols), (60,60)) rather than
  a formula. **The practical consequence: a room buffer should take these
  offsets from the writer calls at runtime, not compute them.** They are
  stated by the game, once per room, in an argument.
- **Why no static per-layer offset rule fits: the region is an ARGUMENT, not
  a layout.** From the recompiled body of `gf_Func_10424`
  (`local/gs011/main/recompiled_023.cpp:11392`, instructions `0x08010432` to
  `0x0801044C`), the function takes four arguments and forms two grid
  pointers:

  ```
  source = 0x02010000 + (r1*128 + r0)*4      // a source cell (x=r0, y=r1)
  dest   = 0x02010000 + (r3*128 + r2)*4      // a dest cell   (x=r2, y=r3)
  ```

  then walks both, merging `source & 0xFFF` into the destination word's low 12
  bits (`0x080104B0`-`0x080104B8`) while writing the matching tiles to VRAM.
  So it **copies grid regions**, with both regions chosen by the caller. There
  is therefore no fixed offset to derive — which is exactly why the
  scroll-derived rule fit a 512x512 room and missed a 272x240 one, and why the
  fixed-quadrant theory failed.
- **The arguments are already capturable.** The function tracer records the
  entry argument registers, and existing 2026-09-04 sessions hold 13 distinct
  argument sets for `0x08010424` — e.g. source `(87,42)` -> dest `(41,12)`,
  source `(0,40)` -> dest `(43,66)`. Thirteen is too few to infer the scheme
  (the tracer keeps only the most recent args per capture window). Getting the
  per-layer regions exactly means capturing these arguments in volume, not
  searching for a rule in snapshot data.
- **CORRECTION, 2026-09-05.** The entry below was written as settled and is
  **not**. It was measured with an inference chain (reverse atlas lookup plus
  a guessed ring anchor) whose own noise floor is ~25%, so "72-96%" was read
  as confirmation when it was only consistency. An exact forward check —
  build the tile entries from the tables, compare byte-for-byte against live
  VRAM (`src/room_buffer.cpp`) — gives a different picture on the same
  captures: **BG3 92%, and 100% on clean single frames**, but BG2 60% and
  BG1 61% on the town captures, 64% and 89% on the dungeon ones. So the
  shared-grid finding and BG3's rule hold; the *region offset rule for BG1
  and BG2 is approximate, not exact*. In a 512x512 room the scroll-derived
  offset (+32,0 and 0,+32) is exactly right and scores 100%; in a 272x240
  room the scroll gives +60 cells while a direct search prefers +64, and even
  that reaches only 91% / 70%. A fixed-quadrant layout (BG2 at +64,0, BG1 at
  0,+64) was tested and is worse (BG1 falls to 9% in the dungeon), so it is
  ruled out. **The exact rule is open.** Do not build on the offsets below
  without re-measuring.
- **Per-layer sourcing (approximate — see the correction above). All three
  layers read the one grid; each reads a different REGION of it, and the
  region offset is close to the layer's own scroll offset from the camera.**

  `layer_cell = (camera_cell + (BGxHOFS - camera_x)/16,
                 camera_cell + (BGxVOFS - camera_y)/16)`

  taking the scroll registers **unmasked** (the game writes values above the
  hardware's 9 bits; the excess is the region offset, which the hardware
  discards but the game means). Match against the `0x02010000` grid's low 12
  bits, per room:

  | room | BG3 | BG2 | BG1 |
  |---|---|---|---|
  | 256x512 | 77% | 81% | 86% |
  | 272x240 | 75% | 76% | 72% |
  | 400x776 | 87% | 96% | 94% |
  | 512x512 | 69% | 78% | 79% |
  | 528x272 | 74% | 86% | 88% |
  | 800x800 | 77% | 81% | 89% |

  BG1 and BG2 score at or above BG3, against **19%** and **18%** when their
  cells were read at BG3's offset. Independently, a blind scan of all 128x128
  offsets ranked `(+32, 0)` first for BG2 and `(0, +32)` for BG1 — exactly the
  512 px scroll offsets those layers carry. **The ~20% residual was read as
  this method's noise floor. That was wrong** — the exact check shows BG3
  reaching 100% where BG1/BG2 do not, so the residual is partly real error in
  the rule. See the correction above.
- **Confirmed from the ROM, not only statistically.** The writer at
  `0x0800FEC8` (store PCs `0x0800FF1E` / `0x0800FF26`) has the literal pool
  `0x06002800`, `0x02010000`, `0x02020000`, `0x02020004` — destination, grid,
  atlas, and atlas+4 for the metatile's second tile row. The recompiled body
  of `gf_Func_10424` (`local/gs011/main/recompiled_023.cpp:11392`) gives the
  addressing exactly: `destination = 0x06002800 + (row*32 + col)*4` with the
  atlas at `id*8`, and `row` running past 15 into the next screenblocks —
  which is how one loop fills all three layers. It also **writes** the grid:
  `word = (word & 0xFFFFF000) | (new_id & 0xFFF)`, so the low 12 bits are the
  id and the upper 20 bits are preserved payload.
- **All three field layers are written by one mechanism into one contiguous
  destination. Settled 2026-09-05**, session_20260905_104619, once the trace
  budgeted records per writer PC (10 distinct writers recorded, 4 each,
  against one pair taking all 512 before). A previously unseen writer
  `0x080104F4`/`0x08010500` wrote `0x060031A0`, `0x060031A4`, `0x06003220`,
  `0x06003224` — inside **BG2's** screenblock — while its literal pool
  (`0x08010540`) holds `0x02010000`, `0x02020000`, `0x02020004`, `0x03001E70`
  and the destination base `0x06002800` with `0x06002840` for the second tile
  row. `0x060031A0 - 0x06002800 = 0x9A0`, past the end of BG3's 2 KB block: the
  three field screenblocks are contiguous (`0x2800` BG3, `0x3000` BG2,
  `0x3800` BG1) and the writers address all 6 KB from that one base, choosing
  the layer by offset. **Six functions** share this literal signature —
  `0x0800FF48`, `0x0800FFF4`, `0x08010418`, `0x08010550`, `0x080106F4`,
  `0x080108B4`. So there is one grid, one atlas and one family of writers for
  the whole field, not a separate source per layer. This is the answer
  milestone 2 needed: no ROM literal names BG1's screenblock because none has
  to.
- **Towns, indoor rooms and dungeons are one mechanism. Settled 2026-09-05**
  with human-labelled captures — the only way to tell them apart, since the
  hardware cannot. Town/shops/houses/upstairs from session_20260905_100233
  against Goma Cave from session_20260905_111146
  (`manual_Goma_Cave_Entrance`, `manual_Goma_Cave`):

  | | town + indoor | dungeon |
  |---|---|---|
  | BG1/BG2/BG3 CNT | `0x0709` / `0x060A` / `0x0503` | identical |
  | room-bounds struct | works, 6 room sizes | works, 496x464 and 664x832 |
  | grid + atlas + per-layer offset rule | BG3 75%, BG2 81%, BG1 83% | BG3 61%, BG2 77%, BG1 89% |
  | regions written per room load | grid, `0x02030000`, `0x02028000`, `0x02000000`, atlas, `0x02038000` | same regions, same rank order |

  The BGxCNT signature is identical (DISPCNT varies only with the window
  effects on transition frames), the same bounds struct decodes both, and the
  same sourcing rule reconstructs both. BG3's lower dungeon score is expected
  rather than anomalous: that session's play was Move on pillars and Whirlwind
  on leaves, i.e. deliberate in-room tile edits. **One milestone-2
  implementation covers all three.** Caveat on strength: the dungeon side is
  2 room loads against 15, so the write-footprint row is a rank comparison,
  not a volume one.
- **Structure of the map tables, as measured.** One grid at `0x02010000`,
  64 KB, **fully rewritten at every room load** (all 2,048 of its 32-byte
  lines, in 7 of 7 loads) — so there are not three per-layer planes; that lead
  is dead. One atlas at `0x02020000` serving all three layers: of freshly
  written 2x2 blocks, 87% (BG3), 85% (BG2) and 99% (BG1) were found verbatim
  as 8-byte entries there. A second 32 KB table at `0x02028000` is about half
  rewritten at every load and its role is **unidentified** — it is not a
  per-layer id table indexed by the ground id (7-8%, i.e. noise).
- **Watch for overworld writers appearing in the Mode 0 stream.** In
  session_20260905_104619 the affine writers `0x0801110A`/`0x08011116`/
  `0x0801113A`/`0x08011146` were logged as `[vram-map-cpu]` with
  `dispcnt=0x1F40` but `bg2cnt=0xAA0E` / `bg3cnt=0xA80A` — the overworld's
  BGCNT values still loaded during a mode transition. Those records are the
  overworld writer caught mid-transition, not field writers; check BGCNT, not
  just the mode bits, before attributing a record to the field.
- **The three field layers do not scroll together.** BG3's scroll equals the
  camera exactly; BG2's horizontal and BG1's vertical are offset from it by a
  **room-dependent** 512 or 960 px. Any per-cell reconstruction must take each
  layer's ring alignment from its own scroll register — ignoring this was what
  first made the layers look like they held unrelated data.
- **An EWRAM->VRAM staging route exists but is not a live mirror.** The table
  at ROM `0x08010FB0` pairs EWRAM `0x02038000`/`0x0203A000`/`0x0203C000`/
  `0x0203E000` with VRAM `0x06008000`/`0x0600A000`/`0x0600C000`/`0x0600E000`
  (character data) and `0x02028000` with the screenblocks, alongside DMA
  control words. Tested against snapshots: byte agreement between those EWRAM
  and VRAM ranges was only 4-28%, so the EWRAM copy is a load-time staging
  buffer, **not** a maintained shadow that a room buffer could read instead.
- **Three room-bounds culling attempts failed identically**, each collapsing the
  view to a wrongly-placed box with the garbage still present. All three assumed
  the position the margin path resolves and the room rect share a coordinate
  space. **That assumption has never been measured.** Bad bounds is a dead
  hypothesis — the room that failed reported a sane 0,0 800x512.
- Camera clamp function `Func_10230`, scroll shadow writer `tfunc_080101CA`.
  Two confirmed landmarks.
- BG layer roles, from layer-toggle observation: BG3 ground, BG2 tiles, BG1
  props, BG0 lighting. Not yet confirmed against the map data itself.

- **Event tile edits touch the ID grid only, and are tiny.** A dungeon hole
  appearing changed **12 of 16,384 metatile cells** — a 4x3 block plus a
  separate pair — and **zero** entries in the `0x02020000` atlas. Measured
  2026-09-04 by diffing two labelled snapshots in one room
  (`manual_Before_hole` frame 211581 vs `manual_Hole_appeared` frame 212951).
  Rebuild-on-write is therefore cheap and can be incremental.
- **Room load rewrites everything; events patch a few cells.** Crossing a portal
  into Inner Sol Sanctum rewrote **all 16,384** metatile cells and 2,612 atlas
  u16 entries, against 12 cells and zero atlas entries for the in-room hole
  event. Measured 2026-09-04. So a room buffer wants a full rebuild on room
  load and an incremental patch on event edits — two tiers, not one.
- **The room preloads its whole tile inventory, including states not yet
  shown.** Every one of the 12 new metatile IDs already had a fully populated
  atlas entry in the *before* snapshot, and **BG character data did not change
  at all** across the event (VRAM blocks 16-31, char base `0x8000`, byte
  identical). Only the scroll ring screenblocks (blocks 5/6/7 = BG3/BG2/BG1) and
  OBJ character data moved. So within a room the BG tileset is static while the
  tilemap ring streams. One room, one event — not yet generalised, but it is
  direct evidence on the char-residency question that is the main risk to
  milestone 2.
- **The upper bits of the metatile word are not unused.** Two cells went
  `0x0000404C` -> `0x00FF40AB`, i.e. bits 16-23 changed from 0 to 0xFF. An
  archived note claiming the 32-bit word has 20 unused upper bits is wrong, or
  at least incomplete.
- **Field register signature is identical across towns, dungeons and indoor
  rooms.** 101 snapshots, 2026-09-04: Mode 0, `DISPCNT=0x1F40`, char bases
  0/0x8000/0x8000/0, screen bases BG0=4, BG3=5, BG2=6, BG1=7, all 256x256, 4bpp,
  no wrap. The only variation anywhere is BG1 priority (1 or 2) in some rooms.
  Labelled Town, Indoor and Dungeon captures all land in the same signature
  group. The screen-base assignment independently confirms the archived
  measurement.

## Scene classes

Recovered 2026-09-04 from `docs/OLD/WIDESCREEN.md`, measured from savestates.
Towns, dungeons and indoor rooms are all Mode 0 field — **hardware cannot tell
them apart**, so distinguishing them needs a human label on the capture.

- **Field (towns/dungeons/indoors), Mode 0.** BG1/BG2/BG3 are 256x256 regular
  text layers held as a resident ring in VRAM. Screen-base blocks are BG0=4,
  BG3=5, BG2=6, BG1=7, packed with zero gap — there is no free screenblock. The
  ring is only ~1-2 tiles larger than the 30-tile viewport, so accumulating a
  world map by observing VRAM writes can never yield more than ~8-16px of
  margin however it is implemented. The real map is the EWRAM tables above.
- **Overworld, Mode 2.** Affine BG2 and BG3, both 512x512 with wrap enabled.
  Maps substantially populated: BG2 2,877/4,096 nonzero entries, BG3 2,865/4,096.
  512x512 pixels is 64x64 tiles, and affine map entries are **1-byte tile
  indices** (no flip bits, no palette bank, 256-colour), so each layer's whole
  map is 4 KB and its tileset is capped by the format at 256 tiles / 16 KB of
  character data. Sampling code: `gbarecomp/src/gba/gba_ppu.cpp:809-841`.
- **Overworld config confirmed in live play** (not just from a savestate),
  2026-09-04, 8-minute session: Mode 2, `DISPCNT=0x1F42`, BG3 map at VRAM
  `0x4000` with char base `0x8000`, BG2 map at `0x5000` with char base
  `0xC000`, both size code 2 (512x512) with wrap enabled.
- **The overworld animates its affine character data continuously.** Across
  1,064 normal-scale overworld frames, BG2 char blocks changed on 177 frames and
  further char blocks on 211, while the two affine **tilemaps never changed
  once**. Whatever the world's extent, its tile *graphics* are live.
- **Battle is Mode 1 with a 128x128 affine BG2** in 98 of 103 snapshots (two at
  256x256, one at 512x512 during a transition). Confirms the archived reading
  that this is a repeating effect surface, not stored arena geometry.
- **The overworld affine tilemap is a streaming ring. Settled 2026-09-04.**
  Across 913 normal-scale overworld frames covering 491 px of camera travel in
  x and 174 px in y, the affine map blocks were written on 14 frames. The writes
  track camera motion: during one stretch `bg2x` held constant at 2736.4 while
  `bg2y` moved 589 -> 597 -> 606 -> 654, with a map write at each step —
  consecutive vertical gaps of 8.3, 8.7, 9.6, 6.6, 8.9 and 6.4 px, i.e. **one
  tile row written per 8 px of camera movement**. So the world is larger than
  512x512 and the game streams rows into the wrapped map exactly as the Mode 0
  field streams its text-BG ring. The overworld therefore needs a pre-rendered
  source buffer, not just a widened clamp.
- **The overworld ring is not a copy of anything findable in a snapshot.**
  Ruled out 2026-09-04 against a freshly streamed row: verbatim byte match in
  EWRAM, IWRAM and the 8 MB ROM (SHA-1 verified); constant-offset match via a
  delta-sequence search over all three; and a clean 2x2 metatile expansion (best
  of four phase alignments was only 43%% horizontal / 39%% vertical). The source
  is compressed or reached through an indirection, and snapshots record writes
  rather than reads, so this cannot be settled from capture data alone.
- **`gba_vram_trace` was widened to Mode 2 affine BG2/BG3** and produced its
  first records on 2026-09-05 (`[vram-affine-cpu]`). It previously gated
  everything on `mode0_field(io)`, so no overworld write was visible.
- **The overworld is built from the same two EWRAM tables as the field.**
  Settled 2026-09-05, session_20260905_092954. Every affine map cell written
  between two consecutive overworld snapshots was found verbatim as a 4-byte
  entry in that snapshot's `0x02010000` table — 67/67, 61/61, 1/1, 66/66,
  60/60, 512/512 and 435/435 across four snapshot pairs on both layers, no
  misses. The ids selecting those entries were then found as a contiguous run
  at `0x02020000`, at offset 0 for map metatile row 0. So the chain is:
  **ids at `0x02020000`, one every 4 bytes, low halfword significant ->
  4-byte entry at `0x02010000` -> a 2x2 block of 1-byte affine tile indices**.
  This answers the roadmap's "is the overworld source findable" question: one
  room-buffer implementation can serve both scene classes.
- **The two tables genuinely swap roles by scene class. Nothing is
  mislabelled.** Same test applied to Mode 0 field, 2026-09-05, 38 snapshot
  pairs inside an unchanged room: of 3,302 freshly written 2x2 tile blocks,
  **2,949 were found as 8-byte entries at `0x02020000` and 0 at
  `0x02010000`** (the shortfall is blocks half-written across the pair). So:

  | | id table | entry table |
  |---|---|---|
  | Mode 0 field | `0x02010000`, `u32` per cell | `0x02020000`, 8 bytes = 2x2 text entries |
  | Mode 2 overworld | `0x02020000`, 4 bytes per cell, low halfword | `0x02010000`, 4 bytes = 2x2 tile indices |

  Both regions are 64 KB and each serves either role. **A room buffer must
  therefore pick the table by scene class and cannot hardcode either
  address.**
- **The overworld writer is three ROM loops**, from `[vram-affine-cpu]`
  records spanning the whole of session_20260905_092954 (4,992 records,
  cycles 36M–3.63B, same 8 store PCs throughout, zero DMA — the affine map is
  CPU-stored). Store PCs `0x0801118C`/`0x08011198` in the function at
  `0x08011164`; `0x0801110A`/`0x08011116` and `0x0801113A`/`0x08011146`
  sharing the literal pool at `0x08011158`; `0x08010996`/`0x080109A4`, whose
  pool at `0x080109CC` also holds `0x03001E70` and DMA3's `0x040000D4` /
  `0x84000010`. Two shapes: a row form (destination `+2`, 32 iterations) and
  a column form (destination `+0x80`, 64 iterations, running from BG3's map
  at `0x4000` straight into BG2's at `0x5000`).
- **Battle, Mode 1.** BG0/BG1 regular 256x256; affine BG2 is only 128x128 with
  wrap. That is a repeating effect surface, **not stored arena geometry** —
  widening it extends a pattern and cannot reveal more world. `BLDCNT=0x3F90`,
  so blending is active evidence rather than an edge case.

## Sprites, NPCs and shadows

- **Cutscene source-census limitation verified by source audit, 2026-09-09.**
  `note_golden_sun_obj_context_near_miss` searches accepted/rejected context
  tables, not raw EC-entry events. `golden_sun_obj_f0_entry_capture` populates
  those tables only when `read_golden_sun_obj_staging_attrs` succeeds; that
  reader rejects all addresses outside physical IWRAM, unaligned addresses,
  and records extending past IWRAM. The rejected table additionally requires
  sprite recording. Thus `context=none` does not prove EC was never entered or
  that B324/B328 never captured coordinates. The second-structure conclusion
  in the earlier write-up remains a hypothesis, not an established identity.
  Recount of `objrec_20260909_213849/lifetime.csv`: 1,817 `ok`, 1,363
  `context-frame`, 3,710 `context-no-match` commits; no `entry_*` columns exist,
  so that capture cannot recover a source discarded before context insertion.
  This blocks a justified placement change until the actual source is captured.

- **Unfiltered EC-to-F0 source diagnostic added, 2026-09-09; first run below.**
  With `GSR_OBJ_RECORD`, `runner_main.cpp` observes R6/R0 at the existing
  authenticated EC entry before actor admission. One pending observation per
  hardware OAM slot is bounded by `kGoldenSunOamShadowSlotCount`; its exact
  table destination, entry PC, frame, auth epoch, depth and return PC must match
  at F0. Every observed F0 commit consumes the observation. Source attributes
  are read only from aligned physical EWRAM/IWRAM records with room for the
  existing three-word LDM. Only ATTR identity words and existing staged
  coordinate metadata are retained, never arbitrary memory or asset payload.
  `ObjLifetimeSample.entry` appends 17 `entry_*` columns to the existing 22
  lifetime columns. States are `missing-entry`, `entry-context-mismatch`,
  `entry-unreadable`, `entry-attrs-mismatch`, and `matched-entry`; non-commit
  rows use `not-sampled`. Flags: 1=RAM attrs readable, 2=known actor shape,
  4=staging found, 8=X valid, 16=Y valid. `entry_stage_source` names the actual
  coordinate lookup (paired body for a known shadow); stage frame/epoch and
  signed X/Y are observations only. Neither the identity predicate nor any
  rendering/provenance decision consults these fields. Existing CSV flush
  cadence is reused, and the pending table resets with sprite provenance.
  Source/schema checks passed before the first gameplay result below.

- **The follow-up cutscene capture joins the missing sources to camera state,
  2026-09-10.** `session_20260910_104417` / `objrec_20260910_104424` produced
  `4,145` `object_camera.csv` rows, all from `matched-entry` EC observations:
  `1,568` Region A and `2,577` Region B. Every row still failed placement as
  `context-no-match` (`3,827`) or `context-frame` (`318`), through the same
  F0 writer `0x030038f0` and EC entry `0x030038ec`. The camera snapshot was
  `DISPCNT=0x1F40`, with BG0 scroll zero and BG1/BG2/BG3 carrying the same
  per-frame scroll; `808` of the `950` camera frames also contain successful
  actor-array commits under that identical scroll state (`1,862` successes
  alongside `3,830` EWRAM-source failures). This rules out a separate camera
  state as the sufficient cause of the clipping and strengthens missing
  source placement provenance as the blocker. Region B candidate fields
  `+0x0C`/`+0x14` were readable in `2,484` rows; their values change across
  the observed source rows but do not equal the committed OAM X/Y bytes
  directly, so their coordinate meaning remains unproven.

- **The bounded Region B trace reaches the producer and OAM handoff, but the
  candidate pair is not the final screen position, 2026-09-10.**
  `session_20260910_130502` / `objrec_20260910_130509` used the required ROM
  image (`5c4695205413df7db52b9a184815a07783999971`). Its boulder trace has
  `17,104` rows with `37` fields on every row: `7,682` `calc-entry` rows,
  `5,216` field stores (`2,608` each at `0x0809496E` and `0x08094970`), and
  `2,103` each of `ec-entry` and `f0-commit`. Every EC row joins exactly one
  F0 row by frame, source and target. Of the F0 rows, `2,068` have both
  measured candidate words readable and every one has a same-frame
  `calc-entry`; `2,037` retain the same pair through F0 and `31` have a
  measured field store between the entry and F0. All `2,103` Region B F0
  rows remain `source-unavailable` with lifetime `checks=0` (`1,789`
  `context-no-match`, `314` `context-frame`), despite `matched-entry` source
  observations. This proves the source and timing handoff is live while the
  placement provider still has no trusted coordinates.

  A representative source `0x02036B64` and target `0x0300355C` retain
  candidate words `0x0109EFE4`/`0x01D38995` from frames `9117..9121` while
  `DISPCNT=0x1F40`, BG0 is `0/0`, BG1 is `1096/300`, BG2 is `616/300`, and
  BG3 is `136/300` in every joined camera row. The committed OAM bytes move
  from `(161,71)` to `(129,135)` over those frames; F0 `R7` carries the
  corresponding packed ATTR1/ATTR0 values. Thus the two candidate words plus
  the recorded display scroll do not determine the committed screen
  position. The producer at `0x08094928` shifts the candidate words before
  calling `0x0800897C`, but this capture does not record that call's result or
  the state that builds the packed F0 `R7`. The remaining gap is therefore a
  correlated trace at that call/handoff to distinguish a separate cutscene
  camera value from object animation or another position source; no placement
  fix is evidenced yet.

- **The post-call `+0x10` handoff is active but constant zero, 2026-09-10.**
  `session_20260910_141441` / `objrec_20260910_141449` used the required ROM
  image (`5c4695205413df7db52b9a184815a07783999971`). Its boulder trace has
  `24,391` rows, including `3,272` `calc-result-write` rows from the measured
  `0x08094980` store after the `0x0809497C` call. Every result row has valid
  `+0x0C`/`+0x14` candidate fields, `R7` equal to the source base, and both
  captured `R0` and stored value equal to `0x00000000`; the rows cover `30`
  Region B sources over frames `8531..10275`. The one additional F0 source,
  `0x02036B84`, is the inclusive measured endpoint and is correctly excluded
  because its `+0x14` word would lie outside that measured span. This proves
  the new boundary is recording the producer's post-call value and rules out
  that value as the per-frame screen-position conversion.

  There are `32` exact source/frame joins between these result rows,
  `ec-entry`, `f0-commit`, and `object_camera.csv`; all `32` have distinct
  final raw OAM `(X,Y)` pairs despite the result remaining zero. For example,
  source `0x02036924` joins at frames `8710`, `8966`, `9334`, and `9430`, with
  final raw positions `(119,156)`, `(75,157)`, `(42,159)`, and `(165,154)`.
  Camera scroll differs between those joins, so this run does not isolate a
  separate cutscene camera from another object-position input. The remaining
  gap is the earlier source that builds the F0 packed attributes/coordinates;
  no placement fix is evidenced.

- **Current conclusion and limits, 2026-09-08.** The latest capture is
  `session_20260908_210056` / `objrec_20260908_210104`. It verifies the normal
  D4 paired-body correction: all `3,815` shadow entry tokens have matching
  `store-token-found` events, with `6` `handoff-body-stale` events and no
  `handoff-body-checks` events. The capture still contains a D8 bypass: the
  prior representative's shadow-shaped attributes occur at slot 8 in frame
  373104 with `store-token-missing`; the DMA carries provenance from frame
  373086 and render records `stale-frame`. Attributes do not identify an
  actor, so this is a bounded remaining path, not a global failure count. The
  user's visual improvement is consistent with the normal path being fixed,
  while a true D8 resume/bypass remains unclassified. Earlier sampled failures
  and the representative use the primary table, ruling out the alternate table
  as their general explanation, not ruling out its separate coverage gap.
  DMA 176 preserves a later matched commit, disproving unconditional upload
  loss, not proving every upload correct. The previously proposed RAM PCs
  `0x030001A4/0x030001A8` are a clear-fill path, not proven shadow writers.
  Do not bridge identities using slot number or aggregate placement percentages.


- **Targeted lifetime capture added and exercised**, 2026-09-08.
  `lifetime.csv` records ordered commits, DMA snapshots and render samples.
  Decimal columns; UINT64_MAX denotes an absent frame. Commit `checks` bits
  0..8: body attributes readable, dimensions valid, staging valid, X valid,
  Y valid, epoch matches, frame matches, coordinates match, identity matches.
  DMA/render bits 0..4: provenance valid, X valid, Y valid, attributes valid,
  commit-resolved. Render rows use the existing raw-Y 159..199 band and
  deduplicate identical slot/frame/epoch/DMA/reason/attribute observations.
  When `GSR_OBJ_RECORD` is enabled, active entries rejected by renderer
  provenance outside that band use the same deduplication path independently
  of `GBARECOMP_VRAM_MAP_TRACE`; aggregate WIDE diagnostics remain band-gated.
  DMA counts observed full uploads, not guest code generations, and is never
  used for placement. This filled the recording gap described below without
  changing gameplay; session `20260908_092127` exercised the correction and
  supplied the linked handoff evidence below.

- **Session `20260908_083414` shows a rendered shadow alias with stale primary
  provenance, but the lifetime capture has no render row for it.** Its matching
  `objrec_20260908_083421/lifetime.csv` has 663 full OAM uploads (613 primary,
  50 alternate) but zero commit rows. At render frame `373103`, the shadow
  signature in slot 11 was in primary upload 60 (frame `373102`; DMA
  `0x080036A4`, channel 3, source `0x0300347C`, destination `0x07000000`,
  1024 bytes), while the visible record remained from frame `373100`, had no
  commit-resolved bit, and failed the current ATTR identity. The session's
  matching alias sample reports raw Y 40 versus the stale logical Y -81 and
  `canonical-provenance-y-mismatch`; the same failure repeats for 18 frames. A
  preceding ROM-to-primary partial DMA (`0x0800C674`, source `0x08009BB8`,
  destination `0x0300347C`, 708 bytes, slots 0..88) is the only recorded table
  write before the transition, but the capture cannot prove the guest source
  value or writer. This confirms that missing/stale provenance drives the
  native-coordinate fallback; it does not yet justify a placement patch. The
  raw Y 40 sample is outside the lifetime render gate (159..199), so enabling
  VRAM map write trace alone cannot produce its missing render row. The
  recorder-only render hook now covers this out-of-band provenance failure;
  it was built and exercised in session `20260908_092127`.

- **The zero commit rows are an instrumentation gap, not proof that the guest
  made no writes.** Record sprite placement arms the normal OAM-shadow
  observer, but generated fast IWRAM stores call that observer only through
  `g_runtime_fast_iwram_write_observer`, which the runner installs only when
  `GBARECOMP_VRAM_MAP_TRACE` is enabled. DMA writes are intentionally excluded
  from the CPU committed callback and are recorded as DMA events instead. The
  session therefore cannot distinguish a fast-store miss, a DMA-filled entry,
  or a later authenticated F0/placement filter rejection. VRAM trace would
  close the fast-store writer gap where such a store occurs; it would not turn
  DMA into a commit or bypass the raw-Y render gate above.

- **Session `20260908_092127` correlates the remaining native-edge shadow loss
  to stale primary DMA provenance.** Its matching
  `logs/objrec_20260908_092134/lifetime.csv` records 603 full uploads from
  primary table `0x0300347C` and 76 from alternate table `0x03005AE0`. Shadow
  render rows joined to their upload contain 2,574 `stale-frame` primary
  cases, 173 `no-provenance` primary cases, 75 `no-provenance` alternate
  cases, and 236 accepted primary cases. All 2,771 commits are on the primary
  table; its shadow placement is 2,040 full-precision out of 2,046 commits.
  The alternate table is therefore a bounded secondary gap, not the dominant
  cause in this run.

  A representative failing slot is 14 at frame `373217`: the renderer reads
  `0x61C3/0x05FD/0x0800` from DMA 174, while visible provenance is from frame
  `373214`, expects `0x603D/0x0081/0x0800`, and has checks `31`, including the
  commit-resolved bit. The same slot's later F0 commit records a different
  identity, `0x6152/0x0067/0x0C00`, from staging `0x0300200C`, with body frame
  `373217` and checks `511`; DMA 175 carries another identity before that
  commit, and DMA 176 is the first upload carrying the later commit's
  attributes. DMA 176 publishes provenance from frame `373217` with matching
  expected attributes and checks `31`, so it does not lose provenance for that
  later identity. The transition sample for the failing slot reports raw Y
  `195..196` and canonical Y `-61..-60`. The provider rejects the stale
  identity, so the PPU falls back to the canonical byte and confines the
  negative row to the native rectangle. This explains the user's
  expanded-view clipping, but the capture does not prove that the later
  commit belongs to the failed `0x61C3/0x05FD/0x0800` sprite generation.

  The capture therefore proves a provenance mismatch at render time, not the
  actual earliest loss for the failing identity. Publishing a pending record
  across this frame/attribute mismatch would risk slot-recycling errors, so no
  gameplay patch is justified until the current failing commit and exact
  uploaded table can be synchronized safely. This moves the shadow milestone
  forward by proving the render-side loss and ruling out DMA 176 as the loss
  point for the later `0x6152/0x0067/0x0C00` identity; the producer or earlier
  handoff for the failing identity remains unproven.

- **The existing capture cannot identify the earliest writer for the failing
  identity.** Audit of `objrec_20260908_092134/lifetime.csv` finds
  `0x61C3/0x05FD/0x0800` only in the DMA-174 upload row; there is no matching
  commit row, and the later slot-14 commit has a different identity. The
  bounded commit-order trace also has no exact row for it. This distinguishes
  neither an unclassified CPU/fast-IWRAM store from a table-filling DMA, so no
  placement patch is justified.

- **A tracked writer observation was added, 2026-09-08.**
  `shadow_writes.csv` records every primary or alternate OAM-shadow table slot
  touched by a CPU/fast-IWRAM write or DMA descriptor. It includes frame/epoch,
  slot, writer PC, address/size or source/destination/control metadata, an
  ATTR0/1/2 word mask, and the identity attributes; it retains no guest
  payload. The CPU callback runs before the authenticated F0 filter, while
  the DMA callback runs before the copy, so the next walking capture can
  distinguish the two producer classes and correlate any slot identity with
  the ordered lifetime events. No gameplay behavior changed; a rebuild and
  rerun with VRAM map trace plus sprite recording are required before this
  observation has a result.

- **Session `20260908_152234` identifies the missing shadow handoff, 2026-09-08.**
  Matching capture `logs/objrec_20260908_152249` has `240,805` writer rows
  (`194,777` CPU and `46,028` DMA), `2,008` lifetime commits, `66,048` full
  DMA rows and `61,920` render rows. Among shadow-shaped final CPU writes,
  `1,872` rows from PC `0x030038D8` have no same-frame lifetime commit, while
  `1,634` rows from PC `0x030038F0` do. The uncommitted identities are still
  present in subsequent uploads: `1,965` such writer rows match an upload in
  the same or one of the next two frames. This rules out a lost table write.

  The representative current failure is slot `14`: frame `373209` writes
  `ATTR0/1/2=0x61BE/0x05FD/0x0800` at PC `0x030038D8`; the next full upload
  carries that identity, and render frame `373211` rejects it as
  `stale-frame`, retaining provenance from frame `373208`. The generated
  image shows `0x030038D8` is the `STM r0!,{r7,r8}` instruction immediately
  after the authenticated D4 entry at `0x030038D4`; `0x030038F0` is the
  separate F0 store. The captured build admitted only F0, so this D4-store
  path never enters the trusted handoff. Source audit found the normal-entry
  mismatch: D4 captures the actual pre-LDM source, then the shadow address
  body+0x0C is looked up directly even though staging provenance is keyed by
  the paired body base. The source now reuses the existing paired-body lookup;
  a generated resume path remains unmeasured.
  The source now admits only the verified D4 STM PC and carries the actual R6
  staging pointer captured immediately before the authenticated D4 LDM into a
  same-frame/current-epoch token. A missing or conflicting capture stays
  unclassified; OAM attributes are not
  used to guess a source identity. No manual gameplay result is available
  yet, so the correction still requires a rebuild and rerun. This moves the
  milestone forward by covering the first missing producer-to-provenance seam
  without loosening identity gates.

  This capture does not record a separate shadow handoff line for slot 14, but
  that absence does not prove the D4 token was missing: the normal shadow
  lookup returned after token capture at body+0x0C versus the paired body base.
  The source now maps the authenticated shadow source through that paired-body
  lookup. The next manual capture must establish whether any remaining D8
  store bypasses the normal entry path before a resume seam is considered.
- **D4 shadow correction source review, 2026-09-08.** Mapping the source to
  its body alone is insufficient: the output must retain the shadow's own
  offset. D4 now checks the paired body's current X/Y against its attributes
  and uses the same `golden_sun_obj_paired_coordinate` recovery as F0 for
  both axes. Handoff diagnostics report those resolved shadow coordinates;
  `handoff-body-checks` records rejected pairs. No build or gameplay result
  is available for this correction.
- **Session `20260908_173410` exercises the D4 correction and exposes the
  paired-body handoff mismatch, 2026-09-08.** Matching capture
  `logs/objrec_20260908_173418` has `2,303` primary commits (`1,925` full
  precision), `1,511` shadows (`1,505` full precision), and `389` commit
  frames. Among shadow-shaped final CPU writes, `1,332` rows from
  `0x030038D8` have no same-frame lifetime commit; every one has the same
  identity in a DMA in the same or one of the next two frames. The
  `0x030038F0` F0 writer has `1,418` shadow-shaped rows and `1,190`
  same-frame commits. This rules out the table or upload as the D4 loss in
  this capture.

  The representative new row is slot `5`, frame `373104`, with
  `ATTR0/1/2=0x6104/0x07C0/0x0800` at `0x030038D8`. The identity is published
  by DMA, but a later renderer row is `no-provenance`; authenticated D4
  handoff lines cover slots `0` through `4` and `11`, not slot `5`. Source
  audit explains that gap: D4 captures the shadow source before the lookup,
  then the old lookup searches for body+0x0C in a table keyed by the body
  base. The source now maps that source through `golden_sun_obj_record_identity`
  before the handoff, reusing the paired-body resolver. No gameplay result is
  claimed until the user reruns the matching capture.

  The generated `gf_resume_030038d4` path still consumes `g_runtime_resume_pc`
  and jumps to the D8 label before the function-entry hook. That remains a
  separate, unmeasured possibility; the session has no alias/resume event.
  The rerun must establish whether any D8 store bypasses the normal entry path
  before a resume seam is considered. Recorder-only `event=d4` rows now report
  entry/store token state, staging, body frame and paired-body rejection reason
  in `lifetime.csv`.

- **Session `20260908_210056` verifies normal D4 paired-body recovery but
  retains a D8 bypass, 2026-09-08.** Matching capture
  `logs/objrec_20260908_210104` has `5,096` primary commits (`4,453` full
  precision), including `3,568` shadows (`3,553` full precision). Its D4 rows
  contain `3,815` shadow `entry-token-found` events and the same `3,815`
  shadow `store-token-found` events, with `6` `handoff-body-stale` events and
  no `handoff-body-checks` events. The `2,414` `store-token-missing` rows have
  no source address and cannot be assigned to an actor from this capture.

  In the overlapping frames `373079..373493`, deduplicated shadow-signature
  renderer observations (`ATTR2=0x0800`, active `ATTR0`) changed from `133`
  `no-provenance`, `149` `accepted`, and `1,440` `stale-frame` in the prior
  capture to `8`, `449`, and `417`. These are renderer observations, not
  sprite counts. The prior representative attributes
  `ATTR0/1/2=0x6104/0x07C0/0x0800` occur at slot `8`, frame `373104`, in the
  new capture: its D4 event is `store-token-missing`, its DMA row is published
  with provenance frame `373086`, and its render row is `stale-frame`.
  Attribute equality does not establish actor identity. The capture therefore
  supports the user's visual improvement and confirms the normal D4 fix, while
  leaving a specific D8 resume/bypass path unclassified; it does not establish
  that all shadow failures are gone.

- **The current performance evidence identifies repeated work but no timing
  attribution**, 2026-09-08. Existing capture
  `logs/session_20260908_210056.log` reports `315,120` expanded scanlines and
  `246,179,658` room-buffer entries supplied. The same run reports `997,480`
  RAM revalidations, `997,481` RAM native calls, `474` RAM heals, and zero RAM
  CRC mismatches. Source review shows `room_buffer_supply` re-reads DISPCNT,
  camera, room rect and layer scroll state for each supplied sample; the
  expanded OBJ X and Y providers independently call the same provenance
  resolver for each OAM slot per rendered scanline. These are optimization
  candidates, not measured wall-time costs; recording/tracing was enabled in
  the capture, so a controlled default run with `GBARECOMP_COST_PROBE=1` is
  required before changing them. Native startup leaves the OBJ providers
  unset, so their duplicate lookup is paid only after an expanded-mode hook
  installation.


- **The previous recorder merged distinct placement failures.** Source audit,
  2026-09-08: `find_golden_sun_obj_f0_context` returns null for a frame
  change, no matching call/attributes, or duplicate matches. The resolver
  reports all of these as `SourceUnavailable`. `PlacementUnavailable` also
  merges missing body attributes, invalid dimensions, absent/stale body
  staging, and coordinate/identity mismatches. `ObjPlacementSample` records
  neither those individual rejection reasons nor body staging frame/epoch;
  the primary DMA publication copies pending provenance without recording a
  handoff generation. This confirms the capture gap in the roadmap, not a
  gameplay cause. A failing rendered slot needs these linked observations
  before a placement or lifetime change can be justified.
- **The body must not inherit the earlier component's cull.** Source audit
  after session `20260906_162111`: B27E's taken branch enters B2D8; its
  fall-through addresses `r7+0x0C`. B2F8-B320 then computes a separate body Y,
  tested at B328 and stored in the base record. Thus the observed parent
  225..222 versus body 199..196 is not one coordinate changing meaning: two
  components have separate positions. The native enhancement's
  `golden_sun_b328_parent_override` incorrectly required the earlier component
  to have been admitted. It now permits a consistently rejected parent too,
  retaining the same staging/frame/call identity checks and the body's exact
  160..199 band. No cutoff was increased. Existing policy tests now cover
  that retained-parent case, boundary 200, and stale frame rejection; they
  pass and the runner object compiles. Full build succeeded at 2026-09-06
  21:23:08 local; the user reported bodies looked correct in session 212401.
  Shadow margin clipping remains unresolved.
  This targets the measured guest-cull case; aggregate no-provenance renderer
  samples alone do not prove that every remaining disappearance has this cause.
- **The expanded compositor bypassed the recorded X coordinate.** Source
  inspection on 2026-09-06 found that `render_scanline_wide` assigns raw-derived
  `true_sx` and sets `expanded_placed` before `resolve_sx()` is called. That
  flag makes the later resolver return without consulting the X provider.
  The same block promotes both axes to trusted even when their providers
  supplied no position. This is a renderer defect separate from the recorded
  publication regression below; its contribution to the user's remaining
  gameplay symptom still needs visual confirmation.
- **The 2026-09-06 correction was rerun; jumping/wrapping no longer
  reproduced, but early disappearance remains.**
  `runner_main.cpp` now retains DMA-latched visible provenance, consults it
  before a same-frame pending candidate, and selects both axes together.
  Candidates require the primary source slot, current scene epoch, complete
  matching attributes, both valid axes and both matching truncated fields.
  Only the primary table's authenticated full OAM upload publishes pending
  records. The compositor resolves X before culling, preserves the supplied
  coordinates, uses doubled affine bounds, and confines unknown placements
  to the native rectangle instead of promoting wrapped coordinates to trusted.
  Camera shifts are applied once and culling uses shifted screen coordinates.
  Both modified C++ objects compile; the existing widescreen policy test and
  86 Python tests pass; the public repository audit passes. Those checks did
  not measure post-fix sprite coverage; the gameplay result below is from the
  first rerun of this executable.
  The normal `build/gs011_opt/GoldenSunRecomp.exe` full build succeeded at
  2026-09-06 16:18:47 local (153,101,331 bytes), with
  `MAKE=C:/msys64/mingw64/bin/mingw32-make.exe` and
  `cmake --build build/gs011_opt --target GoldenSunRecomp -j16`.
  ROM SHA-1 and the BIOS SHA-1 both matched their documented identities
  before linking. The user's `logs/session_20260906_162111.log` rerun reports
  no visible sprite/shadow jumping or wrapping, but sprites disappear too
  early while an NPC remains in the expanded view. Its
  `wide-obj-y-transition-summary` values are renderer/provider diagnostic
  samples, not sprite counts: `no-provenance/offscreen=24,231,840`,
  `stale-frame/native=3,360`, and `stale-frame/offscreen=2,274,720`
  (`2,278,080` stale-frame samples total). The
  `wide-obj-experimental-cull-summary` reports `logs=0 dropped=0`; that only
  shows the optional experimental cull path logged/dropped nothing and does
  not rule out guest culling.
  A bounded `wide-obj-y-cull-decision` sample had 44 decisions: 36 guest
  culls overridden and 8 retained (`original=1 final=1 overridden=0`). The
  retained samples at frames 373434--373437 are record `0x03002070`, with
  B27E operands `225..222` and B328 operands `199..196`; matching objrec
  data has body commits `0` but shadow commits `274` (`263` exact). This
  ties at least one early-disappearance path to a guest cull; the sample is
  bounded and is not a population count.
- **Follow-up session `20260906_212401`: bodies looked correct, but shadows
  still clipped at the native 240x160 bounds.** Matching recorder
  `logs/objrec_20260906_212409/summary.txt` reports `4,677` shadow commits,
  `4,658` full-precision (`99.59%`). The session completed `73` primary OAM
  DMA uploads; each reported `109/128` slots with `raw_y>=160`. Its bounded
  transition samples contain `61` no-provenance cases: `59` for slot 17
  (`0x03003504`), frames `374481..374552`, raw Y `161..194`, and one for
  slot 21 (`0x03003524`) at frame `374481`, raw Y `166`. These samples carry
  zero expected attributes, no provenance frame, and no selected target.
  **Correction from the final worker audit, 2026-09-07:** the observed PCs
  `0x030001A4/0x030001A8` belong to the fixed transient image
  `0x03000164..0x030001D8`, associated with `Func_8d4/8d8`
  (`config/usa/main.toml`, generated `gf_afunc_03000194`). The audited
  initialization zeroes R2..R9 before their stores through R0: this is a
  clear-fill path, not evidence of a shadow producer. The trace was not
  correlated to the failing frame/write generation. Do not hook those PCs
  or slots as shadow writers. Missing provider records explain the native
  clipping, but their actual producer/lifetime is still unproven. The
  proposed direct-writer fix was NOT implemented.
- **Exact ATTR0/1/2 equality is a consistency check, not a unique actor
  identity.** X values separated by 512 and Y values separated by 256 encode
  the same coordinates in OAM; unchanged remaining attributes cannot
  distinguish those positions. Direct consequence of the field widths,
  checked against the provider code on 2026-09-06. Claims below or in the
  roadmap that attribute equality alone makes pending records universally
  safe must not be used as a lifetime proof.
- **OAM cannot address a view taller than 193 rows, and no arithmetic fixes
  that.** The hardware stores a sprite's row in eight bits — 256 values. A
  view with `extra_top` above the native 160 rows and `extra_bottom` below it
  has to name every row from `-(extra_top + 63)` (a 64px sprite, the largest
  OBJ, hanging just into the top edge) to `159 + extra_bottom`, which is
  `223 + extra_top + extra_bottom` values. That fits in a byte only while
  `extra_top + extra_bottom <= 33`, i.e. a total height of 193. The shipped
  360x240 view uses 40 and 40, needing 303 — 47 rows over. Those 47 fold onto
  bytes 152..199, which are also real bottom-margin rows, so a sprite above
  the view and a sprite in the bottom margin become indistinguishable.
  Confirmed against the game: slot 23 sits at logical y=-101 and writes byte
  155, and byte 155 also means row 155 (`session_20260906_001314`). 2026-09-06.
  Consequence: any resolution-independent fix must take the position from the
  guest before truncation, not read it back from OAM.
- **The horizontal axis has the same wall, but is not against it.** OAM X is
  nine bits, 512 values. At 360 wide with the PPU's flat 64px object slack the
  view spans columns -124..363, 488 values. The ceiling is about 384 wide;
  beyond that X wraps exactly as Y does.
- **The guest's own full-precision sprite positions are already captured.**
  `record_golden_sun_obj_staging` (`src/runner_main.cpp`) records the X and Y
  the game computes before it truncates them into OAM, filed under the actor
  record the write came from. Coverage of that capture is unmeasured — see
  the recorder below.
 - **The measured actor-record layout starts at `0x03002000` with stride
   `0x38`.** The body's coordinates sit at `+0x00` and its paired shadow's at
   `+0x0C`. The original producer audit measured body bases `N=0..13`; the
   targeted 2026-09-09 capture then authenticated body `N=15` at `0x03002348`
   through the B324/B328 staging writes and the same-frame EC/F0 ATTR handoff.
   The array's true length has never been measured, and the audited prefix was
   never evidence of one -- see the entry below on why the address allowlist
   was removed.
- **The game keeps three sprite tables and rotates between them.** Every
  upload to hardware OAM is a 1024-byte DMA from `0x080036A4`, and its source
  is one of three addresses, not one. Measured over 256 uploads in
  `logs/session_20260906_105904.log` (30 seconds of town play):
  `0x0300347C` 162 times, `0x03005AE0` 84, `0x03002000` 10. The project
  watches only `0x0300347C` (`kGoldenSunOamShadowStart`), and the provenance
  publish (`golden_sun_obj_provenance_dma_handoff`) fires only when the DMA
  source equals that constant -- so on roughly a third of uploads the
  per-slot position table is never refreshed and still describes the previous
  table's occupants. 2026-09-06. Any object buffer has to follow whichever
  table is live, not a single fixed address.
- **`0x0300347C` is also wholesale-reset from ROM roughly once per frame.**
  506 DMA blits of 708 bytes (89 slots) from `0x08009BB8` via `0x0800C674` in
  the same 30 seconds. Whatever that table means, it is not a stable
  accumulation across frames. 2026-09-06.
- **Slot recycling is live and frequent, and it is what invalidates the
  captured positions.** In the same session, 64 `canonical-provenance-y-mismatch`
  events and 42 large accepted-Y jumps. Example: slot 3 held a sprite at
  logical row -22 (byte 234) on frame 373136 and a different sprite at row 90
  (byte 90) on frame 373137 -- same slot, same writer branch `0x0800B328`,
  same target address, but `attr2` changed from `0x09A4` to `0x0D04`, i.e.
  different artwork. Keying a position record by hardware slot cannot survive
  this. 2026-09-06.
- **The guest's captured positions cover 97.6% of the sprites that can
  visibly jump.** Measured with the sprite recorder across two sessions of
  town play. Placing rotation/scaling sprites (which Golden Sun uses for walk
  squash and shadows, and which the lookup used to refuse) took overall
  coverage from 46.48% to 90.81%, and coverage of sprites whose committed OAM
  Y byte falls in the ambiguous 152..199 band from 42.88% to 97.55% -- 716 of
  734, leaving 18. Shadows are 99.64%. `objrec_20260906_110334` (6,493
  sprites) versus `objrec_20260906_113654` (3,710 sprites), 2026-09-06. The
  data needed to stop the jumping is therefore already captured. Note that
  the shadow share of this figure only became usable once a shadow's own
  position could be derived from its body's -- see the next entry; as first
  measured, a shadow counted as "placed" when only its BODY's position was
  known, which would have drawn every shadow on top of its character.
- **A shadow's own position is recoverable from its body's, and the margin is
  enormous.** A shadow carries no coordinate of its own; the guest derives it
  from the body it belongs to and only the truncated result reaches OAM.
  Reading the difference between the two truncated fields as signed and
  adding it back recovers the shadow exactly, provided the pair sit closer
  together than half the field (128 rows, 256 columns). Measured in play:
  largest offset ever seen was 54 rows and 8 columns, with zero cases within
  reach of the limit (`objrec_20260906_131904`, `objrec_20260906_131954`).
  With this in place shadows resolve at 99.5-100%. 2026-09-06.
- **Bodies stopped jumping once the position table was written at commit time;
  shadows did not, and the reason is the publish, not the placement.** After
  the 2026-09-06 renderer change the user reported sprites loading and
  holding position correctly, with shadows still jumping and culling early at
  the margins. The table was still only made visible by the upload DMA from
  `0x0300347C`, so on the 31% of frames uploaded from `0x03005AE0` it was
  never refreshed -- measured 3 frames stale in
  `session_20260906_131954`. A stale record fails its own truncation check,
  which marks the sprite untrusted, and an untrusted sprite both falls back to
  the wrapping byte and is confined to the native 240x160 window. That single
  cause produces both reported symptoms. 2026-09-06.
- **The renderer's per-slot position table is badly stale, and that is the
  live failure.** `golden_sun_wide_obj_attr_y_provider` reads a table
  published per DMA handoff, and its dominant rejection is `stale-frame`:
  1,337,880 offscreen and 5,520 native events in `session_20260906_113648`.
  A representative sample: slot 14 rendering `attr0=0x61A1` while the stored
  record was `attr0=0x20D0` from frame 373154, consulted at frame 373195 --
  41 frames old and a different sprite. The commit-time lookup the recorder
  uses (`golden_sun_obj_resolve_placement`) has no such staleness because it
  resolves at the moment of the write. 2026-09-06.
- **There is a second sprite table at `0x03005AE0` that the project does not
  know about.** It appears nowhere in the source. It supplied 60 of 256 OAM
  uploads (23%) in `session_20260906_113648` and 84 of 256 in
  `session_20260906_105904`; the rest came from `0x0300347C`. Everything --
  the F0 commit observer's address range, the provenance publish, the
  recorder -- watches only `0x0300347C`, so roughly a quarter of the frames
  presented to the hardware were built somewhere we never looked. The 97.6%
  above is therefore 97.6% of what we can currently see. 2026-09-06.
- **The second sprite table receives nothing through the writer route, yet
  still supplies about a third of uploads.** After the F0 commit observer was
  widened to accept `0x03005AE0` as well as `0x0300347C`, the recorder's
  per-table count showed 6,349 commits into `0x0300347C` and **zero** into
  `0x03005AE0`, while the upload DMAs in the same session split 181/75
  (`objrec_20260906_134126` / `session_20260906_134119`, 1,882 frames).
  Whatever fills that table does not go through `Func_1dc8`'s store path, so
  widening the observer's address range cannot reach it. How it is filled is
  the single biggest unknown left in this area. 2026-09-06.
- **`0x03002000` is reused for several unrelated purposes at different
  times** -- the 0x38-stride actor-record array, a transient code image
  (`kTransientCodeImages`), and occasionally an OAM upload source (10 of 256
  in `session_20260906_105904`). Do not treat an address in that range as
  self-identifying. 2026-09-06.
- **Three writer routes commit sprites (`D4`, `EC`, `F0`); only `F0` sees
  every one.** `WIDE-01_NPC_IDENTITY.md`: "both are always committed via this
  same F0 route", bodies and shadows alike. `D4`, which is where the
  full-precision position is currently handed off to the per-slot table, fires
  only on a successful gated commit — a subset. This is the suspected cause of
  the fallbacks logged as `canonical-provenance-y-mismatch`, and is what the
  recorder measures. 2026-09-06.

- **Session `20260909_130314` measures the remaining expanded-view body gap,
  2026-09-09.** `logs/objrec_20260909_130315/summary.txt` reports `2,468`
  committed bodies but only `19` with full-precision placement (`0.77%`),
  while all `520` committed shadows have full-precision placement. The PPU
  source path (`gbarecomp/src/gba/gba_ppu.cpp`) confines a sprite to the native
  rectangle whenever either placement axis lacks a verified provider, so a
  body with missing provenance can appear only partly in an expanded margin.
  The matching function-tracer screenshot is
  `logs/trace_20260909_130315/overlay_cf5ae0.png`, for window `002` in
  `index.txt` (frames `8531..8879`). That associates the picture with a
  capture window, but there is no exact latched-frame-to-OAM-slot join or actor
  label. The recorder also reports zero call-site overflow, zero actor-record
  overflow and zero dropped unsourced rows, so this is missing producer
  identity rather than a full diagnostic buffer. The end of that window has
  three body-sized entries with negative raw Y and stale provenance; the
  renderer's native-row fallback would retain only their overlap with the
  native top edge, which is the measured mechanism for a partial top-margin
  sprite. The data still cannot identify which entry is Garet or justify an
  actor-specific patch. This moves the expanded-view diagnosis forward by
  measuring the body provenance gap and a scene-correlated clipping pattern;
  the producer path remains unclassified.

- **Marked capture `session_20260909_133629` joins the partial top sprite to
  the F0 writer path, but still cannot identify Garet, 2026-09-09.** The
  marked window is `002`, frames `8531..9045`, and its screenshot is
  `logs/trace_20260909_133629/garet_partial.png`. At frame `8531`, slot `9`
  has a body commit in `lifetime.csv` with
  `ATTR0/1/2=0x20EA/0x80BE/0x093C`, raw `(x,y)=(190,234)`, source table
  `0x0300347C`, `reason=context-frame`, and no staging or context identity.
  The same frame and slot in `shadow_writes.csv` has the final ATTR2 write at
  `0x030034C8` from CPU writer `0x030038F0` (the preceding ATTR0/1 write is at
  `0x030034C4`). `unsourced.csv` repeats this row with return PC
  `0x030038A4`, depth `4`; `sites.csv` aggregates that site as `1,097`
  writes, `588` paired-body placements and `509` source-unavailable writes,
  with the same slot and attributes in its sample.

  The matching runtime lines report `wide-obj-experimental-cull` for slot `9`
  with raw `(190,234)`, shape `0`, size `2`, `32x32` pixels, and
  `reason=source-unavailable`; the staging trace has the only matching
  candidate at `0x03002348` with logical `(190,-22)`. Before the follow-up fix,
  `golden_sun_obj_record_identity` ended its range at `0x030022E0`, so that
  pointer was rejected before F0 context capture. It is numerically aligned as
  `base + 15*0x38`, but that resemblance does not prove it is an actor record
  or that the actor is Garet. With no trusted provider, the PPU keeps only the
  native-row overlap of the negative-y sprite, which explains the captured
  partial top fragment. The next two frames show a different D4 writer
  (`0x030038D8`) for slot `9` with `store-token-missing`; that is a later
  slot occupant, not an actor join. The writer/source seam is therefore
  established, but actor identity remains unproven. The screenshot/OAM frame
  join is available from the existing evidence: `index.txt` ends window `002`
  at frame `9045`, the manual marker runs inside `HostWindow::present` after
  the PPU has latched its framebuffer and OAM snapshot, and `lifetime.csv`
  has `ATTR0/1/2=0x20EA/0x80BE/0x093C` in render frame `9045`, slot `9`, as
  `stale-frame` (its expected shadow identity is
  `0x6193/0x00EF/0x0800` from provenance frame `9035`). This identifies the
  latched frame and OAM row, not Garet or a per-pixel ownership label. No
  gameplay or recorder change is justified.
  Across the marked window, `lifetime.csv` has `1,968` non-shadow
  body commits and `515` shadow commits; every body is `context-frame` and
  every shadow is `paired-body-placement`, so the body/source gap persists in
  the marked scene. The next needed evidence is a trusted actor/record
  mapping and an actor label or other ownership evidence for that OAM row.

- **The accepted NPC path carries a different, authenticated provenance.** In
  the same capture, `lifetime.csv` records shadow `0x030021CC` (body base
  `0x030021C0`, slot index `8`) with `context_frame=body_frame` equal to the
  commit frame and `checks=511`; `records.csv` reports `588/588` full-precision
  shadows for that body base. The rejected slot-9 body has no F0 context,
  `staging=0`, `body_frame` absent and `checks=0`. Its pointer-like alignment
  and matching coordinates therefore do not provide the accepted path's
  same-frame body record, shadow pairing and attribute checks.

 - **The targeted follow-up authenticates the partial top sprite's source and
   fixes its bounded identity gap, 2026-09-09.** The actual diagnostic run is
   `session_20260909_154121` with matching recorder output
   `logs/objrec_20260909_154128` and marked window `002` in
   `logs/trace_20260909_154128/index.txt` (`8531..9082`), including the
   `garet_partial.png` screenshot. `lifetime.csv` contains `679`
   `commit,record-identity` rows, one for every frame `8531..9209`; every row
   has source table `0x0300347C`, staging `0x03002348`, and
   `context_frame == frame`. The initial marked OAM write is frame `8531`,
   slot `9`, `ATTR0/1/2=0x20EA/0x80BE/0x093C`; `shadow_writes.csv` records its
   `ATTR0/1` and `ATTR2` stores at `0x030034C4`/`0x030034C8` from F0 writer
   `0x030038F0`, while `unsourced.csv` supplies return PC `0x030038A4` and
   depth `4`. The matching `wide-obj-stage` lines show the same source at
   frame `8531` with logical `(190,-22)` and both branch PCs. This proves the
   same-frame EC/F0 source-to-writer join across the marked actor lifecycle;
   it is not an actor-name label.

   The screenshot window ends at frame `9082`. Its render row for slot `10`
   carries `ATTR0/1/2=0x20F1/0x80BD/0x093C` with stale expected identity from
   provenance frame `9073`; the same attributes were committed at frame
   `9080`, slot `10`, from staging `0x03002348`, then appeared in the frame
   `9081` DMA. The old range therefore left this measured body untrusted and
   caused the native-row fallback to retain only the top fragment. The source
   now preserves the original range and admits only the exact additional body
   pointer `0x03002348` (`N=15`), preserving the existing same-frame, epoch,
   ATTR and truncation checks. The `N=13` shadow `0x030022E4`, `N=14` body and
   shadow `0x03002310`/`0x0300231C`, and `N=15` shadow `0x03002354` remain
   rejected because they were not authenticated. This is source evidence only
   until the user performs the required manual rebuild and gameplay rerun.

- **The first post-identity-fix gameplay rerun still showed Garet clipped, and
  its log does not prove that the F0 placement seam ran, 2026-09-09.** The
  single root-launcher run is `logs/session_20260909_174751.log`: it loads the
  same savestate at frame `8531`, records `input_record=DISABLED`, reports
  Enhanced timing and the expanded room-buffer view, and ends after `765`
  presented frames. The child executable was
  `build/gs011_opt/GoldenSunRecomp.exe` (written `17:00:53`, before the
  `17:47:51` session); the root launcher was the current
  `GoldenSunLauncher.exe` (written `13:32:47`). The log has no object-recorder
  output, OAM-shadow rows, F0 writer rows or experimental-cull rows, although
  it does contain the bounded D4 Y diagnostics. The user reports that Garet
  remains half on screen.

  The missing rows alone do not identify the cause. Source and executable
  inspection found the earliest production exclusion: the runner armed the
  generated fast-IWRAM observer only when `GBARECOMP_VRAM_MAP_TRACE` was on,
  while the reusable committed OAM callback returned early unless OAM/VRAM
  print tracing was on. The measured F0 writer is generated fast-IWRAM at
  `0x030038F0`, so an Enhanced run with diagnostics and recording off could
  not deliver that commit to `golden_sun_obj_resolve_placement`, regardless
  of the now-admitted `0x03002348` identity. The source now arms that fast
  seam for Enhanced/recording, gates committed callbacks with the existing
  primary/alternate OAM-table predicate, and lets the payload-free callback
  run independently of print tracing. `run_game()` now clears both vram-trace
  observer slots and the range gate before each run. An Enhanced run now also
  emits one bounded `[wide-obj-hooks]` exit line with observer, F0, placement
  and identity counts, so the next normal run can distinguish a missing
  callback from a source-identity rejection. This moves the production gate
  forward; gameplay success remains unverified until a rebuild and user rerun.

- **The production seam is armed and the address allowlist was the remaining
  ceiling, 2026-09-09.** `logs/session_20260909_190746.log` is the first run on
  the rebuilt executable (written `19:04`, run `19:07`) and its
  `[wide-obj-hooks]` exit line reads `fast_iwram=armed oam_commit=armed
  observer_calls=169186 f0_commits=2702 f0_placements=2702 f0_identified=882`.
  Both hooks now fire and every F0 commit reaches
  `golden_sun_obj_resolve_placement`, so the production gate closed in the
  previous entry is confirmed fixed. `882` of `2,702` commits (`32.6%`) were
  traced to an actor record; the other `1,820` were refused before any
  provenance check. That is the ceiling the hand-authenticated allowlist
  imposes, and it is why one more measured actor pointer could not generalise.

- **The actor-record allowlist was removed in favour of the array's shape,
  2026-09-09.** `golden_sun_obj_record_identity` no longer carries an upper
  address bound or a per-actor exception. It admits a pointer that is
  `0x38`-strided from `0x03002000` with sub-offset `0x00` (body) or `0x0C`
  (shadow) and whose record lies inside IWRAM. Nothing about authentication
  changed: `golden_sun_obj_resolve_placement` still requires the record's
  staged `ATTR0/1/2` to equal the committed OAM attributes, its staged
  coordinates to truncate to the committed OAM bytes, and its staging entry to
  carry the same frame and auth epoch, so a merely stride-aligned pointer is
  still rejected. The staging position table itself never had an address
  filter -- `record_golden_sun_obj_staging` keys on the guest's own `R7` at
  B324/B328 -- so widening admission does not widen what is recorded, only
  what may be looked up. `[wide-obj-hooks]` gained `f0_considered` and
  `f0_placed`, which report how many committed sprites ended with a trusted
  full-precision position. Source change only; unverified until a rebuild and
  a user rerun of the prologue chest scene.


- **Removing the allowlist worked, and exposed the next wall: the per-frame
  context stamp, 2026-09-09.** `logs/objrec_20260909_203451` (boulder
  cutscene, `4,339` committed sprites over `788` frames) has zero
  `record-identity` rejections -- the identity predicate no longer refuses
  anything -- and `860` full-precision placements (`19.82%`). Every one of the
  `3,479` failures has reason `context-frame` with `checks=0`, and there is
  not a single `context-no-match` or `context-duplicate`. So the sprites are
  not failing an attribute or provenance test; they never reach one.

  The failure is ordering, not per-sprite coverage. Of the `788` frames, `647`
  contain both a success and a failure, and in **all 647** every failure
  precedes every success, with zero interleaving in either direction (`awk`
  over the commit rows' `sequence` within each frame). The same call sites
  appear on both sides of the split -- `0x030038f0`/return `0x030038b4` has
  `32` placements and `1,752` failures, return `0x030038a4` has `557` and
  `1,284` -- so no site is structurally unhooked. What separates them is when
  in the frame the commit happens: `reset_golden_sun_obj_f0_contexts_for_frame`
  wipes the context table at the frame's first staging entry, so every sprite
  committed before that point is looked up against the previous frame's stamp
  and refused, even though the matching record is still resident and has not
  yet been cleared.

  Record bases that did resolve: `0x03002348` (92 bodies), `0x03002310` (71
  bodies, newly reachable after the allowlist removal), `0x03002000` (48
  bodies, 584 shadows), `0x030021c0` (65 shadows). Staging pointers seen
  anywhere in the capture, including D4 rows: `0x03002000`, `0x0300200c`,
  `0x030021c0`, `0x030021cc`, `0x03002310`, `0x03002348`, `0x03002354`,
  `0x03002498` (`N=21`), and `0x03007e54`, which is not in the `0x38` array at
  all. Shadows remain `649/649` full precision; bodies are `211/3,690`.

- **The context and staging frame checks were widened to one frame,
  2026-09-09.** `golden_sun_obj_record_frame_current` replaces the three
  `frame == writer_frame` equalities in the context lookup and the body
  provenance check with "no older than one frame". One frame is the measured
  bound, not a chosen tolerance: the context table is cleared exactly once per
  frame, so a resident record can never be more than one frame old. What
  authenticates a record is unchanged and still exact -- `ATTR0/1/2` must
  match the committed attributes, and the staged coordinates must truncate to
  the committed OAM bytes, which rejects a stale position that no longer
  describes the sprite. `[wide-obj-hooks]` gained `f0_placed_prev_frame` so
  the next run reports how much coverage comes from the relaxed window.
  Source change only; unverified until a rebuild and a rerun of the boulder
  cutscene.


- **The one-frame window worked; the failure moved to the context key,
  2026-09-09.** `logs/objrec_20260909_211716` (boulder cutscene, `6,319`
  committed sprites over `1,001` frames): full precision rose from `19.82%` to
  `33.42%`, `context-frame` failures fell from `3,479` to `219`, and bodies
  went from `211/3,690` to `1,271/5,478`. `f0_placed_prev_frame` was `0`, so
  the gain came from accepting a previous-frame *context*, not a previous-frame
  staged position -- the staged record had already been refreshed. Record bases
  placed: `0x03002348` (703 bodies), `0x03002000` (430 bodies, 631 shadows),
  `0x03002310` (138 bodies), `0x030021c0` (210 shadows).

  The dominant failure is now `context-no-match`, `3,988` rows, `checks=0`: a
  context table for the right frame exists and holds no entry for these
  sprites. They concentrate on one call site and one sprite class. Writer
  `0x030038f0` returning to `0x030038b4` has `2,707` failures and `80`
  successes across all depths, and its sprites are 256-colour tall sprites
  (`ATTR0` bit 10 set, shape `2`, `ATTR2` `0xd524`/`0xd538`), a signature that
  never appears among the successes; returns `0x0300389c` and `0x030038ac` are
  `131/131` successful and return `0x030038a4` is mostly successful, all with
  16-colour square sprites (`ATTR2` `0x093c`/`0x095c`/`0x0800`). Ordering is
  unchanged -- `903` frames have both and all failures still precede all
  successes -- but ordering no longer explains the rejection, since the stamp
  now matches.

  The context key is `(frame, depth, return_pc, ATTR0/1/2)`. EC entry and the
  F0 store are two points inside one invocation of the same routine, so depth
  and return_pc cannot disagree for a single sprite; only the attributes can.
  That leaves two candidates, not yet separated: the EC seam never runs for
  that call, or it runs and reads a different ATTR triple than the one
  committed. `[wide-obj-nearmiss]` was added to separate them -- on a failed
  lookup it searches the context tables by `(frame, depth, return_pc)` alone
  and reports whether any context existed for that call and which pointer it
  read. Bounded to 32 deduped rows, printed at exit in Enhanced runs, and
  consulted by nothing that draws.


- **Two thirds of the still-missing sprites are never seen by the staging
  seam, 2026-09-09.** `logs/session_20260909_213843.log` carries the first
  `[wide-obj-nearmiss]` census: `6,890` commits, `1,817` placed, `5,073`
  failures, and the census rows account for all `5,073` exactly. `3,432`
  (`67.7%`) have `context=none` -- no context existed for that
  `(frame, depth, return_pc)` at all, so the EC seam never ran for the call.
  `1,641` have `context=yes` with a staging pointer (`0x03002348`,
  `0x03002310`, `0x03002000`, `0x0300200c`) whose attributes do not resemble
  the committed sprite; the census returns the first context at that call site,
  so this proves only that the correct one was absent.

  The largest `context=none` group is `2,264` sprites at writer `0x030038f0`
  returning to `0x030038b4`, across call depths `3..12`, all with `ATTR0`
  shape `2` and OBJ mode `1` (tall, semi-transparent) and `ATTR2` `0xd524` or
  `0xd538` (palette `13`). Returns `0x0300389c` (`558`) and `0x030038a4`
  (`568`) contribute smaller `context=none` groups with `ATTR2` `0x0118` and
  `0x000c`. Every success in the same run is the other class: square,
  256-colour, staged in the `0x03002000` array (`ATTR0` `0x20xx`, `ATTR2`
  `0x093c`/`0x095c`/`0x0904`/`0x0800`).

  Consequence for the milestone: the remaining clipping is not a check that can
  be loosened. These sprites are staged somewhere the project has never
  captured, and finding that structure and its writer instructions is a new
  investigation of the same kind that found the `0x03002000` array and its
  `B324`/`B328` writers. Coverage percentages are not comparable between runs
  of different length; the valid comparison is the two recorder captures of the
  same scene, `objrec_20260909_203451` and `objrec_20260909_211716`. Session
  summary for the user is `WIDE_SPRITE_CLIPPING.md`.


- **The staging seam sees every failed sprite, but two EWRAM structures have no
  captured staging record, 2026-09-09.** `logs/session_20260909_221331.log`
  and `logs/objrec_20260909_221339/lifetime.csv` contain `10,807` F0 commits;
  every commit has `entry_state=matched-entry`, meaning the EC entry was
  observed, its source was readable, and its `ATTR0/1/2` exactly matched the
  later F0 commit. The `3,172` successful placements also have a known staging
  record (`entry_flags=31`); all `7,635` source-unavailable outcomes have no
  staging frame (`entry_flags=1`). This rules out a missing EC seam, an
  unreadable source, and an attribute mismatch as causes in this run.

  The `7,635` failures split into two measured source regions: `4,029` reads
  from `0x02033164..0x02033848` (mostly `0x1c` stride, return `0x030038a4`)
  and `3,606` reads from `0x020367c4..0x02036b84` (`0x20` stride, return
  `0x030038b4`). The latter is the known tall, semi-transparent `ATTR2`
  `0xd524`/`0xd52c`/`0xd530`/`0xd534`/`0xd538` class; generated function
  `0x08094928` independently advances its working pointer by `0x20`. No
  corresponding `B324`/`B328` staging entry was observed for either region.
  Commits stop at frame `9892`; the later trace segment contains no object
  commits. The milestone advances by locating the missing source structures
  and ruling out seam and filter changes; their coordinate writers remain to
  be found.


- **The bounded EWRAM writer census ran during the next cutscene capture,
  2026-09-10.** `logs/session_20260910_093227.log` and
  `logs/objrec_20260910_093234/summary.txt` report `4,445` F0 commits,
  `826` identified/placed, and `3,619` source-unavailable outcomes. The new
  `ewram_writes.csv` has `136,051` CPU-store rows: `5,809` in Region A and
  `130,242` in Region B. Every commit has `entry_state=matched-entry`; the
  `3,619` failures read Region A/B sources (`1,205`/`2,414`), and every one of
  their `874` frames also contains a write to its matching region. Region B
  writer `0x0809496e` always targets offset modulo `0x20` equal to `0x0C`,
  while `0x08094970` always targets `0x14` (`3,162` rows each). These are
  measured candidate fields, not yet proven coordinates. No camera or scroll
  value was recorded; the only two `wide-obj-y-cull-correlation` entries are
  known IWRAM actor cases, so this run supports missing source provenance as
  the blocker but does not prove a cutscene-camera mismatch absent. The failed
  rows' committed OAM Y bytes are `13..139` for Region A and `0..159` for
  Region B; this is no distinct out-of-range-camera signature, but OAM bytes
  alone cannot prove the full camera-space coordinate.

## Display signals

- **Normal/Fast capture identifies a sprite-backed glyph path and a text-delay
  candidate, 2026-09-10.** `session_20260910_153213` links to
  `logs/trace_20260910_153228`. Its `text_vram_writes.csv` contains `38,843`
  well-formed 20-column rows. Labeled windows are Normal `373125..373729`,
  Fast `373729..374201`, and Done `374201..374409`; screenshots of Normal
  and Fast show the same completed NPC line. Fast also includes the intervening
  settings navigation, so whole-window call counts/durations are not isolated
  text-speed measurements. The dialogue-opening A presses are at `373461`
  and `374043`; BG0 DMA updates follow at `373471..373476` and
  `374048..374053`, five 256-byte screen-map transfers per frame from
  `0x08016146`. No subsequent BG0 write marks individual letter appearance.

  Hash-verified ROM/source inspection explains a concrete coverage gap:
  executed glyph routine `Func_18cac` has an upload branch `0x08018E5C`
  (28 Normal / 93 Fast calls, including settings text). At `0x08018E70`
  it programs DMA3 with a stack glyph buffer, destination
  `0x06010000 + (tile_base + allocated_slot) * 32`, and control
  `0x84000020` (128 bytes). It then builds sprite attributes. This is OBJ
  graphics memory, outside this capture's BG0 character range
  `0x06000000..0x06007FFF`; the BG0-only recorder therefore misses this
  executed letter-upload path. Earlier statements treating all dialogue text
  as BG0 are too broad: the box/map and this glyph path are separate.
  `Func_18a50` runs once in each labeled conversation and its non-space
  character-width branch `0x08018AC2` runs 41 times in each; this is layout
  evidence, not glyph timing.

  The executed text processor `Func_168f4` calls the glyph routine at
  `0x08016E48`. At `0x08016E4C..0x08016E5C`, source loads a byte from
  `0x0200044C`, indexes `0x08073808`, and stores the resulting delay at
  text-context `R6+0x22`. `0x0801695E..0x0801696C` checks that halfword,
  decrements a nonzero value and returns before processing another character
  (subject to the preceding bypass condition). This identifies a concrete
  candidate for Message-speed control, but the setting byte/table selection
  and per-frame counter values were not captured; do not assign Normal/Fast
  enum values or claim an instant-text fix from aggregate windows. Next
  targeted evidence should join this delay state with DMA3 glyph uploads at
  `0x08018E70`, rather than another general BG0 capture. ROM SHA-1 freshly
  verified as `5c4695205413df7db52b9a184815a07783999971`.

- **BLDY (brightness) is a dead signal.** Constant at 16 for 2,614 of 2,700
  logged frames, 0 for the rest. It does not fade. 2026-09-04.
- **Room transitions are window-register driven and animate for 17-18 frames.**
  Measured across two sessions: seven at 18 frames and three at 17 in one, five
  at 18 and one at 17 in the other. These are the black-bars and iris-circle
  effects the user described — not fades. A 30-frame debounce covers one
  cleanly.
- **Window registers are not a reliable text-box detector.** In one capture
  with dialogue actually on screen, window enable was 0 for 3,195 of 3,300
  frames and non-zero only during transitions; separate dialogue snapshots
  below have `DISPCNT=0x1F40`. Detecting dialogue needs a different signal;
  VRAM glyph-write bursts are the untested candidate.
- **The function tracer ran but did not isolate text progression, 2026-09-10.**
  The same session has `logs/trace_20260910_104424` and an input recording
  with `24` A-button intervals after the savestate. The launcher log records
  the input path but not which of the Function tracer and new text checkbox
  was selected, so the session proves the generic tracer ran but cannot prove
  the text alias was selected. It closed only four automatic windows (`window_open`,
  `savestate_load`, `window_close`, `overlay`) and no manually labeled text
  window; its `signals.csv` has `2,100` unique frames, with window-enable
  nonzero only during frames `110..172` and zero throughout the cutscene
  input sequence. No text delay routine or control field can be identified
  from this run; a repeat capture must manually mark identical dialogue wait
  and advance actions.
- **Session `20260910_144421` labels the game-owned menus and dialogue, 2026-09-10.**
  Its function-tracer index contains `Menu` (`8531..8918`), `Pause_menu`
  (`8918..9282`), `Settings_menu` (`9282..9661`), `Dialogue` (`9853..10288`),
  and `Dialogue_2` (`10288..10730`), each with a local screenshot. The
  Settings image visibly contains the player option `Message speed: Normal`,
  alongside window color/brightness, speech, and auto-sleep settings. The
  input recording has navigation/confirm events during the menu windows and a
  B event during `Dialogue_2`, but does not capture a before/after speed change
  or a controlled text wait/advance pair. All available `signals.csv` samples
  in these windows have `win_enable=0`; the file ends at frame `10553`, before
  the indexed end of `Dialogue_2`, so its tail display state is unavailable.
  The session has no linked scene snapshot or VRAM writer output, leaving the
  glyph writer and the Message-speed state/timing relationship unmeasured.
- **Session `20260910_145625` does not contain a paired Normal/Fast text trace,
  2026-09-10.** Its linked trace directory has only automatic boot/savestate
  and overlay windows; it has no labeled menu or dialogue window and no menu
  screenshots. The input file contains key transitions, but no recorded marker
  identifies the two NPC conversations or a Message-speed change. The VRAM
  toggle did run, producing `32` `[vram-map-cpu]` rows and `0`
  `[vram-map-dma]` rows, all overlapping active Mode 0 BG1-3 map ranges. The
  trace implementation checks only BG1-3 (`overlaps_active_mode0_bg123`) and
  prints only their map metadata, so it cannot observe BG0 font/glyph writes or
  establish the text-speed controller. The ROM cache identity remains the
  required `5c4695205413df7db52b9a184815a07783999971`.
- **Dialogue and menu screens use the ordinary regular-BG compositor, 2026-09-10.**
  Local scene captures visibly show dialogue at frames `237470` and `240470`
  (`logs/maprec_20260904_211752/snap_00010_periodic.png` and
  `snap_00020_periodic.png`) and a battle menu at frame `331642`
  (`logs/maprec_20260904_214216/snap_00020_periodic.png`). Their metadata has
  Mode 0 `DISPCNT=0x1F40`, `BG0CNT=0x0400`, and BG0 scroll `0/0`; the dialogue
  frames also changed VRAM pages. The PPU's `render_regular_bg` path reads
  each BG's `BGxCNT`, screenblock/tile data from VRAM, and the palette, while
  `room_buffer.cpp` deliberately rebuilds only BG1-3 in field scenes. This
  establishes the game-owned tile/palette composition path and keeps BG0 out
  of the room-map source. It does not identify the exact glyph writer or text
  delay state; window-register use varies between captures and is not a text
  detector.

## Runtime behaviour

- **Savestate loads jump the frame counter.** The PPU frame count is restored
  from the save, so any code baselining against a frame number must re-sync.
  This caused three sessions of confusion and two wrong diagnoses on
  2026-09-04. A `set_savestate_load_hook` extension point now exists.
- **Headless runs need `GBARECOMP_SELFHEAL_RAM=1`.** Without it, IWRAM audio
  code is interpreted forever and the run crawls at ~4 fps. With it and a warm
  `recomp_cache`, ~286 fps. The launcher sets it by default, which is why
  windowed play was always fast and a raw CLI run was not.
- **Execution is deterministic.** Two independent headless runs produced 1,697
  functions and 1,946,675 calls each — identical, same capture count, no
  function present in one and not the other. 2026-09-04. The project had
  previously flagged replay determinism as never verified.
- **Input record and replay already exist**
  (`gbarecomp/src/runtime/runtime.cpp`, `GBARECOMP_INPUT_RECORD` /
  `GBARECOMP_INPUT_REPLAY`, `scripts/gs-replay.ps1`). Recordings now pair a
  savestate automatically.
- **Enhanced Options had been coupled to the room-buffer checker, 2026-09-09.**
  `src/room_buffer.cpp` keeps `GSR_ROOM_BUFFER` checking separate from
  `GSR_ROOM_BUFFER_RENDER` drawing, but the launcher had set both when the
  option was enabled and had hidden the Test controls. The launcher now keeps
  only the room-buffer self-check under Test variables (default off); Enhanced
  Options owns room-buffer rendering and sets only
  `GBARECOMP_EXPERIMENTAL_FIXES` plus `GSR_ROOM_BUFFER_RENDER`.
- **Marked capture `session_20260909_133629` confirms the requested diagnostic
  configuration, 2026-09-09.** The session has the sprite recorder and
  `shadow_writes.csv`, VRAM-map trace lines, and the marked function-tracer
  snapshot. Its log contains experimental OBJ-cull lines and the room-buffer
  expanded-view report, while `BG1/2/3: no cells checked` confirms the room
  buffer self-check was off. No Turbo or decoupled-audio event is logged, and
  the marked `phase.csv` rows have a `16,450` microsecond median guest-frame
  interval; this is consistent with Turbo being off but is not a Turbo
  benchmark.
- **User-reported Turbo observations, 2026-09-09 (not controlled profiling).**
  The user reports `120–160%` in towns with Enhanced Options, roughly
  `200–250%` at a location the user did not specify, and `300%+` in towns
  without Enhanced Options. These observations have no matched input, scene,
  display, or diagnostic baseline, so they are recorded as reports rather
  than measurements.
- **The marked Enhanced run does not isolate rendering overhead, 2026-09-09.**
  Its exit log reports `9,119` guest frames, `607` presented frames,
  `5,244.6 ms` total PPU render time, and `148,098,240` room-buffer entries
  supplied; the marked window's `render_us` median/mean are `1,115/1,231.7`
  microseconds across `515` phase rows. Sprite, VRAM, function, input and
  Enhanced paths were active together, and there is no same-input, same-scene
  no-Enhanced run, so these numbers cannot attribute cost to Enhanced
  rendering or to tracer overhead.
- **The existing phase profiler cannot split room-buffer cost from rendering,
  2026-09-09.** With launcher logging enabled, `phase.csv` records aggregate
  `guest_us`, `render_us`, `present_us`, and pacing fields every presented
  frame; `render_us` is the host view-sync plus latched-copy/fresh-render span.
  `ppu_render_us` is populated only when the separate
  `GBARECOMP_COST_PROBE` is enabled, and the marked run has zero per-frame
  values for that field. Room-buffer supply runs inside the PPU compositor, so
  an ordinary no-tracer Turbo run can compare aggregate guest/render/present
  intervals (and, with the cost probe, aggregate PPU time) only across a
  controlled Turbo interval and matched scene. It cannot attribute that time
  to room-buffer work by itself; no speculative timing probe was added.
- **Session `20260909_130314` cannot measure the reported Turbo percentage,
  2026-09-09.** It enabled input recording, all tracers, BIOS inventories and
  the room-buffer checker; the log reports `9,051` guest frames, `586`
  presented frames, `4,737.4 ms` PPU render time, and `127,623,600` room-buffer
  entries supplied. These settings add diagnostic and checking work, and the
  capture has no matched baseline with the same input and scene. The reported
  “60–70%” therefore cannot be attributed to Enhanced Options or to any one
  subsystem from this session. An adjacent no-tracer capture
  (`session_20260909_130341`) measured `1,522.7` average render-phase
  microseconds per recorded frame versus `2,756.1` in this traced capture, but
  the sessions do not share a controlled input or scene. That comparison shows
  why tracer overhead must be separated before assigning a cost to Enhanced
  Options. A clean rerun after the launcher correction is required before
  changing turbo or render code.
- **Session `20260909_120019` aborts in the existing generated-blitter
  fail-closed path, 2026-09-09.** The crash handler reports `SIGABRT` at
  12:03:48 after 198.343 seconds. The final session diagnostics show
  `0x03006048` healed as `[0x03006048,0x03006054)`, followed by repeated
  misses at `0x03006054`; its compile fails because the function finder finds
  no entry at that PC, then `src/runner_main.cpp` deliberately aborts with
  "generated blitter ... failed to heal ... refusing unsafe interpreter
  bridge". The BIOS inventory directory was created at launch but both CSVs
  are absent because exit diagnostics do not run after this abort, so this
  capture cannot attribute the failure to a BIOS SWI or PC recorder.

## Code corpus

- 26,935 canonical translated functions in the main corpus; 95 overlay banks
  fully recompiled ahead of time. The generated C++ is what executes.
- Dispatch coverage snapshot 2026-09-03: 1 distinct miss, 380 healed native,
  186,079 native calls.
- **Real function count is ~6,000, not the ~28,000 generated bodies.** Most
  generated bodies are basic-block dispatch stubs so indirect branches always
  have a callable target. Main ROM 2,259 real named functions / 533 KB;
  overlays 3,731 / 777 KB.
- Function size distribution: median 80 bytes, mean 242. About 115 functions
  over 1 KB account for 42% of all code bytes.
- **Static analysis cannot identify the code.** The translated corpus never
  contains literal addresses — every bus access takes a computed effective
  address. Grepping all shards for the map tables and save memory returns zero
  hits. 89% of functions are unidentified and only running the game can fix
  that.
- **No type information exists anywhere.** Every function is `void fn(void)` on
  a global register struct. The symbol schema has no field for arguments,
  return type or calling convention.
- **Nothing can replace a guest function with a native one.** The
  function-entry hook is documented observe-only
  (`gbarecomp/src/armv4t/runtime_arm.h:815-816`).

## Tracer coverage

From one 55-minute session, 2026-09-04:

- 1,289 of 2,981 main-ROM functions reached (43.2%), plus 767 overlay-code
  addresses and 719 IWRAM addresses.
- Only 47 functions appear in >=90% of captures — the always-on engine.
- 820 functions appear in <=5% of captures — context-specific, the separable
  material.
- Revisiting the same rooms stops yielding new functions quickly, which is
  coverage saturating as it should.

## Build

- The build is `build/gs011_opt`, `RelWithDebInfo` (`-O2 -g -DNDEBUG`). The
  presets in `CMakePresets.json` are Debug only and are **not** what the user
  builds.
- **LTO is ON by default** (`GSR_ENABLE_LTO`). An older note claiming LTO was
  off and had failed twice was wrong and has been removed.
- **The LTO link runs single-threaded unless `MAKE` is set with forward
  slashes**: `MAKE=C:/msys64/mingw64/bin/mingw32-make.exe`. GCC's lto-wrapper
  parses it with a routine that treats backslash as an escape, so a normal
  Windows path is silently mangled and the link takes hours instead of minutes.
  Verified good state: 8 concurrent `lto1.exe`, ~1000s wall clock.
- `-O3` has never been measured on this project.
- The code generator's `emit_memory` emitted a `_post_*` temporary for every
  single-transfer instruction, even when `pre_indexed=true`. In the existing
  ignored BIOS output, 1,298 `_post_*` declarations were measured and 1,216
  were unused, including `_post_000002A8` and `_post_000002B2` (2026-09-09).
  The source now emits that temporary only for post-indexed forms; pre-indexed
  writeback already uses `_ea_*`. CMake only links an existing
  `bios_recompiled.cpp`, so applying this fix to the current BIOS output
  requires a separate `gba_recompile --bios` regeneration.

## Things that were wrong and why — 2026-09-04

Kept because the pattern matters more than the individual errors.

- A fade threshold was guessed twice, in opposite directions: too loose gave 457
  spurious boundaries, too strict gave zero. Both were guesses about BLDY, a
  value that never moves.
- The screenshot gate was guessed too tight — 12 images across 699 captures —
  after predicting the opposite failure.
- A tracer fault was diagnosed twice from reading code alone: first as a
  hot-path performance bug, then as a stuck frame counter. Both wrong. One
  headless run found the real cause, a savestate frame jump.
- An apparent 1 FPS collapse was not performance at all — clicking Mark while a
  prompt was open left the emulator permanently paused.

Every number chosen without data was wrong. Everything measured resolved
quickly.
