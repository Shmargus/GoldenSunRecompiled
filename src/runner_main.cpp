#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "crc32.h"
#include "blitter_shadow_observer.h"
#include "crash_handler.h"
#include "recompiled.h"
#include "relocatable_identity.h"
#include "runtime.h"
#include "runtime_arm.h"
#include "self_heal.h"
#include "sha1.h"

struct DispatchEntry {
    std::uint32_t addr;
    std::uint8_t thumb;
    void (*fn)(void);
};

extern "C" const DispatchEntry gsr_func2d5c_kDispatchTable[];
extern "C" const unsigned gsr_func2d5c_kDispatchTableLen;
extern "C" const DispatchEntry gsr_funca37c_kDispatchTable[];
extern "C" const unsigned gsr_funca37c_kDispatchTableLen;
extern "C" const DispatchEntry gsr_func2808_03006000_kDispatchTable[];
extern "C" const unsigned gsr_func2808_03006000_kDispatchTableLen;
extern "C" const DispatchEntry gsr_func15e10_03003a84_kDispatchTable[];
extern "C" const unsigned gsr_func15e10_03003a84_kDispatchTableLen;
extern "C" const DispatchEntry gsr_funca418_03003400_kDispatchTable[];
extern "C" const unsigned gsr_funca418_03003400_kDispatchTableLen;
extern "C" const DispatchEntry gsr_func15afc_03003a84_kDispatchTable[];
extern "C" const unsigned gsr_func15afc_03003a84_kDispatchTableLen;
// The flash driver's relocatable working-area routines. Each is ONE
// position-independent corpus serving every base the allocator hands out; see
// kRelocatableCodeImages below.
extern "C" const DispatchEntry gsr_func9bb8_pic_kDispatchTable[];
extern "C" const unsigned gsr_func9bb8_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func9bb8_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func9bb8_pic_kImageSize;
extern "C" const DispatchEntry gsr_func15430_pic_kDispatchTable[];
extern "C" const unsigned gsr_func15430_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func15430_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func15430_pic_kImageSize;
extern "C" const DispatchEntry gsr_func15570_pic_kDispatchTable[];
extern "C" const unsigned gsr_func15570_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func15570_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func15570_pic_kImageSize;
extern "C" const DispatchEntry gsr_func158e8_pic_kDispatchTable[];
extern "C" const unsigned gsr_func158e8_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func158e8_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func158e8_pic_kImageSize;
extern "C" const DispatchEntry gsr_func1b70_pic_kDispatchTable[];
extern "C" const unsigned gsr_func1b70_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func1b70_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func1b70_pic_kImageSize;
extern "C" const DispatchEntry gsr_func2544_pic_kDispatchTable[];
extern "C" const unsigned gsr_func2544_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func2544_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func2544_pic_kImageSize;
// Func_6abc, the four-byte THUMB stack thunk, position-independent.
extern "C" const DispatchEntry gsr_func6abc_pic_kDispatchTable[];
extern "C" const unsigned gsr_func6abc_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func6abc_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func6abc_pic_kImageSize;
// Position-independent: ONE corpus for Func_2cf4 at every stack depth its
// caller reaches. Replaces the four per-base corpora (0x03007ba4, 0x03007dbc,
// 0x03007dc4, 0x03007dc8) that were registered one build at a time before it
// was clear the set is bounded only by the call graph.
extern "C" const DispatchEntry gsr_func2cf4_pic_kDispatchTable[];
extern "C" const unsigned gsr_func2cf4_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func2cf4_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func2cf4_pic_kImageSize;
// Func_1dc8, the flash driver's relocatable working-area routine.
extern "C" const DispatchEntry gsr_func1dc8_pic_kDispatchTable[];
extern "C" const unsigned gsr_func1dc8_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func1dc8_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func1dc8_pic_kImageSize;
// Func_15afc and Func_15e10 share the 0x03003a84 staging slot. Both were
// registered fixed at that one base until a 10,800-frame campaign run copied
// Func_15afc to 0x030044f4 instead; see kRelocatableCodeImages below.
extern "C" const DispatchEntry gsr_func15afc_pic_kDispatchTable[];
extern "C" const unsigned gsr_func15afc_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func15afc_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func15afc_pic_kImageSize;
extern "C" const DispatchEntry gsr_func15e10_pic_kDispatchTable[];
extern "C" const unsigned gsr_func15e10_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func15e10_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func15e10_pic_kImageSize;
// The two remaining ARM routines of the same flash-driver pool family.
extern "C" const DispatchEntry gsr_func15d74_pic_kDispatchTable[];
extern "C" const unsigned gsr_func15d74_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func15d74_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func15d74_pic_kImageSize;
extern "C" const DispatchEntry gsr_func155d0_pic_kDispatchTable[];
extern "C" const unsigned gsr_func155d0_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func155d0_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func155d0_pic_kImageSize;
// Func_9bb8's SHORT (0x1e4) copy variant - a second identity for the SAME
// routine, not a second routine. See kRelocatableCodeImages.
extern "C" const DispatchEntry gsr_func9bb8short_pic_kDispatchTable[];
extern "C" const unsigned gsr_func9bb8short_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func9bb8short_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func9bb8short_pic_kImageSize;
// Code-only Func_9bb8: the identity that survives the game patching its own
// literal pool after the copy. See kRelocatableCodeImages.
extern "C" const DispatchEntry gsr_func9bb8code_pic_kDispatchTable[];
extern "C" const unsigned gsr_func9bb8code_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_func9bb8code_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_func9bb8code_pic_kImageSize;
// Func_b5138, a transient allocator block DMA-copied into IWRAM at whatever
// base the allocator hands out. Observed at 0x0300207c (first scripted
// fight); base is allocator-determined, so this is registered
// position-independent from the start rather than fixed. GS-011 follow-up.
extern "C" const DispatchEntry gsr_funcb5138_pic_kDispatchTable[];
extern "C" const unsigned gsr_funcb5138_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_funcb5138_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_funcb5138_pic_kImageSize;
// Func_a418, the same ROM routine already registered fixed at 0x03003400
// (see kTransientCodeImages below), now also observed at 0x03002000. Routine
// and base vary independently (D-005); registered position-independent so a
// third base costs nothing. GS-011 follow-up.
extern "C" const DispatchEntry gsr_funca418_pic_kDispatchTable[];
extern "C" const unsigned gsr_funca418_pic_kDispatchTableLen;
extern "C" const std::uint32_t gsr_funca418_pic_kImageOrigin;
extern "C" const std::uint32_t gsr_funca418_pic_kImageSize;
extern "C" const DispatchEntry gsr_synth_dc8_030008d4_kDispatchTable[];
extern "C" const unsigned gsr_synth_dc8_030008d4_kDispatchTableLen;
extern "C" const DispatchEntry gsr_synth_dc8_03000904_kDispatchTable[];
extern "C" const unsigned gsr_synth_dc8_03000904_kDispatchTableLen;
extern "C" const DispatchEntry gsr_synth_dc8_030008ec_kDispatchTable[];
extern "C" const unsigned gsr_synth_dc8_030008ec_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_779188_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_779188_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_787e04_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_787e04_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_77dd1c_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_77dd1c_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_784360_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_784360_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_78603c_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_78603c_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_7795e8_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_7795e8_kDispatchTableLen;
extern "C" const DispatchEntry gsr_overlay_rom_780898_kDispatchTable[];
extern "C" const unsigned gsr_overlay_rom_780898_kDispatchTableLen;
#define GSR_OVERLAY(id, start, end, sha1)                                  \
    extern "C" const DispatchEntry gsr_overlay_##id##_kDispatchTable[];   \
    extern "C" const unsigned gsr_overlay_##id##_kDispatchTableLen;
#include "overlay-registry.inc"
#undef GSR_OVERLAY

