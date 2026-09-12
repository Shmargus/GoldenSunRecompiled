// Golden Sun battle presentation for the expanded view.
//
// A battle arena is flat scenery the hardware draws in a band across the
// middle of the screen, with the menus filling the strips above and below it.
// Left as-is, the expanded view shows a battle in a 240x160 island with black
// all round it. This magnifies the arena -- only the arena -- so the band the
// game draws it in covers the whole canvas, and moves the menu panels out to
// the canvas edges. The party, the monsters and every effect keep their
// authentic size and place, and so do the menus.
//
// Measured from battle captures on 2026-09-11 (logs/maprec_20260904_211752,
// _214216, logs/maprec_20260905_084802, _093001, _153555, _161243):
//   - Battle is Mode 1. The arena is drawn on two layers, not one: regular
//     BG1 while the game's camera sits back, affine BG2 while it pushes in
//     (BG2PA animates 256 = 1x to 128 = 2x), and a turn playing out runs with
//     BG1 switched off entirely (snap_00318_periodic, DISPCNT=0x1541). Both
//     are magnified, but NOT by the same factor -- see affine_zoom below.
//   - Neither layer can be extended instead of magnified. BG1's vertical
//     scroll is 32 in all 611 battle frames examined and its art is 144 rows;
//     lifting BG2's clip tiles its art and exposes the pattern stored below
//     it. There is no more arena to reveal, which is why this magnifies.
//     Measured again on 2026-09-12 (logs/battle_layers.csv): with nothing
//     magnified, canvas rows 176..200 hold the party over bare backdrop and
//     201..215 are empty -- the void the user reported.
//   - WIN0V, the band the game draws the arena in, reads 16..136 in most
//     battle frames and 0..136 in the rest: 120 rows, bottom edge stable at
//     136. WIN0H is 0..240 in all of them.
//   - BG1 below the band carries the command icons (72 opaque pixels a row,
//     the three icons, nothing else), which is UI, not arena.
#pragma once

#include <cstdint>

