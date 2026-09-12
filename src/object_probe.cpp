// object_probe.cpp — see object_probe.h.

#include "object_probe.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "gba_bus.h"
#include "runtime_bus_bridge.h"
#include "widescreen_policy.h"

// The one narrow accessor exposed by runner_main.cpp (see its definition,
// right after golden_sun_wide_obj_attr_y_provider, for why it exists and
// what it does and does not touch). Declared here rather than via a shared
// header, the same way room_buffer.cpp reaches g_ws_expanded_diag.
// Extended (this task) with three optional out-parameters -- OAM index and
// raw OAM coordinate bytes -- for the raw sample dump in object_buffer.cpp.
// This file does not need them and keeps calling with the original three
// arguments; the defaults below make that keep compiling unchanged.
extern "C" std::size_t gsr_golden_sun_obj_authenticated_positions(
    std::int32_t* out_screen_x, std::int32_t* out_screen_y,
    std::size_t max_count, std::size_t* out_oam_index = nullptr,
    std::int32_t* out_oam_raw_x = nullptr,
    std::int32_t* out_oam_raw_y = nullptr);

namespace gsr {
namespace {

bool g_enabled = false;

// ---- measured addresses (FACTS.md; same decode as room_buffer.cpp) ------
constexpr std::uint32_t kEwramBase = 0x02000000u;
constexpr std::uint32_t kIwramBase = 0x03000000u;
constexpr std::size_t kEwramBytes = 0x40000u;   // 256 KB
constexpr std::size_t kIwramBytes = 0x8000u;    // 32 KB
constexpr std::uint32_t kCamera = 0x02030DB0u;  // x, y, 16.16 fixed point
constexpr std::uint32_t kRoomRect = 0x02030DC0u;  // min_x, max_x, min_y, max_y (u16)

// 2-byte aligned scan: one slot per 16-bit-aligned address in each region.
constexpr std::size_t kEwramSlots = kEwramBytes / 2u;   // 131,072
constexpr std::size_t kIwramSlots = kIwramBytes / 2u;   // 16,384
constexpr std::size_t kTotalSlots = kEwramSlots + kIwramSlots;  // 147,456

// One membership bitmap bit per possible 16-bit value.
constexpr std::size_t kBitmapBits = 65536u;
constexpr std::size_t kBitmapWords = kBitmapBits / 32u;  // 2,048

// A sprite's OAM coordinate is its artwork's top-left corner; the character's
// stored position is somewhere near that corner but not equal to it. The
// largest hardware sprite is 64x64, so the corner-to-position gap cannot
// exceed 64px on either axis -- that bounds both the membership window below
// and the gap histogram (one bin per integer pixel offset in [-64, +64]).
constexpr int kTolerancePx = 64;
constexpr std::size_t kGapBins = static_cast<std::size_t>(2 * kTolerancePx + 1);  // 129

void set_bit(std::array<std::uint32_t, kBitmapWords>& bitmap,
            std::uint16_t value) {
    bitmap[value >> 5] |= (1u << (value & 31u));
}

bool test_bit(const std::array<std::uint32_t, kBitmapWords>& bitmap,
             std::uint16_t value) {
    return (bitmap[value >> 5] >> (value & 31u)) & 1u;
}

std::size_t popcount_bitmap(const std::array<std::uint32_t, kBitmapWords>& bitmap) {
    std::size_t n = 0;
    for (std::uint32_t w : bitmap) n += static_cast<std::size_t>(std::popcount(w));
    return n;
}

// Which of up to 128 sprite world positions (on one axis) an address's value
// is closest to, within tolerance. Bounded to the OAM shadow's own slot
// count, so this is at most 128 integer subtractions -- called only on a
// bitmap hit, not on every scanned address (see scan_region and the cost
// note above its call sites).
bool nearest_gap(const std::uint16_t* list, std::size_t count, std::uint16_t v,
                 int& out_gap) {
    int best_abs = kTolerancePx + 1;
    int best_gap = 0;
    bool found = false;
    for (std::size_t k = 0; k < count; ++k) {
        const int gap = static_cast<int>(v) - static_cast<int>(list[k]);
        const int ag = gap < 0 ? -gap : gap;
        if (ag <= kTolerancePx && ag < best_abs) {
            best_abs = ag;
            best_gap = gap;
            found = true;
        }
    }
    if (found) out_gap = best_gap;
    return found;
}

// Boyer-Moore majority vote: O(1) state per address instead of a 129-bin
// histogram per address per axis (which would be 147,456 slots * 129 bins *
// 2 axes * 4 bytes = ~152 MB, not a bounded table). This converges on the
// true mode only when one gap value actually dominates that address's hits;
// that is exactly the case the verdict cares about; it is not a general
// histogram and is only trusted where the top-32 hit rate is already high.
void update_gap_mode(std::int8_t& mode, std::int32_t& count, int gap) {
    const std::int8_t g = static_cast<std::int8_t>(gap);
    if (count == 0) {
        mode = g;
        count = 1;
    } else if (mode == g) {
        ++count;
    } else {
        --count;
    }
}

// The room rect's validity test, duplicated from room_buffer.cpp's
// rect_is_room (that function lives in room_buffer.cpp's own anonymous
// namespace and is not exported, so this is the only way to reuse the exact
// test rather than inventing a second one). Keep in sync if that changes.
constexpr std::uint16_t kRoomRectCellPixels = 16;  // matches room_buffer.cpp's kCellPixels
bool room_rect_valid(std::uint16_t min_x, std::uint16_t max_x,
                     std::uint16_t min_y, std::uint16_t max_y) {
    if (max_x == 1 && min_y == 8 && max_y == 256) return false;  // world-map marker
    if (max_x <= min_x || max_y <= min_y) return false;
    return (max_x - min_x) >= kRoomRectCellPixels &&
           (max_y - min_y) >= kRoomRectCellPixels;
}

// Per scan-slot state. Allocated once as static storage, never resized.
// Fields: two exact-match (tolerance-window) hit counters, two delta-match
// hit counters, the first/last observed 16-bit value, two flag bytes, the
// per-axis majority-vote gap state (CHANGE 1), and the per-axis camera-mirror
// counters (CHANGE 2). Layout is 4+4+4+4 bytes of counters, 2+2 bytes of
// values, 1+1 bytes of flags, 1+1 bytes of gap modes, 4+4 bytes of gap-mode
// counts, 4+4 bytes of camera-mirror counters = 40 bytes of fields, no
// padding needed since every field is naturally aligned in this order
// (verified by the static_assert below). 147,456 slots * 40 bytes =
// 5,898,240 bytes (~5.625 MiB) for the table, plus two 8,192-byte membership
// bitmaps (16 KiB, rebuilt each scanned frame) and two 129-entry gap
// histograms (1,032 bytes total). ~5.63 MiB total, fixed for the process
// lifetime.
struct Slot {
    std::uint32_t hits_x = 0;
    std::uint32_t hits_y = 0;
    // Delta-search hit counters (FIX 2): a hit is this slot's frame-to-frame
    // change equalling the camera's frame-to-frame change on that axis.
    std::uint32_t hits_dx = 0;
    std::uint32_t hits_dy = 0;
    std::uint16_t first_value = 0;
    std::uint16_t last_value = 0;  // also doubles as the delta search's
                                    // "previous value" -- no parallel table.
    bool seen = false;
    // A constant cannot be a walking character's position (FACTS.md-style
    // reasoning): tracked so the exact-match report can exclude coincidental
    // matches on an address that never moved. The delta search does not need
    // this flag: a delta hit requires a non-zero camera delta, so a slot that
    // never changes can never score one.
    bool changed = false;
    // CHANGE 1: majority-vote mode of the signed gap (this address's value
    // minus the nearest sprite's world value) across its tolerance-window
    // hits on each axis. See update_gap_mode's comment for why this is a
    // vote, not a full histogram.
    std::int8_t gap_mode_x = 0;
    std::int8_t gap_mode_y = 0;
    std::int32_t gap_mode_count_x = 0;
    std::int32_t gap_mode_count_y = 0;
    // CHANGE 2: how often this address's raw 16-bit value equalled the
    // camera's own integer coordinate this frame, per axis. Counted every
    // frame the memory is scanned, independent of do_exact/eval_dx/eval_dy --
    // this is a same-frame equality test against the camera, not a position
    // or delta candidate, and it exists only to mark camera-mirror rows in
    // the delta report so they are not mistaken for a stored position again.
    std::uint32_t cam_mirror_hits_x = 0;
    std::uint32_t cam_mirror_hits_y = 0;
};
static_assert(sizeof(Slot) == 40,
             "update the size estimate in the comment above if this changes");
std::array<Slot, kTotalSlots> g_slots{};

std::array<std::uint32_t, kBitmapWords> g_bitmap_x{};
std::array<std::uint32_t, kBitmapWords> g_bitmap_y{};

// CHANGE 1: global per-axis gap histogram across every address that ever hit
// the tolerance window, bounded to one bin per integer pixel offset in
// [-64, +64] (kGapBins == 129). Allocated once, never resized.
std::array<std::uint32_t, kGapBins> g_gap_hist_x{};
std::array<std::uint32_t, kGapBins> g_gap_hist_y{};

std::uint64_t g_frames_considered = 0;  // probe ran, regardless of outcome
std::uint64_t g_frames_scanned = 0;     // exact-match: room valid, memory walked
std::uint64_t g_frames_no_room = 0;     // exact-match: skipped, room rect invalid
std::uint64_t g_distinct_x_sum = 0;     // exact-match: distinct in-bitmap values, summed
std::uint64_t g_distinct_y_sum = 0;     // over scanned frames, for the averages report

// Delta search (FIX 2): frame counts are per axis, since the camera can move
// on only one axis in a given frame and a still axis proves nothing.
std::uint64_t g_delta_frames_x = 0;
std::uint64_t g_delta_frames_y = 0;

// CHANGE 2: frames where memory was actually scanned (bus + EWRAM pointer
// valid), the denominator for the camera-mirror marker. Unlike
// g_delta_frames_x/y this is not gated on the camera having moved -- the
// mirror test is same-frame equality, not a delta.
std::uint64_t g_frames_memory_scanned = 0;

std::uint32_t slot_address(std::size_t slot) {
    if (slot < kEwramSlots) return kEwramBase + static_cast<std::uint32_t>(slot * 2u);
    return kIwramBase +
           static_cast<std::uint32_t>((slot - kEwramSlots) * 2u);
}

// One pass over a region does all the per-slot bookkeeping both searches
// need: exact-match (tolerance-window) bitmap membership (gated on do_exact,
// since FIX 1 may have decided this frame has no usable room bound),
// delta-match against the camera's own per-axis delta (gated per axis on
// eval_dx/eval_dy, since FIX 2 only evaluates frames where that axis
// actually moved), and the CHANGE 2 camera-mirror equality test (gated on
// mem_scanned, i.e. run whenever this function runs at all).
//
// Cost note (CHANGE 1): the membership window is now ~129 values wide per
// sprite instead of 1, so more addresses hit the bitmap than before, and
// each hit now runs nearest_gap (<=128 subtractions, bounded by the OAM
// shadow's own slot count) instead of just incrementing a counter. Worst
// case is bounded -- kTotalSlots (147,456) * 128 -- and only reachable when
// nearly every scanned address happens to fall in every sprite's window at
// once, which is itself a degenerate result the honesty guards already call
// out (see print_verdict's plateau warning). This is an opt-in diagnostic
// (GSR_OBJECT_PROBE) with no per-pixel or render-path cost, so the bound
// being loose in the worst case is acceptable.
void scan_region(const std::uint8_t* base, std::size_t bytes,
                 std::size_t slot_base, bool do_exact,
                 bool eval_dx, bool eval_dy,
                 std::int16_t cam_dx, std::int16_t cam_dy,
                 std::uint16_t cam_x_u16, std::uint16_t cam_y_u16,
                 const std::uint16_t* valid_x, std::size_t valid_x_count,
                 const std::uint16_t* valid_y, std::size_t valid_y_count) {
    if (!base) return;
    const std::size_t slot_count = bytes / 2u;
    for (std::size_t i = 0; i < slot_count; ++i) {
        const std::uint16_t v = static_cast<std::uint16_t>(
            base[i * 2u] | (static_cast<std::uint16_t>(base[i * 2u + 1]) << 8));
        Slot& slot = g_slots[slot_base + i];
        const bool had_prev = slot.seen;
        const std::uint16_t prev = slot.last_value;
        if (had_prev) {
            if (prev != v) slot.changed = true;
        } else {
            slot.first_value = v;
            slot.seen = true;
        }
        slot.last_value = v;

        // FIX 1 / CHANGE 1: one bitmap test per axis per address, then (only
        // on a hit) which sprite within tolerance it is closest to, so the
        // signed gap can be recorded both into this address's majority vote
        // and the global per-axis histogram.
        if (do_exact) {
            if (test_bit(g_bitmap_x, v)) {
                ++slot.hits_x;
                int gap = 0;
                if (nearest_gap(valid_x, valid_x_count, v, gap)) {
                    ++g_gap_hist_x[static_cast<std::size_t>(gap + kTolerancePx)];
                    update_gap_mode(slot.gap_mode_x, slot.gap_mode_count_x, gap);
                }
            }
            if (test_bit(g_bitmap_y, v)) {
                ++slot.hits_y;
                int gap = 0;
                if (nearest_gap(valid_y, valid_y_count, v, gap)) {
                    ++g_gap_hist_y[static_cast<std::size_t>(gap + kTolerancePx)];
                    update_gap_mode(slot.gap_mode_y, slot.gap_mode_count_y, gap);
                }
            }
        }

        // CHANGE 2: camera-mirror marker. Same-frame equality, not a delta;
        // runs whenever this function runs, independent of do_exact/eval_dx/
        // eval_dy.
        if (v == cam_x_u16) ++slot.cam_mirror_hits_x;
        if (v == cam_y_u16) ++slot.cam_mirror_hits_y;

        // FIX 2: delta search. Needs a previous value, hence had_prev.
        if (had_prev && (eval_dx || eval_dy)) {
            const std::int16_t delta = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(v - prev));
            if (eval_dx && delta == cam_dx) ++slot.hits_dx;
            if (eval_dy && delta == cam_dy) ++slot.hits_dy;
        }
    }
}

// Top-32 report entry for one axis.
struct TopEntry {
    std::uint32_t address = 0;
    std::uint32_t hits = 0;
    double rate = 0.0;
    // CHANGE 1, exact search only: this address's majority-vote gap. Valid
    // whenever hits > 0 (build_top only includes such rows).
    int gap_mode = 0;
    // CHANGE 2, delta search only: true when this address's raw value
    // equalled the camera's own value on this axis for most evaluated
    // frames (same threshold as print_verdict's real-position candidates,
    // for the same reason: a genuine mirror repeats essentially every frame,
    // not just often).
    bool camera_mirror = false;
};

// Verdict threshold for the plain-language summary line, and (CHANGE 2) for
// the camera-mirror marker. Chosen because a real per-frame position field
// (or a real camera mirror) must be readable essentially every frame it is
// scanned (the game needs it every frame to place the sprite, or to mirror
// the camera), while a coincidental byte match has no reason to land so
// consistently; nothing is filtered by this number, it only changes wording
// and which rows get marked.
constexpr double kVerdictThresholdPct = 90.0;

// A candidate must have changed during the session to be reported at all
// (FACTS.md-style reasoning above); this is the filter, and it is the only
// one. Ties broken by lower address, purely so repeated runs print in a
// stable order.
void build_top(bool is_x, std::array<TopEntry, 32>& out, int& out_count) {
    out_count = 0;
    for (std::size_t i = 0; i < kTotalSlots; ++i) {
        const Slot& slot = g_slots[i];
        if (!slot.changed) continue;
        const std::uint32_t hits = is_x ? slot.hits_x : slot.hits_y;
        if (hits == 0) continue;
        const double rate = g_frames_scanned == 0 ? 0.0 :
            100.0 * static_cast<double>(hits) /
                static_cast<double>(g_frames_scanned);
        TopEntry entry{slot_address(i), hits, rate};
        // CHANGE 1: this address's majority-vote gap (see update_gap_mode).
        entry.gap_mode = is_x ? slot.gap_mode_x : slot.gap_mode_y;
        // Insertion into a small sorted (descending rate) top-32 array.
        int insert_at = out_count;
        for (int j = 0; j < out_count; ++j) {
            if (entry.rate > out[j].rate ||
                (entry.rate == out[j].rate && entry.address < out[j].address)) {
                insert_at = j;
                break;
            }
        }
        if (insert_at >= 32) continue;
        const int last = std::min(out_count, 31);
        for (int j = last; j > insert_at; --j) out[j] = out[j - 1];
        out[insert_at] = entry;
        if (out_count < 32) ++out_count;
    }
}

// Same top-32 selection as build_top, but for the delta search: the hit
// counters are hits_dx/hits_dy and the denominator is the per-axis evaluated
// frame count (g_delta_frames_x/g_delta_frames_y), not g_frames_scanned. No
// "changed" filter is needed: a delta hit requires the slot to have moved by
// the camera's own non-zero delta, so a constant address can never score one.
void build_top_delta(bool is_x, std::array<TopEntry, 32>& out, int& out_count) {
    out_count = 0;
    const std::uint64_t denom = is_x ? g_delta_frames_x : g_delta_frames_y;
    for (std::size_t i = 0; i < kTotalSlots; ++i) {
        const Slot& slot = g_slots[i];
        const std::uint32_t hits = is_x ? slot.hits_dx : slot.hits_dy;
        if (hits == 0) continue;
        const double rate = denom == 0 ? 0.0 :
            100.0 * static_cast<double>(hits) / static_cast<double>(denom);
        TopEntry entry{slot_address(i), hits, rate};
        // CHANGE 2: camera-mirror marker, same threshold as the verdict's
        // real-position candidates and for the same reason (see
        // kVerdictThresholdPct's comment).
        {
            const std::uint32_t cam_hits =
                is_x ? slot.cam_mirror_hits_x : slot.cam_mirror_hits_y;
            const double cam_rate = g_frames_memory_scanned == 0 ? 0.0 :
                100.0 * static_cast<double>(cam_hits) /
                    static_cast<double>(g_frames_memory_scanned);
            entry.camera_mirror = cam_rate >= kVerdictThresholdPct;
        }
        int insert_at = out_count;
        for (int j = 0; j < out_count; ++j) {
            if (entry.rate > out[j].rate ||
                (entry.rate == out[j].rate && entry.address < out[j].address)) {
                insert_at = j;
                break;
            }
        }
        if (insert_at >= 32) continue;
        const int last = std::min(out_count, 31);
        for (int j = last; j > insert_at; --j) out[j] = out[j - 1];
        out[insert_at] = entry;
        if (out_count < 32) ++out_count;
    }
}

void print_axis_report(const char* axis_name, std::uint64_t denom,
                       const std::array<TopEntry, 32>& top, int count,
                       bool show_gap, bool show_mirror) {
    if (count == 0) {
        std::fprintf(stderr, "[object-probe] %s: no address ever hit "
                             "the per-frame membership set\n",
                     axis_name);
        return;
    }
    std::fprintf(stderr, "[object-probe] %s: top %d addresses by hit "
                         "rate (of %llu frames evaluated)\n",
                 axis_name, count, static_cast<unsigned long long>(denom));
    for (int i = 0; i < count; ++i) {
        std::fprintf(stderr,
                     "[object-probe]   0x%08X  hits %u/%llu (%.2f%%)",
                     top[i].address, top[i].hits,
                     static_cast<unsigned long long>(denom),
                     top[i].rate);
        if (show_gap) {
            std::fprintf(stderr, "  most common gap %+dpx", top[i].gap_mode);
        }
        if (show_mirror && top[i].camera_mirror) {
            std::fprintf(stderr, "  [camera-mirror: matches camera >=%.0f%% "
                                 "of scanned frames -- see FACTS.md, not a "
                                 "stored position]",
                         kVerdictThresholdPct);
        }
        std::fprintf(stderr, "\n");
    }
}

// CHANGE 1: the per-axis gap histogram across every address that ever hit
// the tolerance window, most frequent gap first. Bounded to kGapBins (129)
// rows -- the whole table, no allocation.
void print_gap_histogram(const char* axis_name,
                         const std::array<std::uint32_t, kGapBins>& hist) {
    std::array<std::pair<int, std::uint32_t>, kGapBins> rows{};
    std::size_t rows_n = 0;
    for (std::size_t i = 0; i < kGapBins; ++i) {
        if (hist[i] == 0) continue;
        rows[rows_n++] = {static_cast<int>(i) - kTolerancePx, hist[i]};
    }
    if (rows_n == 0) {
        std::fprintf(stderr,
                     "[object-probe] %s gap histogram: no tolerance-window "
                     "hits were recorded\n",
                     axis_name);
        return;
    }
    std::sort(rows.begin(), rows.begin() + rows_n,
             [](const std::pair<int, std::uint32_t>& a,
                const std::pair<int, std::uint32_t>& b) {
                 if (a.second != b.second) return a.second > b.second;
                 return a.first < b.first;
             });
    std::fprintf(stderr,
                 "[object-probe] %s gap histogram (%zu distinct gap(s), most "
                 "frequent first):\n",
                 axis_name, rows_n);
    for (std::size_t i = 0; i < rows_n; ++i) {
        std::fprintf(stderr, "[object-probe]   gap %+dpx: %u hit(s)\n",
                     rows[i].first, rows[i].second);
    }
}

// CHANGE 1 verdict, replacing print_verdict for the tolerance search: name
// the dominant gap only when a gap bin clearly leads (strictly outscores the
// runner-up -- no invented margin) AND some address also matches within
// tolerance at the same high rate print_verdict already requires. Otherwise
// say plainly that no stored position field was found within tolerance; that
// rules out the position being a plain pixel value in EWRAM/IWRAM on this
// axis, it does not mean the search was inconclusive.
void print_verdict_tolerance(const char* axis_name,
                             const std::array<TopEntry, 32>& top, int count,
                             const std::array<std::uint32_t, kGapBins>& hist) {
    int hi_count = 0;
    for (int i = 0; i < count; ++i) {
        if (top[i].rate >= kVerdictThresholdPct) ++hi_count;
    }
    int best_idx = -1;
    std::uint32_t best_count = 0, second_count = 0;
    for (std::size_t i = 0; i < kGapBins; ++i) {
        if (hist[i] > best_count) {
            second_count = best_count;
            best_count = hist[i];
            best_idx = static_cast<int>(i);
        } else if (hist[i] > second_count) {
            second_count = hist[i];
        }
    }
    const bool dominant_gap = best_idx >= 0 && best_count > second_count;
    if (hi_count > 0 && dominant_gap) {
        std::fprintf(stderr,
                     "[object-probe] %s verdict: dominant gap is %+dpx across "
                     "%d address(es) matching within tolerance at >=%.0f%% "
                     "hit rate -- likely the corner-to-position offset\n",
                     axis_name, best_idx - kTolerancePx, hi_count,
                     kVerdictThresholdPct);
    } else {
        std::fprintf(stderr,
                     "[object-probe] %s verdict: no stored position field "
                     "was found within tolerance -- this rules out the "
                     "position being held as a plain pixel value in EWRAM "
                     "or IWRAM on this axis; it does not mean the search "
                     "was inconclusive\n",
                     axis_name);
    }
}

void print_verdict(const char* axis_name, const std::array<TopEntry, 32>& top,
                   int count) {
    // Candidates worth calling out: at/above the verdict threshold.
    std::array<std::uint32_t, 32> hi{};
    int hi_count = 0;
    for (int i = 0; i < count && hi_count < 32; ++i) {
        if (top[i].rate >= kVerdictThresholdPct) hi[hi_count++] = top[i].address;
    }
    if (hi_count == 0) {
        std::fprintf(stderr,
                     "[object-probe] %s verdict: nothing cleared %.0f%% -- if "
                     "the top rates form a flat plateau across unrelated "
                     "addresses, that means this search is matching something "
                     "degenerate, not that the game lacks the field\n",
                     axis_name, kVerdictThresholdPct);
        return;
    }
    std::sort(hi.begin(), hi.begin() + hi_count);
    bool even_stride = hi_count >= 2;
    std::uint32_t stride = hi_count >= 2 ? hi[1] - hi[0] : 0;
    for (int i = 1; even_stride && i < hi_count; ++i) {
        if (hi[i] - hi[i - 1] != stride) even_stride = false;
    }
    if (hi_count == 1) {
        std::fprintf(stderr,
                     "[object-probe] %s verdict: 0x%08X is a candidate real-"
                     "position field (>=%.0f%% hit rate)\n",
                     axis_name, hi[0], kVerdictThresholdPct);
    } else if (even_stride) {
        std::fprintf(stderr,
                     "[object-probe] %s verdict: %d candidate real-position "
                     "fields (>=%.0f%% hit rate), evenly spaced %u bytes apart "
                     "starting at 0x%08X -- this is the object array's %s "
                     "field and stride\n",
                     axis_name, hi_count, kVerdictThresholdPct, stride, hi[0],
                     axis_name);
    } else {
        std::fprintf(stderr,
                     "[object-probe] %s verdict: %d candidate real-position "
                     "fields (>=%.0f%% hit rate), NOT evenly spaced -- list "
                     "them individually, do not assume one array\n",
                     axis_name, hi_count, kVerdictThresholdPct);
    }
}

void report_at_exit() {
    if (!g_enabled) return;
    std::fprintf(stderr,
                 "[object-probe] %llu frames considered, %llu exact-match "
                 "scanned (room rect valid), %llu skipped (no valid room "
                 "rect), EWRAM 0x%08X-0x%08X + IWRAM 0x%08X-0x%08X, %zu scan "
                 "slots, %zu bytes of tables\n",
                 static_cast<unsigned long long>(g_frames_considered),
                 static_cast<unsigned long long>(g_frames_scanned),
                 static_cast<unsigned long long>(g_frames_no_room),
                 kEwramBase, kEwramBase + static_cast<std::uint32_t>(kEwramBytes),
                 kIwramBase, kIwramBase + static_cast<std::uint32_t>(kIwramBytes),
                 kTotalSlots, sizeof(g_slots));
    if (g_frames_considered == 0) {
        std::fprintf(stderr,
                     "[object-probe] the probe never ran -- nothing was "
                     "scanned\n");
        return;
    }

    // ---- exact-value (tolerance) search (FIX 1: zero and out-of-room world
    // values excluded before any window is built; CHANGE 1: the exact value
    // is widened to a +/-64px window per sprite, since a sprite's OAM corner
    // and the character's stored position are not the same point) ----------
    std::fprintf(stderr, "[object-probe] ==== exact-value search "
                         "(+/-%dpx tolerance) ====\n", kTolerancePx);
    if (g_frames_scanned == 0) {
        std::fprintf(stderr,
                     "[object-probe] exact: no frame had both an "
                     "authenticated sprite and a valid room rect -- nothing "
                     "was scanned\n");
    } else {
        std::fprintf(stderr,
                     "[object-probe] exact: bitmap held %.2f distinct x "
                     "values and %.2f distinct y values per scanned frame on "
                     "average (a handful is trustworthy; a hundred is not)\n",
                     static_cast<double>(g_distinct_x_sum) /
                         static_cast<double>(g_frames_scanned),
                     static_cast<double>(g_distinct_y_sum) /
                         static_cast<double>(g_frames_scanned));
        std::array<TopEntry, 32> top_x{}, top_y{};
        int count_x = 0, count_y = 0;
        build_top(/*is_x=*/true, top_x, count_x);
        build_top(/*is_x=*/false, top_y, count_y);
        print_axis_report("exact x", g_frames_scanned, top_x, count_x,
                          /*show_gap=*/true, /*show_mirror=*/false);
        print_gap_histogram("exact x", g_gap_hist_x);
        print_verdict_tolerance("exact x", top_x, count_x, g_gap_hist_x);
        print_axis_report("exact y", g_frames_scanned, top_y, count_y,
                          /*show_gap=*/true, /*show_mirror=*/false);
        print_gap_histogram("exact y", g_gap_hist_y);
        print_verdict_tolerance("exact y", top_y, count_y, g_gap_hist_y);
    }

    // ---- delta search (FIX 2: matches on frame-to-frame change, not on the
    // raw value, so a different origin from the camera does not hide it;
    // CHANGE 2: rows that are actually mirroring the camera are marked, not
    // removed, so they stop being mistaken for a stored position) ---------
    std::fprintf(stderr, "[object-probe] ==== delta search ====\n");
    if (g_delta_frames_x == 0 && g_delta_frames_y == 0) {
        std::fprintf(stderr,
                     "[object-probe] delta: the camera never moved on either "
                     "axis while scanned -- nothing was evaluated\n");
    } else {
        std::array<TopEntry, 32> dtop_x{}, dtop_y{};
        int dcount_x = 0, dcount_y = 0;
        build_top_delta(/*is_x=*/true, dtop_x, dcount_x);
        build_top_delta(/*is_x=*/false, dtop_y, dcount_y);
        print_axis_report("delta x", g_delta_frames_x, dtop_x, dcount_x,
                          /*show_gap=*/false, /*show_mirror=*/true);
        print_verdict("delta x", dtop_x, dcount_x);
        print_axis_report("delta y", g_delta_frames_y, dtop_y, dcount_y,
                          /*show_gap=*/false, /*show_mirror=*/true);
        print_verdict("delta y", dtop_y, dcount_y);
    }
}

}  // namespace

