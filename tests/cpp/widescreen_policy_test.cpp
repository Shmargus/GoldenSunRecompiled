#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "widescreen_policy.h"
#include "view_config.h"

namespace {

void store16(std::array<std::uint8_t, 0x400>& io, std::size_t offset,
             std::uint16_t value) {
    io[offset] = static_cast<std::uint8_t>(value);
    io[offset + 1u] = static_cast<std::uint8_t>(value >> 8);
}

void store32(std::vector<std::uint8_t>& memory, std::size_t offset,
             std::uint32_t value) {
    memory[offset] = static_cast<std::uint8_t>(value);
    memory[offset + 1u] = static_cast<std::uint8_t>(value >> 8);
    memory[offset + 2u] = static_cast<std::uint8_t>(value >> 16);
    memory[offset + 3u] = static_cast<std::uint8_t>(value >> 24);
}

void store16(std::vector<std::uint8_t>& memory, std::size_t offset,
             std::uint16_t value) {
    memory[offset] = static_cast<std::uint8_t>(value);
    memory[offset + 1u] = static_cast<std::uint8_t>(value >> 8);
}

}  // namespace

int main() {
    using namespace gsr::widescreen;
    const auto native = gbarecomp::resolve_view_geometry(
        240, 288, false, 480);
    if (native.width != 240 || native.extra_left != 0 ||
        native.extra_right != 0) {
        std::puts("widescreen_policy_test: native view changed");
        return 1;
    }
    const auto wide = gbarecomp::resolve_view_geometry(
        288, 288, false, 480);
    if (wide.width != 288 || wide.extra_left != 24 ||
        wide.extra_right != 24) {
        std::puts("widescreen_policy_test: fixed wide geometry mismatch");
        return 1;
    }
    if (golden_sun_field_obj_right_cull_limit(0) != 239 ||
        golden_sun_field_obj_right_cull_limit(wide.extra_right) != 263 ||
        golden_sun_field_obj_right_cull_limit(22) != 261 ||
        golden_sun_field_obj_right_cull_limit(60) != 299) {
        std::puts("widescreen_policy_test: field OBJ cull limit mismatch");
        return 1;
    }
    if (golden_sun_field_obj_bottom_cull_limit(0) != 159 ||
        golden_sun_field_obj_bottom_cull_limit(40) != 199) {
        std::puts("widescreen_policy_test: field OBJ bottom cull mismatch");
        return 1;
    }
    if (golden_sun_actor_precull_negative_padding(0, 0) != 32 ||
        golden_sun_actor_precull_negative_padding(60, 40) != 92 ||
        golden_sun_actor_precull_right_half_limit(0) != 136 ||
        golden_sun_actor_precull_right_half_limit(60) != 166 ||
        golden_sun_actor_precull_bottom_limit(0) != 208 ||
        golden_sun_actor_precull_bottom_limit(40) != 248) {
        std::puts("widescreen_policy_test: actor pre-cull mismatch");
        return 1;
    }
    if (golden_sun_field_list_x_upper_literal(0) != 0x012FFFFEu ||
        golden_sun_field_list_x_upper_literal(24) != 0x0147FFFEu ||
        golden_sun_field_list_x_upper_literal(60) != 0x016BFFFEu ||
        golden_sun_field_list_y_lower_literal(0) != 0xFFE00000u ||
        golden_sun_field_list_y_lower_literal(40) != 0xFFB80000u) {
        std::puts("widescreen_policy_test: field-list literal mismatch");
        return 1;
    }
    // The raw 160..199 range collides with hardware's negative-Y decode for
    // an off-top sprite. The runner's rich per-slot hook may resolve it only
    // after authenticated writer provenance.
    if (!golden_sun_field_obj_y_expanded(160, 40, 40) ||
        !golden_sun_field_obj_y_expanded(199, 40, 40) ||
        golden_sun_field_obj_y_expanded(159, 40, 40) ||
        golden_sun_field_obj_y_expanded(200, 40, 40) ||
        golden_sun_field_obj_y_expanded(180, 0, 0) ||
        golden_sun_field_obj_y_expanded(180, 40, 0) ||
        !golden_sun_field_obj_y_expanded(180, 0, 40)) {
        std::puts("widescreen_policy_test: field OBJ Y policy mismatch");
        return 1;
    }
    const auto expanded = gbarecomp::resolve_view_geometry(
        360, 240, 360, 240, false, 480, 240);
    if (expanded.width != 360 || expanded.height != 240 ||
        expanded.extra_left != 60 || expanded.extra_right != 60 ||
        expanded.extra_top != 40 || expanded.extra_bottom != 40) {
        std::puts("widescreen_policy_test: expanded 360x240 geometry mismatch");
        return 1;
    }
    const auto unsupported = gbarecomp::resolve_view_geometry(
        288, 240, false, 480);
    if (unsupported.width != 240) {
        std::puts("widescreen_policy_test: unsupported wide view accepted");
        return 1;
    }
    if (!golden_sun_suppress_bg0_margin(0, 0, 24, 24) ||
        !golden_sun_suppress_bg0_margin(0, 287, 24, 24) ||
        golden_sun_suppress_bg0_margin(0, 24, 24, 24) ||
        golden_sun_suppress_bg0_margin(0, 263, 24, 24) ||
        golden_sun_suppress_bg0_margin(1, 0, 24, 24)) {
        std::puts("widescreen_policy_test: BG0 margin suppression mismatch");
        return 1;
    }
    // The 360x240 form keeps BG0 inside the centered 240x160 canvas on both
    // axes. The old four-argument helper above remains horizontal-only for
    // the pre-height runtime seam.
    if (!golden_sun_suppress_bg0_margin(
            0, 59, 40, 60, 60, 40, 40) ||
        !golden_sun_suppress_bg0_margin(
            0, 60, 39, 60, 60, 40, 40) ||
        !golden_sun_suppress_bg0_margin(
            0, 300, 199, 60, 60, 40, 40) ||
        !golden_sun_suppress_bg0_margin(
            0, 299, 200, 60, 60, 40, 40) ||
        golden_sun_suppress_bg0_margin(
            0, 60, 40, 60, 60, 40, 40) ||
        golden_sun_suppress_bg0_margin(
            0, 299, 199, 60, 60, 40, 40) ||
        golden_sun_suppress_bg0_margin(
            1, 0, 0, 60, 60, 40, 40)) {
        std::puts("widescreen_policy_test: BG0 XY suppression mismatch");
        return 1;
    }
    if (!golden_sun_suppress_mode0_field_margin(1, 0, 24, 24) ||
        !golden_sun_suppress_mode0_field_margin(3, 23, 24, 24) ||
        !golden_sun_suppress_mode0_field_margin(1, 264, 24, 24) ||
        !golden_sun_suppress_mode0_field_margin(3, 287, 24, 24) ||
        golden_sun_suppress_mode0_field_margin(1, 24, 24, 24) ||
        golden_sun_suppress_mode0_field_margin(3, 263, 24, 24) ||
        golden_sun_suppress_mode0_field_margin(0, 0, 24, 24) ||
        golden_sun_suppress_mode0_field_margin(1, 0, 0, 0)) {
        std::puts("widescreen_policy_test: Mode0 field margin suppression mismatch");
        return 1;
    }
    std::array<std::uint8_t, 0x400> io{};
    constexpr std::uint16_t valid_dispcnt =
        kDispcntMode2 | kDispcntBg2 | kDispcntBg3;
    constexpr std::uint16_t valid_bgcnt = kBgcntWrap | kBgcntSize512;

    store16(io, 0x0C, valid_bgcnt);
    store16(io, 0x0E, valid_bgcnt);
    if (golden_sun_wide_margin_policy(valid_dispcnt, io.data()) != 0) {
        std::puts("widescreen_policy_test: valid overworld rejected");
        return 1;
    }

    const auto expect_pillarbox = [&](std::uint16_t dispcnt,
                                      const char* label) {
        if (golden_sun_wide_margin_policy(dispcnt, io.data()) !=
            kPillarboxAll) {
            std::printf("widescreen_policy_test: %s accepted\n", label);
            return false;
        }
        return true;
    };
    if (!expect_pillarbox(0x0000u | kDispcntBg2 | kDispcntBg3,
                          "wrong mode"))
        return 1;
    if (!expect_pillarbox(kDispcntMode2 | kDispcntBg2, "BG3 disabled"))
        return 1;
    if (!expect_pillarbox(valid_dispcnt | 0x2000u, "WIN0 enabled"))
        return 1;
    if (!expect_pillarbox(valid_dispcnt | 0x4000u, "WIN1 enabled"))
        return 1;
    if (!expect_pillarbox(valid_dispcnt | 0x8000u, "OBJ window enabled"))
        return 1;

    store16(io, 0x0C, kBgcntWrap | 0x4000u);  // 256x256, not 512x512.
    if (!expect_pillarbox(valid_dispcnt, "BG2 wrong size")) return 1;
    store16(io, 0x0C, valid_bgcnt);
    store16(io, 0x0E, 0x8000u);  // 512x512 without wrapping.
    if (!expect_pillarbox(valid_dispcnt, "BG3 wrap disabled")) return 1;

    // State1's measured town/dungeon signature: Mode 0, BG1/BG2/BG3 on, and
    // identical field-layer scroll pairs. The policy is deliberately based
    // only on standard PPU IO; it does not infer a room address or camera
    // bounds.
    constexpr std::uint16_t town_dispcnt =
        kDispcntMode0 | kDispcntBg1 | kDispcntBg2 | kDispcntBg3;
    constexpr std::uint16_t town_bgcnt = 0x0180u; // 256-colour, 256x256.
    store16(io, 0x0A, town_bgcnt);  // BG1CNT.
    store16(io, 0x0C, town_bgcnt);  // BG2CNT.
    store16(io, 0x0E, town_bgcnt);  // BG3CNT.
    // The raw field writer keeps atlas-region bits in the scroll registers;
    // the policy compares the effective 9-bit hardware scroll while the
    // provider below must preserve these unmasked values.
    store16(io, 0x14, 115);  // BG1 HOFS.
    store16(io, 0x16, 751);  // BG1 VOFS.
    store16(io, 0x18, 627);  // BG2 HOFS.
    store16(io, 0x1A, 239);  // BG2 VOFS.
    store16(io, 0x1C, 115);  // BG3 HOFS.
    store16(io, 0x1E, 239);  // BG3 VOFS.
    if (golden_sun_wide_margin_policy(town_dispcnt, io.data()) != 0 ||
        golden_sun_wide_margin_policy_reason(town_dispcnt, io.data()) !=
            GoldenSunWidePolicyReason::AuthorizedMode0) {
        std::puts("widescreen_policy_test: valid town rejected");
        return 1;
    }
    // Each required field layer is independently part of authentication;
    // removing any one must not leave a generic unequal-scroll Mode-0 route.
    if (!expect_pillarbox(town_dispcnt & ~kDispcntBg1,
                          "town BG1 disabled") ||
        !expect_pillarbox(town_dispcnt & ~kDispcntBg2,
                          "town BG2 disabled") ||
        !expect_pillarbox(town_dispcnt & ~kDispcntBg3,
                          "town BG3 disabled")) {
        return 1;
    }
    store16(io, 0x0A, town_bgcnt | 0x4000u);
    if (!expect_pillarbox(town_dispcnt, "town BG1 wrong size")) return 1;
    store16(io, 0x0A, town_bgcnt);
    store16(io, 0x0C, town_bgcnt | 0x4000u);
    if (!expect_pillarbox(town_dispcnt, "town BG2 wrong size")) return 1;
    store16(io, 0x0C, town_bgcnt);
    store16(io, 0x0E, town_bgcnt | 0x4000u);
    if (!expect_pillarbox(town_dispcnt, "town BG3 wrong size")) return 1;
    store16(io, 0x0E, town_bgcnt);
    // High register bits are ignored by the hardware classifier, but each
    // effective 9-bit scroll pair must still match.
    store16(io, 0x14, 115u + 0x0200u);
    if (golden_sun_wide_margin_policy(town_dispcnt, io.data()) != 0) {
        std::puts("widescreen_policy_test: equivalent BG1 scroll rejected");
        return 1;
    }
    store16(io, 0x14, 115u);
    // Palace-like evidence from session_20260826_084804: Mode 0 and the same
    // 256x256 layer geometry, but BG1's effective VOFS is 72 while BG2/BG3
    // are 200. The classifier must remain fail-closed; this fixture must not
    // become an implicit Palace whitelist.
    store16(io, 0x16, 584);  // raw BG1 VOFS; effective hardware value is 72.
    if (golden_sun_wide_margin_policy(town_dispcnt, io.data()) !=
            kPillarboxAll ||
        golden_sun_wide_margin_policy_reason(town_dispcnt, io.data()) !=
            GoldenSunWidePolicyReason::Mode0ScrollMismatch) {
        std::puts("widescreen_policy_test: unequal-scroll field accepted");
        return 1;
    }
    store16(io, 0x16, 751);  // Restore the measured State1 signature.

    // McCoy Palace's separate Mode-0 split-scroll class: exact layer
    // configuration, equal effective HOFS with the measured raw BG2-BG1
    // split, and the measured raw VOFS relation.
    const std::uint16_t palace_dispcnt = town_dispcnt;
    store16(io, 0x0A, kGoldenSunPalaceBg1Cnt);
    store16(io, 0x0C, kGoldenSunPalaceBg2Cnt);
    store16(io, 0x0E, kGoldenSunPalaceBg3Cnt);
    store16(io, 0x14, 0x0048u);  // BG1 HOFS, also BG3.
    store16(io, 0x18, 0x0248u);  // BG2 HOFS, raw +0x200.
    store16(io, 0x1C, 0x0048u);
    store16(io, 0x16, 0x0248u);  // BG1 VOFS, raw BG2+0x180.
    store16(io, 0x1A, 0x00C8u);  // BG2 VOFS.
    store16(io, 0x1E, 0x00C8u);
    if (!golden_sun_mode0_split_scroll_authorized(
            palace_dispcnt, io.data()) ||
        golden_sun_wide_margin_policy_reason(palace_dispcnt, io.data()) !=
            GoldenSunWidePolicyReason::Mode0ScrollMismatch ||
        golden_sun_wide_margin_policy(palace_dispcnt, io.data()) !=
            kPillarboxAll) {
        std::puts("widescreen_policy_test: Palace split class mismatch");
        return 1;
    }
    store16(io, 0x18, 0x0249u);
    if (golden_sun_mode0_split_scroll_policy_reason(
            palace_dispcnt, io.data()) !=
        GoldenSunWidePolicyReason::Mode0SplitScrollMismatch) {
        std::puts("widescreen_policy_test: Palace split scroll not exact");
        return 1;
    }
    store16(io, 0x18, 0x0248u);
    store16(io, 0x0E, kGoldenSunPalaceBg3Cnt | 0x4000u);
    if (golden_sun_mode0_split_scroll_policy_reason(
            palace_dispcnt, io.data()) !=
        GoldenSunWidePolicyReason::Mode0SplitGeometry) {
        std::puts("widescreen_policy_test: Palace BG CNT not exact");
        return 1;
    }
    store16(io, 0x0E, kGoldenSunPalaceBg3Cnt);

    // A split-scroll row is not authorized until the next complete clean
    // frame, and one mixed/invalid row clears the candidate immediately.
    GoldenSunMode0SplitScrollFrame split_frame;
    if (split_frame.observe(7u, true, 3u) ||
        split_frame.observe(7u, true, 3u) ||
        split_frame.observe(7u, true, 3u) ||
        !split_frame.complete ||
        split_frame.observe(7u, true, 3u) ||
        split_frame.observe(7u, false, 3u) || split_frame.complete ||
        split_frame.observe(7u, true, 3u) ||
        split_frame.observe(8u, true, 3u) ||
        split_frame.observe(8u, true, 3u) ||
        split_frame.observe(8u, true, 3u) ||
        !split_frame.complete || !split_frame.observe(9u, true, 3u)) {
        std::puts("widescreen_policy_test: split frame gate mismatch");
        return 1;
    }
    if (golden_sun_oam_shadow_slot(0x0300347Cu) != 0 ||
        golden_sun_oam_shadow_slot(0x03003874u) != 127 ||
        golden_sun_oam_shadow_slot(0x0300387Cu) != -1 ||
        golden_sun_oam_shadow_slot(0x03003480u) != -1) {
        std::puts("widescreen_policy_test: OAM shadow slot bounds mismatch");
        return 1;
    }
    if (!golden_sun_field_obj_y_cull_authorized(
            true, 0x0800B27Cu, 159u, 40u) ||
        !golden_sun_field_obj_y_cull_authorized(
            true, 0x0800B326u, 159u, 40u) ||
        golden_sun_field_obj_y_cull_authorized(
            false, 0x0800B27Cu, 159u, 40u) ||
        golden_sun_field_obj_y_cull_authorized(
            true, 0x0800B322u, 159u, 40u) ||
        golden_sun_field_obj_y_cull_authorized(
            true, 0x0800B27Cu, 160u, 40u) ||
        golden_sun_field_obj_y_cull_authorized(
            true, 0x0800B27Cu, 159u, 0u) ||
        !golden_sun_field_obj_y_expanded(183, 0, 24) ||
        golden_sun_field_obj_y_expanded(184, 0, 24)) {
        std::puts("widescreen_policy_test: authenticated OBJ-Y mismatch");
        return 1;
    }
    if (!golden_sun_obj_provenance_frame_fresh(10u, 10u) ||
        !golden_sun_obj_provenance_frame_fresh(10u, 11u) ||
        golden_sun_obj_provenance_frame_fresh(10u, 12u) ||
        golden_sun_obj_provenance_frame_fresh(UINT64_MAX, 0u)) {
        std::puts("widescreen_policy_test: OBJ-Y provenance lifetime mismatch");
        return 1;
    }

    // The split-scroll provider uses each layer's raw scroll. BG1/BG2 are
    // intentionally distinct here; a BG3 cross-layer boundary would reject
    // neither of these independently valid samples.
    const auto split_map_offset = [](std::uint32_t tile_x,
                                     std::uint32_t tile_y) {
        return static_cast<std::size_t>(0x10000u) +
            ((static_cast<std::size_t>((tile_y >> 1) & 127u) * 128u) +
             ((tile_x >> 1) & 127u)) * 4u;
    };
    const auto split_raw_offset = [](std::uint32_t id,
                                     std::uint32_t tile_x,
                                     std::uint32_t tile_y) {
        return static_cast<std::size_t>(0x20000u) +
            static_cast<std::size_t>(id) * 8u +
            static_cast<std::size_t>(tile_y & 1u) * 4u +
            static_cast<std::size_t>(tile_x & 1u) * 2u;
    };
    std::vector<std::uint8_t> split_ewram(0x40000u, 0);
    // BG1 (-24,0) -> raw scroll (0x48,0x248), tile (6,73), map (3,36).
    store32(split_ewram, split_map_offset(6, 73), 0x00000007u);
    store16(split_ewram, split_raw_offset(7, 6, 73), 0x1357u);
    // BG2 (264,0) -> raw scroll (0x248,0xc8), tile (106,25), map (53,12).
    store32(split_ewram, split_map_offset(106, 25), 0x00000009u);
    store16(split_ewram, split_raw_offset(9, 106, 25), 0x2468u);
    std::uint16_t split_entry = 0;
    if (!golden_sun_field_tilemap_entry(
            palace_dispcnt, io.data(), io.size(), split_ewram.data(),
            split_ewram.size(), 1, -24, 0, &split_entry, nullptr, nullptr,
            false, true) || split_entry != 0x1357u ||
        !golden_sun_field_tilemap_entry(
            palace_dispcnt, io.data(), io.size(), split_ewram.data(),
            split_ewram.size(), 2, 264, 0, &split_entry, nullptr, nullptr,
            false, true) || split_entry != 0x2468u) {
        std::puts("widescreen_policy_test: split raw-scroll lookup mismatch");
        return 1;
    }

    // Restore the equal-scroll fixture for the existing field/provider cases.
    store16(io, 0x0A, town_bgcnt);
    store16(io, 0x0C, town_bgcnt);
    store16(io, 0x0E, town_bgcnt);
    store16(io, 0x14, 115);
    store16(io, 0x16, 751);
    store16(io, 0x18, 627);
    store16(io, 0x1A, 239);
    store16(io, 0x1C, 115);
    store16(io, 0x1E, 239);
    const std::array<std::uint32_t, 6> cull_pcs{
        0x0800B27Cu, 0x0800B322u, 0x0800B326u,
        0x0800B3CAu, 0x0800B3D6u, 0x0800B3EAu};
    const std::array<std::uint32_t, 6> cull_immediates{
        159u, 239u, 159u, 32u, 136u, 208u};
    for (std::size_t i = 0; i < cull_pcs.size(); ++i) {
        if (golden_sun_cull_site_index(cull_pcs[i]) !=
                static_cast<int>(i) ||
            golden_sun_cull_site_pc(i) != cull_pcs[i] ||
            golden_sun_cull_site_original(i) != cull_immediates[i] ||
            !golden_sun_cull_site_matches(cull_pcs[i], cull_immediates[i]) ||
            golden_sun_cull_site_matches(cull_pcs[i], cull_immediates[i] + 1u)) {
            std::puts("widescreen_policy_test: cull route mismatch");
            return 1;
        }
    }
    if (golden_sun_cull_site_index(0x0800B3F0u) != -1 ||
        golden_sun_cull_site_matches(0x0800B3F0u, 0u)) {
        std::puts("widescreen_policy_test: unknown cull route accepted");
        return 1;
    }
    // Object authorization is a separate gate from the Mode-0 background
    // classifier. The executed Func_b168 final X route and the two measured
    // equal-scroll Y routes are eligible; Mode 2, split-scroll Mode 0, every
    // other cull site, and the stock native-width route remain fail-closed.
    if (!golden_sun_field_obj_cull_authorized(
            true, 0x0800B322u, 239u, 24u) ||
        golden_sun_field_obj_cull_authorized(
            false, 0x0800B322u, 239u, 24u) ||
        golden_sun_field_obj_cull_authorized(
            true, 0x0800B322u, 239u, 0u) ||
        golden_sun_field_obj_cull_authorized(
            true, 0x0800B322u, 159u, 24u) ||
        golden_sun_field_obj_cull_authorized(
            true, 0x0800B326u, 159u, 24u) ||
        golden_sun_field_obj_cull_authorized(
            true, 0x0800B3CAu, 32u, 24u) ||
        golden_sun_field_obj_cull_authorized(
            true, 0x0800B3F0u, 239u, 24u)) {
        std::puts("widescreen_policy_test: object cull gate mismatch");
        return 1;
    }
    if (!golden_sun_field_obj_x_authorized(true, 0x100, 24u) ||
        !golden_sun_field_obj_x_authorized(true, 0x107, 24u) ||
        golden_sun_field_obj_x_authorized(true, 0x108, 0u) ||
        golden_sun_field_obj_x_authorized(true, 0x118, 24u) ||
        golden_sun_field_obj_x_authorized(false, 0x100, 24u) ||
        golden_sun_field_obj_x_authorized(true, -1, 24u)) {
        std::puts("widescreen_policy_test: object X authorization mismatch");
        return 1;
    }
    // This is intentionally the Mode-2/overworld-shaped negative fixture:
    // equal-scroll is not supplied as a generic object authorization token.
    if (golden_sun_field_obj_x_authorized(false, 0x100, 60u)) {
        std::puts("widescreen_policy_test: Mode2 object route accepted");
        return 1;
    }
    store16(io, 0x0A, town_bgcnt | 0x4000u);
    if (!expect_pillarbox(town_dispcnt, "town BG1 non-256x256")) return 1;
    store16(io, 0x0A, town_bgcnt);
    store16(io, 0x0E, town_bgcnt | 0x8000u);
    if (!expect_pillarbox(town_dispcnt, "town BG3 non-256x256")) return 1;
    store16(io, 0x0E, town_bgcnt);
    if (!expect_pillarbox(town_dispcnt & ~kDispcntBg1,
                          "town BG1 disabled"))
        return 1;
    if (!expect_pillarbox(town_dispcnt & ~kDispcntBg2,
                          "town BG2 disabled"))
        return 1;
    if (!expect_pillarbox(town_dispcnt & ~kDispcntBg3,
                          "town BG3 disabled"))
        return 1;
    store16(io, 0x14, 114);
    if (!expect_pillarbox(town_dispcnt, "town BG1 HOFS mismatch")) return 1;
    store16(io, 0x14, 115);
    store16(io, 0x18, 626);
    if (!expect_pillarbox(town_dispcnt, "town BG2 HOFS mismatch")) return 1;
    store16(io, 0x18, 627);
    store16(io, 0x1C, 114);
    if (!expect_pillarbox(town_dispcnt, "town BG3 HOFS mismatch")) return 1;
    store16(io, 0x1C, 115);
    store16(io, 0x16, 750);
    if (!expect_pillarbox(town_dispcnt, "town BG1 VOFS mismatch")) return 1;
    store16(io, 0x16, 751);
    store16(io, 0x1A, 238);
    if (!expect_pillarbox(town_dispcnt, "town BG2 VOFS mismatch")) return 1;
    store16(io, 0x1A, 239);
    store16(io, 0x1E, 238);
    if (!expect_pillarbox(town_dispcnt, "town BG3 VOFS mismatch")) return 1;
    store16(io, 0x1E, 239); // Restore only to make the following cases clear.
    if (!expect_pillarbox(town_dispcnt | kDispcntForcedBlank,
                          "town forced blank"))
        return 1;
    if (!expect_pillarbox(town_dispcnt | 0x2000u, "town WIN0 enabled"))
        return 1;

    // Mode 1 is the measured battle family and must remain pillarboxed even
    // when its affine BG happens to look like a world map configuration.
    if (!expect_pillarbox(0x0401u, "Mode1 battle")) return 1;

    if (golden_sun_wide_margin_policy(valid_dispcnt, nullptr) !=
        kPillarboxAll) {
        std::puts("widescreen_policy_test: null IO was accepted");
        return 1;
    }

    // The measured Mode0 producer is a pure EWRAM lookup. Give the helper
    // distinct atlas IDs for left/right margins so aliasing the resident VRAM
    // ring cannot accidentally satisfy this test.
    std::vector<std::uint8_t> ewram(0x40000u, 0);
    const auto map_offset = [](std::uint32_t tile_x,
                               std::uint32_t tile_y) {
        return static_cast<std::size_t>(0x10000u) +
            ((static_cast<std::size_t>((tile_y >> 1) & 127u) * 128u) +
             ((tile_x >> 1) & 127u)) * 4u;
    };
    const auto raw_offset = [](std::uint32_t id, std::uint32_t tile_x,
                               std::uint32_t tile_y) {
        return static_cast<std::size_t>(0x20000u) +
            static_cast<std::size_t>(id) * 8u +
            static_cast<std::size_t>(tile_y & 1u) * 4u +
            static_cast<std::size_t>(tile_x & 1u) * 2u;
    };
    // BG1 left: (115 - 24, 751 + 0) -> tile (11, 93), map cell (5, 46).
    store32(ewram, map_offset(11, 93), 0xABCD0007u);
    store16(ewram, raw_offset(7, 11, 93), 0x1234u);
    // BG2 right: (627 + 264, 239 + 0) -> tile (111, 29), map cell (55, 14).
    store32(ewram, map_offset(111, 29), 0xFACE0009u);
    store16(ewram, raw_offset(9, 111, 29), 0x5678u);
    // BG3 right: (115 + 264, 239 + 0) -> tile (47, 29), map cell (23, 14).
    store32(ewram, map_offset(47, 29), 0xBEEF000Bu);
    store16(ewram, raw_offset(11, 47, 29), 0x9ABCu);
    std::uint16_t entry = 0;
    if (!golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry) || entry != 0x1234u) {
        std::puts("widescreen_policy_test: BG1 atlas margin lookup mismatch");
        return 1;
    }
    if (!golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            2, 264, 0, &entry) || entry != 0x5678u) {
        std::puts("widescreen_policy_test: BG2 atlas margin lookup mismatch");
        return 1;
    }
    if (!golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            3, 264, 0, &entry) || entry != 0x9ABCu) {
        std::puts("widescreen_policy_test: BG3 atlas margin lookup mismatch");
        return 1;
    }
    // The 360x240 provider domain includes signed top/bottom scanlines and
    // rejects only the authentic native rectangle. Keep the measured atlas
    // formulas unchanged for corner and vertical samples.
    if (!golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -60, -40, &entry) ||
        !golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 0, -40, &entry) ||
        !golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 0, 160, &entry) ||
        !golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 299, 199, &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 0, 0, &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -61, 0, &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 300, 0, &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 0, -41, &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 0, 200, &entry)) {
        std::puts("widescreen_policy_test: signed 360x240 atlas domain mismatch");
        return 1;
    }
    // An unavailable atlas entry must fall through to the PPU's black
    // backdrop.  Reject the exact raw marker, not map IDs or tile-number
    // bits, so valid terrain remains untouched.
    store16(ewram, raw_offset(7, 11, 93),
            kGoldenSunFieldUnavailableTile);
    GoldenSunFieldTilemapMetadata unavailable_metadata{};
    if (golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry, &unavailable_metadata) ||
        !unavailable_metadata.has_raw_entry ||
        unavailable_metadata.raw_entry != kGoldenSunFieldUnavailableTile ||
        !golden_sun_field_bg3_unavailable(unavailable_metadata)) {
        std::puts("widescreen_policy_test: unavailable atlas tile accepted");
        return 1;
    }
    store16(ewram, raw_offset(7, 11, 93), 0x03ffu);
    if (!golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry) || entry != 0x03ffu) {
        std::puts("widescreen_policy_test: valid tile-number entry rejected");
        return 1;
    }
    GoldenSunFieldTilemapMetadata void_bg3{};
    void_bg3.has_raw_entry = true;
    void_bg3.map_id = kGoldenSunFieldNoMapId;
    void_bg3.raw_entry = kGoldenSunFieldNoMapTile;
    if (!golden_sun_field_bg3_no_map(void_bg3) ||
        !golden_sun_field_atlas_unavailable(void_bg3)) {
        std::puts("widescreen_policy_test: BG3 no-map entry not recognized");
        return 1;
    }
    // Other metadata remains authored; the runner applies this BG3 boundary
    // result across all field layers at the same expanded coordinate.
    GoldenSunFieldTilemapMetadata authored_bg{};
    authored_bg.has_raw_entry = true;
    authored_bg.map_id = 0x021u;
    authored_bg.raw_entry = 0x0000u;
    if (golden_sun_field_bg3_no_map(authored_bg)) {
        std::puts("widescreen_policy_test: BG1/BG2 terrain was suppressed");
        return 1;
    }
    // Metatile 3 is authored room data; the earlier partial trace had
    // misclassified its four ordinary character entries as yellow fill.
    void_bg3.raw_entry = 0x03C4u;
    void_bg3.map_id = 0x003u;
    if (golden_sun_field_bg3_no_map(void_bg3)) {
        std::puts("widescreen_policy_test: authored BG3 map rejected");
        return 1;
    }
    void_bg3.raw_entry = kGoldenSunFieldUnavailableTile;
    if (golden_sun_field_bg3_no_map(void_bg3) ||
        !golden_sun_field_bg3_unavailable(void_bg3) ||
        !golden_sun_field_atlas_unavailable(void_bg3)) {
        std::puts("widescreen_policy_test: raw FFFF unavailable marker missed");
        return 1;
    }
    void_bg3.has_raw_entry = false;
    if (golden_sun_field_bg3_unavailable(void_bg3)) {
        std::puts("widescreen_policy_test: metadata-less FFFF was repaired");
        return 1;
    }
    void_bg3.has_raw_entry = true;

    // A dummy BG3 margin tile is replaced by the nearest authored tile while
    // walking inward in 8px steps. The resolver is synthetic here so this
    // test covers direction/order without touching guest memory.
    const auto make_dummy = [] {
        GoldenSunFieldTilemapMetadata metadata;
        metadata.has_raw_entry = true;
        metadata.map_id = kGoldenSunFieldNoMapId;
        metadata.raw_entry = kGoldenSunFieldNoMapTile;
        return metadata;
    };
    const auto make_authored = [] {
        GoldenSunFieldTilemapMetadata metadata;
        metadata.has_raw_entry = true;
        metadata.map_id = 0x021u;
        metadata.raw_entry = 0x0000u;
        return metadata;
    };
    std::vector<int> probes;
    std::uint16_t fallback = 0;
    auto left_resolver = [&](int candidate, std::uint16_t* candidate_entry,
                             GoldenSunFieldTilemapMetadata* metadata) {
        probes.push_back(candidate);
        if (candidate == -16 || candidate == -8 || candidate == 0) {
            *metadata = make_dummy();
            return true;
        }
        if (candidate == 8) {
            *metadata = make_authored();
            *candidate_entry = 0x1234u;
            return true;
        }
        return false;
    };
    if (!golden_sun_field_bg3_inward_entry(
            -24, 240, 24, left_resolver, &fallback) ||
        fallback != 0x1234u ||
        probes != std::vector<int>({-16, -8, 0, 8})) {
        std::puts("widescreen_policy_test: left BG3 inward search mismatch");
        return 1;
    }

    probes.clear();
    auto right_resolver = [&](int candidate, std::uint16_t* candidate_entry,
                              GoldenSunFieldTilemapMetadata* metadata) {
        probes.push_back(candidate);
        if (candidate == 255 || candidate == 247 || candidate == 239) {
            *metadata = make_dummy();
            return true;
        }
        if (candidate == 231) {
            *metadata = make_authored();
            *candidate_entry = 0x5678u;
            return true;
        }
        return false;
    };
    if (!golden_sun_field_bg3_inward_entry(
            263, 240, 24, right_resolver, &fallback) ||
        fallback != 0x5678u ||
        probes != std::vector<int>({255, 247, 239, 231})) {
        std::puts("widescreen_policy_test: right BG3 inward search mismatch");
        return 1;
    }

    probes.clear();
    auto ffff_resolver = [&](int candidate,
                             std::uint16_t* candidate_entry,
                             GoldenSunFieldTilemapMetadata* metadata) {
        probes.push_back(candidate);
        if (candidate == -16) {
            *metadata = make_authored();
            metadata->raw_entry = kGoldenSunFieldUnavailableTile;
            *candidate_entry = kGoldenSunFieldUnavailableTile;
            return true;
        }
        if (candidate == -8) {
            *metadata = make_authored();
            *candidate_entry = 0x3456u;
            return true;
        }
        return false;
    };
    if (!golden_sun_field_bg3_inward_entry(
            -24, 240, 24, ffff_resolver, &fallback) ||
        fallback != 0x3456u || probes != std::vector<int>({-16, -8})) {
        std::puts("widescreen_policy_test: FFFF BG3 search mismatch");
        return 1;
    }

    probes.clear();
    auto no_candidate_resolver = [&](int candidate,
                                     std::uint16_t* candidate_entry,
                                     GoldenSunFieldTilemapMetadata* metadata) {
        (void)candidate_entry;
        probes.push_back(candidate);
        *metadata = make_dummy();
        return true;
    };
    std::vector<int> expected_left_probes{-16, -8};
    for (int candidate = 0; candidate <= 232; candidate += 8)
        expected_left_probes.push_back(candidate);
    if (golden_sun_field_bg3_inward_entry(
            -24, 240, 24, no_candidate_resolver, &fallback) ||
        probes != expected_left_probes) {
        std::puts("widescreen_policy_test: dummy BG3 search did not fail closed");
        return 1;
    }
    probes.clear();
    if (golden_sun_field_bg3_inward_entry(
            -25, 240, 24, no_candidate_resolver, &fallback) ||
        !probes.empty()) {
        std::puts("widescreen_policy_test: left BG3 search exceeded margin");
        return 1;
    }
    probes.clear();
    if (golden_sun_field_bg3_inward_entry(
            263, 240, 24, no_candidate_resolver, &fallback)) {
        std::puts("widescreen_policy_test: right BG3 search did not fail closed");
        return 1;
    }
    std::vector<int> expected_right_probes;
    for (int candidate = 255; candidate >= 7; candidate -= 8)
        expected_right_probes.push_back(candidate);
    if (probes != expected_right_probes) {
        std::puts("widescreen_policy_test: right BG3 search stopped early");
        return 1;
    }
    probes.clear();
    auto no_ffff_candidate_resolver = [&](int candidate,
                                          std::uint16_t* candidate_entry,
                                          GoldenSunFieldTilemapMetadata* metadata) {
        (void)candidate_entry;
        probes.push_back(candidate);
        *metadata = make_authored();
        metadata->raw_entry = kGoldenSunFieldUnavailableTile;
        return true;
    };
    if (golden_sun_field_bg3_inward_entry(
            263, 240, 24, no_ffff_candidate_resolver, &fallback) ||
        probes != expected_right_probes) {
        std::puts("widescreen_policy_test: FFFF BG3 search did not fail closed");
        return 1;
    }

    // Vertical BG3 repair uses the same inward 8px order and treats both the
    // measured dummy entries and raw 0xffff as unavailable. This is pure
    // presentation logic; the resolver below stands in for the live atlas.
    std::vector<int> vertical_probes;
    auto vertical_resolver = [&](int candidate,
                                 std::uint16_t* candidate_entry,
                                 GoldenSunFieldTilemapMetadata* metadata) {
        vertical_probes.push_back(candidate);
        if (candidate <= 0) {
            *metadata = make_dummy();
            if (candidate == 0) {
                metadata->map_id = 0x021u;
                metadata->raw_entry = kGoldenSunFieldUnavailableTile;
            }
            return true;
        }
        if (candidate == 8) {
            *metadata = make_authored();
            *candidate_entry = 0x9ABCu;
            return true;
        }
        return false;
    };
    if (!golden_sun_field_bg3_inward_entry_vertical(
            64, -40, 160, 40, vertical_resolver, &fallback) ||
        fallback != 0x9ABCu ||
        vertical_probes != std::vector<int>({-32, -24, -16, -8, 0, 8})) {
        std::puts("widescreen_policy_test: top BG3 inward search mismatch");
        return 1;
    }

    vertical_probes.clear();
    auto bottom_resolver = [&](int candidate,
                               std::uint16_t* candidate_entry,
                               GoldenSunFieldTilemapMetadata* metadata) {
        vertical_probes.push_back(candidate);
        if (candidate >= 159) {
            *metadata = make_dummy();
            return true;
        }
        if (candidate == 151) {
            *metadata = make_authored();
            *candidate_entry = 0xBC9Au;
            return true;
        }
        return false;
    };
    if (!golden_sun_field_bg3_inward_entry_vertical(
            64, 199, 160, 40, bottom_resolver, &fallback) ||
        fallback != 0xBC9Au ||
        vertical_probes != std::vector<int>({191, 183, 175, 167, 159, 151})) {
        std::puts("widescreen_policy_test: bottom BG3 inward search mismatch");
        return 1;
    }

    // A corner can require movement on both axes. The 2D helper preserves
    // horizontal/vertical inward order and then reaches a native candidate.
    std::vector<std::pair<int, int>> corner_probes;
    auto corner_resolver = [&](int candidate_x, int candidate_y,
                               std::uint16_t* candidate_entry,
                               GoldenSunFieldTilemapMetadata* metadata) {
        corner_probes.emplace_back(candidate_x, candidate_y);
        // x=-60 is intentionally not tile-aligned; 8px inward steps reach
        // x=4 as the first native candidate.
        if (candidate_x == 4 && candidate_y == 0) {
            *metadata = make_authored();
            *candidate_entry = 0x55AAu;
            return true;
        }
        *metadata = make_dummy();
        return true;
    };
    if (!golden_sun_field_bg3_inward_entry_2d(
            -60, -40, 240, 160, 60, 40, corner_resolver, &fallback) ||
        fallback != 0x55AAu || corner_probes.empty() ||
        corner_probes.back() != std::pair<int, int>{4, 0}) {
        std::puts("widescreen_policy_test: corner BG3 inward search mismatch");
        return 1;
    }
    if (golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, 24, 0, &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            0, -24, 0, &entry) ||
        golden_sun_field_tilemap_entry(
            1u | kDispcntBg1 | kDispcntBg2 | kDispcntBg3,
            io.data(), io.size(), ewram.data(), ewram.size(), 1, -24, 0,
            &entry) ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), 0x20000u, 1,
            -24, 0, &entry)) {
        std::puts("widescreen_policy_test: invalid atlas lookup accepted");
        return 1;
    }
    // Authored-bitmap gate: an unwritten cell is unavailable, a written
    // cell is available, and the bitmap clears on reset. Reuses the BG1
    // left-margin cell/entry staged above (map cell (5,46), raw 0x03ff)
    // rather than restaging fresh EWRAM content.
    GoldenSunFieldAuthoredMap authored;
    if (authored.authored_count() != 0u ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry, nullptr, &authored)) {
        std::puts("widescreen_policy_test: unwritten authored cell accepted");
        return 1;
    }
    authored.mark_write(0x02010000u + (46u * 128u + 5u) * 4u, 4u);
    if (authored.authored_count() != 1u ||
        !golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry, nullptr, &authored) || entry != 0x03ffu) {
        std::puts("widescreen_policy_test: written authored cell rejected");
        return 1;
    }
    authored.reset();
    if (golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry, nullptr, &authored)) {
        std::puts(
            "widescreen_policy_test: authored bitmap did not clear on reset");
        return 1;
    }
    return 0;
}