namespace {

constexpr const char* kRomSha1 =
    "5c4695205413df7db52b9a184815a07783999971";

struct TransientCodeImage {
    std::uint32_t start;
    std::uint32_t end;
    std::uint32_t rom_start;
    int thumb;
    const char* sha1;
    const char* name;
    const DispatchEntry* dispatch_table;
    const unsigned* dispatch_table_len;
};

const auto kTransientCodeImages = std::to_array<TransientCodeImage>({
    {0x03002000u, 0x0300207Cu, 0x08002D5Cu,
     0,
     "a0e2414cc0e339ec6f61fbd387eb6325a8eabe84", "Func_2d5c",
     gsr_func2d5c_kDispatchTable, &gsr_func2d5c_kDispatchTableLen},
    {0x03002000u, 0x0300209Cu, 0x0800A37Cu,
     0,
     "77bec232b22b723f3c5f5e0fedd28680fde50048", "Func_a37c",
     gsr_funca37c_kDispatchTable, &gsr_funca37c_kDispatchTableLen},
    {0x03002000u, 0x03002140u, 0x08015430u,
     0,
     "d49cef2181ce6f9fee635daa0ba3b072d336109a", "Func_15430",
     nullptr, nullptr},
    {0x03002140u, 0x030021A0u, 0x08015570u,
     0,
     "20f9190c9b7aac90e091beff5f7e54a46ba833eb", "Func_15570",
     nullptr, nullptr},
    // Func_2544 at 0x03002000 and 0x03006000 used to be two entries here. The
    // row at 0x0300347c below stays, because it is the identity gate for the
    // PCs the main corpus dispatches statically; the routine itself is now a
    // position-independent image.
    {0x03002400u, 0x030024E0u, 0x08001DC8u,
     0,
     "c2e9ace3620fc779387d91100c97071ebf21640b", "Func_1dc8",
     nullptr, nullptr},
    {0x0300387Cu, 0x0300395Cu, 0x08001DC8u,
     0,
     "c2e9ace3620fc779387d91100c97071ebf21640b", "Func_1dc8_at_0300387c",
     nullptr, nullptr},
    {0x0300347Cu, 0x03003740u, 0x08002544u,
     0,
     "b8967c9f00835f22da02cce31595588bb05496e6", "Func_2544_at_0300347c",
     nullptr, nullptr},
    // GS-011: the allocator reuses 0x03006000 for a second, larger ROM image.
    // DMA3 watch observed src=0x08002808 cnt=315 words; goldensun.elf agrees
    // (Func_2808, size 0x4ec, arm). SHA-1 keeps the two images distinct.
    {0x03006000u, 0x030064ECu, 0x08002808u,
     0,
     "5fd23904086a4129fe3472dbf6658bb5e73f0363",
     "Func_2808_at_03006000",
     gsr_func2808_03006000_kDispatchTable,
     &gsr_func2808_03006000_kDispatchTableLen},
    // Func_6abc's stack thunk used to be registered here at 0x03007df8 and
    // 0x03007bc0. Func_6878 installs it at whatever depth it was entered at,
    // so it is now a single position-independent image below.
    // Func_9bb8, Func_15430, Func_15570 and Func_158e8 used to occupy nine
    // entries here, one per (routine, base) pair a run happened to reach. They
    // are position-independent images now; the DMA3 census that motivated them
    // is preserved in docs/GS011_TRANSIENT_IMAGES.md.
    // Func_a418 sits immediately below the 0x0300347c staging slot, so the
    // flash driver's working area is a contiguous run of separately installed
    // routines. DMA3 watch: src=0x0800a418, 31 words; entry observed through
    // _call_via_r4 in ARM. goldensun.elf sizes Func_a418 at exactly 0x7c.
    {0x03003400u, 0x0300347Cu, 0x0800A418u,
     0,
     "d3261b6f7b57ba4e7da8e44cf555d009c7cc27d1",
     "Func_a418_at_03003400",
     gsr_funca418_03003400_kDispatchTable,
     &gsr_funca418_03003400_kDispatchTableLen},
    // 0x03003a84 is a reusable staging slot for the flash driver. Trace
    // #4699483 reprograms DMA3 SAD=0x08015e10 CNT=0x1f words over the
    // Func_15afc image below, and #4699487 enters it in ARM through
    // _call_via_r4. goldensun.elf sizes Func_15e10 at exactly 0x7c bytes.
    {0x03003A84u, 0x03003B00u, 0x08015E10u,
     0,
     "50cf8d2eaf6ba74b7ae74a97458d04214a00af57",
     "Func_15e10_at_03003a84",
     gsr_func15e10_03003a84_kDispatchTable,
     &gsr_func15e10_03003a84_kDispatchTableLen},
    // Func_15afc, DMA-copied into IWRAM. Trace #4697289..91 programs DMA3
    // SAD=0x08015afc DAD=0x03003a84 CNT=0x9e words, the descriptor at
    // 0x03001e50 records the same 0x03003a84..0x03003cfc bounds, and #4697295
    // enters it in ARM through _call_via_r3. goldensun.elf sizes Func_15afc at
    // exactly 0x278 bytes.
    {0x03003A84u, 0x03003CFCu, 0x08015AFCu,
     0,
     "f6ab01b56876f8d1b72a4bf5239bd5cbac620cb0",
     "Func_15afc_at_03003a84",
     gsr_func15afc_03003a84_kDispatchTable,
     &gsr_func15afc_03003a84_kDispatchTableLen},
    // Func_2cf4's four observed stack depths (0x03007ba4, 0x03007dbc,
    // 0x03007dc4, 0x03007dc8) used to be four entries here. Func_3a7c DMAs the
    // routine to whatever its SP happens to be, so that set is bounded only by
    // the call graph and enumerating it never converges. It is now a single
    // POSITION-INDEPENDENT image in kRelocatableCodeImages below.
    // Ring event #3295783 records the first post-boot write: ARM STMIA at
    // 0x03000800 installs the 24-byte template rooted at 0x030008D4. The
    // writer repeats its six words into four holes while preserving the fixed
    // instructions between them. This identity is valid after the loop exits
    // at 0x03000818 and only until the next writer pass or an IWRAM reset.
    {0x03000828u, 0x030008CCu, 0u, 0,
     "6e801af81bd285752b5efffccc815c7c9dc2cfc3",
     "Func_dc8_synth_template_030008d4",
     gsr_synth_dc8_030008d4_kDispatchTable,
     &gsr_synth_dc8_030008d4_kDispatchTableLen},
    // The strict-static path independently selected the adjacent 24-byte
    // template at 0x030008EC. Keep it as a separate whole-image identity.
    {0x03000828u, 0x030008CCu, 0u, 0,
     "e9437b0a1a071ea8a136fab4ce4c47db3bac2e2e",
     "Func_dc8_synth_template_030008ec",
     gsr_synth_dc8_030008ec_kDispatchTable,
     &gsr_synth_dc8_030008ec_kDispatchTableLen},
    // The third 24-byte template, at 0x03000904, was previously derived but
    // unobserved. A self-heal discovery run reached 0x03000828 holding image
    // SHA-1 d8d1bcfc..., which is exactly what expanding that template
    // produces, so it is now an observed identity like the other two.
    {0x03000828u, 0x030008CCu, 0u, 0,
     "d8d1bcfcf2734deaca85791720f6180f4da4fcca",
     "Func_dc8_synth_template_03000904",
     gsr_synth_dc8_03000904_kDispatchTable,
     &gsr_synth_dc8_03000904_kDispatchTableLen},
#if 0  // Superseded by the generated, complete overlay registry below.
    // TCP capture at frame 293, PC 0x0808c5ba matched the installed bytes
    // uniquely to the pinned rom_779188 overlay build. Its compressed ROM
    // stream has no linear source/runtime bias, so identity is an image SHA-1
    // rather than rom_start + offset.
    //
    // The extent is the overlay's `.text` section, NOT its whole decompressed
    // image. Both overlays carry a writable `.data` section immediately after
    // `.text` (rom_779188: 0x020085f8, 0x5a bytes, plus 0x54 of .bss;
    // rom_7795e8: 0x020094d4, 0x246 bytes). Hashing those bytes made the
    // identity depend on mutable game state: an input-driven run aborted with
    // "unknown transient code identity at 0x020081fc" whose live window
    // differed from pinned rom_7795e8 in exactly two bytes, at 0x020096b8 and
    // 0x020096bc — both inside `.data`. A code identity must cover code only.
    {0x02008000u, 0x020085F8u, 0u, 1,
     "dbead77b3bca167228968dda277639ecb7355e01", "overlay_rom_779188",
     gsr_overlay_rom_779188_kDispatchTable,
     &gsr_overlay_rom_779188_kDispatchTableLen},
    // A third overlay reaches the same slot once the demo-input driver enters
    // gameplay. Its live `.text` matched pinned rom_787e04 byte for byte, and
    // the abort PC 0x02008160 is that build's exact ELF OvlFunc_160 entry.
    {0x02008000u, 0x02009AB4u, 0u, 1,
     "6ee2f3a2a1370a8a42e7d0e25eb983f0ef0576fb", "overlay_rom_787e04",
     gsr_overlay_rom_787e04_kDispatchTable,
     &gsr_overlay_rom_787e04_kDispatchTableLen},
    // A fourth overlay, reached only by the 5,400-frame campaign track. The
    // 5,400-frame run aborted on an unknown identity at 0x02008104; the live
    // EWRAM window matched pinned rom_77dd1c byte for byte over the whole
    // 0x4000 dump, and 0x02008105 is that build's exact ELF OvlFunc_104 THUMB
    // entry (a $t mapping symbol sits at 0x02008104).
    //
    // Extent is `.text` only — 0x02008000 + 0x48bc — NOT the 0x57f8
    // decompressed image. .data starts at 0x0200c8bc (0xf3c bytes) and .bss at
    // 0x0200d7f8; hashing those would make the code identity depend on mutable
    // game state, which is exactly the defect the rom_7795e8 audit found.
    {0x02008000u, 0x0200C8BCu, 0u, 1,
     "b58745ad5c0dc6872de7e75d5bf527714cad7f1e", "overlay_rom_77dd1c",
     gsr_overlay_rom_77dd1c_kDispatchTable,
     &gsr_overlay_rom_77dd1c_kDispatchTableLen},
    // Fifth overlay, found in live play just past the Mt. Aleph boulder scene.
    // Registered .text-only (0x1bdc): .data begins at 0x02009bdc and the game
    // writes it, so including it would make the code identity depend on mutable
    // state - the same defect already fixed for rom_779188 and rom_7795e8.
    {0x02008000u, 0x02009BDCu, 0u, 1,
     "eff53a832d743ae378ec9d07ff54e2e43b4c383c", "overlay_rom_78603c",
     gsr_overlay_rom_78603c_kDispatchTable,
     &gsr_overlay_rom_78603c_kDispatchTableLen},
    // Sixth overlay: the plaza cutscene near the psynergy stone. Found in live
    // play 2026-08-08 as a FREEZE, not a miss — the interpreter bridge for
    // 0x02008BBC ran away ("exceeded 200000000 instructions without returning
    // to stop_pc=0x0808D88C"). Identified by dumping the live window with
    // GBARECOMP_TRANSIENT_DUMP and matching it against the pinned decompressed
    // overlay set: rom_784360, byte for byte, all 16084 bytes.
    // Registered .text-only (0x2854): overlay.elf puts .data at 0x0200a854 and
    // the game writes it, so including it would make the code identity depend
    // on mutable state — the same defect already fixed for rom_779188,
    // rom_7795e8 and rom_78603c.
    {0x02008000u, 0x0200A854u, 0u, 1,
     "ed3b6e11906e313a8a1aaefaa4e6aef4374c6d15", "overlay_rom_784360",
     gsr_overlay_rom_784360_kDispatchTable,
     &gsr_overlay_rom_784360_kDispatchTableLen},
    // The allocator later replaces the shared EWRAM slot with rom_7795e8.
    {0x02008000u, 0x020094D4u, 0u, 1,
     "c728ff67c9e34efa992768081689429f1bd826bc", "overlay_rom_7795e8",
     gsr_overlay_rom_7795e8_kDispatchTable,
     &gsr_overlay_rom_7795e8_kDispatchTableLen},
    // Seventh overlay: the post-fight cutscene after loading state 8. The first
    // unknown PC was 0x02008AA4. Five independently sized live-prefix SHA-1s
    // and all six self-healed block CRC32s uniquely match pinned rom_780898.
    // Its ELF confirms the observed THUMB entries and the 0x02008AAC interior
    // resume. Register `.text` only: writable `.data` begins at 0x0200E190.
    {0x02008000u, 0x0200E190u, 0u, 1,
     "ab81b675700afa375749790d84da3ae5ffafe9fa", "overlay_rom_780898",
     gsr_overlay_rom_780898_kDispatchTable,
     &gsr_overlay_rom_780898_kDispatchTableLen},
#endif
#define GSR_OVERLAY(id, start, end, sha1)                                  \
    {start, end, 0u, 1, sha1, "overlay_" #id,                             \
     gsr_overlay_##id##_kDispatchTable,                                    \
     &gsr_overlay_##id##_kDispatchTableLen},
#include "overlay-registry.inc"
#undef GSR_OVERLAY
});

// Identity validation is needed only after a write can have changed a page
// containing one of these reviewed fixed-address transient images.  Derive
// the masks from the registry so ordinary data traffic does not invalidate
// every cached image and no RAM address is guessed here.
void init_ram_code_page_masks() {
    g_ram_code_page_mask_ewram_lo = 0;
    g_ram_code_page_mask_ewram_hi = 0;
    g_ram_code_page_mask_iwram = 0;
    for (const auto& image : kTransientCodeImages) {
        if (image.end <= image.start) continue;
        const uint32_t last = image.end - 1u;
        const uint32_t region = image.start >> 24;
        if (region == 0x03u) {
            const uint32_t first_page = (image.start & 0x7FFFu) >> 12;
            const uint32_t last_page = (last & 0x7FFFu) >> 12;
            for (uint32_t page = first_page; page <= last_page; ++page)
                g_ram_code_page_mask_iwram |= 1u << page;
        } else if (region == 0x02u) {
            const uint32_t first_page = (image.start & 0x3FFFFu) >> 12;
            const uint32_t last_page = (last & 0x3FFFFu) >> 12;
            for (uint32_t page = first_page; page <= last_page; ++page) {
                if (page < 32u) g_ram_code_page_mask_ewram_lo |= 1ull << page;
                else g_ram_code_page_mask_ewram_hi |= 1ull << (page - 32u);
            }
        }
    }
}

// A RAM code image whose translation is position-independent: guest addresses
// inside it are computed from g_runtime_image_base, so one corpus dispatches
// at every base the game copies the routine to. `origin` is the address the
// corpus was generated at — the dispatch table is expressed in it, and a live
// PC is translated by subtracting the verified base.
//
// This is what turns the relocatable pool from an unbounded enumeration into a
// finite one: a routine is registered once, not once per (routine, base).
struct RelocatableCodeImage {
    std::uint32_t rom_start;   // immutable ROM backing; 0 = no linear source
    int thumb;
    const char* sha1;          // identity over the extent, minus excluded_ranges
    const char* name;
    const std::uint32_t* origin;
    const std::uint32_t* size;
    const DispatchEntry* dispatch_table;
    const unsigned* dispatch_table_len;
    // Declared [[data_range]] spans (offsets from origin/base) the image
    // writes to itself at runtime. Skipped when computing the identity hash
    // so self-modified data doesn't fail a code-only identity. Empty for
    // every image that has no interior data range.
    const gsr::ByteRange* excluded_ranges = nullptr;
    unsigned excluded_ranges_len = 0;
};

// Func_b5138 self-relocates a jump table into the middle of its own extent:
// ROM 0x080b5138+0xcc..0xd4 (a $d literal pool) and 0x080b5138+0xe0..0x120
// (the placeholder jump table the copy's own relocation loop overwrites),
// both declared as [[data_range]] in
// config/usa/transient-func-b5138-relocatable.toml. Offsets are relative to
// the image origin 0x0300207c, which is also the base at runtime since a
// relocatable image's data ranges move with the copy.
constexpr gsr::ByteRange kFuncB5138ExcludedRanges[] = {
    {0x000000CCu, 0x000000D4u},
    {0x000000E0u, 0x00000120u},
};

const std::array<RelocatableCodeImage, 17> kRelocatableCodeImages{{
    // Func_2cf4, DMA-copied onto the stack by Func_3a7c. Observed at
    // 0x03007ba4, 0x03007dbc, 0x03007dc4 and 0x03007dc8; the extent, mode and
    // identity are the same at every one of them, and goldensun.elf bounds the
    // ROM source at 0x08002cf4 size 0x68 with ARM mapping symbols.
    {0x08002CF4u,
     0,
     "87b609455a1aa6cd4f6251a73f8e15f4b6bc9c05",
     "Func_2cf4_relocatable",
     &gsr_func2cf4_pic_kImageOrigin,
     &gsr_func2cf4_pic_kImageSize,
     gsr_func2cf4_pic_kDispatchTable,
     &gsr_func2cf4_pic_kDispatchTableLen},
    // Func_1dc8, copied into the flash driver's working area. Known at
    // 0x03002400 and 0x0300387c, and the campaign track reached a third base
    // 0x03003f9c (trace #9553280..82 programs DMA3 SAD=0x08001dc8
    // DAD=0x03003f9c CNT=0x84000038, and #9553285 exchanges to it in ARM
    // through _call_via_r6). goldensun.elf sizes Func_1dc8 at 0xe0 with ARM
    // mapping symbols; the DMA word count agrees exactly.
    //
    // ADDITIVE, not a replacement: the two fixed rows above for 0x03002400 and
    // 0x0300387c stay, because they are the identity gate for PCs the main
    // corpus dispatches statically. This image covers every other base.
    {0x08001DC8u,
     0,
     "c2e9ace3620fc779387d91100c97071ebf21640b",
     "Func_1dc8_relocatable",
     &gsr_func1dc8_pic_kImageOrigin,
     &gsr_func1dc8_pic_kImageSize,
     gsr_func1dc8_pic_kDispatchTable,
     &gsr_func1dc8_pic_kDispatchTableLen},
    // Func_6abc, the four-byte THUMB thunk Func_6ac0 writes into Func_6878's
    // active stack frame. Observed at 0x03007df8 and, one frame shallower,
    // 0x03007bc0. Four bytes is a weak identity on its own, which is why the
    // mode is checked too and why the resolver only ever tries bases derived
    // from the failing PC itself.
    {0x08006ABCu,
     1,
     "bbfc4c623d6e47e6d00a0c13ca4a9bbf58f6af36",
     "Func_6abc_relocatable",
     &gsr_func6abc_pic_kImageOrigin,
     &gsr_func6abc_pic_kImageSize,
     gsr_func6abc_pic_kDispatchTable,
     &gsr_func6abc_pic_kDispatchTableLen},
    // The flash driver's working-area routines. A DMA3 watch on 0x0300347c
    // across one campaign run counted 219 copies from 0x08009bb8, 12 from
    // 0x08015430, 4 from 0x08002544 and 1 from 0x080158e8, and the same
    // routines also appear at 0x03002000, 0x03002140, 0x030035bc, 0x03003b9c,
    // 0x03003cdc and 0x03006000. Routine and base vary independently, which is
    // exactly what per-(routine, base) registration could not keep up with.
    //
    // Every extent, mode and identity below is the one the fixed registrations
    // carried, re-derived from goldensun.elf STT_FUNC sizes by
    // tools/build_transient_image_config.py. All five SHA-1s matched what the
    // live-verified rows already used, which is an independent check that the
    // ELF extents and the observed DMA extents agree.
    {0x08009BB8u,
     0,
     "38da8bb7176c71ef1e86ce50346af1d72174529c",
     "Func_9bb8_relocatable",
     &gsr_func9bb8_pic_kImageOrigin,
     &gsr_func9bb8_pic_kImageSize,
     gsr_func9bb8_pic_kDispatchTable,
     &gsr_func9bb8_pic_kDispatchTableLen},
    {0x08015430u,
     0,
     "d49cef2181ce6f9fee635daa0ba3b072d336109a",
     "Func_15430_relocatable",
     &gsr_func15430_pic_kImageOrigin,
     &gsr_func15430_pic_kImageSize,
     gsr_func15430_pic_kDispatchTable,
     &gsr_func15430_pic_kDispatchTableLen},
    // Func_15570 is always installed contiguously after Func_15430, so the two
    // are separate images at a fixed stride rather than one larger image.
    {0x08015570u,
     0,
     "20f9190c9b7aac90e091beff5f7e54a46ba833eb",
     "Func_15570_relocatable",
     &gsr_func15570_pic_kImageOrigin,
     &gsr_func15570_pic_kImageSize,
     gsr_func15570_pic_kDispatchTable,
     &gsr_func15570_pic_kDispatchTableLen},
    {0x080158E8u,
     0,
     "03e57ceee902e248334bb546cd32716edf90fca3",
     "Func_158e8_relocatable",
     &gsr_func158e8_pic_kImageOrigin,
     &gsr_func158e8_pic_kImageSize,
     gsr_func158e8_pic_kDispatchTable,
     &gsr_func158e8_pic_kDispatchTableLen},
    // ADDITIVE at 0x0300347c: that fixed row stays, because it gates PCs the
    // main corpus dispatches statically. This image covers every other base.
    {0x08002544u,
     0,
     "b8967c9f00835f22da02cce31595588bb05496e6",
     "Func_2544_relocatable",
     &gsr_func2544_pic_kImageOrigin,
     &gsr_func2544_pic_kImageSize,
     gsr_func2544_pic_kDispatchTable,
     &gsr_func2544_pic_kDispatchTableLen},
    // A NEW pooled routine, found only by the 5,400-frame campaign run. It
    // aborted as an unknown identity at 0x0300387c, 208 bytes different from
    // the Func_1dc8 image that address had been hosting; a DMA3 watch on that
    // address records ch=3 src=0x08001b70 word=0/150 (150 words = 0x258 bytes)
    // as the last writer, the live first word 0xE92D40E2 matches ROM
    // 0x08001b70, and goldensun.elf sizes Func_1b70 at exactly 0x258 with an
    // $a at the entry. Registered position-independent from the start.
    //
    // This is the class relocation does NOT close: it removes the need to
    // enumerate bases, not the need to discover routines.
    {0x08001B70u,
     0,
     "7e06447b0578b9d6ea559eda377aafdd6faecdbe",
     "Func_1b70_relocatable",
     &gsr_func1b70_pic_kImageOrigin,
     &gsr_func1b70_pic_kImageSize,
     gsr_func1b70_pic_kDispatchTable,
     &gsr_func1b70_pic_kDispatchTableLen},
    // Func_15afc and Func_15e10, the last two flash-driver routines still
    // pinned to a single base. 0x03003a84 was treated as a fixed staging slot
    // until the 10,800-frame campaign run aborted at ARM 0x030044f4: a DMA3
    // watch records ch=3 dad=0x030044F4 src=0x08015AFC word=0/158 (158 words =
    // 0x278 bytes) with first word 0xE92D0060, an ARM STMDB sp! prologue, and
    // goldensun.elf sizes Func_15afc at exactly 0x278 with an $a at the entry -
    // DMA extent and ELF extent agree. The SHA-1 below is the ROM source bytes
    // at 0x08015afc over that extent and matches, byte for byte, the identity
    // the live-verified fixed row at 0x03003a84 already carried, which is an
    // independent check that this is the same routine at a new base.
    //
    // Func_15e10 is converted alongside it rather than after its own abort: it
    // shares the same staging slot, so the pool will relocate it for the same
    // reason. Conversion REMOVES an address assumption rather than adding one,
    // and the resolver hashes the full extent before dispatching, so a wrong
    // base aborts loudly instead of running.
    //
    // ADDITIVE, like Func_1dc8: the fixed 0x03003a84 rows above stay. They are
    // a redundant identity gate here rather than a necessary one, but a
    // redundant gate is safe and a missing gate is not; deleting them is a
    // separate change that wants its own verified run.
    {0x08015AFCu,
     0,
     "f6ab01b56876f8d1b72a4bf5239bd5cbac620cb0",
     "Func_15afc_relocatable",
     &gsr_func15afc_pic_kImageOrigin,
     &gsr_func15afc_pic_kImageSize,
     gsr_func15afc_pic_kDispatchTable,
     &gsr_func15afc_pic_kDispatchTableLen},
    {0x08015E10u,
     0,
     "50cf8d2eaf6ba74b7ae74a97458d04214a00af57",
     "Func_15e10_relocatable",
     &gsr_func15e10_pic_kImageOrigin,
     &gsr_func15e10_pic_kImageSize,
     gsr_func15e10_pic_kDispatchTable,
     &gsr_func15e10_pic_kDispatchTableLen},
    // Closing the flash-driver pool FAMILY instead of discovering it one abort
    // at a time. After Func_15afc and Func_15e10 were relocated, the campaign
    // run advanced and then aborted at 0x030044f4 AGAIN - the same base, a
    // different routine. A DMA3 watch on that address shows the slot cycling
    // between src=0x08015AFC (158 words), src=0x08015E10 (31 words) and finally
    // src=0x08015D74 (39 words). It is one pool, reused.
    //
    // In goldensun.elf the ARM STT_FUNCs in the contiguous run
    // 0x08015430..0x08015e10 are exactly: 15430, 15570, 155d0, 158e8, 15afc,
    // 15d74, 15e10. (The odd-valued symbols interleaved with them have bit 0
    // set and are THUMB, not pool members.) Five were already registered, so
    // Func_15d74 and Func_155d0 are the whole remainder - a bounded set, not an
    // open-ended one.
    //
    // Func_15d74 is directly attributed by the DMA watch above: 39 words = 156
    // bytes, and the ELF sizes it at exactly 156 with an $a at the entry.
    // Func_155d0 is registered on the Func_15e10 precedent, which the very
    // first run vindicated: an image that is never copied simply never
    // verifies, while a missing one aborts a run.
    {0x08015D74u,
     0,
     "4058793ba5db0477fc9c5c556edb91984dee3de0",
     "Func_15d74_relocatable",
     &gsr_func15d74_pic_kImageOrigin,
     &gsr_func15d74_pic_kImageSize,
     gsr_func15d74_pic_kDispatchTable,
     &gsr_func15d74_pic_kDispatchTableLen},
    {0x080155D0u,
     0,
     "4b3fe199338f1ecb1399cf9668839e72386bf7bc",
     "Func_155d0_relocatable",
     &gsr_func155d0_pic_kImageOrigin,
     &gsr_func155d0_pic_kImageSize,
     gsr_func155d0_pic_kDispatchTable,
     &gsr_func155d0_pic_kDispatchTableLen},
    // Func_9bb8 copied SHORT. The flash driver copies this one routine at two
    // different lengths from the same source: a DMA3 watch on 0x03003eec across
    // one campaign run records src=0x08009BB8 word=0/177 (0x2c4, the full ELF
    // STT_FUNC extent, registered above) AND word=0/121 (0x1e4).
    //
    // These are two IMAGES, not a wrong extent. goldensun.elf puts a trailing
    // $d literal pool at 0x08009d90 running to Func_9e7c at 0x08009e7c; the
    // short copy ends at 0x08009d9c, i.e. every instruction (code ends at
    // 0x08009d90) plus the three literal words it needs. A SHA-1 over the full
    // 0x2c4 extent covers 224 bytes the short copy never wrote, so it can only
    // ever match the long copy.
    //
    // GENERAL LESSON, recorded in docs/GS011_TRANSIENT_IMAGES.md: an image
    // identity must cover the bytes the WRITER wrote, not the bytes a symbol
    // table says the routine spans. This is the same defect class as hashing an
    // overlay's mutable .data section.
    {0x08009BB8u,
     0,
     "49496bc9fb68cacac07613ccb2ead86bc0583ffc",
     "Func_9bb8_short_relocatable",
     &gsr_func9bb8short_pic_kImageOrigin,
     &gsr_func9bb8short_pic_kImageSize,
     gsr_func9bb8short_pic_kDispatchTable,
     &gsr_func9bb8short_pic_kDispatchTableLen},
    // Func_9bb8, CODE-ONLY extent - the Mt. Aleph crash fix.
    //
    // The game bump-allocates an IWRAM block, DMAs this routine in, and then
    // PATCHES the copy: it writes runtime values into the routine's trailing
    // literal pool. A live dump from a replayed player session shows every one
    // of the 224 differing bytes at or after offset 0x1d8 - exactly where
    // goldensun.elf ends the code ($d at 0x08009d90). Bytes [0, 0x1d8) match
    // ROM byte for byte; the code is never modified.
    //
    // So the 0x2c4 and 0x1e4 identities above CANNOT match a live image, and
    // the runtime aborted on a perfectly healthy copy. Ordering matters: this
    // row must be tried, and it is the only one whose extent excludes bytes the
    // game writes.
    //
    // Third instance of one defect class: an identity must cover only bytes the
    // WRITER leaves alone - not an ELF extent, not a copy length.
    {0x08009BB8u,
     0,
     "36f6cde400bd706d9418bf212705dcfda0a02d94",
     "Func_9bb8_code_relocatable",
     &gsr_func9bb8code_pic_kImageOrigin,
     &gsr_func9bb8code_pic_kImageSize,
     gsr_func9bb8code_pic_kDispatchTable,
     &gsr_func9bb8code_pic_kDispatchTableLen},
    // Func_b5138, a transient allocator block. Trace events #55889096-105
    // (local/crash/terminal.log): allocator base 0x0300207c, next-free
    // 0x030022ac; DMA3SAD=0x080b5138, DMA3DAD=0x0300207c,
    // DMA3CNT=0x8400008c (0x230 bytes, matching the ELF STT_FUNC size
    // exactly); exchange cpsr=0x1f, T clear => ARM. See
    // config/usa/transient-func-b5138-relocatable.toml and
    // docs/GS011_TRANSIENT_IMAGES.md.
    //
    // The image self-relocates a jump table into its own extent (see
    // kFuncB5138ExcludedRanges above), so the whole-extent hash
    // "ec38094f7b1c2b14f7af17fdeb0072aa84d34ebe" stops matching once that
    // loop has run. The SHA-1 below is instead computed over the extent with
    // both declared data ranges removed: ROM 0x080b5138..0x080b5138+0x230,
    // concatenating [0,0xcc) + [0xd4,0xe0) + [0x120,0x230) (488 bytes), from
    // the hash-verified ROM.
    {0x080B5138u,
     0,
     "db6d5389162f3f8971ef3247ee7ed62596c5ed4d",
     "Func_b5138_relocatable",
     &gsr_funcb5138_pic_kImageOrigin,
     &gsr_funcb5138_pic_kImageSize,
     gsr_funcb5138_pic_kDispatchTable,
     &gsr_funcb5138_pic_kDispatchTableLen,
     kFuncB5138ExcludedRanges,
     2},
    // Func_a418, previously registered only fixed at 0x03003400 (see
    // kTransientCodeImages above). A savestate-frame-15982 run aborted with
    // "unknown transient code identity at 0x03002000"; all 16 fixed images and
    // the Func_15430 declared occupant of that slot failed identity (292
    // differing bytes). Searching the hash-verified ROM for the live 32-byte
    // prefix (`BICS r1,r1,#7; TSTNE r2,#7; BXEQ lr; PUSH {r5-r11}; MOV r12,r1;
    // LDM r0,{r4,r5}; ADD r0,r0,r1; LDM r0,{r6,r7}`) finds exactly one match, at
    // ROM 0x0800a418 - the very Func_a418 already registered at 0x03003400.
    // goldensun.elf sizes it at exactly 0x7c with a single $a mapping symbol
    // and no interior $d, matching the observed ARM entry (cpsr=0x1f, T
    // clear). Independent extent confirmation: rom_9000/src/rom_b798.s
    // (Func_bb20) DMA3s SAD=Func_a418, DAD=an allocator-returned buffer,
    // CNT=0x84000000|0x1f (enable, 32-bit, 31 words = 0x7c bytes) - agreeing
    // with the ELF size exactly - and the computed image SHA-1 matches the
    // 0x03003400 identity byte for byte. See
    // config/usa/transient-func-a418-relocatable.toml.
    {0x0800A418u,
     0,
     "d3261b6f7b57ba4e7da8e44cf555d009c7cc27d1",
     "Func_a418_relocatable",
     &gsr_funca418_pic_kImageOrigin,
     &gsr_funca418_pic_kImageSize,
     gsr_funca418_pic_kDispatchTable,
     &gsr_funca418_pic_kDispatchTableLen},
}};

// Last base each relocatable image was verified at. A pooled routine is
// entered many times in a row at the same base, so trying it first keeps the
// common case to one hash instead of a scan over every entry offset. It is a
// hint only — the identity hash still has to pass.
struct RelocatableBaseHint {
    std::uint32_t base;
    bool valid;
};
std::array<RelocatableBaseHint, kRelocatableCodeImages.size()>
    g_relocatable_base_hint{};

struct RelocatableProfileImage {
    std::uint64_t visits = 0;
    std::uint64_t hint_attempts = 0;
    std::uint64_t hint_hits = 0;
    std::uint64_t scan_candidates = 0;
    std::uint64_t prefix_passes = 0;
    std::uint64_t hash_matches = 0;
    std::uint64_t wins = 0;
    std::uint64_t entry_wins = 0;
    std::uint64_t base_switches = 0;
    std::vector<std::uint32_t> bases;
};

struct RelocatableProfilePc {
    std::uint32_t pc = 0;
    std::uint64_t calls = 0;
    std::uint64_t candidates = 0;
    std::uint64_t wins = 0;
};

std::array<RelocatableProfileImage, kRelocatableCodeImages.size()>
    g_relocatable_profile_images{};
std::vector<RelocatableProfilePc> g_relocatable_profile_pcs;
std::uint64_t g_relocatable_profile_calls = 0;
std::uint64_t g_relocatable_profile_wins = 0;
std::uint64_t g_relocatable_profile_active_samples = 0;
std::uint64_t g_relocatable_profile_active_sum = 0;
std::uint32_t g_relocatable_profile_active_max = 0;

// GBARECOMP_RELOCATABLE_PROFILE, if explicitly set, wins outright. Otherwise
// falls back to the config UI's "Additional debug logging" toggle (default
// OFF) — see runtime_arm.h. First call happens from live dispatch, well
// after the config UI has loaded config.ini, so this reflects a saved
// preference from the very first relocatable dispatch of the session
// (cached after that, matching the existing once-per-run idiom).
bool relocatable_profile_enabled() {
    static const bool enabled = [] {
        const char* env = std::getenv("GBARECOMP_RELOCATABLE_PROFILE");
        return env ? (env[0] != '\0' && env[0] != '0')
                   : (gsr_additional_debug_logging() != 0);
    }();
    return enabled;
}

RelocatableProfilePc& relocatable_profile_pc(std::uint32_t pc) {
    for (auto& entry : g_relocatable_profile_pcs) {
        if (entry.pc == pc) return entry;
    }
    g_relocatable_profile_pcs.push_back({pc});
    return g_relocatable_profile_pcs.back();
}

void report_relocatable_profile() {
    std::fprintf(stderr,
        "RELOC_PROFILE calls=%llu wins=%llu distinct_pcs=%zu "
        "active_samples=%llu active_average=%.2f active_max=%u\n",
        static_cast<unsigned long long>(g_relocatable_profile_calls),
        static_cast<unsigned long long>(g_relocatable_profile_wins),
        g_relocatable_profile_pcs.size(),
        static_cast<unsigned long long>(g_relocatable_profile_active_samples),
        g_relocatable_profile_active_samples == 0
            ? 0.0
            : static_cast<double>(g_relocatable_profile_active_sum) /
                  g_relocatable_profile_active_samples,
        g_relocatable_profile_active_max);
    for (std::size_t i = 0; i < kRelocatableCodeImages.size(); ++i) {
        const auto& image = kRelocatableCodeImages[i];
        const auto& stats = g_relocatable_profile_images[i];
        std::fprintf(stderr,
            "RELOC_IMAGE index=%zu name=%s visits=%llu hint_attempts=%llu "
            "hint_hits=%llu scan_candidates=%llu prefix_passes=%llu "
            "hash_matches=%llu wins=%llu entry_wins=%llu bases=%zu "
            "base_switches=%llu\n",
            i, image.name,
            static_cast<unsigned long long>(stats.visits),
            static_cast<unsigned long long>(stats.hint_attempts),
            static_cast<unsigned long long>(stats.hint_hits),
            static_cast<unsigned long long>(stats.scan_candidates),
            static_cast<unsigned long long>(stats.prefix_passes),
            static_cast<unsigned long long>(stats.hash_matches),
            static_cast<unsigned long long>(stats.wins),
            static_cast<unsigned long long>(stats.entry_wins),
            stats.bases.size(),
            static_cast<unsigned long long>(stats.base_switches));
        if (!stats.bases.empty()) {
            std::fprintf(stderr, "RELOC_BASES index=%zu", i);
            for (const std::uint32_t base : stats.bases) {
                std::fprintf(stderr, " 0x%08X", base);
            }
            std::fprintf(stderr, "\n");
        }
    }
    for (const auto& stats : g_relocatable_profile_pcs) {
        if (stats.wins != 0 || stats.candidates >= 1000) {
            std::fprintf(stderr,
                "RELOC_PC pc=0x%08X calls=%llu candidates=%llu wins=%llu\n",
                stats.pc,
                static_cast<unsigned long long>(stats.calls),
                static_cast<unsigned long long>(stats.candidates),
                static_cast<unsigned long long>(stats.wins));
        }
    }
}

// GBARECOMP_RELOCATABLE_LOG=1 reports each (image, base) pair the first time
// it is verified. The whole point of a position-independent image is that the
// base set is NOT enumerated ahead of time, so this is the only way to see
// which bases a run actually used — and the only honest way to claim the
// mechanism replaced a set of per-base registrations rather than skipping
// them. Off by default; one getenv at startup, no per-dispatch cost.
// Same env-wins/toggle-fallback precedence as relocatable_profile_enabled
// above.
bool relocatable_log_enabled() {
    static const bool enabled = [] {
        const char* env = std::getenv("GBARECOMP_RELOCATABLE_LOG");
        return env ? (env[0] != '\0' && env[0] != '0')
                   : (gsr_additional_debug_logging() != 0);
    }();
    return enabled;
}

void (*lookup_entry(const DispatchEntry* table, unsigned len,
                    std::uint32_t addr, int thumb))(void) {
    std::size_t lo = 0;
    std::size_t hi = len;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        const DispatchEntry& entry = table[mid];
        if (entry.addr < addr || (entry.addr == addr && entry.thumb < thumb)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < len) {
        const DispatchEntry& entry = table[lo];
        if (entry.addr == addr && entry.thumb == thumb) return entry.fn;
    }
    return nullptr;
}

std::string live_sha1(std::uint32_t start, std::uint32_t size) {
    std::vector<std::uint8_t> bytes(size);
    for (std::uint32_t offset = 0; offset < size; ++offset) {
        bytes[offset] = bus_read_u8(start + offset);
    }
    return gba::sha1(bytes.data(), bytes.size()).hex();
}

// A single bus_fast_ram() lookup per byte re-does the same region/mask check
// and pointer add every time; for a whole-extent hash that adds up over a
// ~700-byte image. When [base, base+size) provably stays inside one fast-RAM
// backing array (no IWRAM/EWRAM/mirror-boundary crossing), resolve the
// pointer once and let the caller index it directly instead. Returns null
// (never unsafe to do so — the caller falls back to the per-byte slow path)
// whenever fast RAM isn't available or the range isn't provably contiguous.
const std::uint8_t* bus_fast_ram_contiguous(std::uint32_t base,
                                            std::uint32_t size) {
    if (size == 0) return nullptr;
    if ((base >> 24) != ((base + size - 1u) >> 24)) return nullptr;
    return bus_fast_ram(base);
}

// Would `image` be resident at `base`? Rejects the base outright when it does
// not lie in RAM or is misaligned for the image's mode, then screens on the
// first word before paying for a hash of the whole extent.
bool relocatable_resident_at(const RelocatableCodeImage& image,
                             std::uint32_t base, bool* prefix_passed) {
    *prefix_passed = false;
    const std::uint32_t size = *image.size;
    if (base < 0x02000000u || base + size > 0x04000000u) return false;
    if (base & (image.thumb ? 1u : 3u)) return false;
    if (image.rom_start != 0u &&
        bus_read_u32(base) != bus_read_u32(image.rom_start)) {
        return false;
    }
    *prefix_passed = true;
    if (const std::uint8_t* fast = bus_fast_ram_contiguous(base, size)) {
        return gsr::sha1_excluding(
                   size,
                   [fast](std::uint32_t offset) { return fast[offset]; },
                   image.excluded_ranges, image.excluded_ranges_len) ==
               image.sha1;
    }
    return gsr::sha1_excluding(
               size,
               [base](std::uint32_t offset) { return bus_read_u8(base + offset); },
               image.excluded_ranges, image.excluded_ranges_len) == image.sha1;
}

// --- Cached relocatable identity (GS-cost: per-dispatch SHA-1 rehash) ----
//
// try_relocatable_dispatch's hint path calls relocatable_resident_at above
// on EVERY dispatch through a pooled routine, even though the hint means
// "this (image, base) pair already verified last time and nothing else has
// touched it." One cache entry per image, keyed on the base it was last
// verified at, lets a repeat dispatch skip the hash entirely while the pages
// the image occupies are provably unchanged — see relocatable_identity.h's
// "Page-epoch cache core" comment for why mask coverage has to be proven
// per-base here rather than computed once at startup like the fixed-address
// sibling's g_verified_identity_cache.
struct RelocatableIdentityCacheEntry {
    bool valid = false;
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::array<unsigned int, 64> page_epochs{};
    std::vector<std::uint8_t> snapshot;
};
std::array<RelocatableIdentityCacheEntry, kRelocatableCodeImages.size()>
    g_relocatable_identity_cache{};
unsigned long long g_relocatable_identity_hashes = 0;
unsigned long long g_relocatable_identity_cache_hits = 0;

unsigned int ram_code_page_epoch(std::uint32_t addr);

// Cache-aware sibling of relocatable_resident_at, used ONLY on the hint path
// (try_relocatable_dispatch already knows this (image, base) matched last
// time). Never concludes anything relocatable_resident_at would not: a cache
// hit only ever short-circuits to `true`, and any miss, invalidation or
// mismatch falls straight through to the same byte-exact hash the uncached
// path runs, which is the sole authority over whether the image matches.
bool relocatable_resident_at_cached(std::size_t index,
                                    const RelocatableCodeImage& image,
                                    std::uint32_t base, std::uint32_t pc,
                                    bool* prefix_passed) {
    *prefix_passed = false;
    const std::uint32_t size = *image.size;
    if (base < 0x02000000u || base + size > 0x04000000u) return false;
    if (base & (image.thumb ? 1u : 3u)) return false;
    if (image.rom_start != 0u &&
        bus_read_u32(base) != bus_read_u32(image.rom_start)) {
        return false;
    }
    *prefix_passed = true;

    auto& cache = g_relocatable_identity_cache[index];
    if (cache.valid && cache.base == base && cache.size == size &&
        gsr::ram_range_pages_current(base, base + size, cache.page_epochs,
                                     ram_code_page_epoch) &&
        gsr::identity_local_words_current(
            base, pc, cache.snapshot.data(), cache.size, image.excluded_ranges,
            image.excluded_ranges_len, bus_read_u32)) {
        ++g_relocatable_identity_cache_hits;
        return true;  // a cache entry is only ever installed on a real match
    }

    ++g_relocatable_identity_hashes;
    const bool matched = relocatable_resident_at(image, base, prefix_passed);
    if (matched) {
        gsr::ram_range_register_mask(base, base + size,
                                     &g_ram_code_page_mask_iwram,
                                     &g_ram_code_page_mask_ewram_lo,
                                     &g_ram_code_page_mask_ewram_hi);
        cache.valid = true;
        cache.base = base;
        cache.size = size;
        gsr::save_ram_range_page_epochs(base, base + size, cache.page_epochs,
                                        ram_code_page_epoch);
        cache.snapshot.resize(size);
        if (const std::uint8_t* fast = bus_fast_ram_contiguous(base, size)) {
            std::memcpy(cache.snapshot.data(), fast, size);
        } else {
            for (std::uint32_t offset = 0; offset < size; ++offset) {
                cache.snapshot[offset] = bus_read_u8(base + offset);
            }
        }
    } else {
        cache.valid = false;
    }
    return matched;
}

void sample_known_active_relocatable_placements() {
    std::uint32_t active = 0;
    for (std::size_t index = 0; index < kRelocatableCodeImages.size(); ++index) {
        const auto& image = kRelocatableCodeImages[index];
        for (const std::uint32_t base : g_relocatable_profile_images[index].bases) {
            bool prefix_passed = false;
            if (relocatable_resident_at(image, base, &prefix_passed)) ++active;
        }
    }
    ++g_relocatable_profile_active_samples;
    g_relocatable_profile_active_sum += active;
    if (active > g_relocatable_profile_active_max) {
        g_relocatable_profile_active_max = active;
    }
}

struct RelocatableOpcodeCandidate {
    std::uint32_t opcode;
    std::uint16_t image_index;
    const DispatchEntry* entry;
};

const std::vector<RelocatableOpcodeCandidate>& relocatable_opcode_candidates(
    int thumb) {
    static const std::vector<RelocatableOpcodeCandidate> arm = [] {
        std::vector<RelocatableOpcodeCandidate> result;
        for (std::size_t index = 0; index < kRelocatableCodeImages.size();
             ++index) {
            const auto& image = kRelocatableCodeImages[index];
            if (image.thumb || image.rom_start == 0u) continue;
            const std::uint32_t origin = *image.origin;
            const std::uint32_t size = *image.size;
            for (unsigned i = 0; i < *image.dispatch_table_len; ++i) {
                const DispatchEntry& entry = image.dispatch_table[i];
                const std::uint32_t offset = entry.addr - origin;
                if (entry.thumb || offset >= size) continue;
                result.push_back({bus_read_u32(image.rom_start + offset),
                                  static_cast<std::uint16_t>(index), &entry});
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& left,
                                                   const auto& right) {
            if (left.opcode != right.opcode) return left.opcode < right.opcode;
            if (left.image_index != right.image_index) {
                return left.image_index < right.image_index;
            }
            return left.entry->addr < right.entry->addr;
        });
        return result;
    }();
    static const std::vector<RelocatableOpcodeCandidate> thumb_candidates = [] {
        std::vector<RelocatableOpcodeCandidate> result;
        for (std::size_t index = 0; index < kRelocatableCodeImages.size();
             ++index) {
            const auto& image = kRelocatableCodeImages[index];
            if (!image.thumb || image.rom_start == 0u) continue;
            const std::uint32_t origin = *image.origin;
            const std::uint32_t size = *image.size;
            for (unsigned i = 0; i < *image.dispatch_table_len; ++i) {
                const DispatchEntry& entry = image.dispatch_table[i];
                const std::uint32_t offset = entry.addr - origin;
                if (!entry.thumb || offset >= size) continue;
                result.push_back({bus_read_u16(image.rom_start + offset),
                                  static_cast<std::uint16_t>(index), &entry});
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& left,
                                                   const auto& right) {
            if (left.opcode != right.opcode) return left.opcode < right.opcode;
            if (left.image_index != right.image_index) {
                return left.image_index < right.image_index;
            }
            return left.entry->addr < right.entry->addr;
        });
        return result;
    }();
    return thumb ? thumb_candidates : arm;
}

int try_relocatable_dispatch(std::uint32_t pc, int thumb) {
    const bool profiling = relocatable_profile_enabled();
    RelocatableProfilePc* pc_stats = nullptr;
    if (profiling) {
        if (g_relocatable_profile_calls++ == 0) {
            std::atexit(report_relocatable_profile);
        }
        pc_stats = &relocatable_profile_pc(pc);
        ++pc_stats->calls;
    }
    auto attempt = [&](std::size_t index, std::uint32_t base,
                       void (*known_fn)(void), bool from_hint) -> bool {
        const auto& image = kRelocatableCodeImages[index];
        auto& image_stats = g_relocatable_profile_images[index];
        const std::uint32_t origin = *image.origin;
        const std::uint32_t size = *image.size;
        if (profiling) {
            if (from_hint) ++image_stats.hint_attempts;
            else ++image_stats.scan_candidates;
            ++pc_stats->candidates;
        }
        const std::uint32_t offset = pc - base;
        if (offset >= size) return false;
        void (*fn)(void) = known_fn;
        if (fn == nullptr) {
            fn = lookup_entry(image.dispatch_table, *image.dispatch_table_len,
                              origin + offset, thumb);
        }
        if (fn == nullptr) return false;
        bool prefix_passed = false;
        const bool resident =
            from_hint ? relocatable_resident_at_cached(index, image, base, pc,
                                                        &prefix_passed)
                      : relocatable_resident_at(image, base, &prefix_passed);
        if (!resident) {
            if (profiling && prefix_passed) ++image_stats.prefix_passes;
            return false;
        }
        if (profiling) {
            ++image_stats.prefix_passes;
            ++image_stats.hash_matches;
            ++image_stats.wins;
            if (from_hint) ++image_stats.hint_hits;
            if (offset == 0) ++image_stats.entry_wins;
            ++g_relocatable_profile_wins;
            ++pc_stats->wins;
            bool known_base = false;
            for (const std::uint32_t known : image_stats.bases) {
                if (known == base) known_base = true;
            }
            if (!known_base) image_stats.bases.push_back(base);
            if (g_relocatable_base_hint[index].valid &&
                g_relocatable_base_hint[index].base != base) {
                ++image_stats.base_switches;
            }
            if (!from_hint) sample_known_active_relocatable_placements();
        }
        if (relocatable_log_enabled() &&
            !(g_relocatable_base_hint[index].valid &&
              g_relocatable_base_hint[index].base == base)) {
            std::fprintf(stderr,
                "GoldenSunRecomp: relocatable %s verified at base "
                "0x%08X (entry pc=0x%08X offset=0x%X)\n",
                image.name, base, pc, offset);
        }
        g_relocatable_base_hint[index] = {base, true};
        g_runtime_image_base = base;
        fn();
        return true;
    };

    // Try every image's last verified base before doing any discovery work.
    // A later image's good hint must not sit behind an earlier image's scan.
    for (std::size_t index = 0; index < kRelocatableCodeImages.size();
         ++index) {
        const auto& image = kRelocatableCodeImages[index];
        if (thumb != image.thumb) continue;
        if (profiling) ++g_relocatable_profile_images[index].visits;
        const RelocatableBaseHint hint = g_relocatable_base_hint[index];
        if (hint.valid && attempt(index, hint.base, nullptr, true)) return 1;
    }

    // Whole-image identity can only match when the live instruction at `pc`
    // equals the immutable instruction at the same image offset. Indexing by
    // opcode turns the old all-entry scan into a small exact candidate set;
    // the full identity hash below remains the authority.
    const std::uint32_t live_opcode =
        thumb ? bus_read_u16(pc) : bus_read_u32(pc);
    const auto& candidates = relocatable_opcode_candidates(thumb);
    auto first = std::lower_bound(
        candidates.begin(), candidates.end(), live_opcode,
        [](const RelocatableOpcodeCandidate& candidate, std::uint32_t opcode) {
            return candidate.opcode < opcode;
        });
    auto last = std::upper_bound(
        first, candidates.end(), live_opcode,
        [](std::uint32_t opcode, const RelocatableOpcodeCandidate& candidate) {
            return opcode < candidate.opcode;
        });
    for (auto it = first; it != last; ++it) {
        const auto& image = kRelocatableCodeImages[it->image_index];
        const std::uint32_t offset = it->entry->addr - *image.origin;
        const std::uint32_t base = pc - offset;
        const RelocatableBaseHint hint =
            g_relocatable_base_hint[it->image_index];
        if (hint.valid && base == hint.base) continue;
        if (attempt(it->image_index, base, it->entry->fn, false)) return 1;
    }
    // Images without immutable ROM backing cannot use the opcode index. Keep
    // the old exhaustive discovery path for them; identity hashing remains the
    // authority and current ROM-backed images pay no cost here.
    static const bool has_unbacked_image = [] {
        for (const auto& image : kRelocatableCodeImages) {
            if (image.rom_start == 0u) return true;
        }
        return false;
    }();
    if (has_unbacked_image) {
        for (std::size_t index = 0; index < kRelocatableCodeImages.size();
             ++index) {
            const auto& image = kRelocatableCodeImages[index];
            if (thumb != image.thumb || image.rom_start != 0u) continue;
            const std::uint32_t origin = *image.origin;
            const std::uint32_t size = *image.size;
            for (unsigned i = 0; i < *image.dispatch_table_len; ++i) {
                const DispatchEntry& entry = image.dispatch_table[i];
                const std::uint32_t offset = entry.addr - origin;
                if (entry.thumb != thumb || offset >= size) continue;
                const std::uint32_t base = pc - offset;
                const RelocatableBaseHint hint =
                    g_relocatable_base_hint[index];
                if (hint.valid && base == hint.base) continue;
                if (attempt(index, base, entry.fn, false)) return 1;
            }
        }
    }
    return 0;
}

void (*lookup_variant(const TransientCodeImage& image,
                      std::uint32_t pc, int thumb))(void) {
    return lookup_entry(image.dispatch_table, *image.dispatch_table_len, pc,
                        thumb);
}

// Every abort below ends a strict-static run, so the ring of recent branch,
// dispatch and store events is the only remaining evidence about which writer
// installed the bytes at the failing PC. GBARECOMP_TRACE_DUMP_DEPTH tunes it.
void dump_recent_trace() {
    std::uint32_t depth = 160u;
    if (const char* env = std::getenv("GBARECOMP_TRACE_DUMP_DEPTH")) {
        const unsigned long parsed = std::strtoul(env, nullptr, 0);
        if (parsed != 0ul) depth = static_cast<std::uint32_t>(parsed);
    }
    runtime_trace_dump_recent(depth);
}

// --- RAM code the game assembles at runtime ------------------------------
//
// Not every RAM code image is a copy of something. Golden Sun's sprite
// blitter is ASSEMBLED at runtime: a ROM template is used as a skeleton and
// the generator patches immediates, fills branch displacements and omits
// instructions the current parameters do not need. The result is a coherent
// ARM routine that is byte-identical to no ROM range — the live image at
// 0x03006220 during the first scripted fight shares only its 3-word prologue
// with the ROM template at 0x080EDCC4, then diverges by construction (measured
// word alignment: 46 of 97 template words survive, the rest are patched or
// dropped).
//
// No static corpus can cover that: the generated variant depends on runtime
// parameters, so the identity registry legitimately has nothing to match. The
// registry aborting on it is a DEVELOPMENT guard that was right while every
// known RAM image was a copy, and wrong for generated code.
//
// Policy, per AGENTS.md rule 6:
//   * strict-static run  -> abort exactly as before. A strict-static claim
//                           must never absorb a dynamically executed PC, and
//                           the abort's trace ring is the evidence.
//   * ordinary run       -> log loudly once per PC, count it, and fall
//                           through to the ordinary dispatch path, which
//                           tries the healed-overlay tier and then bridges
//                           through the interpreter. Reported at exit so a
//                           run that used this path can never be quoted as
//                           fully static.
// The healed-overlay tier and the interpreter bridge, both from the runtime.
// Routing an unexplained RAM PC through these EXPLICITLY (rather than
// returning 0 and letting runtime_dispatch continue) is deliberate: the main
// dispatch table does contain RAM addresses from declared code copies, and a
// PC whose live bytes match no registered identity must never reach a static
// translation built from different bytes. Silently running the wrong
// translation is worse than the abort this replaces.
extern "C" int overlay_try_dispatch(std::uint32_t pc, int thumb);
extern "C" void runtime_dispatch_miss(std::uint32_t target_pc);

gsr::blitter_shadow::Observer g_blitter_shadow;

// GSR_BLITTER_SHADOW, if explicitly set, wins outright. Otherwise falls back
// to the config UI's "Additional debug logging" toggle (default OFF).
bool blitter_shadow_on() {
    static const bool on = [] {
        const char* e = std::getenv("GSR_BLITTER_SHADOW");
        return e ? (e[0] != '\0' && e[0] != '0')
                 : (gsr_additional_debug_logging() != 0);
    }();
    return on;
}

void report_blitter_shadow() {
    if (!blitter_shadow_on()) return;
    std::fprintf(stderr,
        "GoldenSunRecomp: [blitter-shadow] observer_only=1 "
        "canonical_guest_always_runs=1 "
        "evictions=%llu\n",
        static_cast<unsigned long long>(g_blitter_shadow.evictions()));
    for (const auto& s : g_blitter_shadow.slots()) {
        if (!s.used) continue;
        std::fprintf(stderr,
            "  slot=%u builders=%llu allocator_calls=%llu bytes=%u "
            "descriptor_changes=%llu arm_dispatches=%llu variants=%zu "
            "variant_overflow=%llu wrong_mode=%llu entry_changes=%llu\n",
            s.slot, static_cast<unsigned long long>(s.builder_calls),
            static_cast<unsigned long long>(s.allocator_calls),
            s.requested_bytes,
            static_cast<unsigned long long>(s.descriptor_changes),
            static_cast<unsigned long long>(s.arm_dispatches),
            s.fingerprint_count,
            static_cast<unsigned long long>(s.fingerprint_overflow),
            static_cast<unsigned long long>(s.wrong_mode_dispatches),
            static_cast<unsigned long long>(s.entry_changes));
    }
}

void blitter_function_entry(std::uint32_t entry_pc) {
    if (!blitter_shadow_on()) return;
    static bool report_armed = false;
    if (!report_armed) {
        report_armed = true;
        std::atexit(report_blitter_shadow);
    }
    if (entry_pc == gsr::blitter_shadow::kBuilderEntry) {
        g_blitter_shadow.builder_entry(
            g_cpu.R[0], g_cpu.R[1], g_cpu.R[2], g_cpu.R[3],
            bus_read_u32(g_cpu.R[13]));
    } else if (entry_pc == gsr::blitter_shadow::kAllocatorEntry) {
        g_blitter_shadow.allocator_entry(g_cpu.R[0], g_cpu.R[1]);
    }
}

void blitter_shadow_dispatch(std::uint32_t pc, int thumb) {
    if (!blitter_shadow_on()) return;
    for (const auto& s : g_blitter_shadow.slots()) {
        if (!s.used || s.requested_bytes == 0 ||
            s.requested_bytes > 0x2000u) continue;
        std::uint32_t slot_addr = 0;
        if (!gsr::blitter_shadow::Observer::slot_address(s.slot, &slot_addr))
            continue;
        const std::uint32_t entry = bus_read_u32(slot_addr) & ~1u;
        if (entry < 0x03000000u || entry >= 0x03008000u ||
            static_cast<std::uint64_t>(entry) + s.requested_bytes >
                0x03008000ull ||
            pc < entry || static_cast<std::uint64_t>(pc) >=
                static_cast<std::uint64_t>(entry) + s.requested_bytes) {
            continue;
        }
        std::uint32_t hash = 2166136261u;
        for (std::uint32_t i = 0; i < s.requested_bytes; ++i) {
            hash = (hash ^ bus_read_u8(entry + i)) * 16777619u;
        }
        g_blitter_shadow.generated_dispatch(pc, thumb != 0, entry, hash,
                                             s.slot);
        return;
    }
}

bool strict_static_run() {
    static const bool strict = [] {
        const char* env = std::getenv("GBARECOMP_STRICT_STATIC");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return strict;
}

std::vector<std::uint32_t> g_dynamic_ram_pcs;
unsigned long long g_dynamic_ram_dispatches = 0;

// A verified transient image is safe to reuse until one of its RAM pages
// changes. The global generation is retained for diagnostics, but using it as
// the dispatch-cache key made an unrelated overlay write flush every hot PC.
enum class VerifiedRamAction : std::uint8_t { Static, Native };
constexpr std::size_t kNoVerifiedIdentity =
    static_cast<std::size_t>(-1);
struct VerifiedRamCacheEntry {
    std::size_t identity_index = kNoVerifiedIdentity;
    VerifiedRamAction action = VerifiedRamAction::Static;
    void (*fn)(void) = nullptr;
};
std::unordered_map<std::uint64_t, VerifiedRamCacheEntry>
    g_verified_ram_cache;

// Hash each reviewed image once per RAM-code generation, rather than once
// per PC inside that image. A fight visits many interior PCs, so a PC-only
// cache still repeated the same SHA-1 over and over.
struct VerifiedIdentityCacheEntry {
    bool valid = false;
    bool matched = false;
    // True once this candidate has matched its pinned identity at least once.
    // Overlay .text is immutable; later page writes can therefore reject a
    // known candidate with a few sentinels instead of hashing an unrelated
    // overlay. Unknown candidates still take the conservative full hash path.
    bool known_snapshot = false;
    // A candidate can span several 4 KiB pages. Recheck the bytes only when
    // one of those page generations changes.
    std::array<unsigned int, 64> page_epochs{};
    std::array<std::uint32_t, 4> sparse_words{};
    std::vector<std::uint8_t> snapshot;
};
std::array<VerifiedIdentityCacheEntry, kTransientCodeImages.size()>
    g_verified_identity_cache{};
unsigned long long g_verified_identity_hashes = 0;
unsigned long long g_verified_identity_cache_hits = 0;
unsigned long long g_verified_identity_content_hits = 0;
unsigned long long g_verified_identity_invalidations = 0;
std::array<unsigned long long, kTransientCodeImages.size()>
    g_verified_identity_hash_counts{};

std::uint64_t verified_ram_key(std::uint32_t pc, int thumb) {
    return (static_cast<std::uint64_t>(pc & ~1u) << 1) |
           static_cast<std::uint64_t>(thumb ? 1 : 0);
}

unsigned int ram_code_page_epoch(std::uint32_t addr) {
    const std::uint32_t region = addr >> 24;
    if (region == 0x03u)
        return g_ram_code_page_epoch_iwram[(addr & 0x00007FFFu) >> 12];
    if (region == 0x02u)
        return g_ram_code_page_epoch_ewram[(addr & 0x0003FFFFu) >> 12];
    return 0;
}

bool verified_identity_pages_current(std::size_t index) {
    if (index >= kTransientCodeImages.size()) return false;
    const auto& candidate = kTransientCodeImages[index];
    const auto& identity = g_verified_identity_cache[index];
    if (!identity.valid || candidate.end <= candidate.start) return false;
    const std::uint32_t region = candidate.start >> 24;
    const std::uint32_t mask = region == 0x03u ? 0x00007FFFu : 0x0003FFFFu;
    const std::uint32_t first = (candidate.start & mask) >> 12;
    const std::uint32_t last = ((candidate.end - 1u) & mask) >> 12;
    for (std::uint32_t page = first; page <= last && page < 64u; ++page) {
        const std::uint32_t page_addr =
            (candidate.start & ~mask) | (page << 12);
        if (identity.page_epochs[page] != ram_code_page_epoch(page_addr))
            return false;
    }
    return true;
}

void save_verified_identity_page_epochs(std::size_t index) {
    const auto& candidate = kTransientCodeImages[index];
    auto& identity = g_verified_identity_cache[index];
    identity.page_epochs.fill(0);
    if (candidate.end <= candidate.start) return;
    const std::uint32_t region = candidate.start >> 24;
    const std::uint32_t mask = region == 0x03u ? 0x00007FFFu : 0x0003FFFFu;
    const std::uint32_t first = (candidate.start & mask) >> 12;
    const std::uint32_t last = ((candidate.end - 1u) & mask) >> 12;
    for (std::uint32_t page = first; page <= last && page < 64u; ++page) {
        const std::uint32_t page_addr =
            (candidate.start & ~mask) | (page << 12);
        identity.page_epochs[page] = ram_code_page_epoch(page_addr);
    }
}

std::array<std::uint32_t, 4> identity_sparse_words(
    const TransientCodeImage& candidate,
    const std::vector<std::uint8_t>* image = nullptr) {
    std::array<std::uint32_t, 4> words{};
    const std::uint32_t size = candidate.end - candidate.start;
    std::array<std::uint32_t, 4> offsets{
        0u,
        size > 4u ? (size / 3u) & ~3u : 0u,
        size > 4u ? (2u * size / 3u) & ~3u : 0u,
        size > 4u ? (size - 4u) & ~3u : 0u,
    };
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        const std::uint32_t off = offsets[i];
        if (off + 4u > size) continue;
        if (image) {
            const auto& bytes = *image;
            words[i] = static_cast<std::uint32_t>(bytes[off]) |
                       (static_cast<std::uint32_t>(bytes[off + 1u]) << 8) |
                       (static_cast<std::uint32_t>(bytes[off + 2u]) << 16) |
                       (static_cast<std::uint32_t>(bytes[off + 3u]) << 24);
        } else {
            words[i] = bus_read_u32(candidate.start + off);
        }
    }
    return words;
}

bool identity_sparse_matches(const TransientCodeImage& candidate,
                             const VerifiedIdentityCacheEntry& identity) {
    return identity_sparse_words(candidate) == identity.sparse_words;
}

// Compressed overlay .text is immutable while resident. The synthetic DC8
// templates use the same `rom_start == 0` marker but are deliberately rewritten
// by the guest's template writer, so a sparse mismatch can be a transient
// variant change and must not permanently reject a valid template identity.
bool identity_sparse_fast_reject_safe(const TransientCodeImage& candidate) {
    return candidate.rom_start == 0u && candidate.name != nullptr &&
           std::strncmp(candidate.name, "overlay_", 8) == 0;
}

// Revalidate the identity already attached to a hot PC before falling back to
// the full candidate scan. RAM page epochs are deliberately conservative: a
// data write elsewhere in the same page invalidates the PC cache too. In the
// common case the cached overlay is still resident, so checking that one image
// avoids hashing every other overlay that shares the staging slot.
bool refresh_verified_identity(std::size_t candidate_index) {
    if (candidate_index >= kTransientCodeImages.size()) return false;
    const auto& candidate = kTransientCodeImages[candidate_index];
    auto& identity = g_verified_identity_cache[candidate_index];
    if (!identity.valid) return false;
    if (verified_identity_pages_current(candidate_index))
        return identity.matched;

    ++g_verified_identity_invalidations;
    if (identity.known_snapshot && identity_sparse_fast_reject_safe(candidate) &&
        !identity_sparse_matches(candidate, identity)) {
        identity.matched = false;
        save_verified_identity_page_epochs(candidate_index);
        return false;
    }

    bool changed = identity.snapshot.size() != candidate.end - candidate.start;
    if (!changed) {
        for (std::uint32_t offset = 0;
             offset < candidate.end - candidate.start; ++offset) {
            if (bus_read_u8(candidate.start + offset) !=
                identity.snapshot[offset]) {
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        ++g_verified_identity_hashes;
        ++g_verified_identity_hash_counts[candidate_index];
        std::vector<std::uint8_t> image(candidate.end - candidate.start);
        for (std::uint32_t offset = 0; offset < image.size(); ++offset)
            image[offset] = bus_read_u8(candidate.start + offset);
        identity.matched =
            gba::sha1(image.data(), image.size()).hex() == candidate.sha1;
        identity.snapshot = std::move(image);
        identity.known_snapshot = identity.matched && candidate.rom_start == 0u;
        if (identity.known_snapshot)
            identity.sparse_words = identity_sparse_words(
                candidate, &identity.snapshot);
    } else {
        ++g_verified_identity_content_hits;
    }
    save_verified_identity_page_epochs(candidate_index);
    return identity.matched;
}

// DMA and a few BIOS routines write RAM directly rather than through the
// generated bus helpers.  Those stores cannot advance the page-generation
// counters above, so a cached RAM identity also checks the two words at the
// current PC before dispatching native code.  This is cheap (two RAM reads)
// and catches the important case where a shared overlay slot is replaced while
// its page number stays unchanged.
bool verified_identity_local_words_current(std::size_t candidate_index,
                                           std::uint32_t pc) {
    if (candidate_index >= kTransientCodeImages.size()) return false;
    const auto& candidate = kTransientCodeImages[candidate_index];
    const auto& identity = g_verified_identity_cache[candidate_index];
    if (!identity.valid || !identity.matched || pc < candidate.start ||
        pc >= candidate.end || identity.snapshot.empty()) return false;
    const std::uint32_t size = candidate.end - candidate.start;
    const std::uint32_t offset = (pc - candidate.start) & ~3u;
    bool checked = false;
    for (unsigned word = 0; word < 2u; ++word) {
        const std::uint32_t off = offset + word * 4u;
        if (off >= size || off >= identity.snapshot.size()) break;
        const std::uint32_t available =
            std::min<std::uint32_t>(4u, size - off);
        if (available == 4u && off + 4u <= identity.snapshot.size()) {
            const std::uint32_t live = bus_read_u32(candidate.start + off);
            const auto& b = identity.snapshot;
            const std::uint32_t saved = static_cast<std::uint32_t>(b[off]) |
                                         (static_cast<std::uint32_t>(b[off + 1u]) << 8) |
                                         (static_cast<std::uint32_t>(b[off + 2u]) << 16) |
                                         (static_cast<std::uint32_t>(b[off + 3u]) << 24);
            if (live != saved) return false;
        } else {
            for (std::uint32_t byte = 0; byte < available; ++byte) {
                if (bus_read_u8(candidate.start + off + byte) !=
                    identity.snapshot[off + byte]) return false;
            }
        }
        checked = true;
    }
    return checked;
}

// A PC already reported as dynamic. Generated code is hot — the blitter runs
// per sprite — so everything below this point must stay silent after the
// first report, which is also why the expensive candidate-by-candidate
// diagnostic only runs the first time a PC is seen.
bool dynamic_ram_pc_reported(std::uint32_t pc) {
    for (std::uint32_t seen : g_dynamic_ram_pcs) {
        if (seen == pc) return true;
    }
    return false;
}

// ── TEMPORARY dynamic-RAM-code cost/version probe (GSR_DYNAMIC_RAM_PROBE=1) ──
// Battle-lag investigation: the ~44 IWRAM PCs the runtime reports as
// "DYNAMIC RAM CODE" (no registered identity — almost certainly Golden Sun's
// own runtime-generated sprite-blit unrolls, see docs/OVERLAYS.md and
// BATTLE SLOW.txt) always fall through dispatch_dynamic_ram. The open
// question this probe answers: is the live code at each such PC reused
// across a battle (a small, bounded set of byte-versions per PC — a
// hash-keyed native cache would close it) or effectively unique per call
// (only a pattern-recognizing blitter would help)? Off by default: one
// getenv() + one bool check per dispatch_dynamic_ram() call when disabled,
// matching the existing GBARECOMP_COST_PROBE idiom (see
// gbarecomp/src/runtime/runtime_bus_bridge.cpp). Diagnostic-only; intended
// for removal once the investigation report is written.
// GSR_DYNAMIC_RAM_PROBE, if explicitly set, wins outright. Otherwise falls
// back to the config UI's "Additional debug logging" toggle (default OFF).
bool dynamic_ram_probe_on() {
    static const bool on = [] {
        const char* e = std::getenv("GSR_DYNAMIC_RAM_PROBE");
        return e ? !(e[0] == '0' && e[1] == '\0')
                 : (gsr_additional_debug_logging() != 0);
    }();
    return on;
}

// Bytes hashed per PC to fingerprint the live code version. This is a
// diagnostic fingerprint window, NOT a proven function boundary — these PCs
// are exactly the ones with no registered identity/size in
// kTransientCodeImages, so there is no reviewed size to hash instead.
// Clamped to the containing 32 KiB IWRAM / 256 KiB EWRAM region so a PC near
// the end of RAM never reads past the region. Override with
// GSR_DYNAMIC_RAM_PROBE_WINDOW (bytes) if 64 hides or blends distinct
// versions.
std::uint32_t dynamic_ram_probe_window() {
    static const std::uint32_t window = [] {
        std::uint32_t w = 64u;
        if (const char* e = std::getenv("GSR_DYNAMIC_RAM_PROBE_WINDOW")) {
            const unsigned long parsed = std::strtoul(e, nullptr, 0);
            if (parsed > 0) w = static_cast<std::uint32_t>(parsed);
        }
        return w;
    }();
    return window;
}

struct DynamicRamProbeEntry {
    std::uint64_t executions = 0;      // times dispatch_dynamic_ram(pc) ran
    std::uint64_t overlay_hits = 0;    // of those, served natively by overlay_try_dispatch
    std::uint64_t interp_insns = 0;    // guest instructions interpreted for pc
    std::uint64_t interp_ns = 0;       // wall time inside runtime_dispatch_miss
    std::uint64_t total_ns = 0;        // wall time for the whole dispatch call
    std::vector<std::uint32_t> distinct_hashes;  // CRC32s of the live window seen
};
std::unordered_map<std::uint32_t, DynamicRamProbeEntry> g_dynamic_ram_probe;

void report_dynamic_ram_probe() {
    if (g_dynamic_ram_probe.empty()) return;
    std::vector<std::pair<std::uint32_t, const DynamicRamProbeEntry*>> rows;
    rows.reserve(g_dynamic_ram_probe.size());
    for (const auto& kv : g_dynamic_ram_probe) rows.emplace_back(kv.first, &kv.second);
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.second->total_ns > b.second->total_ns;
    });
    unsigned long long total_exec = 0, total_insns = 0, total_overlay_hits = 0;
    unsigned long long total_ns = 0, total_interp_ns = 0;
    for (const auto& r : rows) {
        total_exec += r.second->executions;
        total_overlay_hits += r.second->overlay_hits;
        total_insns += r.second->interp_insns;
        total_ns += r.second->total_ns;
        total_interp_ns += r.second->interp_ns;
    }
    std::fprintf(stderr,
        "GoldenSunRecomp: [dynamic-ram-probe] window=%u bytes pcs=%zu "
        "total_executions=%llu overlay_native_hits=%llu (%.1f%%) "
        "total_interp_insns=%llu total_ms=%.3f interp_ms=%.3f "
        "interpreter_throughput=%.1f Kinsn/s\n",
        dynamic_ram_probe_window(), rows.size(), total_exec, total_overlay_hits,
        total_exec > 0 ? (100.0 * total_overlay_hits) / total_exec : 0.0,
        total_insns, total_ns / 1e6, total_interp_ns / 1e6,
        total_interp_ns > 0
            ? (static_cast<double>(total_insns) * 1e6) /
                  static_cast<double>(total_interp_ns)
            : 0.0);
    std::fprintf(stderr, "  %-10s %8s %12s %14s %14s %10s\n",
        "pc", "hashes", "executions", "overlay_hits", "interp_insns", "ms");
    for (const auto& r : rows) {
        std::fprintf(stderr, "  0x%08X %8zu %12llu %14llu %14llu %10.3f\n",
            r.first, r.second->distinct_hashes.size(),
            static_cast<unsigned long long>(r.second->executions),
            static_cast<unsigned long long>(r.second->overlay_hits),
            static_cast<unsigned long long>(r.second->interp_insns),
            r.second->total_ns / 1e6);
    }
}

void dynamic_ram_probe_record(std::uint32_t pc, std::uint64_t total_ns,
                              std::uint64_t interp_ns,
                              std::uint64_t interp_insns,
                              bool overlay_hit) {
    // IWRAM (0x03xxxxxx) is 32 KiB; EWRAM (0x02xxxxxx) is 256 KiB. Region
    // boundaries per GBA hardware memory map (docs/OVERLAYS.md), same
    // boundary math kTransientCodeImages/runtime_dispatch already use.
    const std::uint32_t region_end = (pc & 0xFF000000u) +
        ((pc >> 24) == 0x03u ? 0x00008000u : 0x00040000u);
    std::uint32_t len = dynamic_ram_probe_window();
    if (pc + len > region_end) len = region_end - pc;
    std::uint8_t buf[256];
    if (len > sizeof(buf)) len = sizeof(buf);
    for (std::uint32_t i = 0; i < len; ++i) buf[i] = bus_read_u8(pc + i);
    const std::uint32_t hash = gba::crc32(buf, len);

    static bool atexit_armed = false;
    if (!atexit_armed) {
        atexit_armed = true;
        std::atexit(report_dynamic_ram_probe);
    }

    DynamicRamProbeEntry& e = g_dynamic_ram_probe[pc];
    ++e.executions;
    if (overlay_hit) ++e.overlay_hits;
    e.interp_insns += interp_insns;
    e.interp_ns += interp_ns;
    e.total_ns += total_ns;
    if (std::find(e.distinct_hashes.begin(), e.distinct_hashes.end(), hash) ==
        e.distinct_hashes.end()) {
        e.distinct_hashes.push_back(hash);
    }
}

// Run an unexplained RAM PC: healed native first, else the interpreter
// bridge (which also enqueues the heal keyed by the live bytes' CRC).
int dispatch_dynamic_ram(std::uint32_t pc, int thumb) {
    blitter_shadow_dispatch(pc, thumb);
    if (!dynamic_ram_probe_on()) {
        if (overlay_try_dispatch(pc, thumb)) return 1;
        runtime_dispatch_miss(pc | (thumb ? 1u : 0u));
        return 1;
    }
    const auto call_t0 = std::chrono::steady_clock::now();
    if (overlay_try_dispatch(pc, thumb)) {
        const auto call_t1 = std::chrono::steady_clock::now();
        dynamic_ram_probe_record(pc,
            static_cast<std::uint64_t>(std::chrono::duration_cast<
                std::chrono::nanoseconds>(call_t1 - call_t0).count()),
            /*interp_ns=*/0, /*interp_insns=*/0, /*overlay_hit=*/true);
        return 1;
    }
    const std::uint64_t insns_before = gbarecomp::self_heal_interpreted_insns();
    const auto interp_t0 = std::chrono::steady_clock::now();
    runtime_dispatch_miss(pc | (thumb ? 1u : 0u));
    const auto interp_t1 = std::chrono::steady_clock::now();
    const std::uint64_t insns_after = gbarecomp::self_heal_interpreted_insns();
    const std::uint64_t interp_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            interp_t1 - interp_t0).count());
    dynamic_ram_probe_record(pc, interp_ns, interp_ns,
                             insns_after - insns_before, /*overlay_hit=*/false);
    return 1;
}

