#include "gba_vram_trace.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "gba_io.h"
#include "bus.h"

extern "C" unsigned long long g_runtime_cycles;
extern "C" unsigned long long runtime_current_frame();

namespace gba::vram_trace {
namespace {

int g_default_enabled = 0;
int g_env_override = -1;
DmaDescriptorObserver g_dma_descriptor_observer = nullptr;
OamShadowWriteObserver g_oam_shadow_write_observer = nullptr;
OamShadowWriteRangePredicate g_oam_shadow_write_range_predicate = nullptr;
unsigned g_trace_windows = 1u;
constexpr unsigned kMaxTraceWindows = 16u;

bool g_text_trace_enabled = false;
bool g_text_trace_open_failed = false;
bool g_text_trace_atexit_registered = false;
std::string g_text_trace_directory;
std::string g_text_trace_buffer;
std::FILE* g_text_trace_file = nullptr;
std::uint64_t g_text_trace_sequence = 0;
std::size_t g_text_trace_pending_rows = 0;

// Match the existing function-tracer signal flush cadence. This is a flush
// cadence only; the address range remains the live BG0 range selected by the
// guest's display registers and no payload is retained.
constexpr std::size_t kTextTraceFlushRows = 300u;

constexpr std::uint32_t kTextRegionChar = 1u;
constexpr std::uint32_t kTextRegionScreenMap = 2u;

void flush_text_trace();

std::uint16_t load16(const std::uint8_t* io, std::uint32_t off) {
    return static_cast<std::uint16_t>(io[off] |
                                      (std::uint16_t(io[off + 1u]) << 8));
}

bool trace_enabled() {
    if (g_env_override < 0) {
        const char* env = std::getenv("GBARECOMP_VRAM_MAP_TRACE");
        if (env) {
            g_env_override = env[0] != '\0' && env[0] != '0' ? 1 : 0;
        }
    }
    return (g_env_override >= 0 ? g_env_override : g_default_enabled) != 0;
}

bool mode0_field(const std::uint8_t* io) {
    if (!io) return false;
    const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
    // Mode 0 and at least one of the field layers BG1..BG3 enabled.
    return (dispcnt & 0x7u) == 0u && (dispcnt & 0x0E00u) != 0u;
}

bool mode2_affine(const std::uint8_t* io) {
    if (!io) return false;
    const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
    // Mode 2 and at least one of the affine layers BG2/BG3 enabled.
    return (dispcnt & 0x7u) == 2u && (dispcnt & 0x0C00u) != 0u;
}

unsigned block_count(unsigned size) {
    switch (size & 3u) {
        case 1u: return 2u;  // 512x256
        case 2u: return 2u;  // 256x512
        case 3u: return 4u;  // 512x512
        default: return 1u;  // 256x256
    }
}

unsigned block_offset(unsigned size, unsigned index) {
    // A vertical 512x256 text map uses the next screenblock row (+2), not
    // the horizontally adjacent block (+1).
    if ((size & 3u) == 2u && index == 1u) return 2u;
    return index;
}

bool regular_bg_range_intersects(const std::uint8_t* io, unsigned bg,
                                std::uint32_t address,
                                std::uint64_t size) {
    const std::uint16_t cnt = load16(io, IoReg::BG0CNT + bg * 2u);
    const unsigned base = (cnt >> 8) & 0x1Fu;
    const unsigned map_size = (cnt >> 14) & 3u;
    const unsigned blocks = block_count(map_size);
    const std::uint64_t first = address;
    const std::uint64_t last = first + size;
    for (unsigned i = 0; i < blocks; ++i) {
        const std::uint64_t block = static_cast<std::uint64_t>(
            base + block_offset(map_size, i));
        const std::uint64_t map_first = kVramStart + block * 0x800ull;
        const std::uint64_t map_last = map_first + 0x800ull;
        if (first < map_last && last > map_first) return true;
    }
    return false;
}

// Affine BG2/BG3 maps are one contiguous screenblock run (no vertical
// screenblock-row aliasing like text mode), (16 << size_code) tiles square
// at one byte per map entry, so the map is (16 << size_code)^2 bytes
// starting at the BGCNT screen base. Derived live from BGCNT; never a fixed
// screen base or size.
bool affine_range_intersects(const std::uint8_t* io, unsigned bg,
                             std::uint32_t address, std::uint64_t size) {
    const std::uint16_t cnt = load16(io, IoReg::BG0CNT + bg * 2u);
    const unsigned base = (cnt >> 8) & 0x1Fu;
    const unsigned size_code = (cnt >> 14) & 3u;
    const std::uint64_t tiles_per_side = 16ull << size_code;
    const std::uint64_t map_bytes = tiles_per_side * tiles_per_side;
    const std::uint64_t map_first = kVramStart + static_cast<std::uint64_t>(base) * 0x800ull;
    const std::uint64_t map_last = map_first + map_bytes;
    const std::uint64_t first = address;
    const std::uint64_t last = first + size;
    return first < map_last && last > map_first;
}

bool dma_trace_bounds(std::uint32_t destination, std::uint32_t step,
                      std::uint32_t units, std::uint32_t dest_control,
                      std::uint32_t& first, std::uint32_t& last) {
    if (step == 0u || units == 0u) return false;
    const std::uint64_t span = static_cast<std::uint64_t>(step) * units;
    std::uint64_t lo = destination;
    std::uint64_t hi = destination;
    if (dest_control == 1u) {
        lo = destination >= span - step
            ? static_cast<std::uint64_t>(destination) - (span - step)
            : 0u;
        hi = static_cast<std::uint64_t>(destination) + step;
    } else if (dest_control == 2u) {
        hi = static_cast<std::uint64_t>(destination) + step;
    } else {
        hi = static_cast<std::uint64_t>(destination) + span;
    }
    if (lo > 0xFFFFFFFFull || hi > 0xFFFFFFFFull) return false;
    first = static_cast<std::uint32_t>(lo);
    last = static_cast<std::uint32_t>(hi);
    return true;
}

std::uint16_t bgcnt(const std::uint8_t* io, unsigned bg) {
    return load16(io, IoReg::BG0CNT + bg * 2u);
}

std::uint16_t hofs(const std::uint8_t* io, unsigned bg) {
    return load16(io, 0x10u + bg * 4u);
}

std::uint16_t vofs(const std::uint8_t* io, unsigned bg) {
    return load16(io, 0x12u + bg * 4u);
}

std::uint32_t selected_bg0_text_regions(const std::uint8_t* io,
                                        std::uint32_t address,
                                        std::uint64_t size) {
    if (!io || size == 0u) return 0u;
    const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
    const unsigned mode = dispcnt & 7u;
    // BG0 is a regular text layer in display modes 0 and 1. Keep tracing its
    // selected ranges while hidden too: menu/font uploads can precede the
    // display enable write that makes them visible.
    if (mode > 1u) return 0u;

    const std::uint16_t cnt = load16(io, IoReg::BG0CNT);
    const unsigned char_block = (cnt >> 2) & 3u;
    const std::uint64_t char_first =
        kVramStart + static_cast<std::uint64_t>(char_block) * 0x4000ull;
    // Follow the renderer's tile addressing and physical VRAM clamp: tile
    // indices are 10 bits, each tile is 32 bytes in 4bpp or 64 bytes in
    // 8bpp, and reads at/after 96 KiB are skipped.
    const std::uint64_t char_span = (cnt & 0x0080u) != 0u
        ? 0x10000ull : 0x8000ull;
    const std::uint64_t char_last =
        std::min<std::uint64_t>(char_first + char_span, kVramEnd);
    const std::uint64_t first = address;
    const std::uint64_t last = first + size;
    std::uint32_t regions = 0u;
    if (first < char_last && last > char_first)
        regions |= kTextRegionChar;
    if (regular_bg_range_intersects(io, 0u, address, size))
        regions |= kTextRegionScreenMap;
    return regions;
}

const char* text_region_name(std::uint32_t regions) {
    switch (regions) {
        case kTextRegionChar: return "char";
        case kTextRegionScreenMap: return "screenmap";
        case kTextRegionChar | kTextRegionScreenMap: return "char+screenmap";
        default: return "unknown";
    }
}

bool ensure_text_trace_file() {
    if (!g_text_trace_enabled || g_text_trace_open_failed) return false;
    if (g_text_trace_file) return true;
    const std::string path = g_text_trace_directory + "/text_vram_writes.csv";
    g_text_trace_file = std::fopen(path.c_str(), "wb");
    if (!g_text_trace_file) {
        g_text_trace_open_failed = true;
        return false;
    }
    static constexpr char kHeader[] =
        "sequence,frame,cycle,source_kind,pc,address,range_size,dma_channel,"
        "dma_source,dma_destination,dma_bytes,dma_control,dma_start_mode,dispcnt,"
        "bg0cnt,bg0hofs,bg0vofs,char_base,screen_base,region\n";
    if (std::fputs(kHeader, g_text_trace_file) == EOF) {
        std::fclose(g_text_trace_file);
        g_text_trace_file = nullptr;
        g_text_trace_open_failed = true;
        return false;
    }
    return true;
}

void append_text_trace_row(const std::uint8_t* io, const char* source_kind,
                           std::uint32_t pc, std::uint32_t address,
                           std::uint32_t range_size, int channel,
                           std::uint32_t dma_source,
                           std::uint32_t dma_destination,
                           std::uint32_t dma_bytes,
                           std::uint16_t dma_control, int start_mode,
                           std::uint32_t regions) {
    if (!g_text_trace_enabled || !ensure_text_trace_file()) return;
    const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
    const std::uint16_t cnt = load16(io, IoReg::BG0CNT);
    const unsigned char_block = (cnt >> 2) & 3u;
    const unsigned screen_block = (cnt >> 8) & 0x1Fu;
    const std::uint32_t char_base = static_cast<std::uint32_t>(
        kVramStart + static_cast<std::uint32_t>(char_block) * 0x4000u);
    const std::uint32_t screen_base = static_cast<std::uint32_t>(
        kVramStart + static_cast<std::uint32_t>(screen_block) * 0x800u);
    char line[512];
    const int written = std::snprintf(
        line, sizeof(line),
        "%llu,%llu,%llu,%s,0x%08X,0x%08X,%u,%d,0x%08X,0x%08X,%u,0x%04X,%d,"
        "0x%04X,0x%04X,%u,%u,0x%08X,0x%08X,%s\n",
        static_cast<unsigned long long>(g_text_trace_sequence++),
        static_cast<unsigned long long>(runtime_current_frame()),
        static_cast<unsigned long long>(g_runtime_cycles), source_kind,
        static_cast<unsigned>(pc), static_cast<unsigned>(address),
        static_cast<unsigned>(range_size), channel,
        static_cast<unsigned>(dma_source),
        static_cast<unsigned>(dma_destination),
        static_cast<unsigned>(dma_bytes),
        static_cast<unsigned>(dma_control), start_mode,
        static_cast<unsigned>(dispcnt), static_cast<unsigned>(cnt),
        static_cast<unsigned>(hofs(io, 0u)), static_cast<unsigned>(vofs(io, 0u)),
        static_cast<unsigned>(char_base), static_cast<unsigned>(screen_base),
        text_region_name(regions));
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(line)) {
        return;
    }
    g_text_trace_buffer.append(line, static_cast<std::size_t>(written));
    ++g_text_trace_pending_rows;
    if (g_text_trace_pending_rows >= kTextTraceFlushRows)
        flush_text_trace();
}

void flush_text_trace() {
    if (!g_text_trace_file || g_text_trace_buffer.empty()) return;
    const std::size_t bytes = std::fwrite(g_text_trace_buffer.data(), 1,
                                          g_text_trace_buffer.size(),
                                          g_text_trace_file);
    if (bytes != g_text_trace_buffer.size()) {
        g_text_trace_open_failed = true;
    } else {
        g_text_trace_buffer.clear();
        g_text_trace_pending_rows = 0;
        std::fflush(g_text_trace_file);
    }
}

unsigned& cpu_records() {
    static unsigned n = 0;
    return n;
}

unsigned& dma_records() {
    static unsigned n = 0;
    return n;
}

// Independent budgets so an overworld (Mode 2 affine) session cannot exhaust
// the Mode 0 field trace's record budget, or vice versa.
unsigned& affine_cpu_records() {
    static unsigned n = 0;
    return n;
}

unsigned& affine_dma_records() {
    static unsigned n = 0;
    return n;
}

// A per-writer budget for the Mode 0 stream, replacing a single shared count.
// The point of this trace is to ENUMERATE the writers, not to sample writes,
// and a shared count does not do that: in session_20260905_092954 one chatty
// writer pair (`0x0800FF1E` / `0x0800FF26`, the BG3 tilemap builder) took all
// 512 records at a single room load, so the BG1 and BG2 writers were never
// recorded at all. The two capacities below are how many distinct writers to
// keep room for and how many examples of each are enough to read its
// addressing pattern — sizes of the answer wanted, not thresholds tuned to a
// measurement.
constexpr unsigned kMode0MaxWriters = 64u;
constexpr unsigned kMode0RecordsPerWriter = 4u;

std::uint32_t g_mode0_pcs[kMode0MaxWriters] = {};
unsigned g_mode0_counts[kMode0MaxWriters] = {};
unsigned g_mode0_used = 0;

bool mode0_pc_budget(std::uint32_t pc) {
    for (unsigned i = 0; i < g_mode0_used; ++i) {
        if (g_mode0_pcs[i] != pc) continue;
        if (g_mode0_counts[i] >= kMode0RecordsPerWriter) return false;
        ++g_mode0_counts[i];
        return true;
    }
    if (g_mode0_used >= kMode0MaxWriters) return false;
    g_mode0_pcs[g_mode0_used] = pc;
    g_mode0_counts[g_mode0_used] = 1;
    ++g_mode0_used;
    return true;
}

void reset_mode0_pc_budget() { g_mode0_used = 0; }

// Per-visit budgets, not per-session. In session_20260905_084755 all 512
// affine CPU records were spent between cycle 7,878,553 and 57,203,487 --
// the first ~3.4 s of a run that reached cycle 595M, i.e. boot, before the
// savestate load -- so the overworld play the session was captured for
// recorded nothing at all. Reset both affine budgets on each transition
// into an affine Mode 2 scene so every visit gets its own records.
void note_affine_scene_edge(const std::uint8_t* io) {
    static bool s_was_affine = false;
    const bool now_affine = mode2_affine(io);
    if (now_affine && !s_was_affine) {
        affine_cpu_records() = 0;
        affine_dma_records() = 0;
    }
    s_was_affine = now_affine;
}

unsigned& dma_depth() {
    static unsigned n = 0;
    return n;
}

constexpr std::uint32_t kOamShadowStart = 0x0300347Cu;
constexpr std::uint32_t kOamShadowEnd   = 0x0300387Cu;
constexpr std::uint32_t kOamStart = 0x07000000u;
constexpr std::uint32_t kOamEnd = 0x07000400u;
constexpr std::size_t kOamSlotBytes = 8u;
constexpr std::size_t kOamSlotCount = 128u;

OamShadowTraceStats g_oam_shadow_stats{};
OamDmaTraceStats g_oam_dma_stats{};
OamAttr0TraceStats g_oam_attr0_stats{};
std::array<OamAttr0Provenance, kOamSlotCount> g_oam_attr0_provenance{};
std::uint64_t g_oam_attr0_generation = 0;
std::array<bool, kOamSlotCount> g_oam_shadow_slot_seen{};
std::array<bool, kOamSlotCount> g_oam_shadow_slot_overwritten{};
unsigned g_oam_shadow_records = 0;
unsigned g_oam_dma_records = 0;

bool oam_shadow_trace_enabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* shadow_env = std::getenv("GSR_OAM_SHADOW_TRACE");
        const char* wide_env = std::getenv("GBARECOMP_VRAM_MAP_TRACE");
        const bool shadow = shadow_env && shadow_env[0] != '\0' &&
            shadow_env[0] != '0';
        const bool wide = wide_env && wide_env[0] != '\0' &&
            wide_env[0] != '0';
        // The WIDE-01 launcher toggle arms the complete payload-free object
        // capture. Keep the older standalone OAM toggle as a compatible alias.
        cached = (shadow || wide) ? 1 : 0;
    }
    return cached != 0;
}

