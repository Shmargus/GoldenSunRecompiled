// Golden Sun's narrow, evidence-backed expanded-view policy.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace gsr::widescreen {

// This numeric contract mirrors gbarecomp's generic margin-policy flags while
// keeping the game policy pure and independently testable.
inline constexpr unsigned kPillarboxLeft = 1u << 0;
inline constexpr unsigned kPillarboxRight = 1u << 1;
inline constexpr unsigned kPillarboxBoth = kPillarboxLeft | kPillarboxRight;
inline constexpr unsigned kPillarboxTop = 1u << 2;
inline constexpr unsigned kPillarboxBottom = 1u << 3;
inline constexpr unsigned kPillarboxVertical =
    kPillarboxTop | kPillarboxBottom;
inline constexpr unsigned kPillarboxAll =
    kPillarboxBoth | kPillarboxVertical;
inline constexpr std::uint32_t kNativeWidth = 240u;
inline constexpr std::uint32_t kNativeHeight = 160u;

// The 360x240 expanded option is deliberately bounded here even though the
// generic PPU can support a larger horizontal envelope.  These constants are
// presentation policy, not guest camera coordinates: the atlas provider may
// read only the measured 60px/40px margins around the authentic canvas.
inline constexpr std::int32_t kExpandedExtraX = 60;
inline constexpr std::int32_t kExpandedExtraY = 40;
inline constexpr std::int32_t kExpandedWidth =
    static_cast<std::int32_t>(kNativeWidth) + 2 * kExpandedExtraX;
inline constexpr std::int32_t kExpandedHeight =
    static_cast<std::int32_t>(kNativeHeight) + 2 * kExpandedExtraY;

// Exact THUMB immediate-override routes reviewed for the field-object cull
// diagnostics. Keep this table independent of the runner so synthetic tests
// can prove that only the six configured PCs are accounted for; unknown PCs
// remain outside the diagnostic route.
enum class GoldenSunCullSite : std::uint8_t {
    B27C = 0,
    B322,
    B326,
    B3CA,
    B3D6,
    B3EA,
    Count,
};

inline constexpr int golden_sun_cull_site_index(std::uint32_t pc) {
    switch (pc) {
        case 0x0800B27Cu: return 0;
        case 0x0800B322u: return 1;
        case 0x0800B326u: return 2;
        case 0x0800B3CAu: return 3;
        case 0x0800B3D6u: return 4;
        case 0x0800B3EAu: return 5;
        default: return -1;
    }
}

inline constexpr std::uint32_t golden_sun_cull_site_pc(
    std::size_t index) {
    switch (index) {
        case 0: return 0x0800B27Cu;
        case 1: return 0x0800B322u;
        case 2: return 0x0800B326u;
        case 3: return 0x0800B3CAu;
        case 4: return 0x0800B3D6u;
        case 5: return 0x0800B3EAu;
        default: return 0u;
    }
}

inline constexpr std::uint32_t golden_sun_cull_site_original(
    std::size_t index) {
    switch (index) {
        case 0: return 159u;
        case 1: return 239u;
        case 2: return 159u;
        case 3: return 32u;
        case 4: return 136u;
        case 5: return 208u;
        default: return 0u;
    }
}

inline constexpr bool golden_sun_cull_site_matches(
    std::uint32_t pc, std::uint32_t original_value) {
    const int index = golden_sun_cull_site_index(pc);
    return index >= 0 && original_value == golden_sun_cull_site_original(
        static_cast<std::size_t>(index));
}

// Only Func_b168's final X compare is proven on the executed field route.
// Keep the route, original literal, and authenticated field scene separate so
// an unrelated cull site or an unequal-scroll scene cannot opt in by accident.
inline constexpr bool golden_sun_field_obj_cull_authorized(
    bool authenticated_field, std::uint32_t instruction_pc,
    std::uint32_t original_value, std::uint32_t extra_right) {
    return authenticated_field && extra_right != 0u &&
        instruction_pc == golden_sun_cull_site_pc(1u) &&
        original_value == golden_sun_cull_site_original(1u);
}

// The expanded renderer may reinterpret only the raw OBJ-X values that the
// proven field cull can emit. Mode-2/overworld objects remain hardware-signed
// until their producer/cull route is independently authenticated.
inline constexpr bool golden_sun_field_obj_x_authorized(
    bool authenticated_field, int raw_x, std::uint32_t extra_right) {
    if (!authenticated_field || extra_right == 0u || raw_x < 0x100) {
        return false;
    }
    return raw_x < static_cast<int>(kNativeWidth + extra_right);
}

// Func_b168's measured field-object writer rejects screen X > 239 at
// ROM 0x0800B322.  A widened 288px field needs the same inclusive right edge
// as the renderer (239 + 24 = 263); keep this pure so the runner's exact-PC
// immediate hook and its tests share one evidence-backed limit.
inline constexpr int golden_sun_field_obj_right_cull_limit(
    std::uint32_t extra_right) {
    return static_cast<int>(kNativeWidth + extra_right - 1u);
}

inline constexpr bool golden_sun_field_obj_y_cull_authorized(
    bool authenticated_field, std::uint32_t instruction_pc,
    std::uint32_t original_value, std::uint32_t extra_bottom) {
    return authenticated_field && extra_bottom != 0u &&
        original_value == 159u &&
        (instruction_pc == golden_sun_cull_site_pc(0u) ||
         instruction_pc == golden_sun_cull_site_pc(2u));
}