// Fast path for a PC we have already explained: count it and run it without
// printing anything.
bool dynamic_ram_pc_repeat(std::uint32_t pc) {
    if (strict_static_run()) return false;
    if (!dynamic_ram_pc_reported(pc)) return false;
    ++g_dynamic_ram_dispatches;
    return true;
}

void report_dynamic_ram_summary() {
    if (g_dynamic_ram_pcs.empty()) return;
    std::fprintf(stderr,
        "GoldenSunRecomp: dynamic_ram_pcs=%zu dynamic_ram_dispatches=%llu — "
        "RAM code with no registered identity ran through the interpreter/heal "
        "tier. This run is NOT fully static.\n",
        g_dynamic_ram_pcs.size(), g_dynamic_ram_dispatches);
    for (std::uint32_t pc : g_dynamic_ram_pcs) {
        std::fprintf(stderr, "  dynamic_ram_pc=0x%08X\n", pc);
    }
}

// Returns true when the caller should fall through to the ordinary dispatch
// path instead of aborting.
bool allow_dynamic_ram_pc(const char* reason, std::uint32_t pc, int thumb) {
    if (strict_static_run()) return false;
    ++g_dynamic_ram_dispatches;
    if (dynamic_ram_pc_reported(pc)) return true;
    if (g_dynamic_ram_pcs.empty()) std::atexit(report_dynamic_ram_summary);
    g_dynamic_ram_pcs.push_back(pc);
    std::fprintf(stderr,
        "GoldenSunRecomp: DYNAMIC RAM CODE at 0x%08X (%s) — %s. No registered "
        "identity explains these bytes; executing through the heal/interpreter "
        "tier. NOT fully static. A strict-static run aborts here instead.\n",
        pc, thumb ? "thumb" : "arm", reason);
    return true;
}