std::uint32_t normalize_oam_shadow_address(std::uint32_t address) {
    return (address >> 24) == 0x03u
        ? 0x03000000u + (address & 0x00007FFFu) : address;
}

unsigned& oam_shadow_records() {
    return g_oam_shadow_records;
}

bool oam_dma_bounds(std::uint32_t destination, std::uint32_t step,
                    std::uint32_t units, std::uint32_t dest_control,
                    std::uint32_t& first, std::uint32_t& last) {
    if (step == 0u || units == 0u) return false;
    const std::uint64_t span = static_cast<std::uint64_t>(step) * units;
    std::uint64_t lo = destination;
    std::uint64_t hi = destination;
    if (dest_control == 1u) {
        lo = destination >= span - step
            ? static_cast<std::uint64_t>(destination) - (span - step)
            : 0u;
        hi = static_cast<std::uint64_t>(destination) + step;
    } else if (dest_control == 2u) {
        hi = static_cast<std::uint64_t>(destination) + step;
    } else {
        hi = static_cast<std::uint64_t>(destination) + span;
    }
    if (lo > 0xFFFFFFFFull || hi > 0xFFFFFFFFull || hi <= lo) return false;
    first = static_cast<std::uint32_t>(lo);
    last = static_cast<std::uint32_t>(hi);
    return first < kOamEnd && last > kOamStart;
}