// Measured OAM shadow range: 128 slots, 8 bytes each, with end exclusive.
inline constexpr std::uint32_t kGoldenSunOamShadowStart = 0x0300347Cu;
inline constexpr std::uint32_t kGoldenSunOamShadowEnd = 0x0300387Cu;
inline constexpr std::uint32_t kGoldenSunOamShadowSlotBytes = 8u;
inline constexpr std::size_t kGoldenSunOamShadowSlotCount =
    (kGoldenSunOamShadowEnd - kGoldenSunOamShadowStart) /
    kGoldenSunOamShadowSlotBytes;

inline constexpr int golden_sun_oam_shadow_slot(std::uint32_t attr0_address) {
    if (attr0_address < kGoldenSunOamShadowStart ||
        attr0_address >= kGoldenSunOamShadowEnd ||
        (attr0_address - kGoldenSunOamShadowStart) %
                kGoldenSunOamShadowSlotBytes != 0u) {
        return -1;
    }
    return static_cast<int>((attr0_address - kGoldenSunOamShadowStart) /
                            kGoldenSunOamShadowSlotBytes);
}

// The adjacent `cmp r6,#159` at ROM 0x0800B326 is the final screen-Y reject.
inline constexpr int golden_sun_field_obj_bottom_cull_limit(
    std::uint32_t extra_bottom) {
    return static_cast<int>(kNativeHeight + extra_bottom - 1u);
}

// Func_b388 performs an earlier padded actor-box reject: X -32..272 and
// Y -32..208. Preserve its padding while extending every live viewport edge.
inline constexpr std::uint32_t golden_sun_actor_precull_negative_padding(
    std::uint32_t extra_left, std::uint32_t extra_top) {
    return 32u + (extra_left > extra_top ? extra_left : extra_top);
}

// The guest forms 272 as `movs r1,#136; lsl r1,#1`.
inline constexpr std::uint32_t golden_sun_actor_precull_right_half_limit(
    std::uint32_t extra_right) {
    return 136u + ((extra_right + 1u) / 2u);
}

inline constexpr std::uint32_t golden_sun_actor_precull_bottom_limit(
    std::uint32_t extra_bottom) {
    return 208u + extra_bottom;
}

// Func_c62c's earlier field-list culls use these 16.16 fixed-point literal
// bounds before calling Func_b168. The X comparison adds 32px before testing
// against 304px, so its literal grows by the active right margin. The Y lower
// comparison is a signed -32px reject and grows downward by the top margin.
inline constexpr std::uint32_t golden_sun_field_list_x_upper_literal(
    std::uint32_t extra_right) {
    return 0x012FFFFEu + (extra_right << 16);
}

inline constexpr std::uint32_t golden_sun_field_list_y_lower_literal(
    std::uint32_t extra_top) {
    return 0xFFE00000u - (extra_top << 16);
}

// The expanded field writer can emit the bottom-margin OBJ Y range as raw
// 160..(native_height+extra_bottom-1). Reinterpret that range positively only
// while the bottom margin is actually present; otherwise preserve hardware's
// normal signed 8-bit decode.
inline constexpr bool golden_sun_field_obj_y_expanded(
    int raw_y, std::uint32_t extra_top, std::uint32_t extra_bottom) {
    (void)extra_top;
    if (extra_bottom == 0u) return false;
    return raw_y >= static_cast<int>(kNativeHeight) &&
           raw_y <= golden_sun_field_obj_bottom_cull_limit(extra_bottom);
}

// Golden Sun builds the IWRAM OAM shadow before VBlank and copies it for the
// following visible frame. Accept only that one-frame handoff (plus direct
// same-frame use) so a recycled slot cannot retain stale positive-Y meaning.
inline constexpr bool golden_sun_obj_provenance_frame_fresh(
    std::uint64_t writer_frame, std::uint64_t render_frame) {
    return writer_frame == render_frame ||
           (writer_frame != UINT64_MAX && writer_frame + 1u == render_frame);
}

inline constexpr std::uint16_t kDispcntModeMask = 0x0007u;
inline constexpr std::uint16_t kDispcntMode0 = 0x0000u;
inline constexpr std::uint16_t kDispcntMode2 = 0x0002u;
inline constexpr std::uint16_t kDispcntForcedBlank = 0x0080u;
inline constexpr std::uint16_t kDispcntBg1 = 0x0200u;
inline constexpr std::uint16_t kDispcntBg2 = 0x0400u;
inline constexpr std::uint16_t kDispcntBg3 = 0x0800u;
inline constexpr std::uint16_t kDispcntObj = 0x1000u;
inline constexpr std::uint16_t kDispcntWindows = 0xE000u;

inline constexpr std::uint16_t kBgcntWrap = 0x2000u;
inline constexpr std::uint16_t kBgcntSizeMask = 0xC000u;
inline constexpr std::uint16_t kBgcntSize256 = 0x0000u;
inline constexpr std::uint16_t kBgcntSize512 = 0x8000u;

// Mode 0's BG0 is measured as a screen-space/UI layer in State1. Keep it
// inside the authentic 240-pixel canvas when field margins are widened.
inline constexpr bool golden_sun_suppress_bg0_margin(
    int bg, int output_x, std::uint32_t extra_left,
    std::uint32_t extra_right) {
    if (bg != 0) return false;
    const int first = static_cast<int>(extra_left);
    const int last = first + static_cast<int>(kNativeWidth);
    // Keep the right-margin argument in the contract so the helper remains
    // correct for asymmetric views; the output-side check is simply outside
    // the central native interval on either side.
    (void)extra_right;
    return output_x < first || output_x >= last;
}