void object_probe_init() {
    const char* e = std::getenv("GSR_OBJECT_PROBE");
    g_enabled = e != nullptr && e[0] != '\0' && e[0] != '0';
    if (!g_enabled) return;
    std::atexit(report_at_exit);
}

void object_probe_on_entry(std::uint32_t entry_pc) {
    (void)entry_pc;
    if (!g_enabled) return;  // OFF: one predictable branch, nothing else.

    static std::uint64_t s_last_frame = ~std::uint64_t{0};
    const std::uint64_t frame = runtime_current_frame();
    if (frame == s_last_frame) return;
    s_last_frame = frame;
    ++g_frames_considered;

    // Up to 128 hardware OAM slots; see the accessor's own comment in
    // runner_main.cpp for why this is a live OAM read rather than a cached
    // per-frame count.
    std::array<std::int32_t, gsr::widescreen::kGoldenSunOamShadowSlotCount>
        screen_x{}, screen_y{};
    const std::size_t n = gsr_golden_sun_obj_authenticated_positions(
        screen_x.data(), screen_y.data(), screen_x.size());
    // NOTE: no early return when n == 0. The exact-match search needs an
    // authenticated sprite, but the delta search does NOT -- and skipping the
    // whole scan on those frames would silently corrupt it. Every slot's
    // previous value is captured by the scan itself, so a skipped frame makes
    // the next delta span two frames while the camera delta spans one, and
    // they stop agreeing for reasons that have nothing to do with the memory
    // being searched. 317 of 2,083 frames were skipped this way in session
    // 20260911_145030 (15%), each one poisoning the frame after it. The
    // exact-match half is gated below instead, by do_exact.

    const gba::GbaBus* bus = gbarecomp::active_bus();
    if (!bus) return;
    const std::uint8_t* ew = bus->ewram_ptr();
    if (!ew) return;
    // Camera decode identical to room_buffer.cpp: 16.16 fixed point, x then
    // y, integer part is the high 16 bits of each 32-bit field.
    const auto read_u16 = [&](std::uint32_t address) -> std::uint16_t {
        const std::uint32_t off = (address - kEwramBase) & (kEwramBytes - 1u);
        return static_cast<std::uint16_t>(ew[off] | (ew[off + 1] << 8));
    };
    const std::int32_t cam_x = static_cast<std::int32_t>(read_u16(kCamera + 2u));
    const std::int32_t cam_y = static_cast<std::int32_t>(read_u16(kCamera + 6u));

    // FIX 2 setup: the camera's own frame-to-frame delta, independent of
    // whether the room rect below is valid -- movement does not care about
    // the room, only about the previous frame's camera position.
    static bool s_have_prev_cam = false;
    static std::uint16_t s_prev_cam_x = 0, s_prev_cam_y = 0;
    std::int16_t cam_dx = 0, cam_dy = 0;
    const bool have_cam_delta = s_have_prev_cam;
    if (have_cam_delta) {
        cam_dx = static_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(cam_x) - s_prev_cam_x));
        cam_dy = static_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(cam_y) - s_prev_cam_y));
    }
    s_prev_cam_x = static_cast<std::uint16_t>(cam_x);
    s_prev_cam_y = static_cast<std::uint16_t>(cam_y);
    s_have_prev_cam = true;
    // A still camera on an axis proves nothing about that axis (every
    // constant address would "match" a zero delta), so only evaluate axes
    // that actually moved this frame.
    const bool eval_dx = have_cam_delta && cam_dx != 0;
    const bool eval_dy = have_cam_delta && cam_dy != 0;
    if (eval_dx) ++g_delta_frames_x;
    if (eval_dy) ++g_delta_frames_y;

    // FIX 1: the room rect bounds what a real world position can be. Same
    // read and validity test room_buffer.cpp uses (kRoomRect layout comment
    // above); skip the exact-match search entirely this frame rather than
    // search with no bound if the rect does not describe a real room.
    const std::uint16_t rect_min_x = read_u16(kRoomRect);
    const std::uint16_t rect_max_x = read_u16(kRoomRect + 2u);
    const std::uint16_t rect_min_y = read_u16(kRoomRect + 4u);
    const std::uint16_t rect_max_y = read_u16(kRoomRect + 6u);
    const bool do_exact =
        n > 0 && room_rect_valid(rect_min_x, rect_max_x, rect_min_y, rect_max_y);

    // CHANGE 1: the sprites that actually fed the bitmaps this frame, kept
    // for the nearest-sprite gap lookup in scan_region. Bounded to the OAM
    // shadow's own slot count, stack-allocated, read-only once built.
    std::array<std::uint16_t, gsr::widescreen::kGoldenSunOamShadowSlotCount>
        valid_x{}, valid_y{};
    std::size_t valid_x_count = 0, valid_y_count = 0;

    if (do_exact) {
        for (auto& word : g_bitmap_x) word = 0;
        for (auto& word : g_bitmap_y) word = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint16_t world_x =
                static_cast<std::uint16_t>(cam_x + screen_x[i]);
            const std::uint16_t world_y =
                static_cast<std::uint16_t>(cam_y + screen_y[i]);
            // Skip the degenerate zero value and anything outside the room:
            // a world position the room rect itself rules out is not a
            // position this search should be matching against (this is the
            // fix for the flat-plateau flaw -- see report_at_exit). These
            // tests are applied to the WORLD value only, once per sprite --
            // not re-applied to each memory value the tolerance window below
            // admits.
            if (world_x != 0 && world_x >= rect_min_x && world_x < rect_max_x) {
                if (valid_x_count < valid_x.size()) valid_x[valid_x_count++] = world_x;
                // CHANGE 1: allow the fixed gap between a stored position and
                // where its sprite is drawn -- set every value within
                // +/-kTolerancePx of this sprite's world position, not just
                // the exact value.
                const int lo = static_cast<int>(world_x) - kTolerancePx;
                const int hi = static_cast<int>(world_x) + kTolerancePx;
                for (int wv = lo; wv <= hi; ++wv) {
                    if (wv >= 0 && wv <= 0xFFFF) {
                        set_bit(g_bitmap_x, static_cast<std::uint16_t>(wv));
                    }
                }
            }
            if (world_y != 0 && world_y >= rect_min_y && world_y < rect_max_y) {
                if (valid_y_count < valid_y.size()) valid_y[valid_y_count++] = world_y;
                const int lo = static_cast<int>(world_y) - kTolerancePx;
                const int hi = static_cast<int>(world_y) + kTolerancePx;
                for (int wv = lo; wv <= hi; ++wv) {
                    if (wv >= 0 && wv <= 0xFFFF) {
                        set_bit(g_bitmap_y, static_cast<std::uint16_t>(wv));
                    }
                }
            }
        }
        ++g_frames_scanned;
        g_distinct_x_sum += popcount_bitmap(g_bitmap_x);
        g_distinct_y_sum += popcount_bitmap(g_bitmap_y);
    } else {
        ++g_frames_no_room;
    }

    // CHANGE 2: camera-mirror comparison values, the same integer-part decode
    // already used above -- both fit in 16 bits by construction (read_u16).
    const std::uint16_t cam_x_u16 = static_cast<std::uint16_t>(cam_x);
    const std::uint16_t cam_y_u16 = static_cast<std::uint16_t>(cam_y);
    ++g_frames_memory_scanned;

    scan_region(ew, kEwramBytes, 0u, do_exact, eval_dx, eval_dy, cam_dx, cam_dy,
                cam_x_u16, cam_y_u16, valid_x.data(), valid_x_count,
                valid_y.data(), valid_y_count);
    scan_region(bus->iwram_ptr(), kIwramBytes, kEwramSlots, do_exact, eval_dx,
                eval_dy, cam_dx, cam_dy, cam_x_u16, cam_y_u16, valid_x.data(),
                valid_x_count, valid_y.data(), valid_y_count);
}

}  // namespace gsr