struct OamSlotCounts {
    std::uint32_t used = 0;
    std::uint32_t visible = 0;
    std::uint32_t nonzero = 0;
    std::uint32_t raw_x_ge_240 = 0;
    std::uint32_t raw_y_ge_160 = 0;
};

void note_oam_attr0_write(std::uint32_t pc, std::uint32_t address,
                          std::uint32_t size, OamAttr0WriterKind kind) {
    address = normalize_oam_shadow_address(address);
    const std::uint64_t first = address;
    const std::uint64_t last = first + size;
    if (size == 0u || first >= kOamShadowEnd || last <= kOamShadowStart)
        return;
    const std::uint64_t clipped_first =
        std::max<std::uint64_t>(first, kOamShadowStart);
    const std::uint64_t clipped_last =
        std::min<std::uint64_t>(last, kOamShadowEnd);
    for (std::size_t slot = 0; slot < kOamSlotCount; ++slot) {
        const std::uint32_t attr0 = kOamShadowStart +
            static_cast<std::uint32_t>(slot * kOamSlotBytes);
        const std::uint64_t attr0_end = static_cast<std::uint64_t>(attr0) + 2u;
        if (clipped_first >= attr0_end || clipped_last <= attr0) continue;
        OamAttr0Provenance& p = g_oam_attr0_provenance[slot];
        p.kind = kind;
        p.writer_pc = pc;
        p.generation = ++g_oam_attr0_generation;
        p.cycle = g_runtime_cycles;
        p.touched_bytes = 0;
        if (clipped_first <= attr0 && clipped_last > attr0)
            p.touched_bytes |= 1u;
        if (clipped_first < attr0 + 2u && clipped_last > attr0 + 1u)
            p.touched_bytes |= 2u;
    }
}