// Full expanded-view form. BG0 is screen-space/UI composition in the measured
// field scene, so it remains inside the centered native rectangle on both
// axes. Keep the four-argument overload above for the existing horizontal
// runner seam until the generic runtime carries Y margins as well.
inline constexpr bool golden_sun_suppress_bg0_margin(
    int bg, int output_x, int output_y, std::uint32_t extra_left,
    std::uint32_t extra_right, std::uint32_t extra_top,
    std::uint32_t extra_bottom) {
    if (bg != 0) return false;
    const int first_x = static_cast<int>(extra_left);
    const int last_x = first_x + static_cast<int>(kNativeWidth);
    const int first_y = static_cast<int>(extra_top);
    const int last_y = first_y + static_cast<int>(kNativeHeight);
    (void)extra_right;
    (void)extra_bottom;
    return output_x < first_x || output_x >= last_x ||
           output_y < first_y || output_y >= last_y;
}

// Conservative fallback for callers without the measured atlas provider:
// keep every untrusted field sample inside the authentic 240px canvas. The
// runtime adapter uses the pure lookup below when live field tables are valid.
inline constexpr bool golden_sun_suppress_mode0_field_margin(
    int bg, int output_x, std::uint32_t extra_left,
    std::uint32_t extra_right) {
    if (bg < 1 || bg > 3) return false;
    const int native_first = static_cast<int>(extra_left);
    const int native_last = native_first + static_cast<int>(kNativeWidth);
    // Keep the right-margin argument in the contract for symmetric/asymmetric
    // view callers; the native interval itself defines both margin sides.
    (void)extra_right;
    return output_x < native_first || output_x >= native_last;
}

inline constexpr std::uint16_t read_io16(const std::uint8_t* io,
                                          std::size_t offset) {
    return static_cast<std::uint16_t>(io[offset]) |
           static_cast<std::uint16_t>(io[offset + 1u] << 8);
}

// Return zero only for measured field raster configurations. Mode 2 is the
// overworld signature: both affine layers enabled, both layers wrapping
// 512x512 maps, and no WIN0/WIN1/OBJ-window. Mode 0 is the town/dungeon
// signature: BG1/BG2/BG3 are enabled, all three layer scroll pairs match, and
// no WIN0/WIN1/OBJ-window is active. The scroll equality is only a scene
// classifier; it does not synthesize tiles or change guest camera/culling.
// Mode 1 battle and all unknown configurations remain pillarboxed.
enum class GoldenSunWidePolicyReason : std::uint8_t {
    AuthorizedMode2,
    AuthorizedMode0,
    AuthorizedMode0SplitScroll,
    MissingIo,
    ForcedBlank,
    WindowControl,
    UnsupportedMode,
    Mode2Layers,
    Mode2Geometry,
    Mode0Layers,
    Mode0Geometry,
    Mode0ScrollMismatch,
    Mode0SplitLayers,
    Mode0SplitGeometry,
    Mode0SplitScrollMismatch,
};

inline constexpr bool golden_sun_wide_policy_authorized(
    GoldenSunWidePolicyReason reason) {
    return reason == GoldenSunWidePolicyReason::AuthorizedMode2 ||
           reason == GoldenSunWidePolicyReason::AuthorizedMode0 ||
           reason == GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll;
}

inline constexpr GoldenSunWidePolicyReason golden_sun_wide_margin_policy_reason(
    std::uint16_t dispcnt, const std::uint8_t* io) {
    if (!io) return GoldenSunWidePolicyReason::MissingIo;
    if ((dispcnt & kDispcntForcedBlank) != 0)
        return GoldenSunWidePolicyReason::ForcedBlank;
    if ((dispcnt & kDispcntWindows) != 0)
        return GoldenSunWidePolicyReason::WindowControl;

    const std::uint16_t mode = dispcnt & kDispcntModeMask;
    if (mode == kDispcntMode2) {
        const std::uint16_t required_layers = kDispcntBg2 | kDispcntBg3;
        if ((dispcnt & required_layers) != required_layers)
            return GoldenSunWidePolicyReason::Mode2Layers;
        const std::uint16_t required = kBgcntWrap | kBgcntSize512;
        const std::uint16_t bg2cnt = read_io16(io, 0x0Cu);
        const std::uint16_t bg3cnt = read_io16(io, 0x0Eu);
        if ((bg2cnt & (kBgcntWrap | kBgcntSizeMask)) != required ||
            (bg3cnt & (kBgcntWrap | kBgcntSizeMask)) != required)
            return GoldenSunWidePolicyReason::Mode2Geometry;
        return GoldenSunWidePolicyReason::AuthorizedMode2;
    }

    if (mode == kDispcntMode0) {
        constexpr std::uint16_t field_layers =
            kDispcntBg1 | kDispcntBg2 | kDispcntBg3;
        if ((dispcnt & field_layers) != field_layers)
            return GoldenSunWidePolicyReason::Mode0Layers;
        // State1 measures all three field layers as 256x256 text BGs. Do not
        // widen a different regular-BG geometry under the same scroll pattern.
        const std::uint16_t bg1cnt = read_io16(io, 0x0Au);
        const std::uint16_t bg2cnt = read_io16(io, 0x0Cu);
        const std::uint16_t bg3cnt = read_io16(io, 0x0Eu);
        if ((bg1cnt & kBgcntSizeMask) != kBgcntSize256 ||
            (bg2cnt & kBgcntSizeMask) != kBgcntSize256 ||
            (bg3cnt & kBgcntSizeMask) != kBgcntSize256)
            return GoldenSunWidePolicyReason::Mode0Geometry;
        // HOFS/VOFS are 9-bit GBA registers. Compare the effective hardware
        // scroll, not unused high bits retained by a host IO snapshot.
        const auto read_scroll = [&](std::size_t offset) {
            return static_cast<std::uint16_t>(read_io16(io, offset) & 0x01FFu);
        };
        const std::uint16_t bg1_hofs = read_scroll(0x14u);
        const std::uint16_t bg1_vofs = read_scroll(0x16u);
        const std::uint16_t bg2_hofs = read_scroll(0x18u);
        const std::uint16_t bg2_vofs = read_scroll(0x1Au);
        const std::uint16_t bg3_hofs = read_scroll(0x1Cu);
        const std::uint16_t bg3_vofs = read_scroll(0x1Eu);
        if (bg1_hofs != bg2_hofs || bg1_hofs != bg3_hofs ||
            bg1_vofs != bg2_vofs || bg1_vofs != bg3_vofs)
            return GoldenSunWidePolicyReason::Mode0ScrollMismatch;
        return GoldenSunWidePolicyReason::AuthorizedMode0;
    }

    return GoldenSunWidePolicyReason::UnsupportedMode;
}