// Bounds nested verified-RAM native dispatch on this thread. A cached
// native entry that re-dispatches its own pc before returning (a
// generated-code bug, not ordinary nested guest calls) would otherwise
// recurse through runtime_dispatch -> verified_ram_dispatch -> fn()
// forever and blow the host stack. A fixed-capacity array plus a linear
// scan is used instead of a std::set: this path is hit ~100,000 times per
// fight, and per-dispatch hashing/allocation was measured to make that
// crawl (see the comment above verified_ram_dispatch).
constexpr std::size_t kVerifiedRamActiveCapacity = 64;
thread_local std::array<std::uint64_t, kVerifiedRamActiveCapacity>
    g_verified_ram_active_keys{};
thread_local std::size_t g_verified_ram_active_depth = 0;
thread_local std::vector<std::uint32_t> g_verified_ram_self_dispatch_reported;
thread_local std::vector<std::uint32_t> g_verified_ram_depth_cap_reported;

bool verified_ram_pc_in(const std::vector<std::uint32_t>& reported,
                        std::uint32_t pc) {
    for (std::uint32_t seen : reported) {
        if (seen == pc) return true;
    }
    return false;
}

// RAII guard tracking which verified-RAM keys are currently being invoked
// on this thread, so a native fn that re-dispatches its own pc (or a
// A->B->A cycle) can be detected and bypassed instead of recursing without
// bound. The guarded key is always removed on exit, including on an
// exception unwind out of the native fn.
struct VerifiedRamActiveGuard {
    bool pushed = false;
    bool self_dispatch = false;
    bool depth_exceeded = false;