void note_oam_attr0_candidate(std::uint8_t raw_y, std::size_t slot) {
    if (raw_y < 160u || raw_y > 199u || slot >= kOamSlotCount) return;
    ++g_oam_attr0_stats.candidate_slots;
    if (raw_y == 192u) ++g_oam_attr0_stats.exact_192_slots;
    else ++g_oam_attr0_stats.other_slots;
    const OamAttr0Provenance& p = g_oam_attr0_provenance[slot];
    if (p.kind == OamAttr0WriterKind::Unseen) ++g_oam_attr0_stats.unseen_slots;
    for (std::uint32_t i = 0; i < g_oam_attr0_stats.group_count; ++i) {
        OamAttr0CandidateGroup& g = g_oam_attr0_stats.groups[i];
        if (g.raw_y == raw_y && g.kind == p.kind &&
            g.writer_pc == p.writer_pc) {
            g.slot_first = std::min<std::uint32_t>(g.slot_first,
                                                    static_cast<std::uint32_t>(slot));
            g.slot_last = std::max<std::uint32_t>(g.slot_last,
                                                   static_cast<std::uint32_t>(slot));
            ++g.slot_count;
            return;
        }
    }
    if (g_oam_attr0_stats.group_count >=
        sizeof(g_oam_attr0_stats.groups) / sizeof(g_oam_attr0_stats.groups[0])) {
        ++g_oam_attr0_stats.groups_dropped;
        return;
    }
    OamAttr0CandidateGroup& g =
        g_oam_attr0_stats.groups[g_oam_attr0_stats.group_count++];
    g.raw_y = raw_y;
    g.kind = p.kind;
    g.writer_pc = p.writer_pc;
    g.slot_first = g.slot_last = static_cast<std::uint32_t>(slot);
    g.slot_count = 1u;
    g.generation = p.generation;
    g.cycle = p.cycle;
}