// McCoy Palace's measured Mode-0 class is deliberately distinct from the
// equal-scroll field class above. The exact BG CNT values and raw scroll
// deltas are part of the authentication: transitions that merely happen to
// match one relation must not become a Palace policy. The runner additionally
// requires a complete clean frame before using this result.
inline constexpr std::uint16_t kGoldenSunPalaceBg1Cnt = 0x0709u;
inline constexpr std::uint16_t kGoldenSunPalaceBg2Cnt = 0x060Au;
inline constexpr std::uint16_t kGoldenSunPalaceBg3Cnt = 0x0503u;

inline constexpr GoldenSunWidePolicyReason
golden_sun_mode0_split_scroll_policy_reason(
    std::uint16_t dispcnt, const std::uint8_t* io) {
    if (!io) return GoldenSunWidePolicyReason::MissingIo;
    if ((dispcnt & kDispcntForcedBlank) != 0)
        return GoldenSunWidePolicyReason::ForcedBlank;
    if ((dispcnt & kDispcntWindows) != 0)
        return GoldenSunWidePolicyReason::WindowControl;
    if ((dispcnt & kDispcntModeMask) != kDispcntMode0)
        return GoldenSunWidePolicyReason::UnsupportedMode;

    constexpr std::uint16_t field_layers =
        kDispcntBg1 | kDispcntBg2 | kDispcntBg3;
    if ((dispcnt & field_layers) != field_layers)
        return GoldenSunWidePolicyReason::Mode0SplitLayers;
    if (read_io16(io, 0x0Au) != kGoldenSunPalaceBg1Cnt ||
        read_io16(io, 0x0Cu) != kGoldenSunPalaceBg2Cnt ||
        read_io16(io, 0x0Eu) != kGoldenSunPalaceBg3Cnt)
        return GoldenSunWidePolicyReason::Mode0SplitGeometry;

    const std::uint16_t bg1_hofs = read_io16(io, 0x14u);
    const std::uint16_t bg2_hofs = read_io16(io, 0x18u);
    const std::uint16_t bg3_hofs = read_io16(io, 0x1Cu);
    const std::uint16_t bg1_vofs = read_io16(io, 0x16u);
    const std::uint16_t bg2_vofs = read_io16(io, 0x1Au);
    const std::uint16_t bg3_vofs = read_io16(io, 0x1Eu);
    const bool effective_h_equal =
        (bg1_hofs & 0x01FFu) == (bg2_hofs & 0x01FFu) &&
        (bg1_hofs & 0x01FFu) == (bg3_hofs & 0x01FFu);
    const bool raw_h_split =
        bg1_hofs == bg3_hofs &&
        static_cast<std::uint16_t>(bg2_hofs - bg1_hofs) == 0x0200u;
    const bool raw_v_split =
        bg2_vofs == bg3_vofs &&
        static_cast<std::uint16_t>(bg1_vofs - bg2_vofs) == 0x0180u;
    if (!effective_h_equal || !raw_h_split || !raw_v_split)
        return GoldenSunWidePolicyReason::Mode0SplitScrollMismatch;
    return GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll;
}

inline constexpr bool golden_sun_mode0_split_scroll_authorized(
    std::uint16_t dispcnt, const std::uint8_t* io) {
    return golden_sun_mode0_split_scroll_policy_reason(dispcnt, io) ==
        GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll;
}

// Authentication is delayed by one full rendered frame. A row that fails the
// exact split-scroll predicate immediately clears the candidate, so a short
// transition cannot authorize the following frame.
struct GoldenSunMode0SplitScrollFrame {
    static constexpr std::uint64_t kUnsetFrame = UINT64_MAX;

    void reset() {
        frame = kUnsetFrame;
        clean_rows = 0;
        complete = false;
        authorized = false;
    }

    // Returns whether the row may use a previously completed clean frame.
    // `expected_rows` is the authentic guest raster height. Host-only signed
    // margin rows are synthesized after these rows and are not observations.
    bool observe(std::uint64_t sample_frame, bool clean_row,
                 std::uint32_t expected_rows) {
        if (expected_rows == 0u) {
            reset();
            return false;
        }
        if (frame != sample_frame) {
            const bool previous_complete = complete;
            frame = sample_frame;
            clean_rows = 0;
            complete = false;
            authorized = previous_complete;
        }
        if (!clean_row) {
            clean_rows = 0;
            complete = false;
            authorized = false;
            return false;
        }
        if (clean_rows < expected_rows) ++clean_rows;
        if (clean_rows >= expected_rows) complete = true;
        return authorized;
    }