    explicit VerifiedRamActiveGuard(std::uint64_t key) {
        for (std::size_t i = 0; i < g_verified_ram_active_depth; ++i) {
            if (g_verified_ram_active_keys[i] == key) {
                self_dispatch = true;
                return;
            }
        }
        if (g_verified_ram_active_depth >= kVerifiedRamActiveCapacity) {
            depth_exceeded = true;
            return;
        }
        g_verified_ram_active_keys[g_verified_ram_active_depth++] = key;
        pushed = true;
    }
    ~VerifiedRamActiveGuard() {
        if (pushed) --g_verified_ram_active_depth;
    }
    VerifiedRamActiveGuard(const VerifiedRamActiveGuard&) = delete;
    VerifiedRamActiveGuard& operator=(const VerifiedRamActiveGuard&) = delete;
};

int verified_ram_dispatch(std::uint32_t pc, int thumb) {
    // A PC already classified as generated code short-circuits everything
    // below. This must come FIRST: the identity scan re-hashes the whole
    // containing candidate image (SHA-1 over ~1.2 KB) and then every
    // relocatable image at this base (~2.5 KB more) before it can conclude
    // what it already concluded the first time. Generated code is HOT — a
    // single fight dispatches these ~100,000 times — so paying that scan per
    // dispatch cost hundreds of MB of hashing and made the fight crawl.
    if (dynamic_ram_pc_repeat(pc)) return dispatch_dynamic_ram(pc, thumb);

    const std::uint64_t key = verified_ram_key(pc, thumb);

    VerifiedRamActiveGuard active_guard(key);
    if (active_guard.self_dispatch) {
        // A verified-RAM native entry called back into runtime_dispatch for
        // the exact same pc/thumb it is currently executing, before
        // returning. Left alone this recurses through
        // runtime_dispatch -> verified_ram_dispatch -> fn() without bound
        // and overflows the host stack (this is the fix for that crash).
        // Evict the cache entry and fall through to the ordinary
        // static/overlay/interpreter path instead.
        const auto self_cached = g_verified_ram_cache.find(key);
        if (!verified_ram_pc_in(g_verified_ram_self_dispatch_reported, pc)) {
            g_verified_ram_self_dispatch_reported.push_back(pc);
            std::fprintf(stderr,
                "GoldenSunRecomp: SELF-DISPATCH RECURSION at 0x%08X (%s) "
                "identity_index=%zu — a verified-RAM native entry "
                "re-dispatched its own pc before returning. Bypassing this "
                "entry once and falling through to the ordinary dispatch "
                "path instead of recursing without bound.\n",
                pc, thumb ? "thumb" : "arm",
                self_cached != g_verified_ram_cache.end()
                    ? self_cached->second.identity_index
                    : kNoVerifiedIdentity);
        }
        if (self_cached != g_verified_ram_cache.end())
            g_verified_ram_cache.erase(self_cached);
        return 0;
    }
    if (active_guard.depth_exceeded) {
        // Not a same-pc self-dispatch, but this thread's nested
        // verified-RAM native dispatch depth ran past the cap — e.g. an
        // A->B->A cycle across distinct pcs. Refuse to recurse further and
        // fall through to the ordinary dispatch path.
        if (!verified_ram_pc_in(g_verified_ram_depth_cap_reported, pc)) {
            g_verified_ram_depth_cap_reported.push_back(pc);
            std::fprintf(stderr,
                "GoldenSunRecomp: VERIFIED-RAM DISPATCH DEPTH CAP (%zu) hit "
                "at pc=0x%08X (%s) — refusing to nest further and falling "
                "through to the ordinary dispatch path.\n",
                kVerifiedRamActiveCapacity, pc, thumb ? "thumb" : "arm");
        }
        return 0;
    }

    const auto cached = g_verified_ram_cache.find(key);
    if (cached != g_verified_ram_cache.end()) {
        const bool local_words_current =
            cached->second.identity_index < kTransientCodeImages.size() &&
            verified_identity_local_words_current(cached->second.identity_index,
                                                  pc);
        if (verified_identity_pages_current(cached->second.identity_index) &&
            local_words_current) {
            if (cached->second.action == VerifiedRamAction::Native &&
                cached->second.fn != nullptr) {
                cached->second.fn();
                return 1;
            }
            // A fixed identity owns the ordinary static dispatch entry.
            return 0;
        }
        // The page epoch may have changed because of an unrelated write in
        // the same RAM page. Revalidate the identity that already won for
        // this PC first; scanning all overlapping overlays would otherwise
        // pay several full SHA-1s before reaching the same winner.
        const std::size_t identity_index = cached->second.identity_index;
        if (identity_index < kTransientCodeImages.size()) {
            const auto& candidate = kTransientCodeImages[identity_index];
            bool identity_refreshed = false;
            if (!local_words_current) {
                // A DMA/direct store changed the code near this PC without
                // advancing the page epoch.  Force the full identity scan;
                // refresh_verified_identity intentionally trusts a current
                // epoch and therefore cannot be used for this case.
                auto& identity = g_verified_identity_cache[identity_index];
                identity.valid = false;
                identity.matched = false;
                identity.snapshot.clear();
            } else {
                identity_refreshed = refresh_verified_identity(identity_index);
            }
            if (pc >= candidate.start && pc < candidate.end &&
                thumb == candidate.thumb && identity_refreshed) {
                if (cached->second.action == VerifiedRamAction::Native &&
                    cached->second.fn != nullptr) {
                    cached->second.fn();
                    return 1;
                }
                return 0;
            }
        }
        g_verified_ram_cache.erase(cached);
    }

    const auto cache_verified = [&](VerifiedRamAction action,
                                    void (*fn)(void),
                                    std::size_t identity_index) {
        g_verified_ram_cache[key] = {identity_index, action, fn};
    };

    bool covered = false;
    bool mode_matched = false;
    // This used to be a local std::array. Proactive registration grows the
    // candidate set to ~100 images; several KB per recursively nested guest
    // call exhausted the Windows host stack. The runtime is single-threaded,
    // and a successful nested dispatch makes its caller return immediately,
    // so reusable thread-local diagnostics preserve behavior without growing
    // each host call frame.
    thread_local std::array<std::string, kTransientCodeImages.size()>
        observed_sha1{};
    thread_local std::vector<std::size_t> observed_indices;
    for (const std::size_t index : observed_indices) observed_sha1[index].clear();
    observed_indices.clear();
    for (std::size_t candidate_index = 0;
         candidate_index < kTransientCodeImages.size(); ++candidate_index) {
        const auto& candidate = kTransientCodeImages[candidate_index];
        if (pc < candidate.start || pc >= candidate.end) continue;
        covered = true;
        if (thumb != candidate.thumb) continue;
        mode_matched = true;

        auto& identity = g_verified_identity_cache[candidate_index];
        if (!identity.valid || !verified_identity_pages_current(candidate_index)) {
            if (identity.valid) ++g_verified_identity_invalidations;
            // Registered compressed overlays are immutable in their .text
            // extent. Once one has matched, four spread-out words are enough
            // to reject a different overlay image without paying SHA-1. A
            // sparse match remains conservative: it falls through to the
            // existing full comparison and hash.
            if (identity.valid && identity.known_snapshot &&
                identity_sparse_fast_reject_safe(candidate) &&
                !identity_sparse_matches(candidate, identity)) {
                identity.matched = false;
                save_verified_identity_page_epochs(candidate_index);
                continue;
            }
            bool changed = !identity.valid ||
                           identity.snapshot.size() != candidate.end - candidate.start;
            if (!changed) {
                for (std::uint32_t offset = 0;
                     offset < candidate.end - candidate.start; ++offset) {
                    if (bus_read_u8(candidate.start + offset) !=
                        identity.snapshot[offset]) {
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) {
                ++g_verified_identity_hashes;
                ++g_verified_identity_hash_counts[candidate_index];
                std::vector<std::uint8_t> image(candidate.end - candidate.start);
                for (std::uint32_t offset = 0; offset < image.size(); ++offset)
                    image[offset] = bus_read_u8(candidate.start + offset);
                const std::string actual =
                    gba::sha1(image.data(), image.size()).hex();
                observed_sha1[candidate_index] = actual;
                observed_indices.push_back(candidate_index);
                identity.snapshot = std::move(image);
                identity.matched = (actual == candidate.sha1);
                identity.known_snapshot = identity.matched &&
                    candidate.rom_start == 0u;
                if (identity.known_snapshot)
                    identity.sparse_words = identity_sparse_words(
                        candidate, &identity.snapshot);
            } else {
                ++g_verified_identity_content_hits;
            }
            save_verified_identity_page_epochs(candidate_index);
            identity.valid = true;
        } else ++g_verified_identity_cache_hits;
        if (!identity.matched) continue;

        if (candidate.dispatch_table == nullptr) {
            // This identity owns the fixed AOT entry in kDispatchTable.
            cache_verified(VerifiedRamAction::Static, nullptr, candidate_index);
            return 0;
        }
        if (void (*fn)(void) = lookup_variant(candidate, pc, thumb)) {
            cache_verified(VerifiedRamAction::Native, fn, candidate_index);
            fn();
            return 1;
        }
        if (dynamic_ram_pc_repeat(pc)) return dispatch_dynamic_ram(pc, thumb);
        std::fprintf(stderr,
            "GoldenSunRecomp: verified transient image %s has no AOT entry "
            "for 0x%08X\n",
            candidate.name, pc);
        if (allow_dynamic_ram_pc("verified image is short an AOT entry", pc,
                                 thumb)) {
            return dispatch_dynamic_ram(pc, thumb);
        }
        dump_recent_trace();
        std::abort();
    }

    // No fixed-address image explains this PC. Try the position-independent
    // ones, which are keyed by identity rather than by address: a pooled
    // routine at a base nobody has ever recorded still dispatches, provided
    // its live bytes hash to a registered image. This runs before every abort
    // path below, including the mode mismatch — a stale ARM registration can
    // overlap a THUMB thunk installed at the same stack address later.
    if (try_relocatable_dispatch(pc, thumb)) return 1;

    if (!covered) return 0;
    if (dynamic_ram_pc_repeat(pc)) return dispatch_dynamic_ram(pc, thumb);
    if (!mode_matched) {
        std::fprintf(stderr,
            "GoldenSunRecomp: transient code mode mismatch at 0x%08X\n", pc);
        if (allow_dynamic_ram_pc("registered image is the other mode", pc,
                                 thumb)) {
            return dispatch_dynamic_ram(pc, thumb);
        }
        dump_recent_trace();
        std::abort();
    }
    std::fprintf(stderr,
        "GoldenSunRecomp: unknown transient code identity at 0x%08X\n", pc);
    // A compressed overlay has no linear ROM source to diff against, so the
    // byte-level report below can say nothing about it. Dumping the live
    // window instead lets the image be matched offline against the pinned
    // decompressed `orig.bin` set without a TCP capture session.
    if (const char* dump_path = std::getenv("GBARECOMP_TRANSIENT_DUMP")) {
        std::uint32_t window_start = pc & ~0xFFFu;
        std::uint32_t window_end = window_start + 0x4000u;
        for (const auto& candidate : kTransientCodeImages) {
            if (pc < candidate.start || pc >= candidate.end) continue;
            if (candidate.start < window_start) window_start = candidate.start;
            if (candidate.end > window_end) window_end = candidate.end;
        }
        if (std::FILE* dump = std::fopen(dump_path, "wb")) {
            for (std::uint32_t addr = window_start; addr < window_end; ++addr) {
                std::fputc(bus_read_u8(addr), dump);
            }
            std::fclose(dump);
            std::fprintf(stderr,
                "  live window 0x%08X..0x%08X written to %s\n",
                window_start, window_end, dump_path);
        } else {
            std::fprintf(stderr,
                "  could not open GBARECOMP_TRANSIENT_DUMP path %s\n",
                dump_path);
        }
    }
    // The candidate-by-candidate byte report is evidence for a strict-static
    // abort, but the shared EWRAM overlay slot currently has scores of
    // candidates. Printing all of them during ordinary self-heal caused a
    // 200-line synchronous stderr burst on the first unknown identity. Keep
    // normal play concise; diagnostics can opt back into the full report.
    // GSR_TRANSIENT_VERBOSE, if explicitly set (even to an empty string,
    // matching prior behavior), wins outright. Otherwise falls back to the
    // config UI's "Additional debug logging" toggle (default OFF).
    const bool verbose_identity_report =
        strict_static_run() ||
        (std::getenv("GSR_TRANSIENT_VERBOSE") != nullptr
             ? true
             : gsr_additional_debug_logging() != 0);
    if (!verbose_identity_report && g_dynamic_ram_pcs.empty()) {
        std::fprintf(stderr,
            "  identity details suppressed for normal play (%zu fixed, %zu "
            "relocatable candidates); set GSR_TRANSIENT_VERBOSE=1 to dump\n",
            observed_indices.size(), kRelocatableCodeImages.size());
    }
    for (std::size_t candidate_index = 0;
         verbose_identity_report &&
         candidate_index < kTransientCodeImages.size(); ++candidate_index) {
        const auto& candidate = kTransientCodeImages[candidate_index];
        if (observed_sha1[candidate_index].empty()) continue;
        std::fprintf(stderr,
            "  candidate=%s range=0x%08X..0x%08X expected=%s actual=%s\n",
            candidate.name, candidate.start, candidate.end,
            candidate.sha1, observed_sha1[candidate_index].c_str());
        if (candidate.rom_start == 0) {
            std::fprintf(stderr,
                "    compressed image: no linear ROM byte comparison\n");
            continue;
        }
        unsigned differences = 0;
        for (std::uint32_t offset = 0;
             offset < candidate.end - candidate.start; ++offset) {
            const std::uint8_t live = bus_read_u8(candidate.start + offset);
            const std::uint8_t source = bus_read_u8(candidate.rom_start + offset);
            if (live == source) continue;
            if (differences < 8) {
                std::fprintf(stderr,
                    "    offset=0x%X live=0x%02X rom=0x%02X\n",
                    offset, live, source);
            }
            ++differences;
        }
        std::fprintf(stderr, "    differing_bytes=%u\n", differences);
        std::fprintf(stderr, "    live_prefix=");
        for (std::uint32_t offset = 0; offset < 32; ++offset) {
            std::fprintf(stderr, "%02x", bus_read_u8(candidate.start + offset));
        }
        std::fprintf(stderr, "\n");
    }
    // What the position-independent images saw. A relocatable routine has no
    // declared range to report, so report the base the PC would imply if this
    // PC were its entry — the overwhelmingly common case — and the identity
    // actually resident there.
    for (std::size_t image_index = 0;
         verbose_identity_report && image_index < kRelocatableCodeImages.size();
         ++image_index) {
        const auto& image = kRelocatableCodeImages[image_index];
        std::fprintf(stderr,
            "  relocatable=%s mode=%s size=0x%X expected=%s\n",
            image.name, image.thumb ? "thumb" : "arm", *image.size,
            image.sha1);
        if (pc >= 0x02000000u && pc + *image.size <= 0x04000000u) {
            std::fprintf(stderr, "    at_entry_base=0x%08X actual=%s\n", pc,
                         live_sha1(pc, *image.size).c_str());
        }
    }
    if (allow_dynamic_ram_pc("no registered identity matches the live bytes; "
                             "this is runtime-generated code",
                             pc, thumb)) {
        return dispatch_dynamic_ram(pc, thumb);
    }
    dump_recent_trace();
    std::abort();
}

void print_usage() {
    std::printf(
        "GoldenSunRecomp [--bios <path>] [--rom <path>] [game.toml]\n"
        "\n"
        "A canonical GBA BIOS and the Golden Sun USA/Europe ROM are required.\n"
        "Both are SHA-1 gated before guest code executes. The default is the\n"
        "faithful recompiled BIOS path and native 240x160 presentation.\n"
        "\n"
        "Expected ROM SHA-1: %s\n",
        kRomSha1);
}

}  // namespace

int main(int argc, char** argv) {
    // Must be the FIRST thing in main, before SDL init or any other
    // subsystem, so as much of the run as possible is covered. nullptr =
    // default to the directory this executable lives in.
    gbarecomp::crash_handler_install(nullptr);

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    gbarecomp::RunOptions options;
    options.builtin_game_name = "Golden Sun";
    options.builtin_rom_sha1 = kRomSha1;
    options.function_entry_observer = blitter_function_entry;
    options.max_view_width = 240;
    options.frame_interpolation_available = true;
    options.enhanced_timing_available = true;
    options.native_renderer_available = true;
    options.max_resize_view_width = 240;
    options.resize_driven_view = false;
    options.launcher_region = "USA/Europe";
    options.launcher_game_config = "game.toml";
    options.launcher_expose_widescreen = false;
    options.launcher_expose_adaptive_view = false;
    g_runtime_ram_dispatch_hook = verified_ram_dispatch;
    init_ram_code_page_masks();
    g_verified_ram_cache.clear();
    g_verified_identity_cache = {};
    g_relocatable_identity_cache = {};
    const int result = gbarecomp::run_game(argc, argv, options);
    if (std::getenv("GBARECOMP_RAM_CACHE_STATS")) {
        std::fprintf(stderr,
            "ram_cache_stats epoch=%llu hashes=%llu image_hits=%llu "
            "content_hits=%llu invalidations=%llu pc_entries=%zu\n",
            g_ram_write_epoch, g_verified_identity_hashes,
            g_verified_identity_cache_hits, g_verified_identity_content_hits,
            g_verified_identity_invalidations,
            g_verified_ram_cache.size());
        if (std::getenv("GBARECOMP_RAM_CACHE_DETAIL")) {
            for (std::size_t i = 0; i < kTransientCodeImages.size(); ++i) {
                if (g_verified_identity_hash_counts[i] == 0) continue;
                std::fprintf(stderr, "  ram_cache_image=%s hashes=%llu matched=%d known=%d\n",
                    kTransientCodeImages[i].name,
                    g_verified_identity_hash_counts[i],
                    g_verified_identity_cache[i].matched ? 1 : 0,
                    g_verified_identity_cache[i].known_snapshot ? 1 : 0);
            }
        }
        std::fprintf(stderr,
            "relocatable_cache_stats hashes=%llu cache_hits=%llu\n",
            g_relocatable_identity_hashes, g_relocatable_identity_cache_hits);
    }
    gbarecomp::crash_handler_mark_clean_exit();
    return result;
}