OamSlotCounts count_oam_slots(armv4t::Bus* bus) {
    OamSlotCounts counts{};
    if (!bus) return counts;
    for (std::size_t slot = 0; slot < kOamSlotCount; ++slot) {
        const std::uint32_t address = kOamStart +
            static_cast<std::uint32_t>(slot * kOamSlotBytes);
        const std::uint16_t attr0 = bus->read16(address);
        const std::uint16_t attr1 = bus->read16(address + 2u);
        const std::uint16_t attr2 = bus->read16(address + 4u);
        const bool nonzero = attr0 != 0u || attr1 != 0u || attr2 != 0u;
        if (nonzero) ++counts.nonzero;
        const bool affine = (attr0 & 0x0100u) != 0u;
        const bool disabled = !affine && (attr0 & 0x0200u) != 0u;
        const unsigned object_mode = (attr0 >> 10) & 3u;
        const unsigned shape = (attr0 >> 14) & 3u;
        const bool used = nonzero && !disabled && shape < 3u &&
            object_mode != 3u;
        if (!used) continue;
        ++counts.used;
        if (object_mode != 2u) ++counts.visible;
        if ((attr1 & 0x01FFu) >= 240u) ++counts.raw_x_ge_240;
        if ((attr0 & 0x00FFu) >= 160u) ++counts.raw_y_ge_160;
    }
    return counts;
}

}  // namespace

bool overlaps_active_mode0_bg123(const std::uint8_t* io,
                                  std::uint32_t address,
                                  std::uint64_t size) {
    if (!mode0_field(io) || size == 0u) return false;
    const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
    for (unsigned bg = 1u; bg <= 3u; ++bg) {
        if ((dispcnt & (1u << (8u + bg))) == 0u) continue;
        if (regular_bg_range_intersects(io, bg, address, size)) return true;
    }
    return false;
}

bool dma_overlaps_active_mode0_bg123(const std::uint8_t* io,
                                     std::uint32_t destination,
                                     std::uint32_t step,
                                     std::uint32_t units,
                                     std::uint32_t dest_control) {
    std::uint32_t first = 0, last = 0;
    return dma_trace_bounds(destination, step, units, dest_control, first, last) &&
           last > first &&
           overlaps_active_mode0_bg123(io, first,
                                       static_cast<std::uint64_t>(last) - first);
}

bool overlaps_active_affine_bg23(const std::uint8_t* io,
                                 std::uint32_t address,
                                 std::uint64_t size) {
    if (!mode2_affine(io) || size == 0u) return false;
    const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
    for (unsigned bg = 2u; bg <= 3u; ++bg) {
        if ((dispcnt & (1u << (8u + bg))) == 0u) continue;
        if (affine_range_intersects(io, bg, address, size)) return true;
    }
    return false;
}

bool dma_overlaps_active_affine_bg23(const std::uint8_t* io,
                                     std::uint32_t destination,
                                     std::uint32_t step,
                                     std::uint32_t units,
                                     std::uint32_t dest_control) {
    std::uint32_t first = 0, last = 0;
    return dma_trace_bounds(destination, step, units, dest_control, first, last) &&
           last > first &&
           overlaps_active_affine_bg23(io, first,
                                       static_cast<std::uint64_t>(last) - first);
}

void trace_cpu_write(const std::uint8_t* io, std::uint32_t pc,
                     std::uint32_t address, std::uint32_t size) {
    if (dma_active()) return;
    if (g_text_trace_enabled) {
        const std::uint32_t regions =
            selected_bg0_text_regions(io, address, size);
        if (regions != 0u) {
            append_text_trace_row(io, "cpu", pc, address, size, -1, 0u, 0u,
                                  0u, 0u, -1, regions);
        }
    }
    if (!trace_enabled()) return;
    note_affine_scene_edge(io);
    if (mode0_pc_budget(pc) && overlaps_active_mode0_bg123(io, address, size)) {
        const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
        std::fprintf(stderr,
                     "[vram-map-cpu] cycle=%llu pc=0x%08X addr=0x%08X size=%u "
                     "dispcnt=0x%04X bg1cnt=0x%04X bg2cnt=0x%04X bg3cnt=0x%04X "
                     "bg1scroll=%u,%u bg2scroll=%u,%u bg3scroll=%u,%u\n",
                     static_cast<unsigned long long>(g_runtime_cycles), pc,
                     address, size, dispcnt, bgcnt(io, 1u), bgcnt(io, 2u),
                     bgcnt(io, 3u), hofs(io, 1u), vofs(io, 1u), hofs(io, 2u),
                     vofs(io, 2u), hofs(io, 3u), vofs(io, 3u));
        // counted by mode0_pc_budget() above
        return;
    }
    if (affine_cpu_records() < 512u &&
        overlaps_active_affine_bg23(io, address, size)) {
        const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
        std::fprintf(stderr,
                     "[vram-affine-cpu] cycle=%llu pc=0x%08X addr=0x%08X "
                     "size=%u dispcnt=0x%04X bg2cnt=0x%04X bg3cnt=0x%04X\n",
                     static_cast<unsigned long long>(g_runtime_cycles), pc,
                     address, size, dispcnt, bgcnt(io, 2u), bgcnt(io, 3u));
        ++affine_cpu_records();
    }
}