    std::uint64_t frame = kUnsetFrame;
    std::uint32_t clean_rows = 0;
    bool complete = false;
    bool authorized = false;
};

inline constexpr unsigned golden_sun_wide_margin_policy(
    std::uint16_t dispcnt, const std::uint8_t* io) {
    return golden_sun_wide_policy_authorized(
        golden_sun_wide_margin_policy_reason(dispcnt, io)) ? 0u : kPillarboxAll;
}

// Golden Sun's Mode 0 field renderer keeps a 32x32 tile ring in VRAM, but its
// own row/column writers resolve the real map through these EWRAM tables:
//   0x02010000: 128x128 u32 metatile IDs (low 12 bits)
//   0x02020000: 4096 entries, each four u16 8x8 tile entries (2x2)
// The helper is deliberately a pure read-only lookup. `ewram` is the live
// EWRAM image beginning at 0x02000000; no guest state or camera is changed.
// The row/column writers copy the raw u16 entry verbatim.  0xffff is not a
// usable field tile: it selects tile 0x3ff with palette bank 0xf and exposes
// the stale VRAM tile seen in the widescreen margin.  Do not reject map IDs
// based on their value; the raw entry is the measured unavailable sentinel.
inline constexpr std::uint16_t kGoldenSunFieldUnavailableTile = 0xffffu;

// Optional metadata kept separate from the rendered entry so game-owned
// presentation policy can identify an unavailable raw entry (notably 0xffff)
// without changing the lookup result.
struct GoldenSunFieldTilemapMetadata {
    std::uint16_t map_id = 0;
    std::uint16_t raw_entry = 0;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    std::uint32_t map_x = 0;
    std::uint32_t map_y = 0;
    bool has_raw_entry = false;
};
// State1's complete 128x128 field table proves metatile 0x017/raw 0xF200 is
// the un-authored fill: it occupies 13,531/16,384 cells and forms the solid
// rows/columns outside the active 32x32 room. Metatile 0x003 is authored room
// data and must not be suppressed. The equal-scroll field uses BG3 as the
// coordinate-level boundary and suppresses BG1/BG2 there too; the separate
// split-scroll Palace path does not use BG3 as a cross-layer boundary.
inline constexpr std::uint16_t kGoldenSunFieldNoMapId = 0x017u;
inline constexpr std::uint16_t kGoldenSunFieldNoMapTile = 0xF200u;

// Bilibin's own [wide-field-map] table census (session 20260825_161027,
// frame=519890, crc=7ac260c4) measured top=017:13531,01a:1195,003:147,067:69.
// 0x01a is the second-largest occupant after the known 0x017 fill, and its
// occurrences visually correlate with the repeating menu/font-tile leak seen
// in that same session's screenshot. Its exact paired raw value was not
// separately measured, so this rejects the metatile ID alone rather than an
// (id, raw) pair. 0x003 (authored room data, per above) and 0x067 (only 69
// occurrences — too sparse to distinguish fill from legitimate scattered
// content) are deliberately left unsuppressed.
inline constexpr std::uint16_t kGoldenSunFieldNoMapId2 = 0x01Au;

inline constexpr bool golden_sun_field_bg3_no_map(
    const GoldenSunFieldTilemapMetadata& metadata) {
    if (!metadata.has_raw_entry) return false;
    if (metadata.map_id == kGoldenSunFieldNoMapId &&
        metadata.raw_entry == kGoldenSunFieldNoMapTile) {
        return true;
    }
    return metadata.map_id == kGoldenSunFieldNoMapId2;
}

// A raw 0xffff entry is also unavailable even though the map/atlas lookup
// reached a valid raw-table slot.  Keep this separate from malformed-source
// cases: the provider may repair only an authenticated BG3 margin sample whose
// metadata identifies one of these measured unavailable entries.
inline constexpr bool golden_sun_field_bg3_unavailable(
    const GoldenSunFieldTilemapMetadata& metadata) {
    return metadata.has_raw_entry &&
        (metadata.raw_entry == kGoldenSunFieldUnavailableTile ||
         golden_sun_field_bg3_no_map(metadata));
}

// The same source-validity rule applies to every field layer.  BG3 remains
// the cross-layer boundary oracle in the runner, but a BG1/BG2 lookup can also
// land on an unavailable atlas cell while BG3 at that coordinate is authored.
// Keep this predicate separate from the BG3-named compatibility helper so a
// caller cannot accidentally treat layer-local invalid data as valid terrain.
inline constexpr bool golden_sun_field_atlas_unavailable(
    const GoldenSunFieldTilemapMetadata& metadata) {
    return golden_sun_field_bg3_unavailable(metadata);
}

