// runtime_bus_bridge.h — public surface for binding the active bus
// to the recompiled-code runtime.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace gba { class GbaBus; }
namespace gba { class GbaPpu; }

// Count of PPU VBlank-start events (scanline 159->160), incremented in
// runtime_tick. The debug step-one-frame primitive stops on its increment
// so the recomp's TCP `step` parks at VBlank-start, matching the
// interpreter and mGBA oracles. Defined in runtime_bus_bridge.cpp.
extern "C" unsigned long long g_runtime_vblank_starts;

// Current guest frame index (the PPU's frame_count()). Unlike
// g_runtime_vblank_starts, this is the same frame key used by the runtime's
// frame-phase CSV and remains correct after a savestate load.
extern "C" unsigned long long runtime_current_frame();

namespace gbarecomp {

// Install the active bus pointer. Subsequent bus_read_u*/bus_write_u*
// calls from generated code (declared in src/armv4t/runtime_arm.h)
// will delegate to this bus.
void set_active_bus(gba::GbaBus* bus);
void set_active_ppu(gba::GbaPpu* ppu);

// Present-time snapshot hook. Called at VBlank start after the faithful
// scanline framebuffer is latched and before VBlank DMA/IRQ work mutates video
// state. Empty disables it.
void set_frame_snapshot_hook(std::function<void()> hook);

// Savestate-load hook. Called once a savestate has fully applied (guest PC,
// frame_count() and all other snapshot state already updated) — after every
// do_savestate_load() success, regardless of trigger (hotkey, TCP debug
// command, or the --load-state startup arg; see runtime.cpp's shared
// do_savestate_load lambda). A load can move runtime_current_frame()
// discontinuously (backward, or forward by however much guest time the save
// itself recorded), which any observer treating frame numbers as elapsed-time
// deltas within an open span must resynchronize against. Empty disables it.
void set_savestate_load_hook(std::function<void()> hook);

// Fires the hook installed by set_savestate_load_hook(), if any. Called from
// runtime.cpp's do_savestate_load() right after a load applies. Not for use
// outside that one call site.
void notify_savestate_loaded();

// Safe outer emulation boundary. The callback runs after one top-level guest
// dispatch, never from an audio/IRQ hook. It is cleared before runner locals
// captured by the callback leave scope.
void runtime_set_guest_step_boundary_hook(std::function<void()> hook);

// Turbo render-skip gate. Called at most once per guest frame, exactly when
// that frame's scanline counter wraps to 0 — i.e. BEFORE that frame's own
// HBlank scanline rendering starts, with the newly-started frame's index.
// Returning false skips that frame's PPU pixel work (scanline rendering and
// widescreen margin rows) only; every guest-observable PPU effect (HBlank/
// VBlank timing, DISPSTAT, VCOUNT, IRQs, HBlank/VBlank DMA, per-frame state
// latches) still runs unconditionally every frame regardless of the return
// value. Empty hook (default) always renders.
void runtime_set_frame_render_gate_hook(
    std::function<bool(unsigned long long)> hook);

// Retrieve the currently-bound bus / ppu, or nullptr if none.
gba::GbaBus* active_bus();
gba::GbaPpu* active_ppu();

// Minimal, dependency-free 8-bit RGB PNG writer (defined in runtime.cpp;
// the same writer run_game() uses for --dump-png). Exposed so game-owned
// tooling can save a screenshot from outside this translation unit, e.g.
// against gba::GbaPpu::latched_framebuffer()/render_width()/render_height().
bool write_png(const std::string& path, const std::uint8_t* rgb,
               std::uint32_t w, std::uint32_t h);

}  // namespace gbarecomp