void trace_dma(const std::uint8_t* io, int channel, std::uint32_t pc,
               std::uint32_t source, std::uint32_t destination,
               std::uint32_t bytes, std::uint16_t control, int start_mode) {
    if (g_dma_descriptor_observer) {
        g_dma_descriptor_observer(channel, pc, source, destination, bytes,
                                  control, start_mode);
    }
    const std::uint32_t step = (control & 0x0400u) ? 4u : 2u;
    const std::uint32_t units = bytes / step;
    const std::uint32_t dest_control = (control >> 5) & 3u;
    if (g_text_trace_enabled) {
        std::uint32_t first = 0u;
        std::uint32_t last = 0u;
        if (dma_trace_bounds(destination, step, units, dest_control, first,
                             last) && last > first) {
            const std::uint32_t regions = selected_bg0_text_regions(
                io, first, static_cast<std::uint64_t>(last) - first);
            if (regions != 0u) {
                append_text_trace_row(
                    io, "dma", pc, first,
                    static_cast<std::uint32_t>(last - first), channel, source,
                    destination, bytes, control, start_mode, regions);
            }
        }
    }
    if (!trace_enabled()) return;
    note_affine_scene_edge(io);
    if (dma_records() < 256u &&
        dma_overlaps_active_mode0_bg123(io, destination, step, units,
                                        dest_control)) {
        const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
        std::fprintf(stderr,
                     "[vram-map-dma] cycle=%llu pc=0x%08X mode=%d ch=%d "
                     "src=0x%08X dst=0x%08X size=%u cnt_h=0x%04X "
                     "dispcnt=0x%04X bg1cnt=0x%04X bg2cnt=0x%04X bg3cnt=0x%04X "
                     "bg1scroll=%u,%u bg2scroll=%u,%u bg3scroll=%u,%u\n",
                     static_cast<unsigned long long>(g_runtime_cycles), pc,
                     start_mode, channel, source, destination, bytes, control,
                     dispcnt, bgcnt(io, 1u), bgcnt(io, 2u), bgcnt(io, 3u),
                     hofs(io, 1u), vofs(io, 1u), hofs(io, 2u), vofs(io, 2u),
                     hofs(io, 3u), vofs(io, 3u));
        ++dma_records();
        return;
    }
    if (affine_dma_records() < 256u &&
        dma_overlaps_active_affine_bg23(io, destination, step, units,
                                        dest_control)) {
        const std::uint16_t dispcnt = load16(io, IoReg::DISPCNT);
        std::fprintf(stderr,
                     "[vram-affine-dma] cycle=%llu pc=0x%08X mode=%d ch=%d "
                     "src=0x%08X dst=0x%08X size=%u cnt_h=0x%04X "
                     "dispcnt=0x%04X bg2cnt=0x%04X bg3cnt=0x%04X\n",
                     static_cast<unsigned long long>(g_runtime_cycles), pc,
                     start_mode, channel, source, destination, bytes, control,
                     dispcnt, bgcnt(io, 2u), bgcnt(io, 3u));
        ++affine_dma_records();
    }
}

struct OamShadowRange {
    bool valid = false;
    std::size_t first_slot = 0;
    std::size_t last_slot = 0;
    std::uint64_t slot_overwrites = 0;
};