// Resolve a BG3 margin sample by walking toward the authentic 240px canvas in
// 8px steps when the atlas returned one of the measured unavailable entries.
// `resolve` is a presentation-only callback supplied by the caller; it must
// return the atlas entry and metadata for the requested hardware X.  Native
// edge coordinates are included so a completely dummy 24px margin can reuse
// the first authored tile on the canvas.  The helper never writes guest RAM.
template <typename Resolve>
inline bool golden_sun_field_bg3_inward_entry(
    int hw_x, std::uint32_t native_width, std::uint32_t margin,
    Resolve&& resolve, std::uint16_t* out_entry,
    GoldenSunFieldTilemapMetadata* out_metadata = nullptr) {
    if (!out_entry || native_width == 0u || margin == 0u) return false;

    const bool left = hw_x < 0;
    if (!left && hw_x < static_cast<int>(native_width)) return false;
    const int limit = static_cast<int>(margin);
    if ((left && hw_x < -limit) ||
        (!left && hw_x >= static_cast<int>(native_width) + limit)) {
        return false;
    }

    const int direction = left ? 1 : -1;
    const int first = hw_x;
    // Continue through the authentic row after crossing the native edge. A
    // dummy can occupy the edge tile itself, so clamping there would repeat
    // the same unavailable sample forever and leave the margin transparent.
    // The last reachable native candidate is 8px before the opposite edge:
    // left: 0,8,...,native_width-8; right: native_width-1,...,7.
    const int max_distance = limit + static_cast<int>(native_width) - 8;
    for (int distance = 8; distance <= max_distance; distance += 8) {
        int candidate = first + direction * distance;
        // Keep the remaining margin samples in the resolver's search order;
        // after crossing the margin, reject any non-native coordinate.
        if (distance > limit &&
            (candidate < 0 || candidate >= static_cast<int>(native_width))) {
            continue;
        }

        std::uint16_t entry = 0;
        GoldenSunFieldTilemapMetadata metadata;
        if (resolve(candidate, &entry, &metadata) &&
            !golden_sun_field_bg3_unavailable(metadata)) {
            *out_entry = entry;
            if (out_metadata) *out_metadata = metadata;
            return true;
        }
    }
    return false;
}

// Vertical counterpart to the horizontal BG3 repair above. The callback is
// given a candidate screen Y and must resolve the same hardware X. It walks
// inward in 8px steps, crossing the native edge so an unavailable top/bottom
// margin can reuse an authored native row. No guest memory is written.
template <typename Resolve>
inline bool golden_sun_field_bg3_inward_entry_vertical(
    int hw_x, int screen_y, std::uint32_t native_height,
    std::uint32_t margin, Resolve&& resolve, std::uint16_t* out_entry,
    GoldenSunFieldTilemapMetadata* out_metadata = nullptr) {
    if (!out_entry || native_height == 0u || margin == 0u) return false;

    const bool top = screen_y < 0;
    if (!top && screen_y < static_cast<int>(native_height)) return false;
    const int limit = static_cast<int>(margin);
    if ((top && screen_y < -limit) ||
        (!top && screen_y >= static_cast<int>(native_height) + limit)) {
        return false;
    }

    const int direction = top ? 1 : -1;
    const int first = screen_y;
    const int max_distance = limit + static_cast<int>(native_height) - 8;
    for (int distance = 8; distance <= max_distance; distance += 8) {
        const int candidate = first + direction * distance;
        if (distance > limit &&
            (candidate < 0 || candidate >= static_cast<int>(native_height))) {
            continue;
        }

        std::uint16_t entry = 0;
        GoldenSunFieldTilemapMetadata metadata;
        if (resolve(candidate, &entry, &metadata) &&
            !golden_sun_field_bg3_unavailable(metadata)) {
            *out_entry = entry;
            if (out_metadata) *out_metadata = metadata;
            return true;
        }
    }
    (void)hw_x;
    return false;
}