namespace gsr::battle {

// The arena is magnified so that the band the game draws it in covers the
// whole expanded canvas. Chosen by the user on 2026-09-12 from three
// alternatives, after seeing the unmagnified arena leave the party standing in
// a black void: below the arena's bottom edge the game has no scenery at all
// (it is the strip its own command menu covers), so the only way to fill the
// canvas is to magnify what there is.
//
// The factor is fixed by the canvas, not chosen: the band is kBandRows tall
// and the canvas is kBandRows*2, so the arena is magnified 2x and the whole
// band lands on the whole canvas. Sideways that covers the 360 columns with
// room to spare.
//
// The mapping is anchored to the BOTTOM of the band, not its middle. That is
// what keeps the party standing on the ground: the last row of arena art is
// the ground under their feet, so it is pinned to the bottom of the canvas and
// everything above it grows upward.
//
// The affine layer takes the remainder, not the full factor. The game itself
// magnifies that layer while its camera pushes in (BG2PA 256 = 1x to 128 =
// 2x), and the two compound -- applying the full 2x on top of the game's own
// put the arena on screen at 4x. Our factor there is 2*PA/256, clamped to
// [1, 2]: 2x while the camera sits back, 1x once the game has pushed all the
// way in. The arena therefore holds ONE apparent size for the whole battle,
// which is what the user asked for on 2026-09-11 ("make it fit our new
// resolution instead of this camera zoom and make it static"), and the canvas
// is exactly filled either way, because the art's own height in the layer is
// what both factors are derived from.

// The rows the game draws the arena in, measured from WIN0V above. The bottom
// edge is read live; the height is fixed here because the register's top edge
// is not stable enough to scale from -- a change between 16 and 0 would shift
// the picture in the middle of a battle.
inline constexpr int kBandRows = 120;

// The regular layer carrying the arena while the camera sits back.
inline constexpr unsigned kBackdropLayer = 1u;

// The affine layer carrying it while the camera pushes in.
inline constexpr unsigned kArenaAffineLayer = 2u;

// The menu layer: HP windows, command windows, battle messages.
inline constexpr unsigned kMenuLayer = 0u;

// Mode 1 with at least one arena layer on. FACTS.md: "Battle is Mode 1 with a
// 128x128 affine BG2".
inline bool is_battle_frame(std::uint16_t dispcnt) {
    const unsigned arena_layers = (0x0100u << kBackdropLayer) |
                                  (0x0100u << kArenaAffineLayer);
    return (dispcnt & 0x07u) == 1u && (dispcnt & arena_layers) != 0u;
}

// The battle band's bottom row, as the game itself declares it in WIN0V.
// Refuse anything outside the lower half of the native screen: a cleared or
// half-written register must leave the arena alone rather than drag the
// sampling off it.
inline bool band_bottom(std::uint16_t win0v, int native_height, int* out_row) {
    const int y2 = static_cast<int>(win0v & 0x00FFu);
    if (y2 < native_height / 2 || y2 > native_height) return false;
    if (out_row) *out_row = y2;
    return true;
}

// The middle of that band: what the middle of the canvas shows.
inline int band_centre(int band_bottom_row) {
    return band_bottom_row - kBandRows / 2;
}

// Below the band the arena layer stops being scenery and carries the command
// icons instead. Those are UI: they keep their own size and travel with the
// menu strip they belong to (see menu_sample). The exception only applies
// while the game is actually showing them -- with WIN0/WIN1 enabled it hides
// the strip itself, and drawing it then would punch icon-shaped holes in the
// arena.
inline bool icon_strip_row(int out_x, int out_y, int native_width,
                           int native_height, int band_bottom_row,
                           bool windows_enabled) {
    return !windows_enabled && out_x >= 0 && out_x < native_width &&
           out_y >= band_bottom_row && out_y < native_height;
}

// Our magnification, as num/den output pixels per source pixel.
struct Zoom {
    int num = 1;
    int den = 1;
    bool identity() const { return num == den; }
};

// The band is half the canvas, so the arena that fills the canvas is 2x. Used
// as-is on the flat layer, which the game never scales.
inline constexpr Zoom kArenaZoom{2, 1};

// BG2PA is 8.8 fixed point: 256 is 1x, 128 is the 2x the camera pushes in to.
inline constexpr int kAffineUnitScale = 256;

// What the affine arena still needs from us, given what the game is already
// doing to it: kArenaZoom / (256 / PA) = PA / 128, clamped to [1, 2]. At PA
// 256 that is the full 2x; at PA 128 the game has already done all of it and
// we only re-anchor the layer.
inline Zoom affine_zoom(int pa) {
    const int den = kAffineUnitScale / kArenaZoom.num;   // 128
    if (pa <= den) return Zoom{};                        // game did it all
    if (pa >= kAffineUnitScale) return kArenaZoom;       // camera fully back
    return Zoom{pa, den};
}

// Floor division: the horizontal mapping is anchored mid-screen, so its
// operand is negative on one side and C truncation would mirror one pixel
// about the anchor.
inline int floor_div(int value, int divisor) {
    const int q = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? q - 1 : q;
}

// out_x/out_y are native screen coordinates: 0..239 / 0..159 inside the
// authentic window, negative or beyond it in the margins. Writes the hardware
// pixel the magnified arena should be read from.
//
// Vertically the canvas hangs from the band's bottom edge, so the ground the
// party stands on is the bottom row of the canvas and the arena grows upward
// from there. Horizontally it is anchored mid-screen. Columns are left
// unclamped on purpose: a sample beyond the authentic 240 is wrapped by the
// layer's own map, which is how the arena reaches both canvas edges.
//
// Returns false only for an output row outside the canvas, which the caller
// then leaves black.
inline bool arena_sample(int out_x, int out_y, int native_width,
                         int native_height, int extra_bottom,
                         int band_bottom_row, Zoom zoom,
                         int* out_hw_x, int* out_hw_y) {
    const int canvas_last_row = native_height + extra_bottom - 1;
    if (out_y > canvas_last_row) return false;
    const int centre_x = native_width / 2;
    const int hw_x = centre_x +
        floor_div((out_x - centre_x) * zoom.den, zoom.num);
    const int hw_y = band_bottom_row - 1 -
        floor_div((canvas_last_row - out_y) * zoom.den, zoom.num);
    if (out_hw_x) *out_hw_x = hw_x;
    if (out_hw_y) *out_hw_y = hw_y;
    return true;
}

// The menus keep their size and move out to the canvas edges. Which rows move
// is decided by the panels the game actually drew, NOT by a fixed split of the
// screen: splitting at a row tore the Psynergy list in half in play on
// 2026-09-12, because that window straddles the split and its lower half
// jumped to the bottom of the canvas on its own.
//
// A panel is a run of rows the menu layer has drawn something on. One that
// sits entirely inside the top strip travels up into the top margin, one
// entirely inside the bottom strip travels down into the bottom margin, and
// anything taller -- a list, a description box, an enemy name over the arena
// -- stays exactly where the game put it. A strip is as deep as the margin it
// moves into, so a panel that moves is never cut.
inline int menu_block_shift(int block_first_row, int block_last_row,
                            int native_height, int extra_top,
                            int extra_bottom) {
    if (extra_top > 0 && block_first_row >= 0 && block_last_row < extra_top) {
        return -extra_top;
    }
    if (extra_bottom > 0 && block_first_row >= native_height - extra_bottom &&
        block_last_row < native_height) {
        return extra_bottom;
    }
    return 0;
}

}  // namespace gsr::battle
