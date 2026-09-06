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
- **Actor records live at `0x03002000`, stride `0x38`, ending `0x030022E0`
  (thirteen records); the body's coordinates sit at `+0x00` and its shadow's
  at `+0x0C`.** Implemented in `golden_sun_obj_record_identity`; evidence in
  `docs/issues/WIDE-01_NPC_IDENTITY.md`. Whether thirteen is the whole cast or
  only a near-camera slice is **not** established — the recorder counts the
  records actually seen so this can be settled.
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

## Display signals

- **BLDY (brightness) is a dead signal.** Constant at 16 for 2,614 of 2,700
  logged frames, 0 for the rest. It does not fade. 2026-09-04.
- **Room transitions are window-register driven and animate for 17-18 frames.**
  Measured across two sessions: seven at 18 frames and three at 17 in one, five
  at 18 and one at 17 in the other. These are the black-bars and iris-circle
  effects the user described — not fades. A 30-frame debounce covers one
  cleanly.
- **Text boxes do not use the window registers.** Tested with dialogue actually
  on screen: window enable was 0 for 3,195 of 3,300 frames, non-zero only
  during transitions. Detecting dialogue needs a different signal; VRAM
  glyph-write bursts are the untested candidate.

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