OamShadowRange account_oam_shadow_range(std::uint32_t address,
                                        std::uint32_t size, bool from_dma) {
    OamShadowRange result{};
    if (size == 0u) return result;
    address = normalize_oam_shadow_address(address);
    const std::uint64_t first = address;
    const std::uint64_t last = first + size;
    if (first >= kOamShadowEnd || last <= kOamShadowStart) return result;
    const std::uint64_t overlap_first =
        std::max<std::uint64_t>(first, kOamShadowStart);
    const std::uint64_t overlap_last =
        std::min<std::uint64_t>(last, kOamShadowEnd);
    result.first_slot = static_cast<std::size_t>(
        (overlap_first - kOamShadowStart) / kOamSlotBytes);
    result.last_slot = static_cast<std::size_t>(
        (overlap_last - 1u - kOamShadowStart) / kOamSlotBytes);
    ++g_oam_shadow_stats.write_calls;
    g_oam_shadow_stats.bytes += overlap_last - overlap_first;
    if (from_dma) {
        ++g_oam_shadow_stats.dma_write_calls;
        g_oam_shadow_stats.dma_bytes += overlap_last - overlap_first;
    }
    for (std::size_t slot = result.first_slot;
         slot <= result.last_slot; ++slot) {
        ++g_oam_shadow_stats.slot_write_events;
        if (g_oam_shadow_slot_seen[slot]) {
            ++g_oam_shadow_stats.slot_overwrite_events;
            ++result.slot_overwrites;
            if (!g_oam_shadow_slot_overwritten[slot]) {
                g_oam_shadow_slot_overwritten[slot] = true;
                ++g_oam_shadow_stats.overwritten_slots;
            }
        } else {
            g_oam_shadow_slot_seen[slot] = true;
            ++g_oam_shadow_stats.unique_slots;
        }
    }
    result.valid = true;
    return result;
}

void trace_oam_shadow_write(std::uint32_t pc, std::uint32_t address,
                            std::uint32_t size) {
    if (!oam_shadow_trace_enabled() || dma_active() || size == 0u) return;
    note_oam_attr0_write(pc, address, size, OamAttr0WriterKind::Cpu);
    const OamShadowRange range = account_oam_shadow_range(address, size, false);
    if (!range.valid) return;
    if (oam_shadow_records() >= 512u) {
        ++g_oam_shadow_stats.records_dropped;
        return;
    }
    std::fprintf(stderr,
                 "[oam-shadow] cycle=%llu pc=0x%08X addr=0x%08X size=%u "
                 "slot=%zu..%zu slot_overwrites=%llu\n",
                 static_cast<unsigned long long>(g_runtime_cycles), pc,
                 address, size, range.first_slot, range.last_slot,
                 static_cast<unsigned long long>(range.slot_overwrites));
    ++oam_shadow_records();
}

void trace_oam_shadow_write_committed(std::uint32_t pc,
                                      std::uint32_t address,
                                      std::uint32_t size) {
    // The runner callback is also the production object-placement seam. Keep
    // it independent of the bounded diagnostic print toggle; otherwise an
    // Enhanced run never sees generated fast-IWRAM F0 commits.
    if (dma_active() || size == 0u ||
        (g_oam_shadow_write_range_predicate &&
         !g_oam_shadow_write_range_predicate(address, size))) return;
    if (g_oam_shadow_write_observer)
        g_oam_shadow_write_observer(pc, address, size);
}

void trace_oam_shadow_dma(armv4t::Bus* bus, int channel, std::uint32_t pc,
                          std::uint32_t source, std::uint32_t destination,
                          std::uint32_t bytes, std::uint16_t control,
                          int start_mode) {
    if (!oam_shadow_trace_enabled()) return;
    const std::uint32_t step = (control & 0x0400u) != 0u ? 4u : 2u;
    const std::uint32_t units = step != 0u ? bytes / step : 0u;
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    if (bytes == 0u || (bytes % step) != 0u ||
        !dma_trace_bounds(destination, step, units,
                          (control >> 5) & 3u, first, last)) {
        return;
    }
    const std::uint64_t span = static_cast<std::uint64_t>(last) - first;
    const OamShadowRange range = account_oam_shadow_range(
        first, static_cast<std::uint32_t>(span), true);
    if (!range.valid) return;
    // The measured producer DMA writes 708 bytes into the shadow, covering
    // slots 0..88. Establish DMA provenance for that destination span only;
    // the later shadow->OAM handoff must not replace it with a synthetic
    // writer.
    note_oam_attr0_write(pc, first, static_cast<std::uint32_t>(span),
                         OamAttr0WriterKind::Dma);
    if (oam_shadow_records() >= 512u) {
        ++g_oam_shadow_stats.records_dropped;
        return;
    }
    std::fprintf(
        stderr,
        "[oam-shadow-dma] cycle=%llu pc=0x%08X channel=%d start_mode=%d "
        "src=0x%08X dst=0x%08X size=%u slot=%zu..%zu "
        "slot_overwrites=%llu\n",
        static_cast<unsigned long long>(g_runtime_cycles), pc, channel,
        start_mode, source, destination, bytes, range.first_slot,
        range.last_slot, static_cast<unsigned long long>(range.slot_overwrites));
    ++oam_shadow_records();
}

