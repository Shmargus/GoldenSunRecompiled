// object_probe.h — the object-buffer equivalent of room_buffer.h's search: a
// broad memory search for where Golden Sun keeps each character's own real
// (world) position, so the object buffer can stop reading OAM's ambiguous
// 8-bit Y byte entirely (ROADMAP.md, "Sprite placement"; FACTS.md, sprite
// entries 2026-09-06 and 2026-09-11).
//
// Nothing here changes what appears on screen. Every frame that produces at
// least one authenticated sprite position (via runner_main.cpp's
// gsr_golden_sun_obj_authenticated_positions accessor), this computes each
// sprite's WORLD position (authenticated screen position + camera, using the
// exact camera decode room_buffer.cpp uses) and scans EWRAM + IWRAM for
// 16-bit-aligned addresses whose live value keeps landing on one of those
// world coordinates. An address that tracks a walking character's position
// must also CHANGE over the session -- a constant cannot be that -- so a
// per-address changed flag is tracked and the report excludes anything that
// never changed. This is the same method that originally found the map
// tables (room_buffer.h): let the game's own authenticated values pick the
// address out of memory, rather than guessing an offset inside one actor
// record.
#pragma once

#include <cstdint>

namespace gsr {

// Caches the GSR_OBJECT_PROBE env flag and registers the exit report. Call
// once at startup, alongside room_buffer_init() (see runner_main.cpp) --
// independent of it, no shared state.
void object_probe_init();

// Hot path: called for every guest function entry, gated to once per guest
// frame internally, exactly like room_buffer_on_entry. Does nothing unless
// the probe is enabled and the frame produced at least one authenticated
// sprite position.
void object_probe_on_entry(std::uint32_t entry_pc);

}  // namespace gsr
