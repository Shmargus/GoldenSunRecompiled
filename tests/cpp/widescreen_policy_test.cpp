#include <algorithm>
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
    const auto expanded = gbarecomp::resolve_view_geometry(
        360, 240, 360, 240, false, 480, 240);
    if (expanded.width != 360 || expanded.height != 240 ||
        expanded.extra_left != 60 || expanded.extra_right != 60 ||
        expanded.extra_top != 40 || expanded.extra_bottom != 40) {
        std::puts("widescreen_policy_test: expanded 360x240 geometry mismatch");
        return 1;
    }
    if (!golden_sun_expanded_view_active(
            360u, 240u, 60u, 60u, 40u, 40u) ||
        golden_sun_expanded_view_active(240u, 160u, 0u, 0u, 0u, 0u) ||
        golden_sun_expanded_view_active(288u, 160u, 24u, 24u, 0u, 0u) ||
        golden_sun_expanded_view_active(360u, 240u, 60u, 60u, 40u, 39u)) {
        std::puts("widescreen_policy_test: strict expanded gate mismatch");
        return 1;
    }
    const std::array<std::uint32_t, 10> expected_forced_decisions{
        0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u};
    for (std::size_t i = 0; i < expected_forced_decisions.size(); ++i) {
        if (static_cast<std::uint32_t>(
                kGoldenSunViewportBranchReviews[i].forced_decision) !=
            expected_forced_decisions[i]) {
            std::puts("widescreen_policy_test: reviewed decision vector mismatch");
            return 1;
        }
    }
    // Every reviewed PC remains indexed; B27E/B324 keep their operand-bounded
    // behavior while the alternate B328 writer stays fail-closed.
    for (std::size_t i = 0; i < kGoldenSunViewportBranchReviews.size(); ++i) {
        const auto& review = kGoldenSunViewportBranchReviews[i];
        if (golden_sun_viewport_branch_site_index(review.pc) !=
                static_cast<int>(i)) {
            std::puts("widescreen_policy_test: branch route index mismatch");
            return 1;
        }
    }
    std::uint32_t object_decision = 0xFFFFFFFFu;
    for (std::int32_t y : {261, 410}) {
        if (golden_sun_expanded_viewport_branch_override(
                0x0800B328u, 1u, 360u, 240u, 60u, 60u, 40u, 40u, y,
                &object_decision) || object_decision != 0xFFFFFFFFu) {
            std::puts("widescreen_policy_test: out-of-range OBJ Y admitted");
            return 1;
        }
    }
    // The generic branch policy keeps the alternate B328 route closed; the
    // parent-correlated helper below admits only the measured ownership path.
    for (std::int32_t y : {160, 199}) {
        object_decision = 0xFFFFFFFFu;
        if (golden_sun_expanded_viewport_branch_override(
                0x0800B328u, 1u, 360u, 240u, 60u, 60u, 40u, 40u, y,
                &object_decision) || object_decision != 0xFFFFFFFFu) {
            std::puts("widescreen_policy_test: B328 OBJ Y widened");
            return 1;
        }
    }
    GoldenSunObjB328ParentMatch b328_parent{};
    b328_parent.valid = true;
    b328_parent.staging_address = 0x03002070u;
    b328_parent.frame = 100u;
    b328_parent.call_depth = 3u;
    b328_parent.call_return_pc = 0x08001234u;
    b328_parent.original_decision = 1u;
    b328_parent.final_decision = 0u;
    b328_parent.overridden = true;
    object_decision = 0xFFFFFFFFu;
    if (!golden_sun_b328_parent_override(
            true, 1u, 179, 0x03002070u, 100u, 3u, 0x08001234u,
            b328_parent, &object_decision) || object_decision != 0u) {
        std::puts("widescreen_policy_test: correlated B328 route rejected");
        return 1;
    }
    const auto expect_b328_parent_reject =
        [&](bool active, std::uint32_t original, std::int32_t operand,
            std::uint32_t staging, std::uint64_t frame,
            std::uint32_t depth, std::uint32_t return_pc,
            const GoldenSunObjB328ParentMatch& parent,
            const char* message) {
            constexpr std::uint32_t untouched = 0xA5A5A5A5u;
            std::uint32_t decision = untouched;
            if (golden_sun_b328_parent_override(
                    active, original, operand, staging, frame, depth,
                    return_pc, parent, &decision) || decision != untouched) {
                std::puts(message);
                return false;
            }
            return true;
        };
    auto parentless = b328_parent;
    parentless.valid = false;
    auto parent_already_accepted = b328_parent;
    parent_already_accepted.original_decision = 0u;
    parent_already_accepted.final_decision = 0u;
    parent_already_accepted.overridden = false;
    auto parent_stale_staging = b328_parent;
    parent_stale_staging.staging_address ^= 4u;
    if (!expect_b328_parent_reject(
            true, 1u, 179, 0x03002070u, 100u, 3u, 0x08001234u,
            parentless, "widescreen_policy_test: parentless B328 admitted") ||
        !expect_b328_parent_reject(
            true, 1u, 179, 0x03002070u, 100u, 3u, 0x08001234u,
            parent_already_accepted,
            "widescreen_policy_test: accepted parent B328 admitted") ||
        !expect_b328_parent_reject(
            true, 1u, 179, 0x03002070u, 100u, 3u, 0x08001234u,
            parent_stale_staging,
            "widescreen_policy_test: stale B328 staging admitted") ||
        !expect_b328_parent_reject(
            true, 1u, 179, 0x03002070u, 101u, 3u, 0x08001234u,
            b328_parent, "widescreen_policy_test: stale B328 frame admitted") ||
        !expect_b328_parent_reject(
            true, 1u, 179, 0x03002070u, 100u, 4u, 0x08001234u,
            b328_parent, "widescreen_policy_test: stale B328 context admitted") ||
        !expect_b328_parent_reject(
            true, 1u, 179, 0x03002070u, 100u, 3u, 0x08001235u,
            b328_parent, "widescreen_policy_test: stale B328 return admitted") ||
        !expect_b328_parent_reject(
            true, 0u, 179, 0x03002070u, 100u, 3u, 0x08001234u,
            b328_parent, "widescreen_policy_test: accepted B328 widened") ||
        !expect_b328_parent_reject(
            true, 1u, 159, 0x03002070u, 100u, 3u, 0x08001234u,
            b328_parent, "widescreen_policy_test: low B328 band admitted") ||
        !expect_b328_parent_reject(
            true, 1u, 200, 0x03002070u, 100u, 3u, 0x08001234u,
            b328_parent, "widescreen_policy_test: high B328 band admitted") ||
        !expect_b328_parent_reject(
            false, 1u, 179, 0x03002070u, 100u, 3u, 0x08001234u,
            b328_parent, "widescreen_policy_test: inactive B328 widened")) {
        return 1;
    }
    for (std::int32_t x : {240, 299}) {
        object_decision = 0xFFFFFFFFu;
        if (!golden_sun_expanded_viewport_branch_override(
                0x0800B324u, 1u, 360u, 240u, 60u, 60u, 40u, 40u, x,
                &object_decision) || object_decision != 0u) {
            std::puts("widescreen_policy_test: valid expanded OBJ X rejected");
            return 1;
        }
    }
    object_decision = 0xFFFFFFFFu;
    if (golden_sun_expanded_viewport_branch_override(
            0x0800B324u, 1u, 360u, 240u, 60u, 60u, 40u, 40u, 300,
            &object_decision) || object_decision != 0xFFFFFFFFu) {
        std::puts("widescreen_policy_test: out-of-range OBJ X admitted");
        return 1;
    }

    // The seven other routes may admit only their newly visible operand band.
    // A rejected override must leave the output slot untouched.
    const auto expect_route = [&](std::uint32_t pc, std::uint32_t original,
                                  std::int32_t operand, bool expected_override,
                                  std::uint32_t expected_decision) {
        constexpr std::uint32_t untouched = 0xA5A5A5A5u;
        std::uint32_t decision = untouched;
        const bool overridden = golden_sun_expanded_viewport_branch_override(
            pc, original, 360u, 240u, 60u, 60u, 40u, 40u, operand,
            &decision);
        return overridden == expected_override &&
            ((!overridden && decision == untouched) ||
             (overridden && decision == expected_decision));
    };
    const std::int32_t b388_lower_x = -92;
    const std::int32_t b388_lower_y = -72;
    if (!expect_route(0x0800B3D2u, 0u, b388_lower_x, true, 1u) ||
        !expect_route(0x0800B3D2u, 0u, b388_lower_x - 1, false, 0u) ||
        !expect_route(0x0800B3D2u, 0u, -32, false, 0u) ||
        !expect_route(0x0800B3D2u, 1u, b388_lower_x, false, 0u) ||
        !expect_route(0x0800B3E6u, 0u, b388_lower_y, true, 1u) ||
        !expect_route(0x0800B3E6u, 0u, b388_lower_y - 1, false, 0u) ||
        !expect_route(0x0800B3E6u, 0u, -32, false, 0u) ||
        !expect_route(0x0800B3E6u, 1u, b388_lower_y, false, 0u)) {
        std::puts("widescreen_policy_test: B388 lower band mismatch");
        return 1;
    }
    if (!expect_route(0x0800B3DCu, 0u, 273, true, 1u) ||
        !expect_route(0x0800B3DCu, 0u, 332, true, 1u) ||
        !expect_route(0x0800B3DCu, 0u, 272, false, 0u) ||
        !expect_route(0x0800B3DCu, 0u, 333, false, 0u) ||
        !expect_route(0x0800B3DCu, 1u, 300, false, 0u) ||
        !expect_route(0x0800B3ECu, 0u, 209, true, 1u) ||
        !expect_route(0x0800B3ECu, 0u, 248, true, 1u) ||
        !expect_route(0x0800B3ECu, 0u, 208, false, 0u) ||
        !expect_route(0x0800B3ECu, 0u, 249, false, 0u) ||
        !expect_route(0x0800B3ECu, 1u, 220, false, 0u)) {
        std::puts("widescreen_policy_test: B388 upper band mismatch");
        return 1;
    }
    constexpr std::uint32_t c6fa_old_max = 0x012FFFFEu;
    constexpr std::uint32_t c6fa_new_max = c6fa_old_max + (60u << 16);
    constexpr std::int32_t c6fa_lower = -static_cast<std::int32_t>(60u << 16);
    if (!expect_route(0x0800C6FAu, 0u, c6fa_lower, true, 1u) ||
        !expect_route(0x0800C6FAu, 0u, c6fa_lower - 1, false, 0u) ||
        !expect_route(0x0800C6FAu, 0u, -1, true, 1u) ||
        !expect_route(0x0800C6FAu, 0u, 0, false, 0u) ||
        !expect_route(0x0800C6FAu, 1u, c6fa_lower, false, 0u) ||
        !expect_route(0x0800C6FAu, 0u,
                      static_cast<std::int32_t>(c6fa_old_max + 1u),
                      true, 1u) ||
        !expect_route(0x0800C6FAu, 0u,
                      static_cast<std::int32_t>(c6fa_new_max), true, 1u) ||
        !expect_route(0x0800C6FAu, 0u,
                      static_cast<std::int32_t>(c6fa_old_max), false, 0u) ||
        !expect_route(0x0800C6FAu, 0u,
                      static_cast<std::int32_t>(c6fa_new_max + 1u),
                      false, 0u) ||
        !expect_route(0x0800C6FAu, 1u,
                      static_cast<std::int32_t>(c6fa_old_max + 1u),
                      false, 0u)) {
        std::puts("widescreen_policy_test: C6FA band mismatch");
        return 1;
    }
    constexpr std::int32_t c702_lower =
        static_cast<std::int32_t>(0xFFE00000u - (40u << 16));
    constexpr std::int32_t c708_upper = static_cast<std::int32_t>(248u << 16);
    if (!expect_route(0x0800C702u, 1u, c702_lower, false, 1u) ||
        !expect_route(0x0800C702u, 1u, c702_lower + 1, true, 0u) ||
        !expect_route(0x0800C702u, 1u,
                      static_cast<std::int32_t>(0xFFE00000u), true, 0u) ||
        !expect_route(0x0800C702u, 0u, c702_lower + 1, false, 0u) ||
        !expect_route(0x0800C708u, 1u, c708_upper, true, 0u) ||
        !expect_route(0x0800C708u, 1u, c708_upper + 1, false, 1u) ||
        !expect_route(0x0800C708u, 1u, static_cast<std::int32_t>(208u << 16),
                      true, 0u) ||
        !expect_route(0x0800C708u, 0u, c708_upper, false, 0u)) {
        std::puts("widescreen_policy_test: C62C band mismatch");
        return 1;
    }
    std::uint32_t unchanged = 0xA5A5A5A5u;
    if (golden_sun_expanded_viewport_branch_override(
            0x0800B3F0u, 1u, 360u, 240u, 60u, 60u, 40u, 40u, 0, &unchanged) ||
        unchanged != 0xA5A5A5A5u ||
        golden_sun_expanded_viewport_branch_override(
            0x0800B27Eu, 1u, 288u, 160u, 24u, 24u, 0u, 0u, 0, &unchanged) ||
        unchanged != 0xA5A5A5A5u ||
        golden_sun_expanded_viewport_branch_override(
            0x0800B27Eu, 1u, 360u, 240u, 60u, 60u, 40u, 40u, 160, nullptr)) {
        std::puts("widescreen_policy_test: branch fail-closed mismatch");
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
    if (!golden_sun_obj_provenance_dma_handoff(
            0x0300347Cu, 0x07000000u, 1024u, 0x8400u) ||
        golden_sun_obj_provenance_dma_handoff(
            0x0300347Cu, 0x07000000u, 1020u, 0x8400u) ||
        golden_sun_obj_provenance_dma_handoff(
            0x0300347Cu, 0x07000000u, 1024u, 0x8460u) ||
        golden_sun_obj_provenance_dma_handoff(
            0x0300347Eu, 0x07000000u, 1024u, 0x8400u) ||
        golden_sun_obj_provenance_dma_handoff(
            0x0300347Cu, 0x07000008u, 1024u, 0x8400u)) {
        std::puts("widescreen_policy_test: OBJ provenance DMA handoff mismatch");
        return 1;
    }
    std::uint32_t provenance_address = 0;
    if (!golden_sun_obj_y_provenance_address(
            0x0800B27Eu, 0x0300346Cu, &provenance_address) ||
        provenance_address != 0x0300347Cu ||
        !golden_sun_obj_y_provenance_address(
            0x0800B328u, 0x03003478u, &provenance_address) ||
        provenance_address != 0x0300347Cu ||
        golden_sun_obj_y_provenance_address(
            0x0800B27Cu, 0x0300346Cu, &provenance_address) ||
        golden_sun_obj_y_provenance_address(
            0x0800B27Eu, UINT32_MAX, &provenance_address)) {
        std::puts("widescreen_policy_test: OBJ-Y provenance address mismatch");
        return 1;
    }
    if (!golden_sun_obj_y_branch_writes_oam(
            0x0800B27Eu, 0u, false, 0u) ||
        !golden_sun_obj_y_branch_writes_oam(
            0x0800B27Eu, 1u, true, 0u) ||
        !golden_sun_obj_y_branch_writes_oam(
            0x0800B328u, 1u, true, 0u) ||
        golden_sun_obj_y_branch_writes_oam(
            0x0800B27Eu, 1u, true, 1u) ||
        golden_sun_obj_y_branch_writes_oam(
            0x0800B328u, 1u, false, 0u) ||
        golden_sun_obj_y_branch_writes_oam(
            0x0800B324u, 1u, true, 0u)) {
        std::puts("widescreen_policy_test: OBJ-Y fall-through mismatch");
        return 1;
    }
    if (!golden_sun_obj_x_provenance_address(
            0x0800B324u, 0x03003478u, &provenance_address) ||
        provenance_address != 0x0300347Cu ||
        golden_sun_obj_x_provenance_address(
            0x0800B328u, 0x03003478u, &provenance_address)) {
        std::puts("widescreen_policy_test: OBJ-X provenance address mismatch");
        return 1;
    }
    if (!golden_sun_palace_table_fingerprint_matches(
            kGoldenSunPalaceMapCrc, kGoldenSunPalaceRawCrc) ||
        golden_sun_palace_table_fingerprint_matches(
            0x7ac260c4u, 0x060ecd2cu) ||
        golden_sun_palace_table_fingerprint_matches(
            kGoldenSunPalaceMapCrc ^ 1u, kGoldenSunPalaceRawCrc) ||
        !golden_sun_palace_table_authorization_allowed(
            true, false,
            GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll,
            kGoldenSunPalaceMapCrc, kGoldenSunPalaceRawCrc) ||
        golden_sun_palace_table_authorization_allowed(
            false, false,
            GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll,
            kGoldenSunPalaceMapCrc, kGoldenSunPalaceRawCrc) ||
        golden_sun_palace_table_authorization_allowed(
            true, true,
            GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll,
            kGoldenSunPalaceMapCrc, kGoldenSunPalaceRawCrc) ||
        golden_sun_palace_table_authorization_allowed(
            true, false, GoldenSunWidePolicyReason::AuthorizedMode0,
            kGoldenSunPalaceMapCrc, kGoldenSunPalaceRawCrc)) {
        std::puts("widescreen_policy_test: Palace table authorization mismatch");
        return 1;
    }
    // An unprovenanced raw 191 is the wrapped representation of an off-top
    // logical Y (for example -65), so retain canonical GBA wrapping. A fresh
    // matching signed record still resolves its positive logical coordinate.
    constexpr int raw_off_top_y = 191;
    (void)raw_off_top_y;
    if (golden_sun_obj_y_resolution(false) != GoldenSunObjYResolution::Canonical ||
        golden_sun_obj_y_resolution(true) !=
            GoldenSunObjYResolution::SignedProvenance) {
        std::puts("widescreen_policy_test: OBJ-Y provenance resolution mismatch");
        return 1;
    }
    if (!golden_sun_obj_y_alias_candidate(191, 191) ||
        !golden_sun_obj_y_alias_candidate(160, 160) ||
        !golden_sun_obj_y_alias_candidate(95, -161) ||
        golden_sun_obj_y_alias_candidate(159, 191) ||
        golden_sun_obj_y_alias_candidate(191, -65) ||
        golden_sun_obj_y_alias_candidate(95, 95)) {
        std::puts("widescreen_policy_test: OBJ-Y alias predicate mismatch");
        return 1;
    }
    if (!golden_sun_obj_provenance_frame_fresh(10u, 10u) ||
        !golden_sun_obj_provenance_frame_fresh(10u, 11u) ||
        !golden_sun_obj_provenance_frame_fresh(10u, 12u) ||
        golden_sun_obj_provenance_frame_fresh(10u, 13u) ||
        golden_sun_obj_provenance_frame_fresh(11u, 10u) ||
        golden_sun_obj_provenance_frame_fresh(UINT64_MAX, UINT64_MAX) ||
        !golden_sun_obj_provenance_frame_fresh(UINT64_MAX - 2u, UINT64_MAX) ||
        golden_sun_obj_provenance_frame_fresh(UINT64_MAX - 1u, 0u) ||
        golden_sun_obj_provenance_frame_fresh(0u, UINT64_MAX)) {
        std::puts("widescreen_policy_test: OBJ-Y provenance lifetime mismatch");
        return 1;
    }
    if (!golden_sun_obj_provenance_attrs_match(
            true, 0x001Bu, 0x4050u, 0x0008u,
            0x001Bu, 0x4050u, 0x0008u) ||
        golden_sun_obj_provenance_attrs_match(
            true, 0x001Bu, 0x4050u, 0x0008u,
            0x001Cu, 0x4050u, 0x0008u) ||
        golden_sun_obj_provenance_attrs_match(
            true, 0x001Bu, 0x4050u, 0x0008u,
            0x001Bu, 0x4051u, 0x0008u) ||
        golden_sun_obj_provenance_attrs_match(
            true, 0x001Bu, 0x4050u, 0x0008u,
            0x001Bu, 0x4050u, 0x0009u) ||
        golden_sun_obj_provenance_attrs_match(
            false, 0x001Bu, 0x4050u, 0x0008u,
            0x001Bu, 0x4050u, 0x0008u)) {
        std::puts("widescreen_policy_test: OBJ identity mismatch");
        return 1;
    }

    // The measured slot-15 crossing is the only raw-159 repair: require the
    // exact 162/-94, 161/-95, 160/-96, 159/+159 sequence and stable identity.
    const auto edge_sample = [](int raw_y, int canonical_y,
                                std::uint64_t frame,
                                std::uint16_t attr0 = 0x21A2u,
                                std::uint16_t attr1 = 0x863Bu,
                                std::uint16_t attr2 = 0x09A4u) {
        return GoldenSunObjYEdgeAliasSample{
            true, true, false, 15, 0x030034F4u, 1u, frame, raw_y,
            canonical_y, attr0, attr1, attr2};
    };
    GoldenSunObjYEdgeAliasState edge_state;
    auto edge_step = golden_sun_obj_y_edge_alias_step(
        edge_state, edge_sample(162, -94, 519975u, 0x21A2u, 0x863Bu));
    edge_state = edge_step.state;
    if (edge_step.activated || edge_state.count != 1u) {
        std::puts("widescreen_policy_test: edge-alias first sample mismatch");
        return 1;
    }
    for (int i = 0; i < 8; ++i) {
        edge_step = golden_sun_obj_y_edge_alias_step(
            edge_state, edge_sample(162, -94, 519975u, 0x21A2u, 0x863Bu));
        edge_state = edge_step.state;
        if (edge_step.activated || !edge_state.valid || edge_state.count != 1u ||
            edge_state.last_raw_y != 162 ||
            edge_state.last_canonical_y != -94) {
            std::puts("widescreen_policy_test: edge-alias same-frame repeat reset");
            return 1;
        }
    }
    edge_step = golden_sun_obj_y_edge_alias_step(
        edge_state, edge_sample(161, -95, 519976u, 0x21A2u, 0x863Bu));
    edge_state = edge_step.state;
    for (int i = 0; i < 8; ++i)
        edge_state = golden_sun_obj_y_edge_alias_step(
            edge_state, edge_sample(161, -95, 519976u, 0x21A2u, 0x863Bu)).state;
    edge_step = golden_sun_obj_y_edge_alias_step(
        edge_state, edge_sample(160, -96, 519977u, 0x21A2u, 0x863Bu));
    edge_state = edge_step.state;
    for (int i = 0; i < 8; ++i)
        edge_state = golden_sun_obj_y_edge_alias_step(
            edge_state, edge_sample(160, -96, 519977u, 0x21A2u, 0x863Bu)).state;
    edge_step = golden_sun_obj_y_edge_alias_step(
        edge_state, edge_sample(159, 159, 519978u, 0x21A2u, 0x863Bu));
    if (!edge_step.activated || edge_step.state.valid) {
        std::puts("widescreen_policy_test: edge-alias crossing mismatch");
        return 1;
    }
    int edge_activations = 1;
    edge_state = edge_step.state;
    for (int i = 0; i < 8; ++i) {
        edge_step = golden_sun_obj_y_edge_alias_step(
            edge_state, edge_sample(159, 159, 519978u, 0x21A2u, 0x863Bu));
        edge_state = edge_step.state;
        edge_activations += edge_step.activated ? 1 : 0;
    }
    if (edge_activations != 1) {
        std::puts("widescreen_policy_test: edge-alias latch persisted");
        return 1;
    }
    const auto same_frame_mutation_resets = [&](auto changed) {
        GoldenSunObjYEdgeAliasState state;
        state = golden_sun_obj_y_edge_alias_step(
            state, edge_sample(162, -94, 519990u)).state;
        const auto rejected = golden_sun_obj_y_edge_alias_step(state, changed);
        if (rejected.activated || rejected.state.valid)
            return false;
        state = rejected.state;
        state = golden_sun_obj_y_edge_alias_step(
            state, edge_sample(161, -95, 519991u)).state;
        state = golden_sun_obj_y_edge_alias_step(
            state, edge_sample(160, -96, 519992u)).state;
        return !golden_sun_obj_y_edge_alias_step(
                    state, edge_sample(159, 159, 519993u)).activated;
    };
    auto same_frame_raw = edge_sample(161, -95, 519990u);
    auto same_frame_slot = edge_sample(162, -94, 519990u);
    same_frame_slot.slot = 14;
    auto same_frame_target = edge_sample(162, -94, 519990u);
    same_frame_target.target_address = 0x0300347Cu;
    auto same_frame_epoch = edge_sample(162, -94, 519990u);
    same_frame_epoch.auth_epoch = 2u;
    auto same_frame_attr0 = edge_sample(162, -94, 519990u, 0x61A2u);
    auto same_frame_attr1 = edge_sample(162, -94, 519990u, 0x21A2u, 0x863Cu);
    auto same_frame_attr2 = edge_sample(162, -94, 519990u, 0x21A2u, 0x863Bu,
                                        0x09A5u);
    auto same_frame_inactive = edge_sample(162, -94, 519990u);
    same_frame_inactive.sprite_active = false;
    auto same_frame_provenance = edge_sample(162, -94, 519990u);
    same_frame_provenance.exact_provenance = true;
    if (!same_frame_mutation_resets(same_frame_raw) ||
        !same_frame_mutation_resets(same_frame_slot) ||
        !same_frame_mutation_resets(same_frame_target) ||
        !same_frame_mutation_resets(same_frame_epoch) ||
        !same_frame_mutation_resets(same_frame_attr0) ||
        !same_frame_mutation_resets(same_frame_attr1) ||
        !same_frame_mutation_resets(same_frame_attr2) ||
        !same_frame_mutation_resets(same_frame_inactive) ||
        !same_frame_mutation_resets(same_frame_provenance)) {
        std::puts("widescreen_policy_test: edge-alias same-frame mutation accepted");
        return 1;
    }
    const auto edge_activates = [&](const auto& first, const auto& second,
                                    const auto& third, const auto& fourth) {
        GoldenSunObjYEdgeAliasState state;
        state = golden_sun_obj_y_edge_alias_step(state, first).state;
        state = golden_sun_obj_y_edge_alias_step(state, second).state;
        state = golden_sun_obj_y_edge_alias_step(state, third).state;
        return golden_sun_obj_y_edge_alias_step(state, fourth).activated;
    };
    auto changed_slot = edge_sample(161, -95, 11u);
    changed_slot.slot = 0;
    changed_slot.target_address = 0x0300347Cu;
    changed_slot.attr1 = 0x807Bu;  // prior slot-0 X=259 occupant
    changed_slot.attr2 = 0x0924u;  // prior slot-0 tile occupant
    auto changed_target = edge_sample(161, -95, 11u);
    changed_target.target_address = 0x0300347Cu;
    auto changed_epoch = edge_sample(161, -95, 11u);
    changed_epoch.auth_epoch = 2u;
    auto changed_attr0 = edge_sample(161, -95, 11u, 0x61A2u);
    auto changed_attr1 = edge_sample(161, -95, 11u, 0x21A2u, 0x843Cu);
    auto changed_attr2 = edge_sample(161, -95, 11u, 0x21A2u, 0x843Bu,
                                     0x09A5u);
    auto wrong_order = edge_sample(160, -96, 11u);
    auto frame_gap = edge_sample(161, -95, 13u);
    auto exact_provenance = edge_sample(161, -95, 11u);
    exact_provenance.exact_provenance = true;
    const bool n1 = edge_activates(edge_sample(162, -94, 10u), wrong_order,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n2 = edge_activates(edge_sample(162, -94, 10u), changed_slot,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n3 = edge_activates(edge_sample(162, -94, 10u), changed_target,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n4 = edge_activates(edge_sample(162, -94, 10u), changed_epoch,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n5 = edge_activates(edge_sample(162, -94, 10u), changed_attr0,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n6 = edge_activates(edge_sample(162, -94, 10u), changed_attr1,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n7 = edge_activates(edge_sample(162, -94, 10u), changed_attr2,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n8 = edge_activates(edge_sample(162, -94, 10u), frame_gap,
                       edge_sample(160, -96, 14u),
                       edge_sample(159, 159, 15u));
    const bool n9 = edge_activates(edge_sample(162, -94, 10u), exact_provenance,
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    const bool n10 = edge_activates(edge_sample(159, 159, 10u),
                       edge_sample(161, -95, 11u),
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u));
    if (n1 || n2 || n3 || n4 || n5 || n6 || n7 || n8 || n9 || n10) {
        std::puts("widescreen_policy_test: edge-alias fail-closed mismatch");
        return 1;
    }
    auto disabled = edge_sample(162, -94, 10u);
    disabled.sprite_active = false;
    auto edge_native = edge_sample(162, -94, 10u);
    edge_native.expanded_active = false;
    if (edge_activates(disabled, edge_sample(161, -95, 11u),
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u)) ||
        edge_activates(edge_native, edge_sample(161, -95, 11u),
                       edge_sample(160, -96, 12u),
                       edge_sample(159, 159, 13u)) ||
        golden_sun_obj_y_edge_alias_step(
            GoldenSunObjYEdgeAliasState{}, edge_sample(159, 159, 10u))
            .activated) {
        std::puts("widescreen_policy_test: edge-alias mode/activity mismatch");
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
    GoldenSunFieldTilemapMetadata palace_fill{};
    palace_fill.map_id = kGoldenSunPalaceNoMapId;
    palace_fill.has_raw_entry = true;
    if (!golden_sun_palace_out_of_room(palace_fill)) {
        std::puts("widescreen_policy_test: Palace fill not out-of-room");
        return 1;
    }
    // A cell can be inside the finite 128x128 table yet still be Palace fill;
    // the exact table fingerprint must not authorize that residual cell.
    store32(split_ewram, split_map_offset(6, 73),
            static_cast<std::uint32_t>(kGoldenSunPalaceNoMapId));
    if (golden_sun_field_tilemap_entry(
            palace_dispcnt, io.data(), io.size(), split_ewram.data(),
            split_ewram.size(), 1, -24, 0, &split_entry, nullptr, nullptr,
            false, true)) {
        std::puts("widescreen_policy_test: Palace residual cell accepted");
        return 1;
    }

    // The Palace active-region mask must keep each layer's seeded component,
    // stop at 0x026 barriers, and not make another layer's component reachable.
    const auto set_region_map = [&](std::uint32_t map_x,
                                    std::uint32_t map_y,
                                    std::uint16_t id) {
        store32(split_ewram, split_map_offset(map_x * 2u, map_y * 2u), id);
    };
    for (std::uint32_t map_y = 0; map_y < 128u; ++map_y) {
        for (std::uint32_t map_x = 0; map_x < 128u; ++map_x)
            set_region_map(map_x, map_y, kGoldenSunPalaceNoMapId);
    }
    for (std::uint32_t tile_y = 0; tile_y < 2u; ++tile_y) {
        for (std::uint32_t tile_x = 0; tile_x < 2u; ++tile_x)
            store16(split_ewram, split_raw_offset(7, tile_x, tile_y),
                    0x1234u);
    }
    // BG1 seeds at (4,36), BG2 at (36,12), BG3 at (4,17).
    set_region_map(4, 36, 7);
    for (std::uint32_t map_x = 5; map_x <= 24; ++map_x)
        set_region_map(map_x, 36, 7);
    for (std::uint32_t map_y = 37; map_y <= 50; ++map_y)
        set_region_map(4, map_y, 7);
    set_region_map(36, 12, 7);
    set_region_map(37, 12, 7); // connected BG2 cell
    set_region_map(4, 17, 7);
    set_region_map(5, 17, 7);  // connected BG3 cell
    set_region_map(20, 20, 7); // disconnected island, never a seed
    GoldenSunPalaceActiveRegion palace_region;
    if (!palace_region.build(palace_dispcnt, io.data(), io.size(),
                             split_ewram.data(), split_ewram.size(), true) ||
        !palace_region.layer_built(1) || !palace_region.layer_built(2) ||
        !palace_region.layer_built(3) ||
        !palace_region.reachable(1, 4, 36) ||
        !palace_region.reachable(1, 5, 36) ||
        !palace_region.reachable(1, 23, 36) ||
        !palace_region.reachable(1, 4, 49) ||
        palace_region.reachable(1, 4, 50) ||
        palace_region.reachable(1, 24, 36) ||
        palace_region.reachable(1, 20, 20) ||
        palace_region.reachable(1, 36, 12) ||
        palace_region.reachable(2, 4, 36) ||
        !palace_region.reachable(2, 36, 12) ||
        !palace_region.reachable(3, 4, 17) ||
        !palace_region.seeded(1, 4, 36) ||
        palace_region.seeded(1, 20, 36) ||
        !palace_region.seeded(2, 36, 12) ||
        !palace_region.seeded(3, 4, 17) ||
        palace_region.seeded(1, 20, 20)) {
        std::puts("widescreen_policy_test: Palace active-region mismatch");
        return 1;
    }
    palace_region.reset();
    if (palace_region.reachable(1, 4, 36) ||
        palace_region.build(palace_dispcnt, io.data(), io.size(),
                            split_ewram.data(), split_ewram.size(), false) ||
        palace_region.reachable(1, 4, 36)) {
        std::puts("widescreen_policy_test: Palace active-region reset mismatch");
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
    // Equal-scroll provider lookups use restored table contents directly;
    // they must not require post-restore authored bits. The explicit helper
    // gate above remains available to callers that opt into ownership.
    if (!golden_sun_field_tilemap_entry(
            town_dispcnt, io.data(), io.size(), ewram.data(), ewram.size(),
            1, -24, 0, &entry, nullptr, nullptr) || entry != 0x03ffu) {
        std::puts("widescreen_policy_test: restored equal-scroll lookup rejected");
        return 1;
    }
    // Logical atlas coordinates are finite; a signed margin must not wrap a
    // negative tile coordinate into cell 127. Synthetic margin rendering also
    // cannot establish ownership by itself.
    auto zero_scroll_io = io;
    for (std::size_t offset : {0x14u, 0x16u, 0x18u, 0x1Au, 0x1Cu, 0x1Eu})
        store16(zero_scroll_io, offset, 0u);
    GoldenSunFieldAuthoredMap margin_authored;
    margin_authored.mark_write(0x02010000u, 4u);
    if (golden_sun_field_tilemap_entry(
            town_dispcnt, zero_scroll_io.data(), zero_scroll_io.size(),
            ewram.data(), ewram.size(), 1, -24, 0, &entry, nullptr,
            &margin_authored) || margin_authored.authored_count() != 1u) {
        std::puts("widescreen_policy_test: negative atlas coordinate wrapped");
        return 1;
    }
    margin_authored.reset();
    if (margin_authored.authored_count() != 0u ||
        golden_sun_field_tilemap_entry(
            town_dispcnt, zero_scroll_io.data(), zero_scroll_io.size(),
            ewram.data(), ewram.size(), 1, -24, 0, &entry, nullptr,
            &margin_authored)) {
        std::puts("widescreen_policy_test: stale authored margin survived reset");
        return 1;
    }

    // WIDE-01 experimental off-screen cull safety net (Experimental Fixes
    // toggle). See docs/issues/WIDE-01_NPC_IDENTITY.md, "The off-screen
    // rule", for the evidence this is built from.

    // resign: the one proven X exception (exactly raw=256 stays positive,
    // not -256) must hold, alongside plain two's-complement elsewhere.
    if (golden_sun_experimental_resign_oam_x(256u) != 256) {
        std::puts("widescreen_policy_test: X=256 resign exception broke");
        return 1;
    }
    if (golden_sun_experimental_resign_oam_x(0u) != 0 ||
        golden_sun_experimental_resign_oam_x(255u) != 255 ||
        golden_sun_experimental_resign_oam_x(257u) != 257 - 512 ||
        golden_sun_experimental_resign_oam_x(511u) != 511 - 512) {
        std::puts("widescreen_policy_test: X resign wrap broke");
        return 1;
    }
    if (golden_sun_experimental_resign_oam_y(0u) != 0 ||
        golden_sun_experimental_resign_oam_y(127u) != 127 ||
        golden_sun_experimental_resign_oam_y(128u) != 128 - 256 ||
        golden_sun_experimental_resign_oam_y(255u) != 255 - 256) {
        std::puts("widescreen_policy_test: Y resign wrap broke");
        return 1;
    }

    // Ambiguous hardware coordinates must stay visible unless the exact
    // pre-truncation placement is available. In particular, raw Y=160 may be
    // a real positive bottom-margin sprite, not only wrapped Y=-96.
    {
        int resolved = 0;
        if (!golden_sun_experimental_resolve_oam_y(160u, true, 160,
                                                   &resolved) ||
            resolved != 160 ||
            golden_sun_experimental_resolve_oam_y(160u, false, 0,
                                                  &resolved)) {
            std::puts("widescreen_policy_test: ambiguous Y did not fail closed");
            return 1;
        }
        if (!golden_sun_experimental_resolve_oam_x(256u, true, 256,
                                                   &resolved) ||
            resolved != 256 ||
            golden_sun_experimental_resolve_oam_x(256u, false, 0,
                                                  &resolved)) {
            std::puts("widescreen_policy_test: ambiguous X did not fail closed");
            return 1;
        }
    }

    // Half-extent table: every combination WIDE-01_NPC_IDENTITY.md actually
    // observed must decode to its real GBA pixel size.
    {
        struct HalfExtentCase {
            unsigned shape, size;
            int expect_half_w, expect_half_h;
        };
        const HalfExtentCase cases[] = {
            {0u, 1u, 8, 8},    // 16x16
            {0u, 2u, 16, 16},  // 32x32
            {1u, 0u, 8, 4},    // 16x8 (shadow signature)
            {2u, 3u, 16, 32},  // 32x64
        };
        for (const auto& c : cases) {
            int half_w = -1, half_h = -1;
            golden_sun_obj_half_extent(c.shape, c.size, &half_w, &half_h);
            if (half_w != c.expect_half_w || half_h != c.expect_half_h) {
                std::puts("widescreen_policy_test: half-extent table wrong");
                return 1;
            }
        }
    }

    // OAM coordinates are top-left, so the full sprite must clear an edge
    // before it is hidden. A 32x32 sprite at x=283 overlaps the right edge;
    // x=300 is the first fully-clear position.
    {
        constexpr std::uint32_t kLeft = 60u, kRight = 60u, kTop = 40u,
                                kBottom = 40u;
        if (golden_sun_experimental_sprite_fully_offscreen_top_left(
                283, 0, 32, 32, kLeft, kRight, kTop, kBottom) ||
            !golden_sun_experimental_sprite_fully_offscreen_top_left(
                300, 0, 32, 32, kLeft, kRight, kTop, kBottom) ||
            golden_sun_experimental_sprite_fully_offscreen_top_left(
                0, 167, 32, 64, kLeft, kRight, kTop, kBottom)) {
            std::puts("widescreen_policy_test: top-left bounds are wrong");
            return 1;
        }
        int width = 0, height = 0;
        if (!golden_sun_obj_dimensions(2u, 3u, &width, &height) ||
            width != 32 || height != 64 ||
            golden_sun_obj_dimensions(3u, 0u, &width, &height)) {
            std::puts("widescreen_policy_test: sprite dimensions unsafe");
            return 1;
        }
    }
    return 0;
}