void trace_oam_dma(armv4t::Bus* bus, int channel, std::uint32_t pc,
                   std::uint32_t source, std::uint32_t destination,
                   std::uint32_t bytes, std::uint16_t control,
                   int start_mode) {
    if (!oam_shadow_trace_enabled()) return;
    const std::uint32_t step = (control & 0x0400u) != 0u ? 4u : 2u;
    const std::uint32_t units = step != 0u ? bytes / step : 0u;
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    if (bytes == 0u || (bytes % step) != 0u ||
        !oam_dma_bounds(destination, step, units,
                        (control >> 5) & 3u, first, last)) {
        return;
    }
    const OamSlotCounts counts = count_oam_slots(bus);
    ++g_oam_dma_stats.transfers;
    g_oam_dma_stats.bytes += bytes;
    g_oam_dma_stats.used_slot_total += counts.used;
    g_oam_dma_stats.visible_slot_total += counts.visible;
    g_oam_dma_stats.nonzero_slot_total += counts.nonzero;
    g_oam_dma_stats.last_source = source;
    g_oam_dma_stats.last_destination = destination;
    g_oam_dma_stats.last_size = bytes;
    g_oam_dma_stats.last_used_slots = counts.used;
    g_oam_dma_stats.last_visible_slots = counts.visible;
    g_oam_dma_stats.last_nonzero_slots = counts.nonzero;
    g_oam_dma_stats.last_raw_x_ge_240 = counts.raw_x_ge_240;
    g_oam_dma_stats.last_raw_y_ge_160 = counts.raw_y_ge_160;
    // The final measured handoff copies the complete shadow to OAM. Read only
    // raw Y metadata from the committed image and join it with the latest
    // shadow ATTR0 provenance; slots 89..127 remain unseen unless separately
    // written by a CPU/DMA producer.
    if (bus && source == kOamShadowStart && destination == kOamStart &&
        bytes == 1024u && (control & 0x0060u) == 0u) {
        ++g_oam_attr0_stats.post_copies;
        for (std::size_t slot = 0; slot < kOamSlotCount; ++slot) {
            const std::uint32_t attr0_addr = kOamStart +
                static_cast<std::uint32_t>(slot * kOamSlotBytes);
            note_oam_attr0_candidate(
                static_cast<std::uint8_t>(bus->read16(attr0_addr) & 0x00FFu),
                slot);
        }
    }
    if (g_oam_dma_records >= 256u) {
        ++g_oam_dma_stats.records_dropped;
        return;
    }
    std::fprintf(
        stderr,
        "[oam-dma] cycle=%llu pc=0x%08X channel=%d start_mode=%d "
        "src=0x%08X dst=0x%08X size=%u used_slots=%u visible_slots=%u "
        "nonzero_slots=%u raw_x_ge_240=%u raw_y_ge_160=%u\n",
        static_cast<unsigned long long>(g_runtime_cycles), pc, channel,
        start_mode, source, destination, bytes, counts.used, counts.visible,
        counts.nonzero, counts.raw_x_ge_240, counts.raw_y_ge_160);
    ++g_oam_dma_records;
}

void reset_oam_trace_window() {
    g_oam_shadow_stats = {};
    g_oam_dma_stats = {};
    g_oam_attr0_stats = {};
    g_oam_attr0_provenance.fill({});
    g_oam_attr0_generation = 0;
    g_oam_shadow_slot_seen.fill(false);
    g_oam_shadow_slot_overwritten.fill(false);
    g_oam_shadow_records = 0u;
    g_oam_dma_records = 0u;
}

void get_oam_shadow_trace_stats(OamShadowTraceStats* out) {
    if (out) *out = g_oam_shadow_stats;
}

void get_oam_dma_trace_stats(OamDmaTraceStats* out) {
    if (out) *out = g_oam_dma_stats;
}

void get_oam_attr0_trace_stats(OamAttr0TraceStats* out) {
    if (out) *out = g_oam_attr0_stats;
}

bool get_oam_attr0_provenance(std::size_t slot, OamAttr0Provenance* out) {
    if (!out || slot >= kOamSlotCount) return false;
    *out = g_oam_attr0_provenance[slot];
    return true;
}

void begin_dma() { ++dma_depth(); }
void end_dma() { if (dma_depth() != 0u) --dma_depth(); }
bool dma_active() { return dma_depth() != 0u; }

void set_default_enabled(bool enabled) {
    g_default_enabled = enabled ? 1 : 0;
}

void set_text_trace_directory(const char* directory) {
    flush_text_trace();
    if (g_text_trace_file) {
        std::fclose(g_text_trace_file);
        g_text_trace_file = nullptr;
    }
    g_text_trace_directory = directory ? directory : "";
    g_text_trace_enabled = !g_text_trace_directory.empty();
    g_text_trace_open_failed = false;
    g_text_trace_sequence = 0u;
    g_text_trace_pending_rows = 0u;
    g_text_trace_buffer.clear();
    if (g_text_trace_enabled && !g_text_trace_atexit_registered) {
        std::atexit(&flush_text_trace);
        g_text_trace_atexit_registered = true;
    }
    // Create the session artifact immediately so an enabled capture is
    // distinguishable from a run that simply produced no BG0 writes.
    if (g_text_trace_enabled) ensure_text_trace_file();
}

bool rearm_bounded_window() {
    if (g_trace_windows >= kMaxTraceWindows) return false;
    ++g_trace_windows;
    cpu_records() = 0u;
    dma_records() = 0u;
    reset_mode0_pc_budget();
    return true;
}

void set_dma_descriptor_observer(DmaDescriptorObserver observer) {
    g_dma_descriptor_observer = observer;
}

void set_oam_shadow_write_observer(OamShadowWriteObserver observer) {
    g_oam_shadow_write_observer = observer;
}

void set_oam_shadow_write_range_predicate(
    OamShadowWriteRangePredicate predicate) {
    g_oam_shadow_write_range_predicate = predicate;
}

}  // namespace gba::vram_trace
