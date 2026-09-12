// object_buffer.h — milestone "Sprite placement", the object-array
// equivalent of room_buffer.h: build our own list of who is in the room from
// the game's own object position array, and measure how well it explains
// what the game actually drew.
//
// Nothing here changes what appears on screen. It reads the game's object
// position array every frame, decides which entries describe someone
// standing in the current room, and compares those entries against this
// frame's authenticated sprite positions (the same accessor object_probe.cpp
// uses). A dominant, consistent offset between an entry and the sprite it
// explains is the evidence that the array can replace OAM's ambiguous Y byte
// as the object buffer's source of truth (ROADMAP.md, "Sprite placement").
//
// Every address and rule below is measured, with the evidence in FACTS.md:
//
//   object array  0x02030DD0  stride 0x30, X and Y as 16.16 fixed point at
//                 +0x00 and +0x04 (pixel value is the high half, +0x02/+0x06)
//                 ("The game's object position array is at 0x02030DD0",
//                 2026-09-11)
//   camera        0x02030DB0  same shape, sits 0x20 before the array
//   room rect     0x02030DC0  four u16: min_x, max_x, min_y, max_y, in pixels
//
// NOT measured yet, and this module exists to produce both: the array's
// LENGTH (the highest index ever seen occupied) and whether one entry-to-
// sprite offset explains most of what is drawn.
#pragma once

#include <cstdint>

namespace gsr {

// Caches the GSR_OBJECT_BUFFER env flag. Call once at startup, alongside
// room_buffer_init() and object_probe_init() -- independent of both, no
// shared state.
void object_buffer_init();

// Hot path: called for every guest function entry, gated to once per guest
// frame internally, exactly like room_buffer_on_entry and
// object_probe_on_entry. Does nothing unless the check is enabled.
void object_buffer_on_entry(std::uint32_t entry_pc);

}  // namespace gsr