// Two-dimensional BG3 repair used by a renderer that can request both
// horizontal and vertical margins. The resolver receives the candidate
// hardware X/Y pair. For a single-axis margin this is exactly the directional
// 8px search above; corners first try each axis independently, then walk the
// Cartesian inward candidates so both coordinates can reach the native canvas.
template <typename Resolve>
inline bool golden_sun_field_bg3_inward_entry_2d(
    int hw_x, int screen_y, std::uint32_t native_width,
    std::uint32_t native_height, std::uint32_t margin_x,
    std::uint32_t margin_y, Resolve&& resolve, std::uint16_t* out_entry,
    GoldenSunFieldTilemapMetadata* out_metadata = nullptr) {
    if (!out_entry || native_width == 0u || native_height == 0u) return false;

    const int native_last_x = static_cast<int>(native_width);
    const int native_last_y = static_cast<int>(native_height);
    const bool outside_x = hw_x < 0 || hw_x >= native_last_x;
    const bool outside_y = screen_y < 0 || screen_y >= native_last_y;
    if (!outside_x && !outside_y) return false;
    if ((hw_x < -static_cast<int>(margin_x)) ||
        (hw_x >= native_last_x + static_cast<int>(margin_x)) ||
        (screen_y < -static_cast<int>(margin_y)) ||
        (screen_y >= native_last_y + static_cast<int>(margin_y))) {
        return false;
    }

    if (outside_x && !outside_y) {
        return golden_sun_field_bg3_inward_entry(
            hw_x, native_width, margin_x,
            [&](int candidate, std::uint16_t* entry,
                GoldenSunFieldTilemapMetadata* metadata) {
                return resolve(candidate, screen_y, entry, metadata);
            },
            out_entry, out_metadata);
    }
    if (!outside_x && outside_y) {
        return golden_sun_field_bg3_inward_entry_vertical(
            hw_x, screen_y, native_height, margin_y,
            [&](int candidate, std::uint16_t* entry,
                GoldenSunFieldTilemapMetadata* metadata) {
                return resolve(hw_x, candidate, entry, metadata);
            },
            out_entry, out_metadata);
    }

    // A corner has two independent inward directions. Preserve the existing
    // horizontal-first order, then vertical-first, before trying combinations
    // that move both coordinates toward the native rectangle.
    if (margin_x != 0u) {
        if (golden_sun_field_bg3_inward_entry(
                hw_x, native_width, margin_x,
                [&](int candidate, std::uint16_t* entry,
                    GoldenSunFieldTilemapMetadata* metadata) {
                    return resolve(candidate, screen_y, entry, metadata);
                },
                out_entry, out_metadata)) {
            return true;
        }
    }
    if (margin_y != 0u) {
        if (golden_sun_field_bg3_inward_entry_vertical(
                hw_x, screen_y, native_height, margin_y,
                [&](int candidate, std::uint16_t* entry,
                    GoldenSunFieldTilemapMetadata* metadata) {
                    return resolve(hw_x, candidate, entry, metadata);
                },
                out_entry, out_metadata)) {
            return true;
        }
    }

    // If each one-axis candidate is unavailable, try the inward Cartesian
    // product. The bounded margins keep this loop small and deterministic.
    const bool top = screen_y < 0;
    const int y_direction = top ? 1 : -1;
    const bool left = hw_x < 0;
    const int x_direction = left ? 1 : -1;
    const int max_x_distance = static_cast<int>(margin_x) +
        native_last_x - 8;
    const int max_y_distance = static_cast<int>(margin_y) +
        native_last_y - 8;
    for (int y_distance = 8; y_distance <= max_y_distance;
         y_distance += 8) {
        const int candidate_y = screen_y + y_direction * y_distance;
        if (y_distance > static_cast<int>(margin_y) &&
            (candidate_y < 0 || candidate_y >= native_last_y)) {
            continue;
        }
        for (int x_distance = 8; x_distance <= max_x_distance;
             x_distance += 8) {
            const int candidate_x = hw_x + x_direction * x_distance;
            if (x_distance > static_cast<int>(margin_x) &&
                (candidate_x < 0 || candidate_x >= native_last_x)) {
                continue;
            }
            std::uint16_t entry = 0;
            GoldenSunFieldTilemapMetadata metadata;
            if (resolve(candidate_x, candidate_y, &entry, &metadata) &&
                !golden_sun_field_bg3_unavailable(metadata)) {
                *out_entry = entry;
                if (out_metadata) *out_metadata = metadata;
                return true;
            }
        }
    }
    return false;
}

// Per-cell "authored" bitmap for the field's 128x128 metatile table (EWRAM
// 0x02010000..0x02020000, one u32 entry per cell). A bit is set only when
// the guest has actually written that cell since the map was last reset.
// This replaces relying on a single measured sentinel ID/value to detect
// un-authored fill: that approach only recognised the exact 0x017/0xF200
// pair seen in one room and let other rooms' different residual fill IDs
// (e.g. Bilibin's 0x01a) through as visible junk. Kept as a pure,
// independently testable value type; the runner owns the one live instance
// and mirrors EWRAM writes into it only while an expanded view is active.
class GoldenSunFieldAuthoredMap {
public:
    static constexpr std::uint32_t kCells = 128u * 128u;
    static constexpr std::uint32_t kCellBytes = 4u;
    // Absolute guest address of the metatile table (0x02010000).
    static constexpr std::uint32_t kAbsBase = 0x02010000u;
    static constexpr std::uint64_t kAbsEnd =
        static_cast<std::uint64_t>(kAbsBase) + kCells * kCellBytes;

    void reset() { bits_.fill(0u); }

    // `address` is a full guest address (e.g. a bus write address) and
    // `size` the access width in bytes. Marks every metatile cell the
    // access overlaps as authored.
    void mark_write(std::uint32_t address, std::uint32_t size) {
        if (size == 0u) return;
        const std::uint64_t first = address;
        const std::uint64_t last = first + size;
        if (last <= kAbsBase || first >= kAbsEnd) return;
        const std::uint64_t lo = first > kAbsBase ? first : kAbsBase;
        const std::uint64_t hi = last < kAbsEnd ? last : kAbsEnd;
        const std::uint32_t first_cell = static_cast<std::uint32_t>(
            (lo - kAbsBase) / kCellBytes);
        const std::uint32_t last_cell = static_cast<std::uint32_t>(
            (hi - 1u - kAbsBase) / kCellBytes);
        for (std::uint32_t cell = first_cell;
             cell <= last_cell && cell < kCells; ++cell) {
            bits_[cell >> 3] = static_cast<std::uint8_t>(
                bits_[cell >> 3] | (1u << (cell & 7u)));
        }
    }

    bool authored(std::uint32_t map_x, std::uint32_t map_y) const {
        const std::uint32_t cell = map_y * 128u + map_x;
        if (cell >= kCells) return false;
        return (bits_[cell >> 3] & (1u << (cell & 7u))) != 0u;
    }

    // Diagnostic-only census of writes observed since the last reset. This is
    // deliberately separate from the map contents: a savestate may restore a
    // populated EWRAM table while this bitmap correctly starts empty.
    std::uint32_t authored_count() const {
        std::uint32_t count = 0;
        for (std::uint8_t byte : bits_) {
            for (unsigned bit = 0; bit < 8u; ++bit)
                count += (byte >> bit) & 1u;
        }
        return count;
    }

private:
    std::array<std::uint8_t, kCells / 8u> bits_{};
};

