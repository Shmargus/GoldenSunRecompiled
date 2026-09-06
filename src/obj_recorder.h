// obj_recorder.h — GSR_OBJ_RECORD sprite-placement census.
//
// The question this exists to answer, and nothing else: WHICH committed
// sprites can be given a full-precision position, and which cannot?
//
// Background. The PPU currently asks OAM where each sprite is. OAM stores X
// in 9 bits and Y in 8. A 360x240 view spans rows -104..199 -- 304 rows --
// and a byte names 256, so rows below -56 share a byte with rows in the
// bottom margin (measured 2026-09-06: slot 23 sits at logical y=-101 and
// writes byte 155, which also means row 155). No arithmetic on the byte can
// separate them, at any view size. The escape is to stop reading the byte:
// the guest computes each sprite's position at full precision before
// truncating it into OAM, that value is already captured
// (record_golden_sun_obj_staging in runner_main.cpp), and OAM can be demoted
// to an identity tag.
//
// That capture exists but its coverage has never been measured -- it is
// handed off on writer route D4, which fires only on "a successful, gated
// commit", while route F0 is the one every committed sprite passes through,
// body and shadow alike. Before rewiring the renderer to trust the table,
// measure how much of the table is actually there.
//
// Central design constraint, same as map_recorder.h: this MEASURES, it does
// not fix. It never changes a guest write, a coordinate, or a rendered
// pixel. The experimental off-screen culler shares the same lookup
// (golden_sun_obj_resolve_placement) but is a separate toggle; with only
// this recorder on, the run is byte-for-byte an ordinary run plus file I/O.
//
// Golden Sun-specific tooling: lives entirely in src/, matches
// map_recorder.h's shape and toggle style, and is analysed offline by
// tools/decode_obj.py.
#pragma once

#include <cstdint>

#include "widescreen_policy.h"

namespace gsr {

// One committed OAM-shadow write, after the placement lookup has run.
//
// Every field is observed, never derived: `outcome` and `resolved_*` come
// from golden_sun_obj_resolve_placement, the rest are the bytes and call
// context at the F0 commit seam. `record_base`/`shadow` are meaningful only
// when the sprite was traced to a guest actor record; `resolved_*` only when
// golden_sun_obj_placement_is_exact(outcome).
struct ObjPlacementSample {
    std::uint64_t frame = 0;
    int slot = -1;
    std::uint32_t slot_address = 0;
    // Which of the game's sprite tables this commit went into. The game
    // builds its list in two places and uploads whichever is current, so a
    // per-table count is how we confirm both are actually being observed.
    std::uint32_t table_base = 0;
    // Call context at the commit, so an unsourced sprite can be attributed
    // to the guest code path that emitted it. This is the actionable half of
    // the census: it names the next doorway to hook.
    std::uint32_t writer_pc = 0;
    std::uint32_t return_pc = 0;
    std::uint32_t depth = 0;
    // Guest actor record the sprite was traced to, when it was traced at all
    // (body at record+0x00, shadow at record+0x0C).
    std::uint32_t staging = 0;
    std::uint32_t record_base = 0;
    bool shadow = false;
    bool record_identified = false;
    // Rotation/scaling sprite. Its position resolves like any other's; only
    // its bounds need the transform. Counted separately because Golden Sun
    // uses affine sprites heavily in the field, so "how well are affine
    // sprites covered?" is its own question.
    bool affine = false;
    // The committed OAM bytes -- the truncated position we are trying to
    // stop depending on.
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::uint32_t raw_x = 0;
    std::uint32_t raw_y = 0;
    // How far a shadow sits from the body it was placed from. Recorded so
    // the paired recovery's assumption -- that a pair is closer together
    // than half the truncated field, 128 rows or 256 columns -- is measured
    // rather than assumed. Zero for a body.
    int paired_offset_x = 0;
    int paired_offset_y = 0;
    // Full-precision position, valid only for the two exact outcomes.
    int resolved_x = 0;
    int resolved_y = 0;
    int width = 0;
    int height = 0;
    widescreen::GoldenSunObjPlacementOutcome outcome =
        widescreen::GoldenSunObjPlacementOutcome::SourceUnavailable;
};

// Caches the GSR_OBJ_RECORD env flag and, only when it is set, creates the
// session directory and registers the exit summary. Call once at startup
// next to map_recorder_init(). Safe to call regardless of the flag.
void obj_recorder_init();

// Hot-path gate. runner_main.cpp's F0 observer runs its placement lookup
// when this OR the experimental-fixes toggle is on, so the recorder can
// measure without the culler being armed.
bool obj_recorder_enabled();

// Hot path: one committed sprite, already resolved. Tallies it, rolls the
// per-frame row over when the frame changes, and files unsourced sprites by
// call site. No-ops in one branch when the recorder is off.
void obj_recorder_note_placement(const ObjPlacementSample& sample);

}  // namespace gsr
