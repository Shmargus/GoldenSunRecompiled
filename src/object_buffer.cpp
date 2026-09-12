// object_buffer.cpp — see object_buffer.h.

#include "object_buffer.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gba_bus.h"
#include "runtime_bus_bridge.h"
#include "widescreen_policy.h"

// The one narrow accessor exposed by runner_main.cpp, the same one
// object_probe.cpp uses. Declared here rather than via a shared header, the
// same way object_probe.cpp reaches it. Extended (this task) with three
// optional out-parameters -- OAM index and raw OAM coordinate bytes -- so
// the raw sample dump below can show them; it does not change which sprite
// is selected or how it is authenticated.
extern "C" std::size_t gsr_golden_sun_obj_authenticated_positions(
    std::int32_t* out_screen_x, std::int32_t* out_screen_y,
    std::size_t max_count, std::size_t* out_oam_index = nullptr,
    std::int32_t* out_oam_raw_x = nullptr,
    std::int32_t* out_oam_raw_y = nullptr);

namespace gsr {
namespace {

bool g_enabled = false;

std::int32_t abs32(std::int32_t v) { return v < 0 ? -v : v; }

// ---- measured addresses and layout (FACTS.md) ---------------------------
constexpr std::uint32_t kEwramBase = 0x02000000u;
constexpr std::size_t kEwramBytes = 0x40000u;      // 256 KB
constexpr std::uint32_t kCamera = 0x02030DB0u;     // x, y, 16.16 fixed point
constexpr std::uint32_t kRoomRect = 0x02030DC0u;   // min_x, max_x, min_y, max_y (u16)
constexpr std::uint32_t kObjectArray = 0x02030DD0u;  // stride 0x30, X/Y 16.16
constexpr std::uint32_t kEntryStride = 0x30u;
constexpr std::uint32_t kEntryXHi = 0x02u;  // pixel half of the 16.16 X
constexpr std::uint32_t kEntryYHi = 0x06u;  // pixel half of the 16.16 Y

// The array's true length is not yet measured (FACTS.md, 2026-09-11), so the
// walk needs a cap that is bounded without guessing that length. The nearest
// OTHER known EWRAM structure sits at 0x02033164 -- the cutscene sprite
// staging region FACTS.md records for 2026-09-09 ("two EWRAM structures have
// no captured staging record"). 160 entries * 0x30 stride ends at
// 0x02032BD0, 1,428 bytes (about 29 more entries) short of that structure, so
// the walk cannot run off into memory already known to belong to something
// else. If the true length turns out to exceed 160 this cap under-reports it
// (the report says so explicitly), but it cannot over-read into the wrong
// data.
constexpr std::size_t kArrayCap = 160;

// Session 20260911_161420 found ZERO matches at a 64px radius out of 22,302
// authenticated sprites against 76 Confirmed+Static entries -- structurally
// impossible if the array is what it claims to be, so the 64px assumption
// (that the gap between an entry's position and its sprite's position is
// small, a few dozen pixels at most, per artwork-anchor offset) was wrong.
// It baked in an origin: it assumed the array's X/Y and the sprite's
// world-space X/Y (screen + camera) are measured from the same point. If the
// game measures the array from a different origin than the camera, the true
// gap could be large and 64px would never see it.
//
// This is deliberately named a SEARCH BOUND, not a claim about the real
// offset -- it exists only so an origin difference cannot hide from the
// scan. 1024 px per axis is generous: rooms measured so far are hundreds of
// pixels wide (FACTS.md), so 1024 covers a plausible room-scale origin shift
// on either axis while still being far short of wrapping across unrelated,
// far-away memory. Every candidate within this bound is recorded (see
// note_offset call sites below); the true offset, whatever it is, will
// still dominate the resulting histogram once the session accumulates
// enough frames, while a false assumption merely produces noise spread
// across many distinct offsets instead of silently reporting zero matches.
constexpr std::int32_t kMatchSearchRadiusPx = 1024;

// Per-frame movement bound for the "is this a real, moving character" test
// (session 20260911_154329's finding: unrelated memory was passing the old
// "inside the room rect" test on ~141 of 160 entries every single frame,
// because that test never checked that the value behaves like a position).
// A real character's position changes smoothly frame to frame; unrelated
// memory reinterpreted as X/Y does not. FACTS.md has no captured per-frame
// walk-speed measurement as of 2026-09-11 (searched for "per frame",
// "walk speed", "px/frame" -- nothing), so rather than guess a tight number
// this uses a deliberately generous ceiling instead of a measured one:
// TODO-EVIDENCE (capture an actual frame-to-frame position delta trace for a
// walking/running character to replace this with a measured bound).
// 8 px/frame is used because it is well above an ordinary walk step (a
// couple of px/frame per the investigation that found this array) with
// headroom left over for a run/dash state, diagonal movement, and
// fixed-point rounding, while remaining tiny next to the "hundreds of
// pixels" jumps that identified the unrelated memory in the first place --
// so it cannot accidentally accept the noise this fix exists to reject.
constexpr std::int32_t kMovementBoundPx = 8;

// Bounded histogram of distinct (dx, dy) offsets, same shape as
// room_buffer.cpp's g_remainder/g_camera_delta tables: one row per distinct
// pair, filled once per frame, never resized. Raised from 64 to 256
// (2026-09-11): nearest-entry matching used to force one offset per sprite
// per frame, but recording every occupied entry within radius per sprite
// (see note_offset call sites below) means more rows can fill in a
// scattered session, so the cap needs more headroom to still show a real
// signal instead of overflowing immediately. If the table fills, the
// overflow count below says so rather than silently misreporting a clean
// result.
constexpr std::size_t kHistogramCap = 256;

// The array's UNITS are also unproven. The delta search that originally
// found this array (FACTS.md) only established that its high half changes
// by the same amount the camera does -- true for a plain pixel coordinate,
// but equally true for any unit where one step of the stored value equals
// one camera pixel, e.g. an 8px tile index or a 16px cell index. Rather than
// guess and risk another build-and-play round to find out the guess was
// wrong, all three plausible unit hypotheses are measured in parallel, from
// the SAME candidate pairs (same sprite, same occupied entry, same frame --
// only the multiplier applied to the entry's raw value before differencing
// changes). Whichever hypothesis's histogram shows a dominant offset is the
// array's real unit; if none does, the report says so explicitly instead of
// picking one.
struct UnitHypothesis {
    const char* name;
    std::int32_t multiplier;  // entry raw value is multiplied by this before
                               // being differenced against the sprite's
                               // world-space pixel position
};
constexpr std::array<UnitHypothesis, 3> kUnitHypotheses = {{
    {"pixels (entry value as-is)", 1},
    {"8px tiles (entry value x8)", 8},
    {"16px cells (entry value x16)", 16},
}};

// Display-only cutoff for the per-index table, not a physics constant: an
// index occupied on fewer than 5% of scanned frames is "basically never"
// for the purpose of finding the array's real length, while 5% is loose
// enough not to hide a genuine falloff boundary that happens to sit past
// the always-printed first 32 indices.
constexpr double kNonTrivialIndexOccupancyPct = 5.0;

// ---- room rect validity, duplicated from room_buffer.cpp's rect_is_room
// (same reasoning as object_probe.cpp: that function is in room_buffer.cpp's
// anonymous namespace and not exported). Keep in sync if that changes.
constexpr std::uint16_t kRoomRectCellPixels = 16;
bool room_rect_valid(std::uint16_t min_x, std::uint16_t max_x,
                     std::uint16_t min_y, std::uint16_t max_y) {
    if (max_x == 1 && min_y == 8 && max_y == 256) return false;  // world-map marker
    if (max_x <= min_x || max_y <= min_y) return false;
    return (max_x - min_x) >= kRoomRectCellPixels &&
           (max_y - min_y) >= kRoomRectCellPixels;
}

// ---- the histograms ---------------------------------------------------
// One table per unit hypothesis above (3), same OffsetBucket shape and cap
// as before. Total size, up from the single-table 256 * 16 bytes = 4,096
// bytes: 3 * 256 * 16 bytes = 12,288 bytes, still bounded and allocated
// once.
struct OffsetBucket {
    std::int32_t dx = 0, dy = 0;
    std::uint64_t hits = 0;
};
struct OffsetTable {
    std::array<OffsetBucket, kHistogramCap> buckets{};
    std::size_t used = 0;
    std::uint64_t overflow = 0;  // distinct offsets that didn't fit
};
std::array<OffsetTable, kUnitHypotheses.size()> g_unit_histograms{};

void note_offset(OffsetTable& table, std::int32_t dx, std::int32_t dy) {
    for (std::size_t i = 0; i < table.used; ++i) {
        if (table.buckets[i].dx == dx && table.buckets[i].dy == dy) {
            ++table.buckets[i].hits;
            return;
        }
    }
    if (table.used >= kHistogramCap) {
        ++table.overflow;
        return;
    }
    table.buckets[table.used++] = {dx, dy, 1};
}

// ---- per-index history, for the classification below. One entry per
// walked array slot, persistent across frames, allocated once. kArrayCap
// (160) entries * 9 bytes (int32+int32+bool, padded) -- a few hundred
// bytes, bounded, no allocation after startup.
struct EntryHistory {
    std::int32_t prev_x = 0, prev_y = 0;
    bool has_prev = false;
};
std::array<EntryHistory, kArrayCap> g_entry_history{};

// ---- per-index classification (replaces the old one-frame-at-a-time
// "smooth" test, per the 2026-09-11 finding that every constant in EWRAM
// also has zero movement per frame, so a single-frame smoothness check
// barely filtered anything). Each index gets a persistent class for the
// whole session:
//
//   Static    always inside the room rect, never observed to move at all.
//             Could be a stationary NPC or could be dead memory --
//             unresolved either way, which is why it does NOT feed the
//             offset histogram.
//   Confirmed observed at least once to change by a small NON-ZERO amount
//             (more than 0, at most kMovementBoundPx), and never observed
//             to change by more than that bound, and always inside the
//             room rect. A coincidence cannot walk, so this is provably a
//             real character's position.
//   Rejected  jumped by more than kMovementBoundPx, or was seen outside
//             the room rect. Sticky for the rest of the session: a single
//             wild jump or an out-of-room reading proves the slot is not
//             tracking a character's position, and letting it back in
//             would re-admit whatever noise produced the anomaly.
//
// Starts Static (vacuously true before any observation) and only ever
// moves Static -> Confirmed or (from any state) -> Rejected, never back.
enum class EntryClass : std::uint8_t { Static, Confirmed, Rejected };
std::array<EntryClass, kArrayCap> g_entry_class{};  // zero-init = Static

// ---- per-index session stats, same size class as g_entry_history:
// kArrayCap (160) entries * 24 bytes (three uint64_t) = 3,840 bytes.
struct IndexStats {
    std::uint64_t frames_occupied_room = 0;
    std::uint64_t frames_occupied_confirmed = 0;
    std::uint64_t offsets_contributed = 0;
};
std::array<IndexStats, kArrayCap> g_index_stats{};

// ---- bounded record answering "are the STATIC entries explained by the
// same offset as the CONFIRMED ones?" without growing anything per frame.
// The dominant offset is only known at exit (it needs the whole session's
// CONFIRMED histogram), so instead of the offset we record, per distinct
// (dx, dy) a STATIC entry produced against some sprite, which array
// indices produced it -- as a fixed-width bitmask over kArrayCap indices
// (160 bits = 3 uint64_t). At exit, once the dominant offset is known,
// look up its bucket here and count set bits whose index is still
// classified Static. Same shape and cap as the CONFIRMED histogram, so it
// cannot outgrow it: kHistogramCap (256) * (2*int32 + 3*uint64) = 256 * 32
// bytes = 8,192 bytes.
constexpr std::size_t kIndexMaskWords = (kArrayCap + 63u) / 64u;  // 3
struct StaticOffsetBucket {
    std::int32_t dx = 0, dy = 0;
    std::array<std::uint64_t, kIndexMaskWords> index_mask{};
};
std::array<StaticOffsetBucket, kHistogramCap> g_static_histogram{};
std::size_t g_static_histogram_used = 0;
std::uint64_t g_static_histogram_overflow = 0;  // distinct offsets that didn't fit

void mark_static_index(std::array<std::uint64_t, kIndexMaskWords>& mask,
                       std::size_t index) {
    mask[index / 64u] |= (std::uint64_t{1} << (index % 64u));
}
bool static_index_marked(const std::array<std::uint64_t, kIndexMaskWords>& mask,
                         std::size_t index) {
    return ((mask[index / 64u] >> (index % 64u)) & 1u) != 0u;
}

void note_static_offset(std::int32_t dx, std::int32_t dy, std::size_t index) {
    for (std::size_t i = 0; i < g_static_histogram_used; ++i) {
        if (g_static_histogram[i].dx == dx && g_static_histogram[i].dy == dy) {
            mark_static_index(g_static_histogram[i].index_mask, index);
            return;
        }
    }
    if (g_static_histogram_used >= kHistogramCap) {
        ++g_static_histogram_overflow;
        return;
    }
    StaticOffsetBucket& bucket = g_static_histogram[g_static_histogram_used++];
    bucket.dx = dx;
    bucket.dy = dy;
    mark_static_index(bucket.index_mask, index);
}

// ---- raw sample dump (this task) --------------------------------------
// Three rounds of statistics over thousands of frames (the histograms
// above) produced only plausible-looking noise: the dominant offsets they
// found (200-390 px) are exactly what a constant distance between two
// stationary things in room coordinates would also produce, so a histogram
// alone cannot tell "real sprite-to-character gap" from "meaningless
// constant". What is needed instead is a handful of frames with every raw
// number printed plainly, so the arithmetic (camera + sprite screen
// position vs. array entry value) can be read off by eye. This section
// computes and prints NOTHING derived -- no offset, no match, no verdict --
// only raw values, labelled.
//
// Capture is a fixed, one-shot cap: the FIRST kDumpMaxFrames frames that
// have at least one Confirmed entry AND at least one authenticated sprite
// are buffered, then capture stops forever for the rest of the session (see
// g_dump_used below). No growth, no per-frame allocation, read-only with
// respect to guest memory.
constexpr std::size_t kDumpMaxFrames = 20;
constexpr std::size_t kDumpMaxEntriesPerFrame = 16;
constexpr std::size_t kDumpMaxSpritesPerFrame = 24;
// Total lines this section can ever print: kDumpMaxFrames * (1 frame line +
// kDumpMaxEntriesPerFrame entry lines + kDumpMaxSpritesPerFrame sprite
// lines) = 20 * (1 + 16 + 24) = 20 * 41 = 820 lines, from a fixed-size
// buffer of kDumpMaxFrames DumpFrame records (below), each holding fixed
// std::array capacity for its entry/sprite rows -- no dynamic sizing.
struct DumpEntry {
    std::uint32_t index = 0;
    std::uint32_t address = 0;
    std::uint32_t x_raw32 = 0, y_raw32 = 0;  // raw 32-bit field, as stored
    std::int32_t x_hi = 0, y_hi = 0;         // high half, decimal
};
struct DumpSprite {
    std::uint32_t oam_index = 0;
    std::int32_t oam_raw_x = 0, oam_raw_y = 0;  // attr1&0x1FF, attr0&0xFF
    std::int32_t screen_x = 0, screen_y = 0;    // accessor's authenticated result
};
struct DumpFrame {
    std::uint64_t frame = 0;
    std::uint32_t cam_x_raw32 = 0, cam_y_raw32 = 0;  // raw 32-bit @ 0x02030DB0/B4
    std::int32_t cam_x_hi = 0, cam_y_hi = 0;         // high half, decimal
    std::int32_t rect_min_x = 0, rect_max_x = 0, rect_min_y = 0, rect_max_y = 0;
    std::size_t entry_count = 0;
    std::array<DumpEntry, kDumpMaxEntriesPerFrame> entries{};
    std::size_t sprite_count = 0;
    std::array<DumpSprite, kDumpMaxSpritesPerFrame> sprites{};
};
std::array<DumpFrame, kDumpMaxFrames> g_dump{};
std::size_t g_dump_used = 0;  // once == kDumpMaxFrames, capture stops for good

void print_dump(std::FILE* out) {
    if (g_dump_used == 0) {
        std::fprintf(out,
                     "[object-buffer] raw sample dump: no frame ever had "
                     "both a Confirmed entry and an authenticated sprite -- "
                     "nothing captured\n");
        return;
    }
    std::fprintf(out,
                 "[object-buffer] ==== raw sample dump (%zu of %zu frame "
                 "slots used, values are RAW, nothing here is computed) "
                 "====\n",
                 g_dump_used, kDumpMaxFrames);
    for (std::size_t f = 0; f < g_dump_used; ++f) {
        const DumpFrame& d = g_dump[f];
        std::fprintf(out,
                     "[object-buffer] frame=%llu cam_x_raw32=0x%08X "
                     "cam_x_hi=%d cam_y_raw32=0x%08X cam_y_hi=%d "
                     "rect_min_x=%d rect_max_x=%d rect_min_y=%d "
                     "rect_max_y=%d\n",
                     static_cast<unsigned long long>(d.frame), d.cam_x_raw32,
                     d.cam_x_hi, d.cam_y_raw32, d.cam_y_hi, d.rect_min_x,
                     d.rect_max_x, d.rect_min_y, d.rect_max_y);
        std::fprintf(out,
                     "[object-buffer]   entry: index address x_raw32 "
                     "x_hi y_raw32 y_hi\n");
        for (std::size_t i = 0; i < d.entry_count; ++i) {
            const DumpEntry& e = d.entries[i];
            std::fprintf(out,
                         "[object-buffer]   entry idx=%u addr=0x%08X "
                         "x_raw32=0x%08X x_hi=%d y_raw32=0x%08X y_hi=%d\n",
                         e.index, e.address, e.x_raw32, e.x_hi, e.y_raw32,
                         e.y_hi);
        }
        std::fprintf(out,
                     "[object-buffer]   sprite: oam_index oam_raw_x "
                     "oam_raw_y screen_x screen_y\n");
        for (std::size_t i = 0; i < d.sprite_count; ++i) {
            const DumpSprite& s = d.sprites[i];
            std::fprintf(out,
                         "[object-buffer]   sprite oam_index=%u "
                         "oam_raw_x=%d oam_raw_y=%d screen_x=%d "
                         "screen_y=%d\n",
                         s.oam_index, s.oam_raw_x, s.oam_raw_y, s.screen_x,
                         s.screen_y);
        }
    }
}

// ---- accumulated counters (session-wide, not per room: the array length
// question and the offset question are both about the game's own table, not
// about any one room) --------------------------------------------------
std::uint64_t g_frames_scanned = 0;       // room rect was valid, array walked
std::uint64_t g_frames_no_room = 0;       // room rect invalid, frame skipped

// "Occupied" is still tracked two ways: the old weak test (inside the room
// rect, kept only to show how much work the classification does) and the
// new real one (classified Confirmed -- see EntryClass above).
std::size_t g_max_occupied_index_room = 0;
bool g_have_occupied_room = false;
std::size_t g_max_occupied_index_confirmed = 0;
bool g_have_occupied_confirmed = false;

std::uint64_t g_sprites_considered = 0;
std::uint64_t g_sprites_matched = 0;    // had >=1 non-rejected entry within radius
std::uint64_t g_sprites_unmatched = 0;
std::uint64_t g_occupied_observations_room = 0;       // sum of inside-room counts
std::uint64_t g_occupied_observations_confirmed = 0;  // sum of Confirmed-entry counts
// Sum, over all scanned frames, of `occupied_count` -- every Confirmed-or-
// Static entry counted that frame (Rejected entries are never in
// `occupied[]`). This, not g_occupied_observations_confirmed, is the
// correct population for g_occupied_unexplained below: g_occupied_unexplained
// is incremented once per unmatched entry in `occupied[]`, which includes
// BOTH classes, so counting it against the Confirmed-only population let the
// reported percentage exceed 100% (session 20260911_161420: 642.96%).
std::uint64_t g_occupied_observations_nonrejected = 0;
std::uint64_t g_occupied_unexplained = 0;  // non-rejected entry, matched to no sprite

// ---- the check ------------------------------------------------------------
struct OccupiedEntry {
    std::int32_t x = 0, y = 0;
    std::size_t index = 0;  // slot in the object array, for per-index stats
    EntryClass cls = EntryClass::Static;
    bool used = false;      // matched at least one sprite this frame
};

// ---- readout ------------------------------------------------------------
void report_at_exit() {
    if (!g_enabled) return;
    print_dump(stderr);
    std::fprintf(stderr,
                 "[object-buffer] %llu frames scanned, %llu skipped (room "
                 "rect not valid)\n",
                 static_cast<unsigned long long>(g_frames_scanned),
                 static_cast<unsigned long long>(g_frames_no_room));
    if (!g_have_occupied_room) {
        std::fprintf(stderr,
                     "[object-buffer] no array entry was ever seen inside "
                     "the room -- nothing else to report\n");
        return;
    }

    const auto pct = [](std::uint64_t part, std::uint64_t whole) -> double {
        return whole == 0 ? 0.0
                          : 100.0 * static_cast<double>(part) /
                                static_cast<double>(whole);
    };

    // A "highest index" equal to the walk cap means the walk ran past the
    // array, not that the array is that long -- state this explicitly for
    // both occupancy tests rather than only for whichever one hits the cap.
    const auto report_max_index = [&](const char* label, bool have,
                                      std::size_t max_index) {
        if (!have) {
            std::fprintf(stderr, "[object-buffer] %s: never occupied\n", label);
            return;
        }
        std::fprintf(stderr,
                     "[object-buffer] highest %s-occupied array index ever "
                     "seen: %zu (at least %zu entries)%s\n",
                     label, max_index, max_index + 1,
                     max_index + 1 >= kArrayCap
                         ? " -- this equals the walk cap, which means the "
                           "walk ran past the end of the array, NOT that "
                           "the array is this long"
                         : "");
    };
    report_max_index("inside-room", g_have_occupied_room, g_max_occupied_index_room);
    report_max_index("Confirmed", g_have_occupied_confirmed,
                     g_max_occupied_index_confirmed);

    std::fprintf(stderr,
                 "[object-buffer] occupied-entry observations: %llu "
                 "inside-room, %llu Confirmed (%.2f%% survive "
                 "classification)\n",
                 static_cast<unsigned long long>(g_occupied_observations_room),
                 static_cast<unsigned long long>(g_occupied_observations_confirmed),
                 pct(g_occupied_observations_confirmed, g_occupied_observations_room));

    // ---- the three classes (this task): how many array slots ended the
    // session in each bucket.
    std::uint64_t class_static = 0, class_confirmed = 0, class_rejected = 0;
    for (std::size_t i = 0; i < kArrayCap; ++i) {
        switch (g_entry_class[i]) {
            case EntryClass::Static: ++class_static; break;
            case EntryClass::Confirmed: ++class_confirmed; break;
            case EntryClass::Rejected: ++class_rejected; break;
        }
    }
    std::fprintf(stderr,
                 "[object-buffer] classification: %llu Confirmed, %llu "
                 "Static, %llu Rejected (of %zu walked slots)\n",
                 static_cast<unsigned long long>(class_confirmed),
                 static_cast<unsigned long long>(class_static),
                 static_cast<unsigned long long>(class_rejected), kArrayCap);

    std::fprintf(stderr,
                 "[object-buffer] authenticated sprites considered: %llu\n",
                 static_cast<unsigned long long>(g_sprites_considered));
    std::fprintf(stderr,
                 "[object-buffer] matched >=1 entry within the %d px search "
                 "bound: %llu of %llu authenticated sprites (%.2f%%)\n",
                 kMatchSearchRadiusPx,
                 static_cast<unsigned long long>(g_sprites_matched),
                 static_cast<unsigned long long>(g_sprites_considered),
                 pct(g_sprites_matched, g_sprites_considered));
    std::fprintf(stderr,
                 "[object-buffer] no entry within the %d px search bound: "
                 "%llu of %llu authenticated sprites (%.2f%%)\n",
                 kMatchSearchRadiusPx,
                 static_cast<unsigned long long>(g_sprites_unmatched),
                 static_cast<unsigned long long>(g_sprites_considered),
                 pct(g_sprites_unmatched, g_sprites_considered));
    // Population fix (session 20260911_161420 reported 642.96% here): the
    // numerator counts BOTH Confirmed and Static entries in `occupied[]`
    // that matched no sprite, so the denominator must be the sum of that
    // same Confirmed+Static population (g_occupied_observations_nonrejected),
    // not the Confirmed-only sum used before.
    std::fprintf(stderr,
                 "[object-buffer] non-rejected entries with no sprite "
                 "matched (characters in the room but not drawn): %llu of "
                 "%llu Confirmed-or-Static occupied-entry observations "
                 "(%.2f%%)\n",
                 static_cast<unsigned long long>(g_occupied_unexplained),
                 static_cast<unsigned long long>(g_occupied_observations_nonrejected),
                 pct(g_occupied_unexplained, g_occupied_observations_nonrejected));

    // ---- per-index table: where does the array actually end, and what is
    // each slot classified as?
    //
    // Column meaning, spelled out because "class REJECTED, confirmed=64%"
    // used to read as a contradiction (it is not one): `class` is the FINAL
    // sticky state at session end (EntryClass above), computed only from
    // this session's last-seen evidence. `in-room%` is the fraction of
    // scanned frames this slot's raw reading sat inside the room rect, for
    // ANY class, including frames before a later rejection. `confirmed-at-
    // the-time%` is the fraction of scanned frames this slot was counted as
    // Confirmed AT THAT MOMENT -- it can stay high even for a slot whose
    // final `class` below is REJECTED, because rejection is sticky and can
    // arrive on a single later frame (one wild jump) after many earlier
    // frames of genuine small movement; the column is a history, not a
    // restatement of the final class.
    std::fprintf(stderr,
                 "[object-buffer] per-index table -- columns: index, FINAL "
                 "session class, %% of scanned frames the raw reading sat "
                 "in-room (any class), %% of scanned frames this slot was "
                 "Confirmed AT THE TIME (can exceed 0%% even if class below "
                 "ended REJECTED -- see comment above), candidate offsets "
                 "this slot contributed (counted once per candidate pair, "
                 "feeding all three unit histograms below) -- first 32 "
                 "always shown, later indices shown only above %.0f%% "
                 "confirmed-at-the-time occupancy:\n",
                 kNonTrivialIndexOccupancyPct);
    std::uint64_t omitted = 0;
    for (std::size_t i = 0; i < kArrayCap; ++i) {
        const double room_pct = pct(g_index_stats[i].frames_occupied_room, g_frames_scanned);
        const double confirmed_pct =
            pct(g_index_stats[i].frames_occupied_confirmed, g_frames_scanned);
        const bool show = i < 32 || confirmed_pct >= kNonTrivialIndexOccupancyPct;
        if (!show) {
            ++omitted;
            continue;
        }
        const char* class_label = g_entry_class[i] == EntryClass::Confirmed
                                       ? "CONFIRMED"
                                       : g_entry_class[i] == EntryClass::Static
                                             ? "STATIC   "
                                             : "REJECTED ";
        std::fprintf(stderr,
                     "[object-buffer]   [%3zu] class=%s in-room=%6.2f%% "
                     "confirmed-at-time=%6.2f%% offsets=%llu\n",
                     i, class_label, room_pct, confirmed_pct,
                     static_cast<unsigned long long>(g_index_stats[i].offsets_contributed));
    }
    if (omitted > 0) {
        std::fprintf(stderr,
                     "[object-buffer]   (%llu further indices omitted, each "
                     "below %.0f%% confirmed occupancy)\n",
                     static_cast<unsigned long long>(omitted),
                     kNonTrivialIndexOccupancyPct);
    }

    bool any_hypothesis_has_data = false;
    for (const OffsetTable& t : g_unit_histograms) {
        if (t.used > 0) any_hypothesis_has_data = true;
    }
    if (!any_hypothesis_has_data) {
        std::fprintf(stderr,
                     "[object-buffer] no sprite ever matched an entry -- no "
                     "offset histogram to report (for any of the %zu unit "
                     "hypotheses)\n",
                     kUnitHypotheses.size());
        std::fprintf(stderr,
                     "[object-buffer] verdict: no offsets recorded -- the "
                     "array does not yet explain the sprites\n");
        return;
    }

    // Sort each hypothesis's (small, bounded) histogram descending by hit
    // count, and print it under its own name. Same candidate pairs feed all
    // three (see the comment above kUnitHypotheses and at the note_offset
    // call site); only the multiplier applied to the entry's raw value
    // before differencing changes between them.
    std::array<std::array<OffsetBucket, kHistogramCap>, kUnitHypotheses.size()>
        sorted{};
    std::array<double, kUnitHypotheses.size()> top_pct{};
    for (std::size_t h = 0; h < kUnitHypotheses.size(); ++h) {
        const OffsetTable& t = g_unit_histograms[h];
        sorted[h] = t.buckets;
        std::sort(sorted[h].begin(), sorted[h].begin() + t.used,
                 [](const OffsetBucket& a, const OffsetBucket& b) {
                     return a.hits > b.hits;
                 });
        top_pct[h] = t.used == 0 ? 0.0 : pct(sorted[h][0].hits, g_sprites_matched);
        std::fprintf(stderr,
                     "[object-buffer] offset histogram, unit hypothesis "
                     "\"%s\" (sprite world position minus each occupied "
                     "Confirmed entry's raw value x%d, within the search "
                     "bound), most frequent first, %zu distinct offsets%s:\n",
                     kUnitHypotheses[h].name, kUnitHypotheses[h].multiplier,
                     t.used,
                     t.overflow > 0 ? " (table full, some distinct offsets "
                                      "not recorded)"
                                    : "");
        for (std::size_t i = 0; i < t.used; ++i) {
            std::fprintf(stderr,
                         "[object-buffer]   dx=%d dy=%d  hits=%llu (%.2f%% "
                         "of matched)\n",
                         sorted[h][i].dx, sorted[h][i].dy,
                         static_cast<unsigned long long>(sorted[h][i].hits),
                         pct(sorted[h][i].hits, g_sprites_matched));
        }
    }

    // Where does the real occupancy fall away? First index (from the top)
    // whose Confirmed occupancy drops below the same non-trivial threshold
    // used in the table above. Independent of the unit question.
    std::size_t falloff_index = kArrayCap;
    for (std::size_t i = 0; i < kArrayCap; ++i) {
        if (pct(g_index_stats[i].frames_occupied_confirmed, g_frames_scanned) <
            kNonTrivialIndexOccupancyPct) {
            falloff_index = i;
            break;
        }
    }
    if (falloff_index >= kArrayCap) {
        std::fprintf(stderr,
                     "[object-buffer] per-index table shows no falloff -- "
                     "every walked index (0..%zu) is still non-trivially "
                     "occupied, so this session's walk cap of %zu is not "
                     "wide enough to see where the array actually ends\n",
                     kArrayCap - 1, kArrayCap);
    } else {
        std::fprintf(stderr,
                     "[object-buffer] per-index table shows occupancy "
                     "falling away at index %zu (first index below %.0f%% "
                     "confirmed occupancy)\n",
                     falloff_index, kNonTrivialIndexOccupancyPct);
    }

    // Verdict: "most" means a majority of matched sprites landing on one
    // offset under one unit hypothesis, not merely the largest of a
    // scattered set. Whichever hypothesis clears the bar is read off as the
    // array's real unit; if more than one clears it, that is reported as
    // ambiguous rather than silently picking one; if none does, say so.
    constexpr double kDominantOffsetThresholdPct = 50.0;
    std::size_t dominant_count = 0;
    std::size_t dominant_h = kUnitHypotheses.size();
    for (std::size_t h = 0; h < kUnitHypotheses.size(); ++h) {
        if (top_pct[h] >= kDominantOffsetThresholdPct) {
            ++dominant_count;
            dominant_h = h;
        }
    }
    if (dominant_count == 0) {
        std::fprintf(stderr,
                     "[object-buffer] verdict: offsets SCATTER under all %zu "
                     "unit hypotheses -- no single offset explains a "
                     "majority of matched sprites under any of them (top: "
                     "pixels %.2f%%, x8 %.2f%%, x16 %.2f%%). The array does "
                     "not yet explain the sprites; do not read this as "
                     "success.\n",
                     kUnitHypotheses.size(), top_pct[0], top_pct[1], top_pct[2]);
        return;
    }
    if (dominant_count > 1) {
        std::fprintf(stderr,
                     "[object-buffer] verdict: AMBIGUOUS -- more than one "
                     "unit hypothesis shows a dominant offset (top: pixels "
                     "%.2f%%, x8 %.2f%%, x16 %.2f%%); this cannot happen for "
                     "unrelated data by chance, so it likely means two "
                     "hypotheses are redundant descriptions of the same "
                     "underlying offset -- needs a human read of the tables "
                     "above, not a bare percentage\n",
                     top_pct[0], top_pct[1], top_pct[2]);
        return;
    }

    const OffsetBucket& winner = sorted[dominant_h][0];
    std::fprintf(stderr,
                 "[object-buffer] verdict: unit hypothesis \"%s\" explains "
                 "most matched sprites at one offset, dx=%d dy=%d (%.2f%% "
                 "of matched sprites)\n",
                 kUnitHypotheses[dominant_h].name, winner.dx, winner.dy,
                 top_pct[dominant_h]);

    if (dominant_h != 0) {
        std::fprintf(stderr,
                     "[object-buffer] static-entries-explained cross-check "
                     "skipped: it only ever recorded offsets under the "
                     "pixels-as-is hypothesis (see note_static_offset call "
                     "site), so it cannot be read against a dominant offset "
                     "found under a different unit hypothesis\n");
        return;
    }

    // Second, separate tally (unchanged from before this fix): now that the
    // dominant offset is known, and it is under the pixels-as-is hypothesis
    // that g_static_histogram itself was recorded against, how many STATIC
    // entries does it also explain? A STATIC entry counts as explained if
    // some sprite this session sat at exactly the dominant (dx, dy) from it
    // -- recorded live in g_static_histogram since the dominant offset
    // wasn't known until now. Re-check final classification here too: an
    // index recorded in that bucket while Static may have since been
    // Confirmed or Rejected by a later frame, and no longer counts as a
    // static entry at session end.
    std::uint64_t static_total = 0, static_explained = 0;
    const StaticOffsetBucket* dominant_bucket = nullptr;
    for (std::size_t i = 0; i < g_static_histogram_used; ++i) {
        if (g_static_histogram[i].dx == winner.dx &&
            g_static_histogram[i].dy == winner.dy) {
            dominant_bucket = &g_static_histogram[i];
            break;
        }
    }
    for (std::size_t i = 0; i < kArrayCap; ++i) {
        if (g_entry_class[i] != EntryClass::Static) continue;
        ++static_total;
        if (dominant_bucket != nullptr &&
            static_index_marked(dominant_bucket->index_mask, i)) {
            ++static_explained;
        }
    }
    std::fprintf(stderr,
                 "[object-buffer] static entries explained by the dominant "
                 "offset: %llu of %llu%s\n",
                 static_cast<unsigned long long>(static_explained),
                 static_cast<unsigned long long>(static_total),
                 g_static_histogram_overflow > 0
                     ? " (static offset table was full this session, this "
                       "may undercount)"
                     : "");
}

}  // namespace

void object_buffer_init() {
    const char* e = std::getenv("GSR_OBJECT_BUFFER");
    g_enabled = e != nullptr && e[0] != '\0' && e[0] != '0';
    if (!g_enabled) return;
    std::atexit(report_at_exit);
}

void object_buffer_on_entry(std::uint32_t entry_pc) {
    (void)entry_pc;
    if (!g_enabled) return;  // OFF: one predictable branch, nothing else.

    static std::uint64_t s_last_frame = ~std::uint64_t{0};
    const std::uint64_t frame = runtime_current_frame();
    if (frame == s_last_frame) return;
    s_last_frame = frame;

    const gba::GbaBus* bus = gbarecomp::active_bus();
    if (!bus) return;
    const std::uint8_t* ew = bus->ewram_ptr();
    if (!ew) return;

    const auto read_u16 = [&](std::uint32_t address) -> std::uint16_t {
        const std::uint32_t off = (address - kEwramBase) & (kEwramBytes - 1u);
        return static_cast<std::uint16_t>(ew[off] | (ew[off + 1] << 8));
    };
    // Raw 32-bit read, used only by the sample dump below (this task) to
    // show the whole stored field, not just the pixel high half the
    // classification logic uses.
    const auto read_u32 = [&](std::uint32_t address) -> std::uint32_t {
        const std::uint32_t off = (address - kEwramBase) & (kEwramBytes - 1u);
        return static_cast<std::uint32_t>(ew[off]) |
               (static_cast<std::uint32_t>(ew[off + 1]) << 8) |
               (static_cast<std::uint32_t>(ew[off + 2]) << 16) |
               (static_cast<std::uint32_t>(ew[off + 3]) << 24);
    };

    // Step 1: camera and room rect, same decode room_buffer.cpp and
    // object_probe.cpp use. Skip the frame outright when the room rect is
    // not valid -- there is nothing to test occupancy against.
    const std::uint16_t rect_min_x = read_u16(kRoomRect);
    const std::uint16_t rect_max_x = read_u16(kRoomRect + 2u);
    const std::uint16_t rect_min_y = read_u16(kRoomRect + 4u);
    const std::uint16_t rect_max_y = read_u16(kRoomRect + 6u);
    if (!room_rect_valid(rect_min_x, rect_max_x, rect_min_y, rect_max_y)) {
        ++g_frames_no_room;
        return;
    }
    const std::int32_t cam_x = static_cast<std::int32_t>(read_u16(kCamera + 2u));
    const std::int32_t cam_y = static_cast<std::int32_t>(read_u16(kCamera + 6u));

    // Step 2: walk the object array over the bounded range and update each
    // slot's classification (see EntryClass above). "occupied" here means
    // "not yet proven fake" -- Confirmed or Static -- and feeds sprite
    // matching below; only Confirmed entries feed the offset histogram.
    // Bounded local table, no allocation.
    std::array<OccupiedEntry, kArrayCap> occupied{};
    std::size_t occupied_count = 0;       // Confirmed-or-Static count, this frame
    std::size_t occupied_count_room = 0;  // inside-room count, this frame
    for (std::size_t i = 0; i < kArrayCap; ++i) {
        const std::uint32_t base = kObjectArray + static_cast<std::uint32_t>(i) * kEntryStride;
        const std::int32_t x = static_cast<std::int32_t>(read_u16(base + kEntryXHi));
        const std::int32_t y = static_cast<std::int32_t>(read_u16(base + kEntryYHi));
        const bool in_room = x >= rect_min_x && x < rect_max_x &&
                             y >= rect_min_y && y < rect_max_y;

        EntryHistory& h = g_entry_history[i];
        bool jumped_too_far = false;
        bool moved_small_nonzero = false;
        if (h.has_prev) {
            const std::int32_t dx = x - h.prev_x;
            const std::int32_t dy = y - h.prev_y;
            const bool within_bound =
                abs32(dx) <= kMovementBoundPx && abs32(dy) <= kMovementBoundPx;
            jumped_too_far = !within_bound;
            moved_small_nonzero = within_bound && (dx != 0 || dy != 0);
        }
        h.prev_x = x;
        h.prev_y = y;
        h.has_prev = true;

        EntryClass& cls = g_entry_class[i];
        if (cls != EntryClass::Rejected) {
            if (!in_room || jumped_too_far) {
                // Sticky rejection: a jump bigger than a real character can
                // make in one frame, or a reading outside the room rect,
                // proves this slot is not tracking a character's position.
                // Once wrong it stays excluded for the session so it can't
                // pollute the histogram or the sprite-match counts later.
                cls = EntryClass::Rejected;
            } else if (moved_small_nonzero) {
                cls = EntryClass::Confirmed;  // sticky: a coincidence can't walk
            }
            // else: no new evidence this frame (stationary while in room)
            // -- class unchanged (stays Static, or stays Confirmed).
        }

        if (in_room) {
            ++occupied_count_room;
            ++g_index_stats[i].frames_occupied_room;
            if (!g_have_occupied_room || i > g_max_occupied_index_room) {
                g_max_occupied_index_room = i;
                g_have_occupied_room = true;
            }
        }
        if (in_room && cls != EntryClass::Rejected) {
            if (cls == EntryClass::Confirmed) {
                ++g_index_stats[i].frames_occupied_confirmed;
                if (!g_have_occupied_confirmed || i > g_max_occupied_index_confirmed) {
                    g_max_occupied_index_confirmed = i;
                    g_have_occupied_confirmed = true;
                }
            }
            occupied[occupied_count++] = {x, y, i, cls, false};
        }
    }
    ++g_frames_scanned;
    g_occupied_observations_room += occupied_count_room;
    g_occupied_observations_nonrejected += occupied_count;
    for (std::size_t o = 0; o < occupied_count; ++o) {
        if (occupied[o].cls == EntryClass::Confirmed) ++g_occupied_observations_confirmed;
    }

    // Step 3: this frame's authenticated sprite screen positions -> world
    // positions, exactly as the brief specifies (world = screen + camera).
    // OAM index and raw OAM coordinate bytes are also requested, purely for
    // the raw sample dump below; they play no part in the measurement that
    // follows.
    std::array<std::int32_t, gsr::widescreen::kGoldenSunOamShadowSlotCount>
        screen_x{}, screen_y{}, oam_raw_x{}, oam_raw_y{};
    std::array<std::size_t, gsr::widescreen::kGoldenSunOamShadowSlotCount>
        oam_index{};
    const std::size_t n = gsr_golden_sun_obj_authenticated_positions(
        screen_x.data(), screen_y.data(), screen_x.size(), oam_index.data(),
        oam_raw_x.data(), oam_raw_y.data());

    // Raw sample dump capture (this task): the FIRST kDumpMaxFrames frames
    // with >=1 Confirmed entry AND >=1 authenticated sprite, then never
    // again -- see g_dump_used's comment above. Bounded, read-only, one
    // check plus (only on those rare frames) a handful of extra raw reads.
    if (g_dump_used < kDumpMaxFrames && n > 0) {
        bool have_confirmed = false;
        for (std::size_t o = 0; o < occupied_count && !have_confirmed; ++o) {
            if (occupied[o].cls == EntryClass::Confirmed) have_confirmed = true;
        }
        if (have_confirmed) {
            DumpFrame& d = g_dump[g_dump_used++];
            d.frame = frame;
            d.cam_x_raw32 = read_u32(kCamera);
            d.cam_y_raw32 = read_u32(kCamera + 4u);
            d.cam_x_hi = cam_x;
            d.cam_y_hi = cam_y;
            d.rect_min_x = rect_min_x;
            d.rect_max_x = rect_max_x;
            d.rect_min_y = rect_min_y;
            d.rect_max_y = rect_max_y;
            d.entry_count = 0;
            for (std::size_t o = 0; o < occupied_count &&
                                    d.entry_count < kDumpMaxEntriesPerFrame;
                 ++o) {
                if (occupied[o].cls != EntryClass::Confirmed) continue;
                const std::size_t idx = occupied[o].index;
                const std::uint32_t base =
                    kObjectArray + static_cast<std::uint32_t>(idx) * kEntryStride;
                DumpEntry& e = d.entries[d.entry_count++];
                e.index = static_cast<std::uint32_t>(idx);
                e.address = base;
                e.x_raw32 = read_u32(base);
                e.y_raw32 = read_u32(base + 4u);
                e.x_hi = occupied[o].x;
                e.y_hi = occupied[o].y;
            }
            d.sprite_count = std::min(n, kDumpMaxSpritesPerFrame);
            for (std::size_t s = 0; s < d.sprite_count; ++s) {
                DumpSprite& sp = d.sprites[s];
                sp.oam_index = static_cast<std::uint32_t>(oam_index[s]);
                sp.oam_raw_x = oam_raw_x[s];
                sp.oam_raw_y = oam_raw_y[s];
                sp.screen_x = screen_x[s];
                sp.screen_y = screen_y[s];
            }
        }
    }

    // The measurement: for each sprite, record the offset to EVERY
    // non-rejected entry within the search radius, not just the nearest
    // one. Nearest-entry matching biases the offset toward zero and matches
    // noise when candidates are dense; recording every candidate lets the
    // true sprite-to-character offset accumulate across the session while
    // noise spreads thinly across many distinct offsets instead. The
    // matched/unmatched totals count "had at least one candidate" per
    // sprite against Confirmed-or-Static entries, but the offset histograms
    // only take Confirmed entries (that is the point of the
    // classification): Static offsets go to the separate static-offset
    // record instead, so an unresolved constant can't shape the dominant
    // offset the report reads off.
    //
    // Candidacy itself (had_candidate / kMatchSearchRadiusPx) is decided on
    // the entry's RAW value against the sprite's world pixel position --
    // that is the origin-difference search from the comment above
    // kMatchSearchRadiusPx, and it stays raw regardless of the unit
    // question below, since 1024 px is generous enough to still catch a
    // real pairing even if the raw value is actually a small tile or cell
    // index (its magnitude only makes the raw dx/dy bigger, not outside a
    // 1024 px bound for any room measured so far).
    //
    // For each Confirmed candidate found this way, the SAME pair (this
    // sprite, this entry) is then differenced three ways -- raw, x8, x16 --
    // into the three parallel histograms (kUnitHypotheses), so all three
    // unit hypotheses are measured from identical candidate pairs in one
    // pass, rather than three separate scans.
    for (std::size_t s = 0; s < n; ++s) {
        ++g_sprites_considered;
        const std::int32_t world_x = cam_x + screen_x[s];
        const std::int32_t world_y = cam_y + screen_y[s];

        bool had_candidate = false;
        for (std::size_t o = 0; o < occupied_count; ++o) {
            const std::int32_t dx = world_x - occupied[o].x;
            const std::int32_t dy = world_y - occupied[o].y;
            if (dx > kMatchSearchRadiusPx || dx < -kMatchSearchRadiusPx ||
                dy > kMatchSearchRadiusPx || dy < -kMatchSearchRadiusPx) {
                continue;
            }
            had_candidate = true;
            occupied[o].used = true;
            if (occupied[o].cls == EntryClass::Confirmed) {
                ++g_index_stats[occupied[o].index].offsets_contributed;
                for (std::size_t h = 0; h < kUnitHypotheses.size(); ++h) {
                    const std::int32_t mul = kUnitHypotheses[h].multiplier;
                    note_offset(g_unit_histograms[h],
                                world_x - occupied[o].x * mul,
                                world_y - occupied[o].y * mul);
                }
            } else {  // EntryClass::Static (Rejected is never in `occupied`)
                note_static_offset(dx, dy, occupied[o].index);
            }
        }
        if (had_candidate) {
            ++g_sprites_matched;
        } else {
            ++g_sprites_unmatched;
        }
    }

    for (std::size_t o = 0; o < occupied_count; ++o) {
        if (!occupied[o].used) ++g_occupied_unexplained;
    }
}

}  // namespace gsr