inline bool golden_sun_field_tilemap_entry(
    std::uint16_t dispcnt, const std::uint8_t* io, std::size_t io_size,
    const std::uint8_t* ewram, std::size_t ewram_size, int bg, int hw_x,
    int screen_y, std::uint16_t* out_entry,
    GoldenSunFieldTilemapMetadata* out_metadata = nullptr,
    const GoldenSunFieldAuthoredMap* authored = nullptr,
    bool allow_native_x = false, bool allow_split_scroll = false) {
    // `allow_split_scroll` is reserved for the separately authenticated Palace
    // class. It does not relax any coordinate, source-bound, or sentinel check.
    constexpr std::size_t kMapBase = 0x10000u;
    constexpr std::size_t kRawBase = 0x20000u;
    constexpr std::size_t kMapBytes = 128u * 128u * 4u;
    constexpr std::size_t kRawBytes = 4096u * 8u;
    constexpr std::size_t kRequiredEwram = kRawBase + kRawBytes;
    constexpr std::size_t kRequiredIo = 0x20u;
    if (!out_entry || !io || io_size < kRequiredIo || !ewram ||
        ewram_size < kRequiredEwram || bg < 1 || bg > 3 ||
        hw_x < -kExpandedExtraX ||
        hw_x >= static_cast<int>(kNativeWidth) + kExpandedExtraX ||
        screen_y < -kExpandedExtraY ||
        screen_y >= static_cast<int>(kNativeHeight) + kExpandedExtraY ||
        (!allow_native_x && hw_x >= 0 &&
         hw_x < static_cast<int>(kNativeWidth) && screen_y >= 0 &&
         screen_y < static_cast<int>(kNativeHeight))) {
        return false;
    }
    const GoldenSunWidePolicyReason policy_reason =
        golden_sun_wide_margin_policy_reason(dispcnt, io);
    if (policy_reason != GoldenSunWidePolicyReason::AuthorizedMode0 &&
        !(allow_split_scroll &&
          golden_sun_mode0_split_scroll_policy_reason(dispcnt, io) ==
              GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll)) {
        return false;
    }

    const std::size_t scroll_off = 0x14u +
        static_cast<std::size_t>(bg - 1) * 4u;
    // Keep the raw 16-bit register values. The guest's hardware samples mask
    // them to 9 bits, while these high bits select the field atlas region.
    const std::int32_t hofs = static_cast<std::int32_t>(
        read_io16(io, scroll_off));
    const std::int32_t vofs = static_cast<std::int32_t>(
        read_io16(io, scroll_off + 2u));
    const std::int32_t abs_x = hofs + static_cast<std::int32_t>(hw_x);
    const std::int32_t abs_y = vofs + static_cast<std::int32_t>(screen_y);
    const auto floor_div8 = [](std::int32_t value) {
        if (value >= 0) return value / 8;
        return -static_cast<std::int32_t>(
            (-static_cast<std::int64_t>(value) + 7) / 8);
    };
    const std::int32_t tile_x = floor_div8(abs_x);
    const std::int32_t tile_y = floor_div8(abs_y);
    const std::uint32_t map_x =
        static_cast<std::uint32_t>(tile_x >> 1) & 127u;
    const std::uint32_t map_y =
        static_cast<std::uint32_t>(tile_y >> 1) & 127u;
    if (out_metadata) {
        out_metadata->tile_x = tile_x;
        out_metadata->tile_y = tile_y;
        out_metadata->map_x = map_x;
        out_metadata->map_y = map_y;
    }
    // A cell the guest has not written since the current area loaded is
    // untrustworthy: it may be residue from whatever previously occupied
    // this EWRAM table (e.g. a different room's fill, or another feature's
    // scratch use). Reject it before reading map/raw content.
    if (authored && !authored->authored(map_x, map_y)) return false;
    const std::size_t map_off = kMapBase +
        (static_cast<std::size_t>(map_y) * 128u + map_x) * 4u;
    if (map_off < kMapBase || map_off + 4u > kMapBase + kMapBytes ||
        map_off + 4u > ewram_size) {
        return false;
    }
    const std::uint32_t map_word =
        static_cast<std::uint32_t>(ewram[map_off]) |
        (static_cast<std::uint32_t>(ewram[map_off + 1u]) << 8) |
        (static_cast<std::uint32_t>(ewram[map_off + 2u]) << 16) |
        (static_cast<std::uint32_t>(ewram[map_off + 3u]) << 24);
    const std::uint32_t id = map_word & 0x0FFFu;
    if (out_metadata)
        out_metadata->map_id = static_cast<std::uint16_t>(id);
    const std::size_t raw_off = kRawBase +
        static_cast<std::size_t>(id) * 8u +
        (static_cast<std::size_t>(static_cast<std::uint32_t>(tile_y) & 1u) *
         4u) +
        (static_cast<std::size_t>(static_cast<std::uint32_t>(tile_x) & 1u) *
         2u);
    if (raw_off < kRawBase || raw_off + 2u > kRawBase + kRawBytes ||
        raw_off + 2u > ewram_size) {
        return false;
    }
    const std::uint16_t raw_entry =
        static_cast<std::uint16_t>(ewram[raw_off]) |
        static_cast<std::uint16_t>(ewram[raw_off + 1u] << 8);
    if (out_metadata) {
        out_metadata->raw_entry = raw_entry;
        out_metadata->has_raw_entry = true;
    }
    if (raw_entry == kGoldenSunFieldUnavailableTile) return false;
    *out_entry = raw_entry;
    return true;
}

}  // namespace gsr::widescreen
