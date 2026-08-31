#include <algorithm>
#include <array>
#include <atomic>
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
#include "gba_bus.h"
#include "gba_ppu.h"
#include "gba_vram_trace.h"
#include "recompiled.h"
#include "relocatable_identity.h"
#include "relocatable_writer_policy.h"
#include "player_speed_cheat.h"
#include "runtime.h"
#include "runtime_bus_bridge.h"
#include "runtime_arm.h"
#include "overlay_loader.h"
#include "self_heal.h"
#include "sha1.h"
#include "widescreen_policy.h"

extern "C" unsigned g_ws_active;
extern "C" unsigned long long runtime_current_frame();

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

namespace gbarecomp {
extern "C" RuntimeGuestFn overlay_resolve(std::uint32_t pc, int thumb);
}

namespace {

constexpr const char* kRomSha1 =
    "5c4695205413df7db52b9a184815a07783999971";

constexpr const char* kGoldenSunAspectLabels[] = {
    "Native 240x160",
    "Widescreen 288x160",
    "Expanded Widescreen 360x240",
};
constexpr std::uint16_t kGoldenSunAspectWidths[] = {240u, 288u, 360u};
constexpr std::uint16_t kGoldenSunAspectHeights[] = {160u, 160u, 240u};

std::uint32_t g_golden_sun_wide_extra_left = 0;
std::uint32_t g_golden_sun_wide_extra_right = 0;
std::uint32_t g_golden_sun_wide_extra_top = 0;
std::uint32_t g_golden_sun_wide_extra_bottom = 0;
bool g_golden_sun_mode0_field = false;
bool g_golden_sun_mode0_split_scroll = false;
gsr::widescreen::GoldenSunFieldAuthoredMap g_golden_sun_field_authored;
enum class GoldenSunFieldAuthScene : std::uint8_t { None, EqualScroll, SplitScroll };
GoldenSunFieldAuthScene g_golden_sun_field_auth_scene =
    GoldenSunFieldAuthScene::None;
bool g_golden_sun_expanded_obj_scene = false;
gsr::widescreen::GoldenSunMode0SplitScrollFrame
    g_golden_sun_mode0_split_scroll_frame;

// Declared here because the diagnostics-only B328 candidate table is defined
// before the staging-record helpers below.
struct GoldenSunObjStagingProvenance;
bool read_golden_sun_obj_staging_attrs(std::uint32_t address,
                                       std::uint16_t* attr0,
                                       std::uint16_t* attr1,
                                       std::uint16_t* attr2);
GoldenSunObjStagingProvenance* find_golden_sun_obj_staging(
    std::uint32_t staging_address);
bool golden_sun_func1dc8_writer_pc(std::uint32_t pc,
                                   gsr::Func1dc8WriterRoute route);
void reset_golden_sun_func1dc8_writer_diagnostics();
// WIDE-01 body/shadow identity trace (see the full definition and comment
// near kGoldenSunObjCommitOrderLimit below); forward-declared so the EC/F0
// Func_1dc8 route observers above can call it.
enum class GoldenSunObjCommitOrderRoute : std::uint8_t { D4, EC, F0 };
void note_golden_sun_obj_commit_order(GoldenSunObjCommitOrderRoute route,
                                      std::uint32_t source, int slot,
                                      std::uint16_t attr0,
                                      std::uint16_t attr1,
                                      std::uint16_t attr2, bool x_valid,
                                      std::int16_t logical_x, bool y_valid,
                                      std::int16_t logical_y);
struct RelocatableCodeImage;
bool relocatable_resident_at(const RelocatableCodeImage& image,
                             std::uint32_t base, bool* prefix_passed);
unsigned int ram_code_page_epoch(std::uint32_t addr);
std::uint64_t g_golden_sun_func1dc8_writer_recognized = 0;
std::uint64_t g_golden_sun_func1dc8_writer_identity_mismatch = 0;
std::uint64_t g_golden_sun_func1dc8_writer_unknown_variant = 0;
unsigned g_golden_sun_func1dc8_writer_logs = 0;
std::uint32_t g_golden_sun_func1dc8_last_base = 0;
std::uint64_t g_golden_sun_func1dc8_last_generation = 0;
struct GoldenSunObjPlacementProvenance {
    bool valid = false;
    bool x_valid = false;
    bool y_valid = false;
    std::int16_t logical_x = 0;
    std::int16_t logical_y = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    // Y-writer identity is kept separately from the X/Y freshness fields so
    // a later X placement cannot hide which branch supplied logical Y.
    std::uint32_t writer_branch_pc = 0;
    std::uint32_t target_address = 0;
    std::uint64_t writer_generation = 0;
    // ATTR0/1/2 copied from the exact staging record consumed by 030038D4.
    // This identity, rather than a short frame-age window, bounds the signed
    // coordinate's lifetime after the shadow->OAM DMA latch.
    bool oam_identity_valid = false;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
};

// Diagnostic-only link between a B328 positive-Y candidate and the signed-Y
// cull routes that ran just before it. Keep register values only; never read
// or print guest payload bytes here.
struct GoldenSunObjSignedCullObservation {
    bool valid = false;
    std::uint64_t frame = UINT64_MAX;
    std::uint32_t pc = 0;
    std::int32_t operand = 0;
    std::uint32_t r7 = 0;
    std::uint32_t r10 = 0;
    std::uint32_t r11 = 0;
    std::uint32_t sp = 0;
    std::uint32_t lr = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t call_return_pc = 0;
    std::uint64_t sequence = 0;
};

struct GoldenSunObjB27EParentRoute {
    bool valid = false;
    std::uint32_t pc = 0;
    std::uint32_t staging_address = 0;
    std::uint64_t frame = UINT64_MAX;
    std::int32_t operand = 0;
    std::uint32_t original_decision = 0;
    std::uint32_t final_decision = 0;
    bool overridden = false;
    std::uint32_t r7 = 0;
    std::uint32_t r10 = 0;
    std::uint32_t r11 = 0;
    std::uint32_t sp = 0;
    std::uint32_t lr = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t call_return_pc = 0;
};

struct GoldenSunObjYCorrelation {
    bool valid = false;
    std::uint32_t b328_pc = 0;
    std::int32_t b328_operand = 0;
    std::uint32_t b328_r1 = 0;
    std::uint32_t b328_r3 = 0;
    std::uint32_t b328_r11 = 0;
    bool stack_inputs_valid = false;
    std::uint32_t b328_stack_sp_plus4 = 0;
    std::uint32_t b328_stack_sp_plus18 = 0;
    GoldenSunObjB27EParentRoute parent{};
    std::uint32_t b328_r6 = 0;
    std::uint32_t b328_r7 = 0;
    std::uint32_t b328_sp = 0;
    std::uint32_t b328_lr = 0;
    std::uint32_t b328_call_depth = 0;
    std::uint32_t b328_call_return_pc = 0;
    std::array<GoldenSunObjSignedCullObservation, 4> preceding{};
};

std::array<GoldenSunObjPlacementProvenance,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_pending_provenance{};
// The guest fills the IWRAM shadow before VBlank, then DMA copies that image
// to visible OAM.  Keep the two timelines separate: the renderer may only
// consume the image latched by the exact shadow->OAM handoff.
std::array<GoldenSunObjPlacementProvenance,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_visible_provenance{};

// B324/B328 operate on the guest's transient 12-byte sprite records in
// IWRAM, not on the OAM shadow.  Keep the logical coordinates keyed by that
// record address until the copied 0300387C routine hands the record to its
// 030038D4 writer.  The writer entry has both identities: R6 is the staging
// record and R0 is the final OAM-shadow destination.
struct GoldenSunObjStagingProvenance {
    bool valid = false;
    bool x_valid = false;
    bool y_valid = false;
    std::uint32_t staging_address = 0;
    std::int16_t logical_x = 0;
    std::int16_t logical_y = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    std::uint32_t x_writer_branch_pc = 0;
    std::uint32_t y_writer_branch_pc = 0;
    GoldenSunObjYCorrelation y_correlation{};
};
std::array<GoldenSunObjStagingProvenance, 256>
    g_golden_sun_obj_staging_provenance{};

// F0's entry sees the source record before the generated writer advances R6;
// the later OAM write sees the destination slot. Keep that exact handoff for
// one frame so culling never has to infer a source from a reused OAM slot.
struct GoldenSunObjF0Context {
    bool valid = false;
    std::uint64_t frame = UINT64_MAX;
    std::uint32_t depth = 0;
    std::uint32_t return_pc = 0;
    std::uint32_t staging = 0;
    std::uint32_t record_base = 0;
    bool shadow = false;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
};
std::array<GoldenSunObjF0Context,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_f0_contexts{};
std::uint64_t g_golden_sun_obj_f0_context_frame = UINT64_MAX;
std::array<GoldenSunObjB27EParentRoute, 256>
    g_golden_sun_obj_b27e_parent_routes{};
std::array<std::uint64_t,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_y_alias_last_frame{};
std::array<std::uint64_t,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_y_canonical_last_frame{};
unsigned g_golden_sun_obj_y_alias_logs_in_epoch = 0;
std::uint64_t g_golden_sun_obj_y_alias_logs_dropped = 0;
std::array<GoldenSunObjSignedCullObservation, 4>
    g_golden_sun_obj_signed_cull_observations{};
std::uint64_t g_golden_sun_obj_signed_cull_sequence = 0;
unsigned g_golden_sun_obj_y_correlation_logs_in_epoch = 0;
std::uint64_t g_golden_sun_obj_y_correlation_logs_dropped = 0;
std::array<unsigned, gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_y_correlation_slot_logs{};
struct GoldenSunObjYCorrelationKey {
    bool valid = false;
    std::uint32_t staging_address = 0;
    int slot = -1;
    std::int32_t b328_operand = 0;
    std::uint32_t b328_r1 = 0;
    std::uint32_t b328_r3 = 0;
    std::uint32_t b328_r11 = 0;
    std::uint32_t stack_sp_plus4 = 0;
    std::uint32_t stack_sp_plus18 = 0;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::array<std::uint64_t, 4> cull_sequences{};
};
std::array<GoldenSunObjYCorrelationKey, 64>
    g_golden_sun_obj_y_correlation_keys{};
// B328 can reject the final Y before a staging handoff exists. Keep a
// separate, payload-free decision trace for those calls so the B27E parent
// decision and the B328 decision can be compared using the transient staging
// record identity and the register/call context. This is diagnostics-only.
struct GoldenSunObjYCullDecisionKey {
    bool valid = false;
    std::uint32_t staging_address = 0;
    std::uint64_t frame = UINT64_MAX;
    std::int32_t operand = 0;
    std::uint32_t original_decision = 0;
    std::uint32_t final_decision = 0;
    std::uint32_t r1 = 0;
    std::uint32_t r3 = 0;
    std::uint32_t r11 = 0;
};
std::array<GoldenSunObjYCullDecisionKey, 64>
    g_golden_sun_obj_y_cull_decision_keys{};
unsigned g_golden_sun_obj_y_cull_decision_logs_in_epoch = 0;
std::uint64_t g_golden_sun_obj_y_cull_decision_logs_dropped = 0;
constexpr unsigned kGoldenSunObjB328ClassificationSampleLimit = 32u;

enum class GoldenSunObjB328ParentClassification : std::uint8_t {
    ExactCurrent = 0,
    SameStagingMismatch,
    NoParent,
    Count,
};

const char* golden_sun_obj_b328_parent_classification_name(
    GoldenSunObjB328ParentClassification classification) {
    switch (classification) {
        case GoldenSunObjB328ParentClassification::ExactCurrent:
            return "exact-current";
        case GoldenSunObjB328ParentClassification::SameStagingMismatch:
            return "same-staging-mismatch";
        case GoldenSunObjB328ParentClassification::NoParent:
            return "no-parent";
        case GoldenSunObjB328ParentClassification::Count:
            break;
    }
    return "unknown";
}

constexpr unsigned kB328ParentMismatchFrame = 1u << 0;
constexpr unsigned kB328ParentMismatchCallDepth = 1u << 1;
constexpr unsigned kB328ParentMismatchReturnPc = 1u << 2;

struct GoldenSunObjB328ClassificationSample {
    bool valid = false;
    GoldenSunObjB328ParentClassification classification =
        GoldenSunObjB328ParentClassification::NoParent;
    unsigned mismatch_flags = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint32_t staging_address = 0;
    std::int32_t operand = 0;
    std::uint64_t parent_frame = UINT64_MAX;
    std::uint32_t parent_call_depth = 0;
    std::uint32_t parent_return_pc = 0;
};

enum class GoldenSunObjB328WriterOutcome : std::uint8_t {
    Pending = 0,
    Consumed,
    Unrelated,
    IdentityUnproven,
    Expired,
    ContextMismatch,
    AttrMismatch,
    Count,
};

struct GoldenSunObjB328AcceptedCandidate {
    bool valid = false;
    bool handoff_seen = false;
    GoldenSunObjB328ParentClassification classification =
        GoldenSunObjB328ParentClassification::NoParent;
    std::uint32_t staging_address = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    std::int32_t operand = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t call_return_pc = 0;
    bool attr_identity_valid = false;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
    GoldenSunObjB328WriterOutcome f0_outcome =
        GoldenSunObjB328WriterOutcome::Pending;
    bool f0_writer_seen = false;
    std::uint64_t f0_writer_frame = UINT64_MAX;
    std::uint32_t f0_writer_depth = 0;
    std::uint32_t f0_writer_return_pc = 0;
    std::uint32_t f0_target_address = 0;
    int f0_slot = -1;
    std::uint16_t f0_attr0 = 0;
    std::uint16_t f0_attr1 = 0;
    std::uint16_t f0_attr2 = 0;
    std::uint32_t f0_register_mask = 0;
    bool f0_entry_seen = false;
    std::uint64_t f0_entry_frame = UINT64_MAX;
    std::uint32_t f0_entry_depth = 0;
    std::uint32_t f0_entry_return_pc = 0;
    std::uint32_t f0_entry_staging = 0;
};

enum class GoldenSunObjB328WriterRoute : std::uint8_t {
    D4 = 0,
    F0,
    Count,
};

const char* golden_sun_obj_b328_writer_route_name(
    GoldenSunObjB328WriterRoute route) {
    return route == GoldenSunObjB328WriterRoute::D4 ? "D4" : "F0";
}

const char* golden_sun_obj_b328_writer_outcome_name(
    GoldenSunObjB328WriterOutcome outcome) {
    switch (outcome) {
        case GoldenSunObjB328WriterOutcome::Pending: return "pending";
        case GoldenSunObjB328WriterOutcome::Consumed: return "consumed";
        case GoldenSunObjB328WriterOutcome::Unrelated: return "unrelated";
        case GoldenSunObjB328WriterOutcome::IdentityUnproven:
            return "identity-unproven";
        case GoldenSunObjB328WriterOutcome::Expired: return "expired";
        case GoldenSunObjB328WriterOutcome::ContextMismatch:
            return "context-mismatch";
        case GoldenSunObjB328WriterOutcome::AttrMismatch:
            return "attr-mismatch";
        case GoldenSunObjB328WriterOutcome::Count: break;
    }
    return "unknown";
}

struct GoldenSunObjB328RejectedRoute {
    GoldenSunObjB328WriterOutcome outcome =
        GoldenSunObjB328WriterOutcome::Pending;
    bool writer_seen = false;
    std::uint64_t writer_frame = UINT64_MAX;
    std::uint32_t writer_depth = 0;
    std::uint32_t writer_return_pc = 0;
    std::uint32_t target_address = 0;
    std::uint32_t writer_staging = 0;
    int slot = -1;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::uint32_t f0_register_mask = 0;
    bool f0_entry_seen = false;
    std::uint64_t f0_entry_frame = UINT64_MAX;
    std::uint32_t f0_entry_depth = 0;
    std::uint32_t f0_entry_return_pc = 0;
    std::uint32_t f0_entry_staging = 0;
};

struct GoldenSunObjB328RejectedCandidate {
    bool valid = false;
    GoldenSunObjB328ParentClassification classification =
        GoldenSunObjB328ParentClassification::NoParent;
    unsigned mismatch_flags = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    std::uint32_t staging_address = 0;
    std::int32_t operand = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t call_return_pc = 0;
    bool attr_identity_valid = false;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
    std::array<GoldenSunObjB328RejectedRoute,
               static_cast<std::size_t>(GoldenSunObjB328WriterRoute::Count)>
        routes{};
};

struct GoldenSunObjB328RejectedSample {
    bool valid = false;
    GoldenSunObjB328WriterRoute route = GoldenSunObjB328WriterRoute::D4;
    GoldenSunObjB328WriterOutcome outcome =
        GoldenSunObjB328WriterOutcome::Expired;
    GoldenSunObjB328ParentClassification classification =
        GoldenSunObjB328ParentClassification::NoParent;
    unsigned mismatch_flags = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t writer_frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    std::uint32_t staging_address = 0;
    std::uint32_t writer_staging = 0;
    std::int32_t operand = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t call_return_pc = 0;
    std::uint32_t writer_depth = 0;
    std::uint32_t writer_return_pc = 0;
    std::uint32_t f0_register_mask = 0;
    bool f0_entry_seen = false;
    std::uint64_t f0_entry_frame = UINT64_MAX;
    std::uint32_t f0_entry_depth = 0;
    std::uint32_t f0_entry_return_pc = 0;
    std::uint32_t f0_entry_staging = 0;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
    int slot = -1;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
};

struct GoldenSunObjB328AcceptedF0Sample {
    bool valid = false;
    GoldenSunObjB328WriterOutcome outcome =
        GoldenSunObjB328WriterOutcome::Expired;
    GoldenSunObjB328ParentClassification classification =
        GoldenSunObjB328ParentClassification::NoParent;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t writer_frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    std::uint32_t staging_address = 0;
    std::int32_t operand = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t call_return_pc = 0;
    std::uint32_t writer_depth = 0;
    std::uint32_t writer_return_pc = 0;
    std::uint32_t target_address = 0;
    bool f0_entry_seen = false;
    std::uint64_t f0_entry_frame = UINT64_MAX;
    std::uint32_t f0_entry_depth = 0;
    std::uint32_t f0_entry_return_pc = 0;
    std::uint32_t f0_entry_staging = 0;
    std::uint32_t f0_register_mask = 0;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
    int slot = -1;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
};

struct GoldenSunObjB328Diagnostics {
    std::array<std::uint64_t, static_cast<std::size_t>(
        GoldenSunObjB328ParentClassification::Count)> classifications{};
    std::uint64_t mismatch_frame = 0;
    std::uint64_t mismatch_call_depth = 0;
    std::uint64_t mismatch_return_pc = 0;
    std::array<GoldenSunObjB328ClassificationSample,
               kGoldenSunObjB328ClassificationSampleLimit> samples{};
    std::size_t sample_count = 0;
    std::uint64_t samples_dropped = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(
        GoldenSunObjB328ParentClassification::Count)> accepted{};
    std::array<std::uint64_t, static_cast<std::size_t>(
        GoldenSunObjB328ParentClassification::Count)> handoffs{};
    std::array<std::uint64_t, static_cast<std::size_t>(
        GoldenSunObjB328ParentClassification::Count)> no_handoff{};
    std::uint64_t accepted_tracking_dropped = 0;
    std::array<GoldenSunObjB328AcceptedCandidate, 256> candidates{};
    std::uint64_t rejected_total = 0;
    std::array<std::array<std::uint64_t,
                          static_cast<std::size_t>(
                              GoldenSunObjB328WriterOutcome::Count)>,
               static_cast<std::size_t>(GoldenSunObjB328WriterRoute::Count)>
        rejected_outcomes{};
    std::array<GoldenSunObjB328RejectedCandidate, 256> rejected{};
    std::uint64_t rejected_tracking_dropped = 0;
    std::array<GoldenSunObjB328RejectedSample, 32> rejected_samples{};
    std::size_t rejected_sample_count = 0;
    std::uint64_t rejected_samples_dropped = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(
        GoldenSunObjB328WriterOutcome::Count)> accepted_f0_outcomes{};
    std::uint64_t accepted_f0_tracking_dropped = 0;
    std::array<GoldenSunObjB328AcceptedF0Sample, 32> accepted_f0_samples{};
    std::size_t accepted_f0_sample_count = 0;
    std::uint64_t accepted_f0_samples_dropped = 0;

    void reset() { *this = {}; }
};
GoldenSunObjB328Diagnostics g_golden_sun_obj_b328_diagnostics{};

// A bounded handoff token for the legitimate-looking parentless B328 route.
// It is diagnostic-only: the branch decision remains unchanged.
constexpr std::size_t kGoldenSunObjB328ParentlessTokenLimit = 64u;
struct GoldenSunObjB328ParentlessToken {
    bool valid = false;
    std::uint64_t sequence = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint64_t auth_epoch = 0;
    std::uint32_t staging = 0;
    std::int32_t operand = 0;
    std::uint32_t r6 = 0;
    std::uint32_t r7 = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t return_pc = 0;
    std::uint32_t entry_pc = 0x0800B328u;
    bool attr_valid = false;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    bool ec_seen = false;
    std::uint32_t ec_entry_pc = 0;
    std::uint32_t ec_r6 = 0;
    std::uint32_t ec_r7 = 0;
    std::uint32_t ec_source = 0;
    const char* ec_reason = "not-seen";
    bool f0_seen = false;
    std::uint32_t f0_entry_pc = 0;
    std::uint32_t f0_r6 = 0;
    std::uint32_t f0_r7 = 0;
    std::uint32_t f0_source = 0;
    std::uint32_t f0_target = 0;
    int f0_slot = -1;
    std::uint16_t f0_attr0 = 0;
    std::uint16_t f0_attr1 = 0;
    std::uint16_t f0_attr2 = 0;
    const char* f0_reason = "not-seen";
};
std::array<GoldenSunObjB328ParentlessToken,
           kGoldenSunObjB328ParentlessTokenLimit>
    g_golden_sun_obj_b328_parentless_tokens{};
std::uint64_t g_golden_sun_obj_b328_parentless_sequence = 0;
std::uint64_t g_golden_sun_obj_b328_parentless_dropped = 0;
struct GoldenSunObjYAcceptedState {
    bool valid = false;
    std::uint8_t raw_y = 0;
    std::int16_t logical_y = 0;
    std::uint64_t frame = UINT64_MAX;
    std::uint32_t writer_branch_pc = 0;
    std::uint32_t target_address = 0;
    std::uint64_t writer_generation = 0;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
};
std::array<GoldenSunObjYAcceptedState,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_y_accepted_state{};
std::array<gsr::widescreen::GoldenSunObjYEdgeAliasState,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_y_edge_alias_state{};
std::uint64_t g_golden_sun_obj_y_edge_alias_activations = 0;
unsigned g_golden_sun_obj_y_jump_logs_in_epoch = 0;
std::uint64_t g_golden_sun_obj_y_jump_logs_dropped = 0;
std::uint64_t g_golden_sun_field_auth_epoch = 0;
std::array<std::uint8_t, 0x20> g_golden_sun_wide_line_io{};
bool g_golden_sun_wide_line_io_valid = false;
std::uint16_t g_golden_sun_wide_line_dispcnt = 0;
std::vector<std::uint64_t> g_golden_sun_wide_scene_signatures;

constexpr std::size_t kGoldenSunFieldMapOffset = 0x10000u;
constexpr std::size_t kGoldenSunFieldMapBytes = 128u * 128u * 4u;
constexpr std::size_t kGoldenSunFieldRawOffset = 0x20000u;
constexpr std::size_t kGoldenSunFieldRawBytes = 4096u * 8u;
constexpr std::uint32_t kGoldenSunFieldMapAddress = 0x02010000u;
constexpr std::uint32_t kGoldenSunFieldRawAddress = 0x02020000u;
constexpr unsigned kGoldenSunWidePolicyTransitionLimit = 16u;
constexpr unsigned kGoldenSunWidePolicySampleLimit = 16u;
constexpr unsigned kGoldenSunFieldTableCpuLogLimitPerEpoch = 64u;
constexpr unsigned kGoldenSunFieldTableDmaLogLimitPerEpoch = 32u;
constexpr unsigned kGoldenSunObjYProvenanceLogLimitPerEpoch = 128u;
constexpr unsigned kGoldenSunObjYAliasLogLimitPerEpoch = 64u;
constexpr unsigned kGoldenSunObjYJumpLogLimitPerEpoch = 64u;
constexpr unsigned kGoldenSunObjYCorrelationLogLimitPerEpoch = 64u;
constexpr unsigned kGoldenSunObjYCorrelationPerSlotLimit = 4u;
constexpr unsigned kGoldenSunObjYCorrelationPerStagingLimit = 2u;
constexpr unsigned kGoldenSunObjYCullDecisionPerStagingLimit = 4u;
constexpr unsigned kGoldenSunFieldMapIdAttributionLimit = 32u;
constexpr unsigned kGoldenSunPalaceMarginSampleLimit = 96u;
constexpr unsigned kGoldenSunExperimentalCullLogLimit = 128u;

struct GoldenSunExperimentalCullLogState {
    bool valid = false;
    int slot = -1;
    std::uint32_t source = 0;
    std::uint32_t raw_x = 0;
    std::uint32_t raw_y = 0;
    int resolved_x = 0;
    int resolved_y = 0;
    unsigned shape = 0;
    unsigned size = 0;
    int width = 0;
    int height = 0;
    bool culled = false;
    const char* reason = nullptr;
};
std::array<GoldenSunExperimentalCullLogState,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_experimental_cull_last{};
unsigned g_golden_sun_experimental_cull_logs = 0;
std::uint64_t g_golden_sun_experimental_cull_logs_dropped = 0;
bool g_golden_sun_experimental_cull_write_in_progress = false;

const char* golden_sun_field_margin_region(int hw_x, int screen_y,
                                           std::size_t* out_region);
bool golden_sun_wide_diagnostics_enabled();
bool golden_sun_experimental_fixes_enabled();
bool golden_sun_expanded_obj_view_active();

enum class GoldenSunPalaceMarginOutcome : std::uint8_t {
    AcceptedNativeSeed = 0,
    AcceptedConnected,
    RejectedFill,
    RejectedMask,
    RejectedRaw,
    RejectedLookup,
    Count,
};

const char* golden_sun_palace_margin_outcome_name(
    GoldenSunPalaceMarginOutcome outcome) {
    switch (outcome) {
        case GoldenSunPalaceMarginOutcome::AcceptedNativeSeed:
            return "accepted-native-seed";
        case GoldenSunPalaceMarginOutcome::AcceptedConnected:
            return "accepted-connected";
        case GoldenSunPalaceMarginOutcome::RejectedFill:
            return "rejected-fill";
        case GoldenSunPalaceMarginOutcome::RejectedMask:
            return "rejected-mask";
        case GoldenSunPalaceMarginOutcome::RejectedRaw:
            return "rejected-raw";
        case GoldenSunPalaceMarginOutcome::RejectedLookup:
            return "rejected-lookup";
        case GoldenSunPalaceMarginOutcome::Count:
            break;
    }
    return "unknown";
}

struct GoldenSunPalaceMarginSample {
    std::uint8_t bg = 0;
    std::uint8_t region = 0;
    GoldenSunPalaceMarginOutcome outcome =
        GoldenSunPalaceMarginOutcome::RejectedLookup;
    std::int16_t hw_x = 0;
    std::int16_t screen_y = 0;
    std::uint16_t map_x = 0;
    std::uint16_t map_y = 0;
    std::uint16_t map_id = 0;
    bool metadata_valid = false;
};

struct GoldenSunPalaceMarginDiagnostics {
    static constexpr std::size_t kBgCount = 3u;
    static constexpr std::size_t kRegionCount = 4u;
    static constexpr std::size_t kOutcomeCount = static_cast<std::size_t>(
        GoldenSunPalaceMarginOutcome::Count);

    std::array<std::array<std::array<std::uint64_t, kOutcomeCount>,
                          kRegionCount>, kBgCount>
        counts{};
    std::array<std::array<std::array<bool, kOutcomeCount>, kRegionCount>,
               kBgCount>
        sample_seen{};
    std::array<GoldenSunPalaceMarginSample,
               kGoldenSunPalaceMarginSampleLimit>
        samples{};
    std::size_t sample_count = 0;
    std::uint64_t samples_dropped = 0;

    void reset() {
        counts = {};
        sample_seen = {};
        sample_count = 0;
        samples_dropped = 0;
    }

    void record(int bg, int hw_x, int screen_y,
                GoldenSunPalaceMarginOutcome outcome,
                const gsr::widescreen::GoldenSunFieldTilemapMetadata& metadata,
                bool metadata_valid) {
        if (bg < 1 || bg > 3) return;
        std::size_t region = 0;
        golden_sun_field_margin_region(hw_x, screen_y, &region);
        const std::size_t bg_index = static_cast<std::size_t>(bg - 1);
        const std::size_t outcome_index = static_cast<std::size_t>(outcome);
        ++counts[bg_index][region][outcome_index];
        if (sample_seen[bg_index][region][outcome_index]) return;
        sample_seen[bg_index][region][outcome_index] = true;
        if (sample_count >= samples.size()) {
            ++samples_dropped;
            return;
        }
        auto& sample = samples[sample_count++];
        sample.bg = static_cast<std::uint8_t>(bg);
        sample.region = static_cast<std::uint8_t>(region);
        sample.outcome = outcome;
        sample.hw_x = static_cast<std::int16_t>(hw_x);
        sample.screen_y = static_cast<std::int16_t>(screen_y);
        sample.map_x = static_cast<std::uint16_t>(metadata.map_x);
        sample.map_y = static_cast<std::uint16_t>(metadata.map_y);
        sample.map_id = metadata.map_id;
        sample.metadata_valid = metadata_valid;
    }
};

enum class GoldenSunObjYOutcome : std::uint8_t {
    CoordinateRejected = 0,
    ProvenanceMissing,
    ProvenanceStaleFrame,
    ProvenanceStaleEpoch,
    ProvenanceYMismatch,
    AcceptedProvenance,
    OutputUnavailable,
    Count,
};

const char* golden_sun_obj_y_outcome_name(GoldenSunObjYOutcome outcome) {
    switch (outcome) {
        case GoldenSunObjYOutcome::CoordinateRejected: return "coordinate-rejected";
        case GoldenSunObjYOutcome::ProvenanceMissing: return "provenance-missing";
        case GoldenSunObjYOutcome::ProvenanceStaleFrame: return "provenance-stale-frame";
        case GoldenSunObjYOutcome::ProvenanceStaleEpoch: return "provenance-stale-epoch";
        case GoldenSunObjYOutcome::ProvenanceYMismatch: return "provenance-y-mismatch";
        case GoldenSunObjYOutcome::AcceptedProvenance: return "accepted-provenance";
        case GoldenSunObjYOutcome::OutputUnavailable: return "output-unavailable";
        case GoldenSunObjYOutcome::Count: break;
    }
    return "unknown";
}

struct GoldenSunObjYSlotDiagnostics {
    std::uint64_t calls = 0;
    std::uint64_t raw_160_191 = 0;
    std::uint64_t raw_192 = 0;
    std::uint64_t raw_193_199 = 0;
    std::uint64_t raw_200_255 = 0;
    std::array<std::uint64_t,
               static_cast<std::size_t>(GoldenSunObjYOutcome::Count)>
        outcomes{};
};
std::array<GoldenSunObjYSlotDiagnostics,
           gsr::widescreen::kGoldenSunOamShadowSlotCount>
    g_golden_sun_obj_y_slot_diagnostics{};

// Pixel-relevant provider seam diagnostics. This is deliberately separate
// from the older steady-state counters: those counters only answer how often
// a slot was queried and conflate several canonical fallbacks. Keep the
// aggregate uncapped, while samples are first/change-per-slot and bounded.
enum class GoldenSunObjYTransitionResolution : std::uint8_t {
    Signed = 0,
    Canonical,
    Count,
};

enum class GoldenSunObjYTransitionReason : std::uint8_t {
    NoProvenance = 0,
    StaleFrame,
    StaleEpoch,
    TargetMismatch,
    RawMismatch,
    Attr0Mismatch,
    Attr1Mismatch,
    Attr2Mismatch,
    Accepted,
    OutputUnavailable,
    Count,
};

enum class GoldenSunObjYTransitionRegion : std::uint8_t {
    Top = 0,
    Native,
    Bottom,
    Offscreen,
    Count,
};

// Keep the three evidence questions independent. A flood of canonical
// top-wrap records must not consume the budget needed for accepted bottom
// candidates or canonical<->signed transitions.
constexpr unsigned kGoldenSunObjYTransitionSampleLimit = 128u;
constexpr unsigned kGoldenSunObjYTransitionBucketCount = 3u;

enum class GoldenSunObjYTransitionBucket : std::uint8_t {
    ActiveCandidateCanonical = 0,
    SignedBottom,
    OutcomeTransition,
    Count,
};

struct GoldenSunObjYTransitionSample {
    bool valid = false;
    std::uint64_t frame = UINT64_MAX;
    int slot = -1;
    int raw_y = 0;
    int canonical_y = 0;
    int output_y = 0;
    GoldenSunObjYTransitionResolution resolution =
        GoldenSunObjYTransitionResolution::Canonical;
    GoldenSunObjYTransitionReason reason =
        GoldenSunObjYTransitionReason::NoProvenance;
    GoldenSunObjYTransitionRegion region =
        GoldenSunObjYTransitionRegion::Offscreen;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
    std::uint32_t expected_target = 0;
    std::uint64_t provenance_frame = UINT64_MAX;
    std::uint64_t provenance_epoch = 0;
    std::uint32_t target_address = 0;
    std::uint32_t writer_pc = 0;
    std::uint64_t writer_generation = 0;
};

struct GoldenSunObjYTransitionIdentity {
    bool valid = false;
    int slot = -1;
    std::uint32_t target = 0;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::uint16_t expected_attr0 = 0;
    std::uint16_t expected_attr1 = 0;
    std::uint16_t expected_attr2 = 0;
};

struct GoldenSunObjYTransitionOutcomeState {
    bool valid = false;
    int slot = -1;
    std::uint32_t target = 0;
    GoldenSunObjYTransitionResolution resolution =
        GoldenSunObjYTransitionResolution::Canonical;
};

std::array<std::array<std::array<std::uint64_t,
                                 static_cast<std::size_t>(
                                     GoldenSunObjYTransitionResolution::Count)>,
                              static_cast<std::size_t>(
                                  GoldenSunObjYTransitionRegion::Count)>,
                     static_cast<std::size_t>(
                         GoldenSunObjYTransitionReason::Count)>
    g_golden_sun_obj_y_transition_counts{};
std::array<std::array<GoldenSunObjYTransitionSample,
                      kGoldenSunObjYTransitionSampleLimit>,
           kGoldenSunObjYTransitionBucketCount>
    g_golden_sun_obj_y_transition_samples{};
std::array<unsigned, kGoldenSunObjYTransitionBucketCount>
    g_golden_sun_obj_y_transition_sample_counts{};
std::array<std::uint64_t, kGoldenSunObjYTransitionBucketCount>
    g_golden_sun_obj_y_transition_samples_dropped{};
std::array<std::array<GoldenSunObjYTransitionIdentity,
                      kGoldenSunObjYTransitionSampleLimit>,
           2>
    g_golden_sun_obj_y_transition_seen{};
std::array<unsigned, 2> g_golden_sun_obj_y_transition_seen_counts{};
std::array<GoldenSunObjYTransitionOutcomeState, 256>
    g_golden_sun_obj_y_transition_outcome_states{};

const char* golden_sun_obj_y_transition_resolution_name(
    GoldenSunObjYTransitionResolution resolution) {
    return resolution == GoldenSunObjYTransitionResolution::Signed
        ? "signed" : "canonical";
}

const char* golden_sun_obj_y_transition_reason_name(
    GoldenSunObjYTransitionReason reason) {
    switch (reason) {
        case GoldenSunObjYTransitionReason::NoProvenance: return "no-provenance";
        case GoldenSunObjYTransitionReason::StaleFrame: return "stale-frame";
        case GoldenSunObjYTransitionReason::StaleEpoch: return "stale-epoch";
        case GoldenSunObjYTransitionReason::TargetMismatch: return "target-mismatch";
        case GoldenSunObjYTransitionReason::RawMismatch: return "raw-mismatch";
        case GoldenSunObjYTransitionReason::Attr0Mismatch: return "attr0-mismatch";
        case GoldenSunObjYTransitionReason::Attr1Mismatch: return "attr1-mismatch";
        case GoldenSunObjYTransitionReason::Attr2Mismatch: return "attr2-mismatch";
        case GoldenSunObjYTransitionReason::Accepted: return "accepted";
        case GoldenSunObjYTransitionReason::OutputUnavailable:
            return "output-unavailable";
        case GoldenSunObjYTransitionReason::Count: break;
    }
    return "unknown";
}

const char* golden_sun_obj_y_transition_region_name(
    GoldenSunObjYTransitionRegion region) {
    switch (region) {
        case GoldenSunObjYTransitionRegion::Top: return "top";
        case GoldenSunObjYTransitionRegion::Native: return "native";
        case GoldenSunObjYTransitionRegion::Bottom: return "bottom";
        case GoldenSunObjYTransitionRegion::Offscreen: return "offscreen";
        case GoldenSunObjYTransitionRegion::Count: break;
    }
    return "unknown";
}

const char* golden_sun_obj_y_transition_bucket_name(
    GoldenSunObjYTransitionBucket bucket) {
    switch (bucket) {
        case GoldenSunObjYTransitionBucket::ActiveCandidateCanonical:
            return "active-candidate-canonical";
        case GoldenSunObjYTransitionBucket::SignedBottom:
            return "signed-bottom";
        case GoldenSunObjYTransitionBucket::OutcomeTransition:
            return "outcome-transition";
        case GoldenSunObjYTransitionBucket::Count:
            break;
    }
    return "unknown";
}

GoldenSunObjYTransitionRegion golden_sun_obj_y_transition_region(int output_y) {
    // Expanded logical rows are -40..199. Keep rows outside that actual
    // output envelope distinct from top/bottom margin rows.
    if (output_y < -40 || output_y >= 200)
        return GoldenSunObjYTransitionRegion::Offscreen;
    if (output_y < 0) return GoldenSunObjYTransitionRegion::Top;
    if (output_y < 160) return GoldenSunObjYTransitionRegion::Native;
    return GoldenSunObjYTransitionRegion::Bottom;
}

int golden_sun_obj_oam_truncated(int logical, int bits);

GoldenSunObjYTransitionReason classify_golden_sun_obj_y_transition(
    const GoldenSunObjPlacementProvenance& provenance, std::uint64_t frame,
    std::uint64_t auth_epoch, std::uint32_t expected_target, int raw_y,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    if (!provenance.valid || !provenance.y_valid ||
        !provenance.oam_identity_valid)
        return GoldenSunObjYTransitionReason::NoProvenance;
    if (!gsr::widescreen::golden_sun_obj_provenance_frame_fresh(
            provenance.frame, frame))
        return GoldenSunObjYTransitionReason::StaleFrame;
    if (provenance.auth_epoch != auth_epoch)
        return GoldenSunObjYTransitionReason::StaleEpoch;
    if (provenance.target_address != expected_target)
        return GoldenSunObjYTransitionReason::TargetMismatch;
    if (golden_sun_obj_oam_truncated(provenance.logical_y, 8) != raw_y)
        return GoldenSunObjYTransitionReason::RawMismatch;
    if (provenance.expected_attr0 != attr0)
        return GoldenSunObjYTransitionReason::Attr0Mismatch;
    if (provenance.expected_attr1 != attr1)
        return GoldenSunObjYTransitionReason::Attr1Mismatch;
    if (provenance.expected_attr2 != attr2)
        return GoldenSunObjYTransitionReason::Attr2Mismatch;
    return GoldenSunObjYTransitionReason::Accepted;
}

bool golden_sun_obj_y_steady_state() {
    return golden_sun_wide_diagnostics_enabled() && g_ws_active &&
           g_golden_sun_expanded_obj_scene &&
           (g_golden_sun_mode0_field || g_golden_sun_mode0_split_scroll);
}

void record_golden_sun_obj_y_outcome(int slot, int raw_y,
                                     GoldenSunObjYOutcome outcome) {
    if (!golden_sun_obj_y_steady_state() || slot < 0 ||
        static_cast<std::size_t>(slot) >= g_golden_sun_obj_y_slot_diagnostics.size() ||
        raw_y < 160 || raw_y > 255) return;
    auto& stat = g_golden_sun_obj_y_slot_diagnostics[static_cast<std::size_t>(slot)];
    ++stat.calls;
    if (raw_y <= 191) ++stat.raw_160_191;
    else if (raw_y == 192) ++stat.raw_192;
    else if (raw_y <= 199) ++stat.raw_193_199;
    else ++stat.raw_200_255;
    ++stat.outcomes[static_cast<std::size_t>(outcome)];
}

void reset_golden_sun_obj_y_alias_diagnostics() {
    g_golden_sun_obj_y_alias_last_frame.fill(UINT64_MAX);
    g_golden_sun_obj_y_canonical_last_frame.fill(UINT64_MAX);
    g_golden_sun_obj_y_alias_logs_in_epoch = 0;
    g_golden_sun_obj_y_alias_logs_dropped = 0;
    g_golden_sun_obj_y_accepted_state = {};
    g_golden_sun_obj_y_edge_alias_state = {};
    g_golden_sun_obj_y_edge_alias_activations = 0;
    g_golden_sun_obj_y_jump_logs_in_epoch = 0;
    g_golden_sun_obj_y_jump_logs_dropped = 0;
    g_golden_sun_obj_signed_cull_observations = {};
    g_golden_sun_obj_signed_cull_sequence = 0;
    g_golden_sun_obj_y_correlation_logs_in_epoch = 0;
    g_golden_sun_obj_y_correlation_logs_dropped = 0;
    g_golden_sun_obj_y_correlation_slot_logs = {};
    g_golden_sun_obj_y_correlation_keys = {};
    g_golden_sun_obj_y_cull_decision_keys = {};
    g_golden_sun_obj_y_cull_decision_logs_in_epoch = 0;
    g_golden_sun_obj_y_cull_decision_logs_dropped = 0;
    g_golden_sun_obj_b328_diagnostics.reset();
    g_golden_sun_obj_b27e_parent_routes = {};
    g_golden_sun_obj_b328_parentless_tokens = {};
    g_golden_sun_obj_b328_parentless_sequence = 0;
    g_golden_sun_obj_b328_parentless_dropped = 0;
    if (golden_sun_wide_diagnostics_enabled()) {
        g_golden_sun_obj_y_transition_counts = {};
        g_golden_sun_obj_y_transition_samples = {};
        g_golden_sun_obj_y_transition_sample_counts = {};
        g_golden_sun_obj_y_transition_samples_dropped = {};
        g_golden_sun_obj_y_transition_seen = {};
        g_golden_sun_obj_y_transition_seen_counts = {};
        g_golden_sun_obj_y_transition_outcome_states = {};
    }
}

std::uint32_t golden_sun_obj_call_return_pc(std::uint32_t depth) {
    if (depth == 0u) return 0u;
    const std::uint32_t* stack = runtime_call_stack_data();
    return stack ? (stack[depth - 1u] & ~1u) : 0u;
}

GoldenSunObjB27EParentRoute* find_golden_sun_obj_b27e_parent(
    std::uint32_t staging_address) {
    for (auto& route : g_golden_sun_obj_b27e_parent_routes) {
        if (route.valid && route.staging_address == staging_address)
            return &route;
    }
    for (auto& route : g_golden_sun_obj_b27e_parent_routes) {
        if (!route.valid) {
            route.staging_address = staging_address;
            return &route;
        }
    }
    return nullptr;
}

void record_golden_sun_b27e_parent_route(std::uint32_t original_decision,
                                         bool overridden,
                                         std::uint32_t final_decision,
                                         std::int32_t operand) {
    if (!golden_sun_expanded_obj_view_active()) return;
    auto* route = find_golden_sun_obj_b27e_parent(g_cpu.R[7]);
    if (!route) return;
    route->valid = true;
    route->pc = 0x0800B27Eu;
    route->frame = runtime_current_frame();
    route->operand = operand;
    route->original_decision = original_decision;
    route->final_decision = final_decision;
    route->overridden = overridden;
    route->r7 = g_cpu.R[7];
    route->r10 = g_cpu.R[10];
    route->r11 = g_cpu.R[11];
    route->sp = g_cpu.R[13];
    route->lr = g_cpu.R[14];
    route->call_depth = runtime_call_stack_depth();
    route->call_return_pc = golden_sun_obj_call_return_pc(route->call_depth);
}

const GoldenSunObjB27EParentRoute* find_golden_sun_current_b27e_parent(
    std::uint32_t staging_address, std::uint64_t frame,
    std::uint32_t call_depth, std::uint32_t call_return_pc) {
    for (const auto& route : g_golden_sun_obj_b27e_parent_routes) {
        if (route.valid && route.staging_address == staging_address &&
            route.frame == frame && route.call_depth == call_depth &&
            route.call_return_pc == call_return_pc) {
            return &route;
        }
    }
    return nullptr;
}

const GoldenSunObjB27EParentRoute* find_golden_sun_any_b27e_parent(
    std::uint32_t staging_address) {
    for (const auto& route : g_golden_sun_obj_b27e_parent_routes) {
        if (route.valid && route.staging_address == staging_address)
            return &route;
    }
    return nullptr;
}

struct GoldenSunObjB328ClassificationResult {
    GoldenSunObjB328ParentClassification classification =
        GoldenSunObjB328ParentClassification::NoParent;
    unsigned mismatch_flags = 0;
    const GoldenSunObjB27EParentRoute* parent = nullptr;
};

GoldenSunObjB328ClassificationResult classify_golden_sun_b328_parent(
    std::uint32_t staging_address, std::uint64_t frame,
    std::uint32_t call_depth, std::uint32_t call_return_pc) {
    GoldenSunObjB328ClassificationResult result{};
    result.parent = find_golden_sun_any_b27e_parent(staging_address);
    if (!result.parent) return result;
    if (result.parent->frame != frame)
        result.mismatch_flags |= kB328ParentMismatchFrame;
    if (result.parent->call_depth != call_depth)
        result.mismatch_flags |= kB328ParentMismatchCallDepth;
    if (result.parent->call_return_pc != call_return_pc)
        result.mismatch_flags |= kB328ParentMismatchReturnPc;
    result.classification = result.mismatch_flags == 0u
        ? GoldenSunObjB328ParentClassification::ExactCurrent
        : GoldenSunObjB328ParentClassification::SameStagingMismatch;
    return result;
}

void record_golden_sun_b328_classification(
    std::int32_t operand, const GoldenSunObjB328ClassificationResult& result,
    std::uint32_t staging_address, std::uint64_t frame,
    std::uint32_t call_depth, std::uint32_t call_return_pc) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active() || operand < 160 ||
        operand > 199) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    const auto class_index = static_cast<std::size_t>(result.classification);
    ++stats.classifications[class_index];
    if (result.mismatch_flags & kB328ParentMismatchFrame)
        ++stats.mismatch_frame;
    if (result.mismatch_flags & kB328ParentMismatchCallDepth)
        ++stats.mismatch_call_depth;
    if (result.mismatch_flags & kB328ParentMismatchReturnPc)
        ++stats.mismatch_return_pc;

    for (std::size_t i = 0; i < stats.sample_count; ++i) {
        const auto& sample = stats.samples[i];
        if (sample.classification == result.classification &&
            sample.mismatch_flags == result.mismatch_flags &&
            sample.staging_address == staging_address &&
            sample.frame == frame && sample.operand == operand) return;
    }
    if (stats.sample_count >= stats.samples.size()) {
        ++stats.samples_dropped;
        return;
    }
    auto& sample = stats.samples[stats.sample_count++];
    sample.valid = true;
    sample.classification = result.classification;
    sample.mismatch_flags = result.mismatch_flags;
    sample.frame = frame;
    sample.staging_address = staging_address;
    sample.operand = operand;
    if (result.parent) {
        sample.parent_frame = result.parent->frame;
        sample.parent_call_depth = result.parent->call_depth;
        sample.parent_return_pc = result.parent->call_return_pc;
    }
    (void)call_depth;
    (void)call_return_pc;
}

void record_golden_sun_b328_accepted_candidate(
    std::int32_t operand,
    GoldenSunObjB328ParentClassification classification,
    std::uint32_t staging_address, std::uint64_t frame,
    std::uint32_t call_depth, std::uint32_t call_return_pc) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active() || operand < 160 ||
        operand > 199) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    const auto class_index = static_cast<std::size_t>(classification);
    ++stats.accepted[class_index];
    for (auto& candidate : stats.candidates) {
        if (!candidate.valid) {
            candidate.valid = true;
            candidate.handoff_seen = false;
            candidate.classification = classification;
            candidate.staging_address = staging_address;
            candidate.frame = frame;
            candidate.auth_epoch = g_golden_sun_field_auth_epoch;
            candidate.operand = operand;
            candidate.call_depth = call_depth;
            candidate.call_return_pc = call_return_pc;
            candidate.attr_identity_valid = read_golden_sun_obj_staging_attrs(
                staging_address, &candidate.expected_attr0,
                &candidate.expected_attr1, &candidate.expected_attr2);
            return;
        }
    }
    ++stats.accepted_tracking_dropped;
    ++stats.accepted_f0_tracking_dropped;
}

void record_golden_sun_b328_parentless_token(
    std::uint64_t frame, std::uint32_t staging, std::int32_t operand,
    std::uint32_t depth, std::uint32_t return_pc) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active()) return;
    for (const auto& token : g_golden_sun_obj_b328_parentless_tokens) {
        if (token.valid && token.frame == frame && token.auth_epoch ==
            g_golden_sun_field_auth_epoch && token.staging == staging &&
            token.operand == operand) return;
    }
    GoldenSunObjB328ParentlessToken* token = nullptr;
    for (auto& candidate : g_golden_sun_obj_b328_parentless_tokens) {
        if (!candidate.valid) {
            token = &candidate;
            break;
        }
    }
    if (!token) {
        ++g_golden_sun_obj_b328_parentless_dropped;
        return;
    }
    *token = {};
    token->valid = true;
    token->sequence = ++g_golden_sun_obj_b328_parentless_sequence;
    token->frame = frame;
    token->auth_epoch = g_golden_sun_field_auth_epoch;
    token->staging = staging;
    token->operand = operand;
    token->r6 = g_cpu.R[6];
    token->r7 = g_cpu.R[7];
    token->call_depth = depth;
    token->return_pc = return_pc;
    token->attr_valid = read_golden_sun_obj_staging_attrs(
        staging, &token->attr0, &token->attr1, &token->attr2);
}

void link_golden_sun_b328_parentless_ec(
    std::uint64_t frame, std::uint32_t staging, std::uint32_t depth,
    std::uint32_t return_pc, std::uint32_t entry_pc) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    GoldenSunObjB328ParentlessToken* found = nullptr;
    for (auto& token : g_golden_sun_obj_b328_parentless_tokens) {
        if (!token.valid || token.frame != frame || token.auth_epoch !=
            g_golden_sun_field_auth_epoch || token.staging != staging) continue;
        if (found) {
            found->ec_reason = "ambiguous-token";
            return;
        }
        found = &token;
    }
    if (!found) return;
    found->ec_entry_pc = entry_pc;
    found->ec_r6 = g_cpu.R[6];
    found->ec_r7 = g_cpu.R[7];
    found->ec_source = staging;
    if (found->call_depth != depth || found->return_pc != return_pc) {
        found->ec_reason = "context-mismatch";
        return;
    }
    found->ec_seen = true;
    found->ec_reason = "exact-token";
}

void link_golden_sun_b328_parentless_f0(
    std::uint64_t frame, std::uint32_t depth, std::uint32_t return_pc,
    std::uint32_t entry_pc, std::uint32_t target, int slot,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    GoldenSunObjB328ParentlessToken* found = nullptr;
    GoldenSunObjB328ParentlessToken* context_match = nullptr;
    bool context_ambiguous = false;
    for (auto& token : g_golden_sun_obj_b328_parentless_tokens) {
        if (!token.valid || token.frame != frame || token.auth_epoch !=
            g_golden_sun_field_auth_epoch || token.call_depth != depth ||
            token.return_pc != return_pc) continue;
        if (context_match) {
            context_ambiguous = true;
        } else {
            context_match = &token;
        }
        if (!token.attr_valid || token.attr0 != attr0 ||
            token.attr1 != attr1 || token.attr2 != attr2) continue;
        if (found) {
            found->f0_reason = "ambiguous-token";
            return;
        }
        found = &token;
    }
    if (context_ambiguous) {
        if (found) found->f0_reason = "ambiguous-token";
        return;
    }
    if (!found) {
        if (context_match && context_match->ec_seen)
            context_match->f0_reason = "attr-mismatch";
        return;
    }
    found->f0_seen = true;
    found->f0_entry_pc = entry_pc;
    found->f0_r6 = g_cpu.R[6];
    found->f0_r7 = g_cpu.R[7];
    found->f0_source = found->staging;
    found->f0_target = target;
    found->f0_slot = slot;
    found->f0_attr0 = attr0;
    found->f0_attr1 = attr1;
    found->f0_attr2 = attr2;
    found->f0_reason = "exact-token";
}

void record_golden_sun_b328_rejected_candidate(
    std::int32_t operand,
    GoldenSunObjB328ParentClassification classification,
    unsigned mismatch_flags, std::uint32_t staging_address,
    std::uint64_t frame, std::uint32_t call_depth,
    std::uint32_t call_return_pc) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active() || operand < 160 ||
        operand > 199) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    ++stats.rejected_total;
    for (auto& candidate : stats.rejected) {
        if (!candidate.valid) {
            candidate.valid = true;
            candidate.classification = classification;
            candidate.mismatch_flags = mismatch_flags;
            candidate.frame = frame;
            candidate.auth_epoch = g_golden_sun_field_auth_epoch;
            candidate.staging_address = staging_address;
            candidate.operand = operand;
            candidate.call_depth = call_depth;
            candidate.call_return_pc = call_return_pc;
            candidate.attr_identity_valid = read_golden_sun_obj_staging_attrs(
                staging_address, &candidate.expected_attr0,
                &candidate.expected_attr1, &candidate.expected_attr2);
            return;
        }
    }
    // The aggregate remains uncapped; only the short-lived correlation table
    // can overflow.
    ++stats.rejected_tracking_dropped;
}

void record_golden_sun_b328_rejected_sample(
    const GoldenSunObjB328RejectedCandidate& candidate,
    GoldenSunObjB328WriterRoute route,
    const GoldenSunObjB328RejectedRoute& result) {
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    if (stats.rejected_sample_count >= stats.rejected_samples.size()) {
        ++stats.rejected_samples_dropped;
        return;
    }
    auto& sample = stats.rejected_samples[stats.rejected_sample_count++];
    sample.valid = true;
    sample.route = route;
    sample.outcome = result.outcome;
    sample.classification = candidate.classification;
    sample.mismatch_flags = candidate.mismatch_flags;
    sample.frame = candidate.frame;
    sample.writer_frame = result.writer_frame;
    sample.auth_epoch = candidate.auth_epoch;
    sample.staging_address = candidate.staging_address;
    sample.writer_staging = result.writer_staging;
    sample.operand = candidate.operand;
    sample.call_depth = candidate.call_depth;
    sample.call_return_pc = candidate.call_return_pc;
    sample.writer_depth = result.writer_depth;
    sample.writer_return_pc = result.writer_return_pc;
    sample.f0_register_mask = result.f0_register_mask;
    sample.f0_entry_seen = result.f0_entry_seen;
    sample.f0_entry_frame = result.f0_entry_frame;
    sample.f0_entry_depth = result.f0_entry_depth;
    sample.f0_entry_return_pc = result.f0_entry_return_pc;
    sample.f0_entry_staging = result.f0_entry_staging;
    sample.expected_attr0 = candidate.expected_attr0;
    sample.expected_attr1 = candidate.expected_attr1;
    sample.expected_attr2 = candidate.expected_attr2;
    sample.slot = result.slot;
    sample.attr0 = result.attr0;
    sample.attr1 = result.attr1;
    sample.attr2 = result.attr2;
}

void record_golden_sun_b328_rejected_writer(
    GoldenSunObjB328WriterRoute route, std::uint32_t writer_staging,
    int slot, std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2,
    std::uint64_t writer_frame, std::uint32_t writer_depth,
    std::uint32_t writer_return_pc, std::uint32_t f0_register_mask) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active()) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    const std::size_t route_index = static_cast<std::size_t>(route);
    for (auto& candidate : stats.rejected) {
        if (!candidate.valid || candidate.auth_epoch !=
            g_golden_sun_field_auth_epoch) continue;
        auto& result = candidate.routes[route_index];
        if (result.outcome != GoldenSunObjB328WriterOutcome::Pending)
            continue;
        const bool same_frame = candidate.frame == writer_frame;
        const bool same_context = same_frame &&
            candidate.call_depth == writer_depth &&
            candidate.call_return_pc == writer_return_pc;
        std::uint32_t observed_f0_register_mask = f0_register_mask;
        if (route == GoldenSunObjB328WriterRoute::F0) {
            observed_f0_register_mask = 0u;
            for (unsigned reg = 0; reg < 16u; ++reg) {
                if (g_cpu.R[reg] == candidate.staging_address)
                    observed_f0_register_mask |= 1u << reg;
            }
        }
        const bool same_staging = route == GoldenSunObjB328WriterRoute::D4 &&
            candidate.staging_address == writer_staging;
        const bool f0_entry_identity = route == GoldenSunObjB328WriterRoute::F0 &&
            result.f0_entry_seen &&
            result.f0_entry_staging == candidate.staging_address;
        const bool f0_entry_context = f0_entry_identity &&
            result.f0_entry_frame == candidate.frame &&
            result.f0_entry_depth == candidate.call_depth &&
            result.f0_entry_return_pc == candidate.call_return_pc &&
            same_context;
        result.writer_seen = true;
        result.writer_frame = writer_frame;
        result.writer_depth = writer_depth;
        result.writer_return_pc = writer_return_pc;
        result.writer_staging = route == GoldenSunObjB328WriterRoute::F0 &&
                result.f0_entry_seen ? result.f0_entry_staging : writer_staging;
        result.slot = slot;
        result.attr0 = attr0;
        result.attr1 = attr1;
        result.attr2 = attr2;
        result.f0_register_mask = observed_f0_register_mask;
        if (route == GoldenSunObjB328WriterRoute::D4 && same_staging &&
            same_context) {
            result.outcome = GoldenSunObjB328WriterOutcome::Consumed;
        } else if (route == GoldenSunObjB328WriterRoute::F0 &&
                   f0_entry_context) {
            const bool attrs_match = candidate.attr_identity_valid &&
                candidate.expected_attr0 == attr0 &&
                candidate.expected_attr1 == attr1 &&
                candidate.expected_attr2 == attr2;
            result.outcome = attrs_match
                ? GoldenSunObjB328WriterOutcome::Consumed
                : GoldenSunObjB328WriterOutcome::AttrMismatch;
        } else if (route == GoldenSunObjB328WriterRoute::F0 &&
                   f0_entry_identity) {
            result.outcome = GoldenSunObjB328WriterOutcome::ContextMismatch;
        } else if (route == GoldenSunObjB328WriterRoute::F0 && same_frame) {
            // A writer with no matching 030038EC entry is retained as
            // observational evidence only.
            result.outcome = GoldenSunObjB328WriterOutcome::IdentityUnproven;
        } else if (same_frame || same_staging) {
            result.outcome = GoldenSunObjB328WriterOutcome::Unrelated;
        } else {
            continue;
        }
        ++stats.rejected_outcomes[route_index][static_cast<std::size_t>(
            result.outcome)];
        record_golden_sun_b328_rejected_sample(candidate, route, result);
    }
}

void record_golden_sun_b328_accepted_f0_sample(
    const GoldenSunObjB328AcceptedCandidate& candidate) {
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    if (stats.accepted_f0_sample_count >= stats.accepted_f0_samples.size()) {
        ++stats.accepted_f0_samples_dropped;
        return;
    }
    auto& sample = stats.accepted_f0_samples[
        stats.accepted_f0_sample_count++];
    sample.valid = true;
    sample.outcome = candidate.f0_outcome;
    sample.classification = candidate.classification;
    sample.frame = candidate.frame;
    sample.writer_frame = candidate.f0_writer_frame;
    sample.auth_epoch = candidate.auth_epoch;
    sample.staging_address = candidate.staging_address;
    sample.operand = candidate.operand;
    sample.call_depth = candidate.call_depth;
    sample.call_return_pc = candidate.call_return_pc;
    sample.writer_depth = candidate.f0_writer_depth;
    sample.writer_return_pc = candidate.f0_writer_return_pc;
    sample.target_address = candidate.f0_target_address;
    sample.f0_entry_seen = candidate.f0_entry_seen;
    sample.f0_entry_frame = candidate.f0_entry_frame;
    sample.f0_entry_depth = candidate.f0_entry_depth;
    sample.f0_entry_return_pc = candidate.f0_entry_return_pc;
    sample.f0_entry_staging = candidate.f0_entry_staging;
    sample.f0_register_mask = candidate.f0_register_mask;
    sample.expected_attr0 = candidate.expected_attr0;
    sample.expected_attr1 = candidate.expected_attr1;
    sample.expected_attr2 = candidate.expected_attr2;
    sample.slot = candidate.f0_slot;
    sample.attr0 = candidate.f0_attr0;
    sample.attr1 = candidate.f0_attr1;
    sample.attr2 = candidate.f0_attr2;
}

void record_golden_sun_b328_accepted_f0_writer(
    std::uint32_t target_address, int slot, std::uint16_t attr0,
    std::uint16_t attr1,
    std::uint16_t attr2, std::uint64_t writer_frame,
    std::uint32_t writer_depth, std::uint32_t writer_return_pc) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active()) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    for (auto& candidate : stats.candidates) {
        if (!candidate.valid || candidate.auth_epoch !=
            g_golden_sun_field_auth_epoch || candidate.f0_outcome !=
            GoldenSunObjB328WriterOutcome::Pending) continue;
        const bool same_frame = candidate.frame == writer_frame;
        const bool same_context = same_frame &&
            candidate.call_depth == writer_depth &&
            candidate.call_return_pc == writer_return_pc;
        std::uint32_t register_mask = 0u;
        for (unsigned reg = 0; reg < 16u; ++reg) {
            if (g_cpu.R[reg] == candidate.staging_address)
                register_mask |= 1u << reg;
        }
        const bool entry_identity = candidate.f0_entry_seen &&
            candidate.f0_entry_staging == candidate.staging_address;
        const bool entry_context = entry_identity &&
            candidate.f0_entry_frame == candidate.frame &&
            candidate.f0_entry_depth == candidate.call_depth &&
            candidate.f0_entry_return_pc == candidate.call_return_pc &&
            same_context;
        candidate.f0_writer_seen = true;
        candidate.f0_writer_frame = writer_frame;
        candidate.f0_writer_depth = writer_depth;
        candidate.f0_writer_return_pc = writer_return_pc;
        candidate.f0_target_address = target_address;
        candidate.f0_slot = slot;
        candidate.f0_attr0 = attr0;
        candidate.f0_attr1 = attr1;
        candidate.f0_attr2 = attr2;
        candidate.f0_register_mask = register_mask;
        if (entry_context) {
            const bool attrs_match = candidate.attr_identity_valid &&
                candidate.expected_attr0 == attr0 &&
                candidate.expected_attr1 == attr1 &&
                candidate.expected_attr2 == attr2;
            candidate.f0_outcome = attrs_match
                ? GoldenSunObjB328WriterOutcome::Consumed
                : GoldenSunObjB328WriterOutcome::AttrMismatch;
        } else if (entry_identity) {
            candidate.f0_outcome =
                GoldenSunObjB328WriterOutcome::ContextMismatch;
        } else if (same_frame) {
            candidate.f0_outcome = GoldenSunObjB328WriterOutcome::Unrelated;
        } else {
            candidate.f0_writer_seen = false;
            continue;
        }
        ++stats.accepted_f0_outcomes[static_cast<std::size_t>(
            candidate.f0_outcome)];
        record_golden_sun_b328_accepted_f0_sample(candidate);
    }
}

bool golden_sun_experimental_record_identity(std::uint32_t staging,
                                             std::uint32_t* record_base,
                                             bool* shadow) {
    constexpr std::uint32_t kRecordBase = 0x03002000u;
    constexpr std::uint32_t kRecordEnd = 0x030022E0u;
    constexpr std::uint32_t kRecordStride = 0x38u;
    if (!record_base || !shadow || staging < kRecordBase ||
        staging >= kRecordEnd) return false;
    const std::uint32_t offset = staging - kRecordBase;
    const std::uint32_t sub_offset = offset % kRecordStride;
    if (sub_offset != 0u && sub_offset != 0x0Cu) return false;
    *record_base = staging - sub_offset;
    *shadow = sub_offset == 0x0Cu;
    return true;
}

void remember_golden_sun_obj_f0_context(
    std::uint64_t frame, std::uint32_t depth, std::uint32_t return_pc,
    std::uint32_t staging, std::uint32_t record_base, bool shadow,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    if (g_golden_sun_obj_f0_context_frame != frame) {
        g_golden_sun_obj_f0_contexts = {};
        g_golden_sun_obj_f0_context_frame = frame;
    }
    GoldenSunObjF0Context* context = nullptr;
    for (auto& candidate : g_golden_sun_obj_f0_contexts) {
        if (candidate.valid && candidate.staging == staging &&
            candidate.depth == depth && candidate.return_pc == return_pc) {
            context = &candidate;
            break;
        }
    }
    if (!context) {
        for (auto& candidate : g_golden_sun_obj_f0_contexts) {
            if (!candidate.valid) {
                context = &candidate;
                break;
            }
        }
    }
    // More than one source in a frame is possible. If the bounded table ever
    // fills, leave later writes unclassified instead of recycling an identity.
    if (!context) return;
    *context = {true, frame, depth, return_pc, staging, record_base, shadow,
                attr0, attr1, attr2};
}

const GoldenSunObjF0Context* find_golden_sun_obj_f0_context(
    std::uint64_t frame, std::uint32_t depth, std::uint32_t return_pc,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    if (g_golden_sun_obj_f0_context_frame != frame) return nullptr;
    const GoldenSunObjF0Context* found = nullptr;
    for (const auto& candidate : g_golden_sun_obj_f0_contexts) {
        if (!candidate.valid || candidate.frame != frame ||
            candidate.depth != depth || candidate.return_pc != return_pc ||
            candidate.attr0 != attr0 || candidate.attr1 != attr1 ||
            candidate.attr2 != attr2) continue;
        if (found) return nullptr; // duplicate identity: fail closed
        found = &candidate;
    }
    return found;
}

void trace_golden_sun_experimental_cull(
    int slot, std::uint32_t source, std::uint32_t raw_x,
    std::uint32_t raw_y, int resolved_x, int resolved_y, unsigned shape,
    unsigned size, int width, int height, bool culled, const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled() || slot < 0 ||
        static_cast<std::size_t>(slot) >= g_golden_sun_experimental_cull_last.size() ||
        !reason) return;
    auto& last = g_golden_sun_experimental_cull_last[
        static_cast<std::size_t>(slot)];
    const bool unchanged = last.valid && last.source == source &&
        last.raw_x == raw_x && last.raw_y == raw_y &&
        last.resolved_x == resolved_x && last.resolved_y == resolved_y &&
        last.shape == shape && last.size == size && last.width == width &&
        last.height == height && last.culled == culled && last.reason &&
        std::strcmp(last.reason, reason) == 0;
    if (unchanged) return;
    last = {true, slot, source, raw_x, raw_y, resolved_x, resolved_y, shape,
            size, width, height, culled, reason};
    if (g_golden_sun_experimental_cull_logs >=
            kGoldenSunExperimentalCullLogLimit) {
        ++g_golden_sun_experimental_cull_logs_dropped;
        return;
    }
    ++g_golden_sun_experimental_cull_logs;
    std::fprintf(stderr,
                 "[wide-obj-experimental-cull] frame=%llu slot=%d "
                 "source=0x%08x raw=(%u,%u) resolved=(%d,%d) "
                 "shape=%u size=%u pixels=%dx%d decision=%s reason=%s\n",
                 static_cast<unsigned long long>(runtime_current_frame()),
                 slot, source, raw_x, raw_y, resolved_x, resolved_y, shape,
                 size, width, height, culled ? "culled" : "kept", reason);
}

void golden_sun_obj_f0_entry_capture(std::uint32_t entry_pc) {
    // EC is the exact generated entry immediately before the LDM that
    // destroys R6. The relocation-aware resolver authenticates its image and
    // base before capture; rendering and guest state are never modified.
    if (!golden_sun_func1dc8_writer_pc(
            entry_pc, gsr::Func1dc8WriterRoute::EC) ||
        (!golden_sun_wide_diagnostics_enabled() &&
         !golden_sun_experimental_fixes_enabled()) ||
        !golden_sun_expanded_obj_view_active()) return;
    const std::uint64_t frame = runtime_current_frame();
    const std::uint32_t depth = runtime_call_stack_depth();
    const std::uint32_t return_pc = golden_sun_obj_call_return_pc(depth);
    const std::uint32_t staging = g_cpu.R[6];
    link_golden_sun_b328_parentless_ec(
        frame, staging, depth, return_pc, entry_pc);
    if (golden_sun_experimental_fixes_enabled()) {
        std::uint16_t attr0 = 0, attr1 = 0, attr2 = 0;
        std::uint32_t record_base = 0;
        bool shadow = false;
        if (read_golden_sun_obj_staging_attrs(staging, &attr0, &attr1,
                                              &attr2) &&
            golden_sun_experimental_record_identity(
                staging, &record_base, &shadow)) {
            remember_golden_sun_obj_f0_context(
                frame, depth, return_pc, staging, record_base, shadow, attr0,
                attr1, attr2);
        }
    }
    // WIDE-01 body/shadow identity: EC fires unconditionally on every
    // active object, every frame (unlike D4, which only fires on a
    // successful, gated commit), and the ATTR-shaped bytes at this seam
    // are still mid-update from other per-frame writers -- logging every
    // EC entry flooded the cap and stalled scene transitions in an earlier
    // pass of this diagnostic (see the dated note below). Admission is two
    // narrow, bounded cases, not a general trace: (a) the small/fixed
    // shadow-candidate signature (tile=0, shape=1, size=0) identified in
    // WIDE-01_NPC_IDENTITY.md, and (b) a handful of record slots (index
    // < kGoldenSunObjCommitOrderBodySlotLimit, computed directly from
    // `staging` -- this is a body event, so `staging` is the record base
    // with no +0x0C offset) -- added to capture independent body-sprite
    // ground truth for the scroll-transform check in
    // WIDE-01_NPC_IDENTITY.md, since D4's own attrs are read from the same
    // record and cannot serve as ground truth for that question. Anything
    // outside those two cases is silently skipped (not deduped -- never
    // recorded at all), keeping volume bounded regardless of how often EC
    // itself fires. Slot is unresolved at this seam (only known later, at
    // F0), so this reports slot=-1, truthfully. Signed logical X/Y is
    // likewise not available without threading through the D4 route's
    // staging-provenance lookup; left invalid rather than approximated.
    {
        std::uint16_t ec_attr0 = 0, ec_attr1 = 0, ec_attr2 = 0;
        if (read_golden_sun_obj_staging_attrs(staging, &ec_attr0, &ec_attr1,
                                              &ec_attr2)) {
            const std::uint16_t ec_tile = ec_attr2 & 0x3FFu;
            const std::uint16_t ec_shape =
                static_cast<std::uint16_t>((ec_attr0 >> 14) & 0x3u);
            const std::uint16_t ec_size =
                static_cast<std::uint16_t>((ec_attr1 >> 14) & 0x3u);
            const bool ec_is_shadow_signature =
                ec_tile == 0u && ec_shape == 1u && ec_size == 0u;
            constexpr std::uint32_t kBodyRecordBase = 0x03002000u;
            constexpr std::uint32_t kBodyRecordStride = 0x38u;
            constexpr std::uint32_t kGoldenSunObjCommitOrderBodySlotLimit = 6u;
            const bool ec_is_sampled_body_slot =
                staging >= kBodyRecordBase &&
                ((staging - kBodyRecordBase) % kBodyRecordStride) == 0u &&
                ((staging - kBodyRecordBase) / kBodyRecordStride) <
                    kGoldenSunObjCommitOrderBodySlotLimit;
            if (ec_is_shadow_signature || ec_is_sampled_body_slot) {
                note_golden_sun_obj_commit_order(
                    GoldenSunObjCommitOrderRoute::EC, staging, -1, ec_attr0,
                    ec_attr1, ec_attr2, false, 0, false, 0);
            }
        }
    }
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    for (auto& candidate : stats.candidates) {
        if (!candidate.valid || candidate.auth_epoch !=
            g_golden_sun_field_auth_epoch || candidate.f0_entry_seen ||
            candidate.f0_outcome != GoldenSunObjB328WriterOutcome::Pending ||
            candidate.staging_address != staging) continue;
        candidate.f0_entry_seen = true;
        candidate.f0_entry_frame = frame;
        candidate.f0_entry_depth = depth;
        candidate.f0_entry_return_pc = return_pc;
        candidate.f0_entry_staging = staging;
    }
    for (auto& candidate : stats.rejected) {
        if (!candidate.valid || candidate.auth_epoch !=
            g_golden_sun_field_auth_epoch) continue;
        auto& result = candidate.routes[static_cast<std::size_t>(
            GoldenSunObjB328WriterRoute::F0)];
        if (result.outcome != GoldenSunObjB328WriterOutcome::Pending ||
            result.f0_entry_seen || candidate.staging_address != staging)
            continue;
        result.f0_entry_seen = true;
        result.f0_entry_frame = frame;
        result.f0_entry_depth = depth;
        result.f0_entry_return_pc = return_pc;
        result.f0_entry_staging = staging;
    }
}

void golden_sun_oam_shadow_write_observer(std::uint32_t writer_pc,
                                          std::uint32_t address,
                                          std::uint32_t size) {
    // F0's register identity is captured at the authenticated EC entry,
    // before its LDM overwrites R6. The write observer only supplies committed
    // slot/ATTR identity; correlation is completed against that snapshot.
    // This function is also the WIDE-01 experimental off-screen cull safety
    // net's hook point (see below), so it must run when either diagnostics
    // or the "Experimental Fixes" toggle is on -- not diagnostics alone. All
    // diagnostics-only work inside self-gates on
    // golden_sun_wide_diagnostics_enabled() independently.
    if ((!golden_sun_wide_diagnostics_enabled() &&
         !golden_sun_experimental_fixes_enabled()) ||
        !golden_sun_expanded_obj_view_active() ||
        !golden_sun_func1dc8_writer_pc(
            writer_pc, gsr::Func1dc8WriterRoute::F0) || size == 0u) return;
    if (address < gsr::widescreen::kGoldenSunOamShadowStart ||
        address >= gsr::widescreen::kGoldenSunOamShadowEnd ||
        ((address - gsr::widescreen::kGoldenSunOamShadowStart) %
            gsr::widescreen::kGoldenSunOamShadowSlotBytes) != 4u)
        return;
    const std::uint32_t offset = address -
        gsr::widescreen::kGoldenSunOamShadowStart;
    const int slot = static_cast<int>(offset /
        gsr::widescreen::kGoldenSunOamShadowSlotBytes);
    const std::uint32_t slot_address =
        gsr::widescreen::kGoldenSunOamShadowStart +
        static_cast<std::uint32_t>(slot) *
            gsr::widescreen::kGoldenSunOamShadowSlotBytes;
    // WIDE-01 experimental off-screen cull safety net (launcher's
    // "Experimental Fixes" toggle only -- normal play, including plain
    // widescreen diagnostics with the toggle off, is byte-for-byte
    // unaffected). Applies to every committed OAM entry this observer sees,
    // body and shadow alike, using the exact same rule and the exact same
    // generous-but-wrap-safe margin either way -- there is no separate
    // "is this a shadow" branch here, which is what keeps a body and its
    // shadow from ever being culled as separate decisions: both are always
    // committed via this same F0 route, so both always get evaluated by the
    // identical predicate, and in practice never disagree because a shadow
    // never sits far enough from its body to cross the margin alone. See
    // docs/issues/WIDE-01_NPC_IDENTITY.md ("The off-screen rule") for the
    // evidence this is built on: the just-read ATTR0/ATTR1 are already the
    // on-screen coordinate (ground-truth confirmed, no scroll subtraction),
    // so this reads directly from the bytes already fetched below rather
    // than re-deriving anything from the 0x03002000 record -- it never
    // touches that range, so the scene-load code-residency window documented
    // in WIDE-01_ENTITY_PRODUCER_FINDINGS.md cannot affect it.
    const auto writer_frame = runtime_current_frame();
    const auto writer_depth = runtime_call_stack_depth();
    const auto writer_return_pc = golden_sun_obj_call_return_pc(writer_depth);
    const std::uint16_t committed_attr0 = bus_read_u16(slot_address);
    const std::uint16_t committed_attr1 = bus_read_u16(slot_address + 2u);
    const std::uint16_t committed_attr2 = bus_read_u16(slot_address + 4u);
    if (golden_sun_wide_diagnostics_enabled()) {
        // Capture the committed ATTR payload before the optional experimental
        // culler can mark it hidden; diagnostics must describe the guest
        // write, not our later presentation-only change.
        link_golden_sun_b328_parentless_f0(
            writer_frame, writer_depth, writer_return_pc, writer_pc,
            slot_address, slot, bus_read_u16(slot_address),
            bus_read_u16(slot_address + 2u),
            bus_read_u16(slot_address + 4u));
    }
    if (golden_sun_experimental_fixes_enabled()) {
        const std::uint16_t cull_attr0 = committed_attr0;
        const std::uint16_t cull_attr1 = committed_attr1;
        const std::uint16_t cull_attr2 = committed_attr2;
        // Already disabled (or an empty/dormant entry) -- nothing to do.
        // Matches the existing dormant-entry recognition in
        // record_golden_sun_obj_y_transition above.
        const bool cull_affine = (cull_attr0 & 0x0100u) != 0u;
        const bool cull_already_disabled =
            !cull_affine && (cull_attr0 & 0x0200u) != 0u;
        if (!cull_already_disabled &&
            !(cull_attr0 == 0u && cull_attr1 == 0u && cull_attr2 == 0u)) {
            const unsigned cull_shape =
                static_cast<unsigned>((cull_attr0 >> 14) & 0x3u);
            const unsigned cull_size =
                static_cast<unsigned>((cull_attr1 >> 14) & 0x3u);
            int width = 0, height = 0;
            const std::uint32_t raw_x = cull_attr1 & 0x1FFu;
            const std::uint32_t raw_y = cull_attr0 & 0xFFu;
            int resolved_x = 0, resolved_y = 0;
            std::uint32_t source = 0;
            bool should_cull = false;
            const char* reason = "source-unavailable";
            const bool dimensions_valid =
                gsr::widescreen::golden_sun_obj_dimensions(
                    cull_shape, cull_size, &width, &height);
            const auto* context = find_golden_sun_obj_f0_context(
                writer_frame, writer_depth, writer_return_pc, cull_attr0,
                cull_attr1, cull_attr2);
            if (!dimensions_valid) {
                reason = "invalid-shape-size";
            } else if (cull_affine) {
                // Affine and double-size sprites need transformed bounds;
                // leaving them visible is safer than estimating them.
                reason = "affine-or-double-size";
            } else {
                bool source_is_shadow = false;
                if (!context ||
                    !golden_sun_experimental_record_identity(
                        context->staging, &source, &source_is_shadow)) {
                reason = "source-unavailable";
                } else {
                const std::uint32_t body_source = source;
                auto* staging = find_golden_sun_obj_staging(body_source);
                std::uint16_t body_attr0 = 0, body_attr1 = 0, body_attr2 = 0;
                const bool body_attrs_valid = read_golden_sun_obj_staging_attrs(
                    body_source, &body_attr0, &body_attr1, &body_attr2);
                const bool body_dimensions_valid = body_attrs_valid &&
                    gsr::widescreen::golden_sun_obj_dimensions(
                        (body_attr0 >> 14) & 0x3u, (body_attr1 >> 14) & 0x3u,
                        &width, &height);
                const bool body_provenance_valid = staging && staging->valid &&
                    staging->x_valid && staging->y_valid &&
                    staging->auth_epoch == g_golden_sun_field_auth_epoch &&
                    staging->frame == writer_frame && !((body_attr0 & 0x0100u) != 0u);
                int checked_x = 0, checked_y = 0;
                const bool body_raw_matches = body_provenance_valid &&
                    gsr::widescreen::golden_sun_experimental_resolve_oam_x(
                        body_attr1 & 0x1FFu, true, staging->logical_x,
                        &checked_x) &&
                    gsr::widescreen::golden_sun_experimental_resolve_oam_y(
                        body_attr0 & 0xFFu, true, staging->logical_y,
                        &checked_y);
                const bool body_identity_matches = body_provenance_valid &&
                    (source_is_shadow ||
                     (body_attr0 == cull_attr0 && body_attr1 == cull_attr1 &&
                      body_attr2 == cull_attr2));
                if (!body_attrs_valid || !body_dimensions_valid ||
                    !body_provenance_valid || !body_raw_matches ||
                    !body_identity_matches) {
                    reason = "placement-unavailable";
                } else {
                    resolved_x = staging->logical_x;
                    resolved_y = staging->logical_y;
                    should_cull =
                        gsr::widescreen::golden_sun_experimental_sprite_fully_offscreen_top_left(
                            resolved_x, resolved_y, width, height,
                            g_golden_sun_wide_extra_left,
                            g_golden_sun_wide_extra_right,
                            g_golden_sun_wide_extra_top,
                            g_golden_sun_wide_extra_bottom);
                    reason = source_is_shadow ? "paired-body-placement"
                                              : "exact-placement";
                }
                }
            }
            trace_golden_sun_experimental_cull(
                slot, source, raw_x, raw_y, resolved_x, resolved_y, cull_shape,
                cull_size, width, height, should_cull, reason);
            if (should_cull) {
                const std::uint16_t hidden_attr0 = static_cast<std::uint16_t>(
                    cull_attr0 | static_cast<std::uint16_t>(0x0200u));
                g_golden_sun_experimental_cull_write_in_progress = true;
                bus_write_u16(slot_address, hidden_attr0);
                g_golden_sun_experimental_cull_write_in_progress = false;
            }
        }
    }
    record_golden_sun_b328_accepted_f0_writer(
        slot_address, slot, bus_read_u16(slot_address),
        bus_read_u16(slot_address + 2u),
        bus_read_u16(slot_address + 4u), writer_frame, writer_depth,
        writer_return_pc);
    record_golden_sun_b328_rejected_writer(
        GoldenSunObjB328WriterRoute::F0, 0u, slot,
        bus_read_u16(slot_address), bus_read_u16(slot_address + 2u),
        bus_read_u16(slot_address + 4u), writer_frame, writer_depth,
        writer_return_pc, 0u);
    // WIDE-01 body/shadow identity: F0 fires on every committed OAM-shadow
    // write for every active object, and ATTR0/ATTR1 carry the live raw
    // Y/X, so an unfiltered trace changes almost every frame for every
    // moving object -- logging it unconditionally flooded the cap and
    // stalled scene transitions in an earlier pass of this diagnostic.
    // D4 already gives a complete, low-volume trace of body writes with
    // sequence numbers in the same shared per-frame counter, so this route
    // is restricted to two narrow, bounded cases: (a) the small/fixed
    // shadow-candidate signature (tile=0, shape=1, size=0) identified in
    // WIDE-01_NPC_IDENTITY.md, and (b) a handful of destination OAM slots
    // (index < kGoldenSunObjCommitOrderBodySlotLimit) -- added to capture
    // independent, ground-truth body-sprite ATTR for the scroll-transform
    // check in WIDE-01_NPC_IDENTITY.md, since D4's own attrs are read from
    // the record itself and cannot serve as ground truth for that
    // question. This destination-slot carve-out does not use the same
    // numbering as the EC route's record-slot carve-out (OAM destination
    // slots are reused/reassigned independently of record slots, per the
    // existing findings), so the two do not guarantee a 1:1 pairing on
    // the same object every time -- only frames where both happen to
    // admit the same object are usable for that cross-check, and that is
    // determined empirically in the doc, not assumed here. The originating
    // source record address is not resolved at this seam (only the EC
    // entry captured it); reported as source=0, never fabricated. Signed
    // logical X/Y is likewise not available here (the ATTR bytes are
    // already hardware-shaped, a different concept from the pre-truncation
    // logical coordinate the D4 route reports).
    {
        const std::uint16_t f0_attr0 = bus_read_u16(slot_address);
        const std::uint16_t f0_attr1 = bus_read_u16(slot_address + 2u);
        const std::uint16_t f0_attr2 = bus_read_u16(slot_address + 4u);
        const std::uint16_t f0_tile = f0_attr2 & 0x3FFu;
        const std::uint16_t f0_shape =
            static_cast<std::uint16_t>((f0_attr0 >> 14) & 0x3u);
        const std::uint16_t f0_size =
            static_cast<std::uint16_t>((f0_attr1 >> 14) & 0x3u);
        const bool f0_is_shadow_signature =
            f0_tile == 0u && f0_shape == 1u && f0_size == 0u;
        constexpr int kGoldenSunObjCommitOrderBodySlotLimit = 6;
        const bool f0_is_sampled_body_slot =
            slot < kGoldenSunObjCommitOrderBodySlotLimit;
        if (f0_is_shadow_signature || f0_is_sampled_body_slot) {
            note_golden_sun_obj_commit_order(
                GoldenSunObjCommitOrderRoute::F0, 0u, slot, f0_attr0,
                f0_attr1, f0_attr2, false, 0, false, 0);
        }
    }
}

void expire_golden_sun_b328_rejected_candidates() {
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    for (auto& candidate : stats.rejected) {
        if (!candidate.valid) continue;
        for (std::size_t route = 0; route < candidate.routes.size(); ++route) {
            auto& result = candidate.routes[route];
            if (result.outcome != GoldenSunObjB328WriterOutcome::Pending)
                continue;
            result.outcome = GoldenSunObjB328WriterOutcome::Expired;
            ++stats.rejected_outcomes[route][static_cast<std::size_t>(
                result.outcome)];
            record_golden_sun_b328_rejected_sample(
                candidate, static_cast<GoldenSunObjB328WriterRoute>(route),
                result);
        }
        candidate.valid = false;
    }
    for (auto& candidate : stats.candidates) {
        if (!candidate.valid) continue;
        if (candidate.f0_outcome == GoldenSunObjB328WriterOutcome::Pending) {
            candidate.f0_outcome = GoldenSunObjB328WriterOutcome::Expired;
            ++stats.accepted_f0_outcomes[static_cast<std::size_t>(
                candidate.f0_outcome)];
            record_golden_sun_b328_accepted_f0_sample(candidate);
        }
        candidate.valid = false;
    }
}

void record_golden_sun_b328_handoff(std::uint32_t staging_address,
                                    std::uint64_t frame,
                                    bool attr_identity_valid) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active() || !attr_identity_valid) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    for (auto it = stats.candidates.rbegin(); it != stats.candidates.rend();
         ++it) {
        if (!it->valid || it->handoff_seen ||
            it->staging_address != staging_address || it->frame != frame ||
            it->auth_epoch != g_golden_sun_field_auth_epoch) continue;
        it->handoff_seen = true;
        ++stats.handoffs[static_cast<std::size_t>(it->classification)];
        return;
    }
}

void report_golden_sun_b328_parentless_tokens(std::uint64_t frame,
                                              const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled() || !reason) return;
    for (const auto& token : g_golden_sun_obj_b328_parentless_tokens) {
        if (!token.valid) continue;
        std::fprintf(
            stderr,
            "[wide-obj-y-parentless-token] frame=%llu auth_epoch=%llu "
            "reason=%s sequence=%llu b328_entry_pc=0x%08x "
            "staging=0x%08x operand=%d r6=0x%08x r7=0x%08x depth=%u "
            "return_pc=0x%08x attr_valid=%u attr0=0x%04x attr1=0x%04x "
            "attr2=0x%04x ec_seen=%u ec_entry_pc=0x%08x ec_r6=0x%08x "
            "ec_r7=0x%08x ec_source=0x%08x ec_reason=%s f0_seen=%u "
            "f0_entry_pc=0x%08x f0_r6=0x%08x f0_r7=0x%08x "
            "f0_source=0x%08x target=0x%08x slot=%d f0_attr0=0x%04x "
            "f0_attr1=0x%04x f0_attr2=0x%04x raw_y=%u f0_reason=%s\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(token.auth_epoch), reason,
            static_cast<unsigned long long>(token.sequence), token.entry_pc,
            token.staging, token.operand, token.r6, token.r7,
            token.call_depth, token.return_pc, token.attr_valid ? 1u : 0u,
            static_cast<unsigned>(token.attr0),
            static_cast<unsigned>(token.attr1),
            static_cast<unsigned>(token.attr2), token.ec_seen ? 1u : 0u,
            token.ec_entry_pc, token.ec_r6, token.ec_r7, token.ec_source,
            token.ec_reason, token.f0_seen ? 1u : 0u, token.f0_entry_pc,
            token.f0_r6, token.f0_r7, token.f0_source, token.f0_target,
            token.f0_slot, static_cast<unsigned>(token.f0_attr0),
            static_cast<unsigned>(token.f0_attr1),
            static_cast<unsigned>(token.f0_attr2),
            static_cast<unsigned>(token.f0_attr0 & 0xFFu), token.f0_reason);
    }
    std::fprintf(stderr,
                 "[wide-obj-y-parentless-token-summary] frame=%llu "
                 "auth_epoch=%llu reason=%s dropped=%llu\n",
                 static_cast<unsigned long long>(frame),
                 static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                 reason,
                 static_cast<unsigned long long>(
                     g_golden_sun_obj_b328_parentless_dropped));
}

void report_golden_sun_obj_b328_diagnostics(std::uint64_t frame,
                                            const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    auto& stats = g_golden_sun_obj_b328_diagnostics;
    report_golden_sun_b328_parentless_tokens(frame, reason);
    expire_golden_sun_b328_rejected_candidates();
    for (auto& candidate : stats.candidates) {
        if (!candidate.valid) continue;
        if (!candidate.handoff_seen) {
            ++stats.no_handoff[static_cast<std::size_t>(
                candidate.classification)];
        }
        candidate.valid = false;
    }
    std::fprintf(stderr,
                 "[wide-obj-y-b328-summary] frame=%llu auth_epoch=%llu "
                 "reason=%s candidates=%llu exact=%llu mismatch=%llu "
                 "no_parent=%llu mismatch_frame=%llu mismatch_depth=%llu "
                 "mismatch_return=%llu accepted_exact=%llu accepted_mismatch=%llu "
                 "accepted_no_parent=%llu handoff_exact=%llu "
                 "handoff_mismatch=%llu handoff_no_parent=%llu "
                 "no_handoff_exact=%llu no_handoff_mismatch=%llu "
                 "no_handoff_no_parent=%llu tracking_dropped=%llu "
                 "samples=%zu samples_dropped=%llu\n",
                 static_cast<unsigned long long>(frame),
                 static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                 reason,
                 static_cast<unsigned long long>(stats.classifications[0] +
                                                 stats.classifications[1] +
                                                 stats.classifications[2]),
                 static_cast<unsigned long long>(stats.classifications[0]),
                 static_cast<unsigned long long>(stats.classifications[1]),
                 static_cast<unsigned long long>(stats.classifications[2]),
                 static_cast<unsigned long long>(stats.mismatch_frame),
                 static_cast<unsigned long long>(stats.mismatch_call_depth),
                 static_cast<unsigned long long>(stats.mismatch_return_pc),
                 static_cast<unsigned long long>(stats.accepted[0]),
                 static_cast<unsigned long long>(stats.accepted[1]),
                 static_cast<unsigned long long>(stats.accepted[2]),
                 static_cast<unsigned long long>(stats.handoffs[0]),
                 static_cast<unsigned long long>(stats.handoffs[1]),
                 static_cast<unsigned long long>(stats.handoffs[2]),
                 static_cast<unsigned long long>(stats.no_handoff[0]),
                 static_cast<unsigned long long>(stats.no_handoff[1]),
                 static_cast<unsigned long long>(stats.no_handoff[2]),
                 static_cast<unsigned long long>(stats.accepted_tracking_dropped),
                 stats.sample_count,
                 static_cast<unsigned long long>(stats.samples_dropped));
    std::fprintf(
        stderr,
        "[wide-obj-y-b328-writer-summary] frame=%llu auth_epoch=%llu "
        "rejected=%llu d4_consumed=%llu d4_unrelated=%llu "
        "d4_expired=%llu f0_consumed=%llu f0_unrelated=%llu "
        "f0_identity_unproven=%llu f0_expired=%llu "
        "f0_context_mismatch=%llu f0_attr_mismatch=%llu tracking_dropped=%llu "
        "samples=%zu samples_dropped=%llu\n",
        static_cast<unsigned long long>(frame),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        static_cast<unsigned long long>(stats.rejected_total),
        static_cast<unsigned long long>(stats.rejected_outcomes[0][1]),
        static_cast<unsigned long long>(stats.rejected_outcomes[0][2]),
        static_cast<unsigned long long>(stats.rejected_outcomes[0][4]),
        static_cast<unsigned long long>(stats.rejected_outcomes[1][1]),
        static_cast<unsigned long long>(stats.rejected_outcomes[1][2]),
        static_cast<unsigned long long>(stats.rejected_outcomes[1][3]),
        static_cast<unsigned long long>(stats.rejected_outcomes[1][4]),
        static_cast<unsigned long long>(stats.rejected_outcomes[1][5]),
        static_cast<unsigned long long>(stats.rejected_outcomes[1][6]),
        static_cast<unsigned long long>(stats.rejected_tracking_dropped),
        stats.rejected_sample_count,
        static_cast<unsigned long long>(stats.rejected_samples_dropped));
    for (std::size_t i = 0; i < stats.rejected_sample_count; ++i) {
        const auto& sample = stats.rejected_samples[i];
        std::fprintf(
            stderr,
            "[wide-obj-y-b328-writer-sample] frame=%llu writer_frame=%llu "
            "auth_epoch=%llu route=%s outcome=%s classification=%s "
            "mismatch_flags=0x%x "
            "staging=0x%08x writer_staging=0x%08x operand=%d "
            "depth=%u return_pc=0x%08x writer_depth=%u "
            "writer_return_pc=0x%08x slot=%d attr0=0x%04x attr1=0x%04x "
            "attr2=0x%04x f0_register_mask=0x%04x "
            "f0_entry_seen=%u f0_entry_frame=%llu f0_entry_depth=%u "
            "f0_entry_return_pc=0x%08x f0_entry_staging=0x%08x "
            "expected_attr0=0x%04x expected_attr1=0x%04x expected_attr2=0x%04x\n",
            static_cast<unsigned long long>(sample.frame),
            static_cast<unsigned long long>(sample.writer_frame),
            static_cast<unsigned long long>(sample.auth_epoch),
            golden_sun_obj_b328_writer_route_name(sample.route),
            golden_sun_obj_b328_writer_outcome_name(sample.outcome),
            golden_sun_obj_b328_parent_classification_name(
                sample.classification), sample.mismatch_flags,
            sample.staging_address,
            sample.writer_staging, sample.operand, sample.call_depth,
            sample.call_return_pc, sample.writer_depth,
            sample.writer_return_pc, sample.slot,
            static_cast<unsigned>(sample.attr0),
            static_cast<unsigned>(sample.attr1),
            static_cast<unsigned>(sample.attr2),
            static_cast<unsigned>(sample.f0_register_mask),
            sample.f0_entry_seen ? 1u : 0u,
            static_cast<unsigned long long>(sample.f0_entry_frame),
            sample.f0_entry_depth, sample.f0_entry_return_pc,
            sample.f0_entry_staging,
            static_cast<unsigned>(sample.expected_attr0),
            static_cast<unsigned>(sample.expected_attr1),
            static_cast<unsigned>(sample.expected_attr2));
    }
    std::fprintf(
        stderr,
        "[wide-obj-y-b328-accepted-f0-summary] frame=%llu "
        "auth_epoch=%llu accepted=%llu consumed=%llu unrelated=%llu "
        "expired=%llu context_mismatch=%llu attr_mismatch=%llu "
        "tracking_dropped=%llu samples=%zu samples_dropped=%llu\n",
        static_cast<unsigned long long>(frame),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        static_cast<unsigned long long>(stats.accepted_f0_outcomes[1] +
                                         stats.accepted_f0_outcomes[2] +
                                         stats.accepted_f0_outcomes[4] +
                                         stats.accepted_f0_outcomes[5] +
                                         stats.accepted_f0_outcomes[6]),
        static_cast<unsigned long long>(stats.accepted_f0_outcomes[1]),
        static_cast<unsigned long long>(stats.accepted_f0_outcomes[2]),
        static_cast<unsigned long long>(stats.accepted_f0_outcomes[4]),
        static_cast<unsigned long long>(stats.accepted_f0_outcomes[5]),
        static_cast<unsigned long long>(stats.accepted_f0_outcomes[6]),
        static_cast<unsigned long long>(stats.accepted_f0_tracking_dropped),
        stats.accepted_f0_sample_count,
        static_cast<unsigned long long>(stats.accepted_f0_samples_dropped));
    for (std::size_t i = 0; i < stats.accepted_f0_sample_count; ++i) {
        const auto& sample = stats.accepted_f0_samples[i];
        std::fprintf(
            stderr,
            "[wide-obj-y-b328-accepted-f0-sample] frame=%llu "
            "writer_frame=%llu auth_epoch=%llu outcome=%s "
            "classification=%s staging=0x%08x operand=%d "
            "depth=%u return_pc=0x%08x writer_depth=%u "
            "writer_return_pc=0x%08x target=0x%08x slot=%d "
            "attr0=0x%04x attr1=0x%04x "
            "attr2=0x%04x f0_register_mask=0x%04x f0_entry_seen=%u "
            "f0_entry_frame=%llu f0_entry_depth=%u "
            "f0_entry_return_pc=0x%08x f0_entry_staging=0x%08x "
            "expected_attr0=0x%04x expected_attr1=0x%04x "
            "expected_attr2=0x%04x\n",
            static_cast<unsigned long long>(sample.frame),
            static_cast<unsigned long long>(sample.writer_frame),
            static_cast<unsigned long long>(sample.auth_epoch),
            golden_sun_obj_b328_writer_outcome_name(sample.outcome),
            golden_sun_obj_b328_parent_classification_name(
            sample.classification), sample.staging_address, sample.operand,
            sample.call_depth, sample.call_return_pc, sample.writer_depth,
            sample.writer_return_pc, sample.target_address, sample.slot,
            static_cast<unsigned>(sample.attr0),
            static_cast<unsigned>(sample.attr1),
            static_cast<unsigned>(sample.attr2), sample.f0_register_mask,
            sample.f0_entry_seen ? 1u : 0u,
            static_cast<unsigned long long>(sample.f0_entry_frame),
            sample.f0_entry_depth, sample.f0_entry_return_pc,
            sample.f0_entry_staging,
            static_cast<unsigned>(sample.expected_attr0),
            static_cast<unsigned>(sample.expected_attr1),
            static_cast<unsigned>(sample.expected_attr2));
    }
    static constexpr const char* kMismatchNames[] = {
        "frame", "call-depth", "return-pc"};
    for (std::size_t i = 0; i < stats.sample_count; ++i) {
        const auto& sample = stats.samples[i];
        std::fprintf(stderr,
                     "[wide-obj-y-b328-sample] frame=%llu auth_epoch=%llu "
                     "classification=%s staging=0x%08x operand=%d "
                     "mismatch=",
                     static_cast<unsigned long long>(sample.frame),
                     static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                     golden_sun_obj_b328_parent_classification_name(
                         sample.classification), sample.staging_address,
                     sample.operand);
        bool first = true;
        for (unsigned bit = 0; bit < 3u; ++bit) {
            if ((sample.mismatch_flags & (1u << bit)) == 0u) continue;
            std::fprintf(stderr, "%s%s", first ? "" : ",",
                         kMismatchNames[bit]);
            first = false;
        }
        if (first) std::fputs("none", stderr);
        std::fprintf(stderr,
                     " parent_frame=%llu parent_depth=%u parent_return_pc=0x%08x\n",
                     static_cast<unsigned long long>(sample.parent_frame),
                     sample.parent_call_depth, sample.parent_return_pc);
    }
}

bool read_golden_sun_obj_stack_word(std::uint32_t sp, std::uint32_t offset,
                                    std::uint32_t* out) {
    constexpr std::uint32_t kIwramStart = 0x03000000u;
    constexpr std::uint32_t kIwramEnd = 0x03008000u;
    if (!out || (sp & 3u) != 0u || (offset != 4u && offset != 0x18u) ||
        sp < kIwramStart || sp > kIwramEnd - 4u - offset) return false;
    *out = bus_read_u32(sp + offset);
    return true;
}

void record_golden_sun_signed_y_cull(std::uint32_t pc,
                                     std::int32_t operand) {
    if (!golden_sun_expanded_obj_view_active() ||
        (pc != 0x0800B3E6u && pc != 0x0800B3ECu &&
         pc != 0x0800C702u && pc != 0x0800C708u)) return;
    GoldenSunObjSignedCullObservation observation{};
    observation.valid = true;
    observation.frame = runtime_current_frame();
    observation.pc = pc;
    observation.operand = operand;
    observation.r7 = g_cpu.R[7];
    observation.r10 = g_cpu.R[10];
    observation.r11 = g_cpu.R[11];
    observation.sp = g_cpu.R[13];
    observation.lr = g_cpu.R[14];
    observation.call_depth = runtime_call_stack_depth();
    observation.call_return_pc = golden_sun_obj_call_return_pc(
        observation.call_depth);
    observation.sequence = ++g_golden_sun_obj_signed_cull_sequence;
    g_golden_sun_obj_signed_cull_observations[
        pc == 0x0800B3E6u ? 0u : pc == 0x0800B3ECu ? 1u
        : pc == 0x0800C702u ? 2u : 3u] = observation;
}

GoldenSunObjYCorrelation capture_golden_sun_obj_y_correlation(
    std::int32_t operand) {
    GoldenSunObjYCorrelation result{};
    if (!golden_sun_expanded_obj_view_active() || operand < 160 ||
        operand > 199) return result;
    result.valid = true;
    result.b328_pc = 0x0800B328u;
    result.b328_operand = operand;
    result.b328_r1 = g_cpu.R[1];
    result.b328_r3 = g_cpu.R[3];
    result.b328_r11 = g_cpu.R[11];
    result.b328_r6 = g_cpu.R[6];
    result.b328_r7 = g_cpu.R[7];
    result.b328_sp = g_cpu.R[13];
    result.b328_lr = g_cpu.R[14];
    result.b328_call_depth = runtime_call_stack_depth();
    result.b328_call_return_pc = golden_sun_obj_call_return_pc(
        result.b328_call_depth);
    result.stack_inputs_valid =
        read_golden_sun_obj_stack_word(g_cpu.R[13], 4u,
                                       &result.b328_stack_sp_plus4) &&
        read_golden_sun_obj_stack_word(g_cpu.R[13], 0x18u,
                                       &result.b328_stack_sp_plus18);
    for (const auto& parent : g_golden_sun_obj_b27e_parent_routes) {
        if (parent.valid && parent.staging_address == result.b328_r7 &&
            parent.frame == runtime_current_frame() &&
            parent.call_depth == result.b328_call_depth &&
            parent.call_return_pc == result.b328_call_return_pc) {
            result.parent = parent;
            break;
        }
    }
    const std::uint32_t pcs[] = {
        0x0800B3E6u, 0x0800B3ECu, 0x0800C702u, 0x0800C708u};
    for (std::size_t i = 0; i < result.preceding.size(); ++i) {
        const auto& candidate = g_golden_sun_obj_signed_cull_observations[i];
        if (candidate.valid && candidate.frame == runtime_current_frame() &&
            candidate.call_depth == result.b328_call_depth &&
            candidate.call_return_pc == result.b328_call_return_pc &&
            candidate.sequence <= g_golden_sun_obj_signed_cull_sequence) {
            result.preceding[i] = candidate;
        } else {
            result.preceding[i].pc = pcs[i];
        }
    }
    return result;
}

void trace_golden_sun_obj_y_cull_decision(
    std::uint32_t original_decision, std::uint32_t final_decision,
    bool overridden, std::int32_t operand) {
    // Only the newly visible bottom band reaches this diagnostic. The
    // staging address is the transient object identity available before OAM
    // handoff; the register and call fields make parentless routes explicit
    // without retaining guest payload bytes.
    if (!golden_sun_wide_diagnostics_enabled() ||
        !golden_sun_expanded_obj_view_active() || operand < 160 ||
        operand > 199) return;
    const auto correlation = capture_golden_sun_obj_y_correlation(operand);
    GoldenSunObjYCullDecisionKey key{};
    key.staging_address = g_cpu.R[7];
    key.frame = runtime_current_frame();
    key.operand = operand;
    key.original_decision = original_decision;
    key.final_decision = final_decision;
    key.r1 = g_cpu.R[1];
    key.r3 = g_cpu.R[3];
    key.r11 = g_cpu.R[11];
    for (const auto& seen : g_golden_sun_obj_y_cull_decision_keys) {
        if (seen.valid && seen.staging_address == key.staging_address &&
            seen.frame == key.frame && seen.operand == key.operand &&
            seen.original_decision == key.original_decision &&
            seen.final_decision == key.final_decision && seen.r1 == key.r1 &&
            seen.r3 == key.r3 && seen.r11 == key.r11) return;
    }
    unsigned staging_records = 0;
    for (const auto& seen : g_golden_sun_obj_y_cull_decision_keys) {
        if (seen.valid && seen.staging_address == key.staging_address)
            ++staging_records;
    }
    if (g_golden_sun_obj_y_cull_decision_logs_in_epoch >=
            kGoldenSunObjYCorrelationLogLimitPerEpoch ||
        staging_records >= kGoldenSunObjYCullDecisionPerStagingLimit) {
        ++g_golden_sun_obj_y_cull_decision_logs_dropped;
        return;
    }
    for (auto& seen : g_golden_sun_obj_y_cull_decision_keys) {
        if (!seen.valid) {
            seen = key;
            seen.valid = true;
            break;
        }
    }
    ++g_golden_sun_obj_y_cull_decision_logs_in_epoch;
    const auto& parent = correlation.parent;
    std::fprintf(
        stderr,
        "[wide-obj-y-cull-decision] frame=%llu auth_epoch=%llu "
        "staging=0x%08x operand=%d original=%u final=%u overridden=%u "
        "r1=0x%08x r3=0x%08x r6=0x%08x r7=0x%08x r10=0x%08x "
        "r11=0x%08x sp=0x%08x lr=0x%08x depth=%u return_pc=0x%08x "
        "stack_valid=%u stack_sp4=0x%08x stack_sp18=0x%08x "
        "parent_valid=%u parent_frame=%llu parent_operand=%d "
        "parent_original=%u parent_final=%u parent_overridden=%u "
        "parent_r7=0x%08x dropped=%llu\n",
        static_cast<unsigned long long>(runtime_current_frame()),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        key.staging_address, operand, original_decision, final_decision,
        overridden ? 1u : 0u, g_cpu.R[1], g_cpu.R[3], g_cpu.R[6], g_cpu.R[7],
        g_cpu.R[10], g_cpu.R[11], g_cpu.R[13], g_cpu.R[14],
        runtime_call_stack_depth(),
        golden_sun_obj_call_return_pc(runtime_call_stack_depth()),
        correlation.stack_inputs_valid ? 1u : 0u,
        correlation.b328_stack_sp_plus4, correlation.b328_stack_sp_plus18,
        parent.valid ? 1u : 0u,
        static_cast<unsigned long long>(parent.frame), parent.operand,
        parent.original_decision, parent.final_decision,
        parent.overridden ? 1u : 0u, parent.r7,
        static_cast<unsigned long long>(
            g_golden_sun_obj_y_cull_decision_logs_dropped));
}

void trace_golden_sun_obj_y_correlation(
    const GoldenSunObjYCorrelation& correlation, std::uint32_t staging_address,
    int slot, std::uint16_t attr0, std::uint16_t attr1,
    std::uint16_t attr2) {
    if (!correlation.valid || !golden_sun_expanded_obj_view_active() || slot < 0 ||
        static_cast<std::size_t>(slot) >=
            g_golden_sun_obj_y_correlation_slot_logs.size()) return;
    GoldenSunObjYCorrelationKey key{};
    key.staging_address = staging_address;
    key.slot = slot;
    key.b328_operand = correlation.b328_operand;
    key.b328_r1 = correlation.b328_r1;
    key.b328_r3 = correlation.b328_r3;
    key.b328_r11 = correlation.b328_r11;
    key.stack_sp_plus4 = correlation.b328_stack_sp_plus4;
    key.stack_sp_plus18 = correlation.b328_stack_sp_plus18;
    key.attr0 = attr0;
    key.attr1 = attr1;
    key.attr2 = attr2;
    for (std::size_t i = 0; i < key.cull_sequences.size(); ++i)
        key.cull_sequences[i] = correlation.preceding[i].valid
            ? correlation.preceding[i].sequence : 0u;
    for (const auto& seen : g_golden_sun_obj_y_correlation_keys) {
        if (seen.valid && seen.staging_address == key.staging_address &&
            seen.slot == key.slot &&
            seen.b328_operand == key.b328_operand && seen.attr0 == key.attr0 &&
            seen.b328_r1 == key.b328_r1 && seen.b328_r3 == key.b328_r3 &&
            seen.b328_r11 == key.b328_r11 &&
            seen.stack_sp_plus4 == key.stack_sp_plus4 &&
            seen.stack_sp_plus18 == key.stack_sp_plus18 &&
            seen.attr1 == key.attr1 && seen.attr2 == key.attr2 &&
            seen.cull_sequences == key.cull_sequences) return;
    }
    unsigned staging_records = 0;
    for (const auto& seen : g_golden_sun_obj_y_correlation_keys) {
        if (seen.valid && seen.staging_address == staging_address)
            ++staging_records;
    }
    if (g_golden_sun_obj_y_correlation_slot_logs[static_cast<std::size_t>(slot)] >=
            kGoldenSunObjYCorrelationPerSlotLimit ||
        staging_records >= kGoldenSunObjYCorrelationPerStagingLimit ||
        g_golden_sun_obj_y_correlation_logs_in_epoch >=
            kGoldenSunObjYCorrelationLogLimitPerEpoch) {
        ++g_golden_sun_obj_y_correlation_logs_dropped;
        return;
    }
    for (auto& seen : g_golden_sun_obj_y_correlation_keys) {
        if (!seen.valid) {
            seen = key;
            seen.valid = true;
            break;
        }
    }
    ++g_golden_sun_obj_y_correlation_slot_logs[static_cast<std::size_t>(slot)];
    ++g_golden_sun_obj_y_correlation_logs_in_epoch;
    std::fprintf(stderr,
        "[wide-obj-y-cull-correlation] frame=%llu auth_epoch=%llu "
        "b328_pc=0x%08x b328_operand=%d b328_r1=0x%08x b328_r3=0x%08x "
        "b328_r11=0x%08x stack_valid=%u stack_sp4=0x%08x stack_sp18=0x%08x "
        "b328_r6=0x%08x b328_r7=0x%08x "
        "b328_sp=0x%08x b328_lr=0x%08x depth=%u return_pc=0x%08x "
        "staging=0x%08x slot=%d attr0=0x%04x attr1=0x%04x attr2=0x%04x "
        "parent_valid=%u parent_pc=0x%08x parent_operand=%d "
        "parent_original=%u parent_overridden=%u parent_final=%u "
        "parent_r7=0x%08x parent_r10=0x%08x parent_r11=0x%08x "
        "parent_sp=0x%08x parent_lr=0x%08x parent_depth=%u "
        "parent_return_pc=0x%08x dropped=%llu\n",
        static_cast<unsigned long long>(runtime_current_frame()),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        correlation.b328_pc, correlation.b328_operand, correlation.b328_r1,
        correlation.b328_r3, correlation.b328_r11,
        correlation.stack_inputs_valid ? 1u : 0u,
        correlation.b328_stack_sp_plus4, correlation.b328_stack_sp_plus18,
        correlation.b328_r6, correlation.b328_r7, correlation.b328_sp,
        correlation.b328_lr,
        correlation.b328_call_depth, correlation.b328_call_return_pc,
        staging_address, slot, static_cast<unsigned>(attr0),
        static_cast<unsigned>(attr1), static_cast<unsigned>(attr2),
        correlation.parent.valid ? 1u : 0u, correlation.parent.pc,
        correlation.parent.operand, correlation.parent.original_decision,
        correlation.parent.overridden ? 1u : 0u,
        correlation.parent.final_decision, correlation.parent.r7,
        correlation.parent.r10, correlation.parent.r11, correlation.parent.sp,
        correlation.parent.lr, correlation.parent.call_depth,
        correlation.parent.call_return_pc,
        static_cast<unsigned long long>(g_golden_sun_obj_y_correlation_logs_dropped));
}

void trace_golden_sun_obj_y_jump(
    int slot, int raw_y, int logical_y,
    const GoldenSunObjPlacementProvenance& provenance,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    // A large accepted-Y transition is the signature of a recycled slot or
    // an accidental modulo-256 interpretation. Keep this automatic and
    // bounded so it remains useful when the broad WIDE probe is disabled.
    if (!golden_sun_expanded_obj_view_active() || slot < 0 ||
        static_cast<std::size_t>(slot) >=
            g_golden_sun_obj_y_accepted_state.size()) return;
    auto& previous = g_golden_sun_obj_y_accepted_state[
        static_cast<std::size_t>(slot)];
    const auto frame = runtime_current_frame();
    if (previous.valid) {
        const int delta = logical_y - static_cast<int>(previous.logical_y);
        const int magnitude = delta < 0 ? -delta : delta;
        if (magnitude >= 64) {
            if (g_golden_sun_obj_y_jump_logs_in_epoch >=
                kGoldenSunObjYJumpLogLimitPerEpoch) {
                ++g_golden_sun_obj_y_jump_logs_dropped;
            } else {
                ++g_golden_sun_obj_y_jump_logs_in_epoch;
                std::fprintf(
                    stderr,
                    "[wide-obj-y-jump] frame=%llu previous_frame=%llu "
                    "auth_epoch=%llu slot=%d delta=%d "
                    "previous_raw_y=%u raw_y=%d previous_logical_y=%d "
                    "logical_y=%d previous_writer_branch_pc=0x%08x "
                    "writer_branch_pc=0x%08x previous_target_address=0x%08x "
                    "target_address=0x%08x previous_generation=%llu "
                    "writer_generation=%llu previous_attr0=0x%04x "
                    "attr0=0x%04x previous_attr1=0x%04x attr1=0x%04x "
                    "previous_attr2=0x%04x attr2=0x%04x\n",
                    static_cast<unsigned long long>(frame),
                    static_cast<unsigned long long>(previous.frame),
                    static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                    slot, delta, static_cast<unsigned>(previous.raw_y), raw_y,
                    static_cast<int>(previous.logical_y), logical_y,
                    previous.writer_branch_pc, provenance.writer_branch_pc,
                    previous.target_address, provenance.target_address,
                    static_cast<unsigned long long>(previous.writer_generation),
                    static_cast<unsigned long long>(provenance.writer_generation),
                    static_cast<unsigned>(previous.attr0),
                    static_cast<unsigned>(attr0),
                    static_cast<unsigned>(previous.attr1),
                    static_cast<unsigned>(attr1),
                    static_cast<unsigned>(previous.attr2),
                    static_cast<unsigned>(attr2));
            }
        }
    }
    previous.valid = true;
    previous.raw_y = static_cast<std::uint8_t>(raw_y);
    previous.logical_y = static_cast<std::int16_t>(logical_y);
    previous.frame = frame;
    previous.writer_branch_pc = provenance.writer_branch_pc;
    previous.target_address = provenance.target_address;
    previous.writer_generation = provenance.writer_generation;
    previous.attr0 = attr0;
    previous.attr1 = attr1;
    previous.attr2 = attr2;
}

void trace_golden_sun_obj_y_alias(
    const char* reason, int slot, int raw_y, int logical_y,
    const GoldenSunObjPlacementProvenance& provenance,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2,
    bool canonical_rejection) {
    // This is the one bounded diagnostic that must remain visible even when
    // the broad WIDE-01 probe is off. Keep it tied strictly to the geometry
    // whose signed-Y path it diagnoses, with no scene/authentication gate.
    if (!golden_sun_expanded_obj_view_active() || slot < 0 ||
        static_cast<std::size_t>(slot) >=
            g_golden_sun_obj_y_alias_last_frame.size() ||
        (!canonical_rejection &&
         !gsr::widescreen::golden_sun_obj_y_alias_candidate(
             raw_y, logical_y))) return;
    const std::uint64_t frame = runtime_current_frame();
    auto& last_frame = canonical_rejection
        ? g_golden_sun_obj_y_canonical_last_frame[static_cast<std::size_t>(slot)]
        : g_golden_sun_obj_y_alias_last_frame[static_cast<std::size_t>(slot)];
    if (last_frame == frame) return;
    last_frame = frame;
    if (g_golden_sun_obj_y_alias_logs_in_epoch >=
        kGoldenSunObjYAliasLogLimitPerEpoch) {
        ++g_golden_sun_obj_y_alias_logs_dropped;
        return;
    }
    ++g_golden_sun_obj_y_alias_logs_in_epoch;
    std::fprintf(
        stderr,
        "[wide-obj-y-alias] frame=%llu provenance_frame=%llu "
        "auth_epoch=%llu reason=%s slot=%d raw_y=%d logical_y=%d "
        "writer_branch_pc=0x%08x target_address=0x%08x "
        "writer_generation=%llu attr0=0x%04x attr1=0x%04x attr2=0x%04x\n",
        static_cast<unsigned long long>(frame),
        static_cast<unsigned long long>(provenance.frame),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch), reason,
        slot, raw_y, logical_y, provenance.writer_branch_pc,
        provenance.target_address,
        static_cast<unsigned long long>(provenance.writer_generation),
        static_cast<unsigned>(attr0), static_cast<unsigned>(attr1),
        static_cast<unsigned>(attr2));
}

void trace_golden_sun_obj_y_edge_alias(
    int slot, std::uint32_t target_address, std::uint64_t frame,
    std::uint64_t auth_epoch, std::uint16_t attr0, std::uint16_t attr1,
    std::uint16_t attr2) {
    ++g_golden_sun_obj_y_edge_alias_activations;
    if (!golden_sun_wide_diagnostics_enabled()) return;
    std::fprintf(stderr,
                 "[wide-obj-y-edge-alias] frame=%llu auth_epoch=%llu "
                 "slot=%d target=0x%08x raw_y=159 logical_y=-97 "
                 "attr0=0x%04x attr1=0x%04x attr2=0x%04x activations=%llu\n",
                 static_cast<unsigned long long>(frame),
                 static_cast<unsigned long long>(auth_epoch), slot,
                 target_address, static_cast<unsigned>(attr0),
                 static_cast<unsigned>(attr1), static_cast<unsigned>(attr2),
                 static_cast<unsigned long long>(
                     g_golden_sun_obj_y_edge_alias_activations));
}

void begin_golden_sun_field_auth_epoch();

// GBARECOMP_VRAM_MAP_TRACE is the existing explicit kill switch for this
// payload-free map investigation. The default is deliberately off in main().
bool golden_sun_wide_diagnostics_enabled() {
    static const bool enabled = [] {
        const char* env = std::getenv("GBARECOMP_VRAM_MAP_TRACE");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

// WIDE-01 experimental off-screen cull safety net. GBARECOMP_EXPERIMENTAL_FIXES
// is the launcher's "Experimental Fixes" checkbox (src/launcher_main.cpp),
// always sent explicitly (0 or 1), same discipline as the diagnostics kill
// switch above. Default off; normal play is byte-for-byte unaffected. See
// src/widescreen_policy.h (golden_sun_experimental_sprite_fully_offscreen)
// and docs/issues/WIDE-01_NPC_IDENTITY.md for what this gates.
bool golden_sun_experimental_fixes_enabled() {
    static const bool enabled = [] {
        const char* env = std::getenv("GBARECOMP_EXPERIMENTAL_FIXES");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

struct GoldenSunFieldMapCensus {
    std::uint64_t auth_epoch = 0;
    std::uint32_t map_crc = 0;
    std::uint32_t raw_crc = 0;
};
std::vector<GoldenSunFieldMapCensus> g_golden_sun_wide_field_map_census;
std::uint64_t g_golden_sun_wide_field_map_trace_frame = UINT64_MAX;
bool g_golden_sun_palace_table_authorized = false;
bool g_golden_sun_palace_table_invalidated = false;
bool g_golden_sun_palace_table_auth_attempted = false;
gsr::widescreen::GoldenSunPalaceActiveRegion
    g_golden_sun_palace_active_region;
std::array<std::uint16_t, 6> g_golden_sun_palace_region_scroll{};
bool g_golden_sun_palace_region_scroll_valid = false;
bool g_golden_sun_palace_region_build_attempted = false;
unsigned g_golden_sun_obj_y_provenance_logs_in_epoch = 0;
using GoldenSunWidePolicyReason = gsr::widescreen::GoldenSunWidePolicyReason;

void invalidate_golden_sun_palace_table_authorization();
bool refresh_golden_sun_palace_table_authorization();

void reset_golden_sun_palace_active_region() {
    if (g_golden_sun_palace_region_scroll_valid ||
        g_golden_sun_palace_region_build_attempted) {
        g_golden_sun_palace_active_region.reset();
    }
    g_golden_sun_palace_region_scroll_valid = false;
    g_golden_sun_palace_region_build_attempted = false;
}

// Payload-free provider accounting separates a missing/invalid atlas source
// from a successful replacement. The existing map census proves which table
// image was resident, while these counters prove what the PPU actually did
// with margin requests; neither path retains or prints guest tile bytes.
struct GoldenSunFieldProviderTrace {
    std::uint64_t calls = 0;
    std::uint64_t equal_scroll_calls = 0;
    std::uint64_t split_scroll_calls = 0;
    std::uint64_t replacements = 0;
    std::uint64_t unavailable = 0;
    std::uint64_t precondition_rejects = 0;
    std::uint64_t boundary_rejects = 0;
    std::uint64_t palace_region_rejects = 0;
    std::uint64_t raw_unavailable = 0;
    std::uint64_t lookup_misses = 0;
    std::int32_t min_x = INT32_MAX;
    std::int32_t max_x = INT32_MIN;
    std::int32_t min_y = INT32_MAX;
    std::int32_t max_y = INT32_MIN;
};
std::array<GoldenSunFieldProviderTrace, 4>
    g_golden_sun_field_provider_trace{};
GoldenSunPalaceMarginDiagnostics g_golden_sun_palace_margin_diagnostics{};

struct GoldenSunFieldMapIdAttribution {
    std::uint16_t map_id = 0;
    std::uint64_t replacements = 0;
};

// A bounded top-of-observed set per layer and margin region. This records no
// map/table payload and avoids a 4x4x4096 hot-path counter matrix.
std::array<std::array<std::array<GoldenSunFieldMapIdAttribution,
                                  kGoldenSunFieldMapIdAttributionLimit>, 4>, 4>
    g_golden_sun_field_map_id_attribution{};
std::array<std::array<std::uint64_t, 4>, 4>
    g_golden_sun_field_map_id_attribution_overflow{};
GoldenSunWidePolicyReason g_golden_sun_wide_policy_last_reason =
    GoldenSunWidePolicyReason::UnsupportedMode;
bool g_golden_sun_wide_policy_seen = false;
std::uint64_t g_golden_sun_wide_policy_transition_count = 0;
std::uint64_t g_golden_sun_wide_policy_logged_transitions = 0;
std::uint64_t g_golden_sun_wide_policy_sample_count = 0;
unsigned g_golden_sun_wide_policy_last_flags =
    gsr::widescreen::kPillarboxAll;
std::uint64_t g_golden_sun_wide_policy_sample_frame = UINT64_MAX;
std::uint64_t g_golden_sun_wide_policy_sample_end_frame = UINT64_MAX;
std::uint64_t g_golden_sun_wide_policy_sample_transition = 0;
unsigned g_golden_sun_wide_policy_samples_in_window = 0;

// The PPU-side margin observer reports only counters and layer/source
// categories. Keep a cumulative copy for the bounded exit summary while also
// emitting a sparse per-frame line that can identify the side of a seam.
std::uint64_t g_golden_sun_margin_diagnostic_callbacks = 0;
std::uint64_t g_golden_sun_margin_diagnostic_logged = 0;
std::uint64_t g_golden_sun_margin_diagnostic_last_log_frame = UINT64_MAX;
gba::WsMarginDiagnostics g_golden_sun_margin_diagnostic_total{};
gba::WsMarginDiagnostics g_golden_sun_margin_diagnostic_last{};

void accumulate_golden_sun_margin_diagnostics(
    gba::WsMarginDiagnostics& total, const gba::WsMarginDiagnostics& sample) {
    total.margin_pixels += sample.margin_pixels;
    total.left_margin_pixels += sample.left_margin_pixels;
    total.right_margin_pixels += sample.right_margin_pixels;
    total.top_margin_pixels += sample.top_margin_pixels;
    total.bottom_margin_pixels += sample.bottom_margin_pixels;
    for (std::size_t bg = 0; bg < total.provider_results.size(); ++bg) {
        for (std::size_t result = 0;
             result < total.provider_results[bg].size(); ++result) {
            total.provider_results[bg][result] +=
                sample.provider_results[bg][result];
        }
    }
    for (std::size_t layer = 0;
         layer < total.final_selected.size(); ++layer) {
        for (std::size_t source = 0;
             source < total.final_selected[layer].size(); ++source) {
            total.final_selected[layer][source] +=
                sample.final_selected[layer][source];
        }
    }
    for (std::size_t side = 0;
         side < total.horizontal_final_selected.size(); ++side) {
        for (std::size_t layer = 0;
             layer < total.horizontal_final_selected[side].size(); ++layer) {
            for (std::size_t source = 0;
                 source < total.horizontal_final_selected[side][layer].size();
                 ++source) {
                total.horizontal_final_selected[side][layer][source] +=
                    sample.horizontal_final_selected[side][layer][source];
            }
        }
    }
}

const char* golden_sun_wide_policy_reason_name(
    GoldenSunWidePolicyReason reason) {
    switch (reason) {
        case GoldenSunWidePolicyReason::AuthorizedMode2:
            return "authorized-mode2";
        case GoldenSunWidePolicyReason::AuthorizedMode0:
            return "authorized-mode0";
        case GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll:
            return "authorized-mode0-split-scroll";
        case GoldenSunWidePolicyReason::MissingIo:
            return "missing-io";
        case GoldenSunWidePolicyReason::ForcedBlank:
            return "forced-blank";
        case GoldenSunWidePolicyReason::WindowControl:
            return "window-control";
        case GoldenSunWidePolicyReason::UnsupportedMode:
            return "unsupported-mode";
        case GoldenSunWidePolicyReason::Mode2Layers:
            return "mode2-layers";
        case GoldenSunWidePolicyReason::Mode2Geometry:
            return "mode2-geometry";
        case GoldenSunWidePolicyReason::Mode0Layers:
            return "mode0-layers";
        case GoldenSunWidePolicyReason::Mode0Geometry:
            return "mode0-geometry";
        case GoldenSunWidePolicyReason::Mode0ScrollMismatch:
            return "mode0-scroll-mismatch";
        case GoldenSunWidePolicyReason::Mode0SplitLayers:
            return "mode0-split-layers";
        case GoldenSunWidePolicyReason::Mode0SplitGeometry:
            return "mode0-split-geometry";
        case GoldenSunWidePolicyReason::Mode0SplitScrollMismatch:
            return "mode0-split-scroll-mismatch";
    }
    return "unknown";
}

void report_golden_sun_margin_diagnostic_sample(
    std::uint64_t frame, const char* reason,
    const gba::WsMarginDiagnostics& sample) {
    const auto final = [&](std::size_t layer, std::size_t source) {
        return sample.final_selected[layer][source];
    };
    const auto horizontal = [&](std::size_t side, std::size_t layer,
                                std::size_t source) {
        return sample.horizontal_final_selected[side][layer][source];
    };
    const auto provider = [&](std::size_t bg, std::size_t result) {
        return sample.provider_results[bg][result];
    };
    const auto u64 = [](std::uint64_t value) {
        return static_cast<unsigned long long>(value);
    };
    std::fprintf(
        stderr,
        "[wide-margin] frame=%llu auth_epoch=%llu reason=%s "
        "pixels=%llu left=%llu right=%llu top=%llu bottom=%llu "
        "provider=bg0:%llu/%llu/%llu,bg1:%llu/%llu/%llu,bg2:%llu/%llu/%llu,bg3:%llu/%llu/%llu "
        "final_bg0=%llu/%llu/%llu final_bg1=%llu/%llu/%llu "
        "final_bg2=%llu/%llu/%llu final_bg3=%llu/%llu/%llu "
        "final_obj=%llu final_backdrop=%llu final_pillarbox=%llu "
        "final_forced_blank=%llu "
        "left_bg0=%llu/%llu/%llu left_bg1=%llu/%llu/%llu "
        "left_bg2=%llu/%llu/%llu left_bg3=%llu/%llu/%llu "
        "right_bg0=%llu/%llu/%llu right_bg1=%llu/%llu/%llu "
        "right_bg2=%llu/%llu/%llu right_bg3=%llu/%llu/%llu "
        "right_obj=%llu right_backdrop=%llu right_pillarbox=%llu\n",
        u64(frame), u64(g_golden_sun_field_auth_epoch), reason,
        u64(sample.margin_pixels), u64(sample.left_margin_pixels),
        u64(sample.right_margin_pixels), u64(sample.top_margin_pixels),
        u64(sample.bottom_margin_pixels),
        u64(provider(0, gba::kWsMarginProviderReplace)),
        u64(provider(0, gba::kWsMarginProviderKeepWrapped)),
        u64(provider(0, gba::kWsMarginProviderUnavailable)),
        u64(provider(1, gba::kWsMarginProviderReplace)),
        u64(provider(1, gba::kWsMarginProviderKeepWrapped)),
        u64(provider(1, gba::kWsMarginProviderUnavailable)),
        u64(provider(2, gba::kWsMarginProviderReplace)),
        u64(provider(2, gba::kWsMarginProviderKeepWrapped)),
        u64(provider(2, gba::kWsMarginProviderUnavailable)),
        u64(provider(3, gba::kWsMarginProviderReplace)),
        u64(provider(3, gba::kWsMarginProviderKeepWrapped)),
        u64(provider(3, gba::kWsMarginProviderUnavailable)),
        u64(final(gba::kWsMarginTraceBg0, gba::kWsMarginSourceWrapped)),
        u64(final(gba::kWsMarginTraceBg0,
                  gba::kWsMarginSourceProviderReplace)),
        u64(final(gba::kWsMarginTraceBg0,
                  gba::kWsMarginSourceProviderKeepWrapped)),
        u64(final(gba::kWsMarginTraceBg1, gba::kWsMarginSourceWrapped)),
        u64(final(gba::kWsMarginTraceBg1,
                  gba::kWsMarginSourceProviderReplace)),
        u64(final(gba::kWsMarginTraceBg1,
                  gba::kWsMarginSourceProviderKeepWrapped)),
        u64(final(gba::kWsMarginTraceBg2, gba::kWsMarginSourceWrapped)),
        u64(final(gba::kWsMarginTraceBg2,
                  gba::kWsMarginSourceProviderReplace)),
        u64(final(gba::kWsMarginTraceBg2,
                  gba::kWsMarginSourceProviderKeepWrapped)),
        u64(final(gba::kWsMarginTraceBg3, gba::kWsMarginSourceWrapped)),
        u64(final(gba::kWsMarginTraceBg3,
                  gba::kWsMarginSourceProviderReplace)),
        u64(final(gba::kWsMarginTraceBg3,
                  gba::kWsMarginSourceProviderKeepWrapped)),
        u64(final(gba::kWsMarginTraceObj, gba::kWsMarginSourceObj)),
        u64(final(gba::kWsMarginTraceBackdrop, gba::kWsMarginSourceBackdrop)),
        u64(final(gba::kWsMarginTracePillarbox,
                  gba::kWsMarginSourcePillarbox)),
        u64(final(gba::kWsMarginTraceForcedBlank,
                  gba::kWsMarginSourceForcedBlank)),
        u64(horizontal(0, gba::kWsMarginTraceBg0,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg0,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(0, gba::kWsMarginTraceBg0,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg1,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg1,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(0, gba::kWsMarginTraceBg1,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg2,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg2,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(0, gba::kWsMarginTraceBg2,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg3,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(0, gba::kWsMarginTraceBg3,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(0, gba::kWsMarginTraceBg3,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg0,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg0,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(1, gba::kWsMarginTraceBg0,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg1,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg1,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(1, gba::kWsMarginTraceBg1,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg2,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg2,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(1, gba::kWsMarginTraceBg2,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg3,
                        gba::kWsMarginSourceWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceBg3,
                        gba::kWsMarginSourceProviderReplace)),
        u64(horizontal(1, gba::kWsMarginTraceBg3,
                        gba::kWsMarginSourceProviderKeepWrapped)),
        u64(horizontal(1, gba::kWsMarginTraceObj, gba::kWsMarginSourceObj)),
        u64(horizontal(1, gba::kWsMarginTraceBackdrop,
                        gba::kWsMarginSourceBackdrop)),
        u64(horizontal(1, gba::kWsMarginTracePillarbox,
                        gba::kWsMarginSourcePillarbox)));
}

void golden_sun_wide_margin_diagnostics_callback(
    const gba::WsMarginDiagnostics& sample) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    ++g_golden_sun_margin_diagnostic_callbacks;
    g_golden_sun_margin_diagnostic_last = sample;
    accumulate_golden_sun_margin_diagnostics(
        g_golden_sun_margin_diagnostic_total, sample);
    const std::uint64_t frame = runtime_current_frame();
    if (g_golden_sun_margin_diagnostic_logged >= 32u ||
        (g_golden_sun_margin_diagnostic_last_log_frame != UINT64_MAX &&
         frame < g_golden_sun_margin_diagnostic_last_log_frame + 120u)) {
        return;
    }
    ++g_golden_sun_margin_diagnostic_logged;
    g_golden_sun_margin_diagnostic_last_log_frame = frame;
    report_golden_sun_margin_diagnostic_sample(frame, "frame", sample);
}

struct GoldenSunFieldTableStats {
    std::uint64_t writes = 0;
    std::uint64_t bytes = 0;
    std::uint64_t first_frame = UINT64_MAX;
    std::uint64_t last_frame = UINT64_MAX;
    std::uint32_t first_pc = 0;
    std::uint32_t last_pc = 0;
};
GoldenSunFieldTableStats g_golden_sun_field_map_writes;
GoldenSunFieldTableStats g_golden_sun_field_raw_writes;
GoldenSunFieldTableStats g_golden_sun_field_map_dma_writes;
GoldenSunFieldTableStats g_golden_sun_field_raw_dma_writes;

// The detailed per-store trace is intentionally capped at 64 records per
// authentication epoch. That cap hid raw-table producers when map stores
// arrived first, so keep a separate payload-free histogram/range summary keyed
// by writer PC. It is bounded, reset per epoch, and diagnostics-only.
constexpr std::size_t kGoldenSunFieldProducerLimit = 32u;
struct GoldenSunFieldProducerStats {
    bool used = false;
    std::uint32_t pc = 0;
    std::uint64_t writes = 0;
    std::uint64_t bytes = 0;
    std::uint64_t first_frame = UINT64_MAX;
    std::uint64_t last_frame = UINT64_MAX;
    std::uint32_t min_address = UINT32_MAX;
    std::uint32_t max_end = 0;
};
std::array<GoldenSunFieldProducerStats, kGoldenSunFieldProducerLimit>
    g_golden_sun_field_map_producers{};
std::array<GoldenSunFieldProducerStats, kGoldenSunFieldProducerLimit>
    g_golden_sun_field_raw_producers{};
unsigned g_golden_sun_field_map_producer_overflow = 0;
unsigned g_golden_sun_field_raw_producer_overflow = 0;

void reset_golden_sun_field_producers() {
    g_golden_sun_field_map_producers = {};
    g_golden_sun_field_raw_producers = {};
    g_golden_sun_field_map_producer_overflow = 0;
    g_golden_sun_field_raw_producer_overflow = 0;
}

void record_golden_sun_field_producer(
    std::array<GoldenSunFieldProducerStats, kGoldenSunFieldProducerLimit>&
        producers,
    unsigned* overflow, std::uint32_t pc, std::uint32_t address,
    std::uint32_t size, std::uint64_t bytes, std::uint64_t frame) {
    if (!overflow || size == 0u || bytes == 0u) return;
    GoldenSunFieldProducerStats* producer = nullptr;
    for (auto& candidate : producers) {
        if (candidate.used && candidate.pc == pc) {
            producer = &candidate;
            break;
        }
    }
    if (!producer) {
        for (auto& candidate : producers) {
            if (!candidate.used) {
                candidate = {};
                candidate.used = true;
                candidate.pc = pc;
                producer = &candidate;
                break;
            }
        }
    }
    if (!producer) {
        ++*overflow;
        return;
    }
    const std::uint64_t end = static_cast<std::uint64_t>(address) + size;
    ++producer->writes;
    producer->bytes += bytes;
    producer->first_frame = std::min(producer->first_frame, frame);
    producer->last_frame = producer->writes == 1u
        ? frame : std::max(producer->last_frame, frame);
    producer->min_address = std::min(producer->min_address, address);
    producer->max_end = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        end, UINT32_MAX));
}

void report_golden_sun_field_producers(const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled() || !reason) return;
    const auto report = [&](const char* table,
                            const auto& producers, unsigned overflow) {
        for (const auto& producer : producers) {
            if (!producer.used) continue;
            std::fprintf(
                stderr,
                "[wide-field-producer] auth_epoch=%llu reason=%s "
                "table=%s writer_pc=0x%08x writes=%llu bytes=%llu "
                "addr=0x%08x..0x%08x frames=%llu..%llu\n",
                static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                reason, table, producer.pc,
                static_cast<unsigned long long>(producer.writes),
                static_cast<unsigned long long>(producer.bytes),
                producer.min_address, producer.max_end,
                static_cast<unsigned long long>(producer.first_frame),
                static_cast<unsigned long long>(producer.last_frame));
        }
        if (overflow != 0u) {
            std::fprintf(stderr,
                         "[wide-field-producer-overflow] auth_epoch=%llu "
                         "reason=%s table=%s producers=%u\n",
                         static_cast<unsigned long long>(
                             g_golden_sun_field_auth_epoch),
                         reason, table, overflow);
        }
    };
    report("map", g_golden_sun_field_map_producers,
           g_golden_sun_field_map_producer_overflow);
    report("raw", g_golden_sun_field_raw_producers,
           g_golden_sun_field_raw_producer_overflow);
}

// WIDE-01 entity-producer census: bounded IWRAM write census for the
// observed 0x38-stride object staging array at 0x03002000..0x030022E0.
// Diagnostics-only, deduped by (address, pc, size), capped per epoch, and
// reset with the same auth-epoch boundaries as the field producer trace
// above. Produces nothing unless widescreen diagnostics are enabled; never
// consulted by gameplay/render code.
constexpr std::uint32_t kGoldenSunObjRecordCensusStart = 0x03002000u;
constexpr std::uint32_t kGoldenSunObjRecordCensusEnd = 0x030022E0u;
// The 64-entry cap truncated (Bilibin overflow ~20005, Palace ~2912 write
// *events*, not unique keys -- overflow increments per rejected write, not
// per distinct key). The address range is exactly 0x2E0 = 736 bytes; the
// worst pattern measured so far is one writer identity per byte (Palace's
// sequential fill) and a handful of distinct (pc,size) writers per byte in
// the steady-state field case. 4096 covers 736 bytes x up to ~5 distinct
// (pc,size) identities per byte with headroom, while staying a few hundred
// KB and a bounded linear scan restricted to writes inside this one 736-byte
// window (never the hot path for the rest of guest memory).
constexpr std::size_t kGoldenSunObjRecordCensusLimit = 4096u;
struct GoldenSunObjRecordCensusStat {
    bool used = false;
    std::uint32_t address = 0;
    std::uint32_t pc = 0;
    std::uint32_t size = 0;
    std::uint64_t writes = 0;
    std::uint64_t first_frame = UINT64_MAX;
    std::uint64_t last_frame = UINT64_MAX;
};
std::array<GoldenSunObjRecordCensusStat, kGoldenSunObjRecordCensusLimit>
    g_golden_sun_obj_record_census{};
unsigned g_golden_sun_obj_record_census_overflow = 0;

void reset_golden_sun_obj_record_census() {
    g_golden_sun_obj_record_census = {};
    g_golden_sun_obj_record_census_overflow = 0;
}

void note_golden_sun_obj_record_write(std::uint32_t pc, std::uint32_t address,
                                      std::uint32_t size) {
    if (size == 0u) return;
    const std::uint64_t first = address;
    const std::uint64_t last = static_cast<std::uint64_t>(address) + size;
    if (last <= kGoldenSunObjRecordCensusStart ||
        first >= kGoldenSunObjRecordCensusEnd) return;
    const std::uint64_t frame = runtime_current_frame();
    GoldenSunObjRecordCensusStat* stat = nullptr;
    for (auto& candidate : g_golden_sun_obj_record_census) {
        if (candidate.used && candidate.address == address &&
            candidate.pc == pc && candidate.size == size) {
            stat = &candidate;
            break;
        }
    }
    if (!stat) {
        for (auto& candidate : g_golden_sun_obj_record_census) {
            if (!candidate.used) {
                candidate = {};
                candidate.used = true;
                candidate.address = address;
                candidate.pc = pc;
                candidate.size = size;
                stat = &candidate;
                break;
            }
        }
    }
    if (!stat) {
        ++g_golden_sun_obj_record_census_overflow;
        return;
    }
    ++stat->writes;
    stat->first_frame = std::min(stat->first_frame, frame);
    stat->last_frame = stat->writes == 1u ? frame
                                          : std::max(stat->last_frame, frame);
}

void report_golden_sun_obj_record_census(const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled() || !reason) return;
    for (const auto& stat : g_golden_sun_obj_record_census) {
        if (!stat.used) continue;
        std::fprintf(
            stderr,
            "[wide-obj-record-writer] auth_epoch=%llu reason=%s "
            "addr=0x%08x pc=0x%08x size=%u writes=%llu frames=%llu..%llu\n",
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            reason, stat.address, stat.pc, stat.size,
            static_cast<unsigned long long>(stat.writes),
            static_cast<unsigned long long>(stat.first_frame),
            static_cast<unsigned long long>(stat.last_frame));
    }
    if (g_golden_sun_obj_record_census_overflow != 0u) {
        std::fprintf(stderr,
                     "[wide-obj-record-writer-overflow] auth_epoch=%llu "
                     "reason=%s overflow=%u\n",
                     static_cast<unsigned long long>(
                         g_golden_sun_field_auth_epoch),
                     reason, g_golden_sun_obj_record_census_overflow);
    }
}

// WIDE-01 entity-producer census: bounded value-event trace for the same
// 0x38-stride record, restricted to relative offsets 0x00..0x13 of each
// slot -- the region under test for a signed X/Y correlation against
// [wide-obj-stage]/[wide-obj-handoff]. The offset window is layout-relative
// (not tied to any specific map), so it applies unchanged to a different
// town using the same record shape. Values logged here are decoded guest
// game state (small integers/coordinates), not ROM/asset payload bytes.
// Diagnostics-only, capped, and reset with the same epoch boundary as the
// rest of this census.
constexpr std::uint32_t kGoldenSunObjRecordValueOffsetEnd = 0x14u;
constexpr std::size_t kGoldenSunObjRecordValueLimit = 4096u;
struct GoldenSunObjRecordValueEvent {
    std::uint64_t frame = 0;
    std::uint32_t address = 0;
    std::uint32_t pc = 0;
    std::uint32_t size = 0;
    std::uint32_t value = 0;
};
std::array<GoldenSunObjRecordValueEvent, kGoldenSunObjRecordValueLimit>
    g_golden_sun_obj_record_value_events{};
std::size_t g_golden_sun_obj_record_value_count = 0;
std::uint64_t g_golden_sun_obj_record_value_dropped = 0;

void reset_golden_sun_obj_record_value_events() {
    g_golden_sun_obj_record_value_count = 0;
    g_golden_sun_obj_record_value_dropped = 0;
}

void note_golden_sun_obj_record_value(std::uint32_t pc, std::uint32_t address,
                                      std::uint32_t size) {
    if (address < kGoldenSunObjRecordCensusStart) return;
    if (address >= kGoldenSunObjRecordCensusEnd) return;
    const std::uint32_t offset_in_slot =
        (address - kGoldenSunObjRecordCensusStart) % 0x38u;
    if (offset_in_slot >= kGoldenSunObjRecordValueOffsetEnd) return;
    if (g_golden_sun_obj_record_value_count >=
            kGoldenSunObjRecordValueLimit) {
        ++g_golden_sun_obj_record_value_dropped;
        return;
    }
    std::uint32_t value = 0;
    switch (size) {
        case 1u: value = bus_read_u8(address); break;
        case 2u: value = bus_read_u16(address); break;
        case 4u: value = bus_read_u32(address); break;
        default: return;
    }
    auto& event = g_golden_sun_obj_record_value_events[
        g_golden_sun_obj_record_value_count++];
    event.frame = runtime_current_frame();
    event.address = address;
    event.pc = pc;
    event.size = size;
    event.value = value;
}

void report_golden_sun_obj_record_values(const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled() || !reason) return;
    for (std::size_t i = 0; i < g_golden_sun_obj_record_value_count; ++i) {
        const auto& event = g_golden_sun_obj_record_value_events[i];
        std::fprintf(
            stderr,
            "[wide-obj-record-value] auth_epoch=%llu reason=%s frame=%llu "
            "addr=0x%08x pc=0x%08x size=%u value=0x%08x\n",
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            reason, static_cast<unsigned long long>(event.frame),
            event.address, event.pc, event.size, event.value);
    }
    if (g_golden_sun_obj_record_value_dropped != 0u) {
        std::fprintf(stderr,
                     "[wide-obj-record-value-overflow] auth_epoch=%llu "
                     "reason=%s dropped=%llu\n",
                     static_cast<unsigned long long>(
                         g_golden_sun_field_auth_epoch),
                     reason, static_cast<unsigned long long>(
                         g_golden_sun_obj_record_value_dropped));
    }
}

// WIDE-01 body/shadow identity: bounded per-frame write-order trace of the
// OAM-shadow commit, across all three Func_1dc8 writer routes (D4/EC/F0 --
// golden_sun_obj_staging_handoff, golden_sun_obj_f0_entry_capture, and
// golden_sun_oam_shadow_write_observer respectively; see
// src/relocatable_writer_policy.h for the route offsets). Records each
// committed write in the exact order the guest performs it within a frame,
// tagged with its route, so a body write and its shadow write for the same
// in-game entity can later be matched by adjacent sequence numbers within
// one frame regardless of which route produced them. Diagnostics-only,
// deduped against the immediately preceding entry for the same (route,
// slot) pair (skips frames where that slot's committed source/ATTR/
// coordinates are unchanged from the previous frame on that same route --
// this is the "only log frames where the set of active slots changes"
// bound), capped, and reset at the same auth-epoch boundaries as the rest
// of this census. Never consulted by gameplay/render code. See
// docs/issues/WIDE-01_NPC_IDENTITY.md.
// (GoldenSunObjCommitOrderRoute is forward-declared near the top of this
// file, alongside note_golden_sun_obj_commit_order.)
constexpr const char* golden_sun_obj_commit_order_route_name(
    GoldenSunObjCommitOrderRoute route) {
    switch (route) {
        case GoldenSunObjCommitOrderRoute::D4: return "D4";
        case GoldenSunObjCommitOrderRoute::EC: return "EC";
        case GoldenSunObjCommitOrderRoute::F0: return "F0";
    }
    return "?";
}
// 256 was sized for the D4 route alone (3 events dropped once EC/F0 were
// added in a follow-up pass); 512 covers all three routes with headroom
// while staying well under the per-epoch volume that caused the
// synchronous-fprintf transition stalls documented in
// WIDE-01_ENTITY_PRODUCER_FINDINGS.md ("Performance").
constexpr std::size_t kGoldenSunObjCommitOrderLimit = 512u;
constexpr std::size_t kGoldenSunObjCommitOrderSlots = 128u;
constexpr std::size_t kGoldenSunObjCommitOrderRouteCount = 3u;
struct GoldenSunObjCommitOrderEvent {
    std::uint64_t frame = 0;
    std::uint32_t sequence = 0;
    std::uint32_t source = 0;
    int slot = -1;
    GoldenSunObjCommitOrderRoute route = GoldenSunObjCommitOrderRoute::D4;
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::int16_t logical_x = 0;
    std::int16_t logical_y = 0;
    bool x_valid = false;
    bool y_valid = false;
    bool used = false;
};
std::array<GoldenSunObjCommitOrderEvent, kGoldenSunObjCommitOrderLimit>
    g_golden_sun_obj_commit_order_events{};
std::size_t g_golden_sun_obj_commit_order_count = 0;
std::uint64_t g_golden_sun_obj_commit_order_dropped = 0;
std::uint64_t g_golden_sun_obj_commit_order_frame = UINT64_MAX;
std::uint32_t g_golden_sun_obj_commit_order_sequence_in_frame = 0;
// Last committed identity per (route, OAM slot), used only to dedupe
// repeat writes; never consulted by gameplay/render code.
std::array<std::array<GoldenSunObjCommitOrderEvent, kGoldenSunObjCommitOrderSlots>,
           kGoldenSunObjCommitOrderRouteCount>
    g_golden_sun_obj_commit_order_last_by_route_slot{};

void reset_golden_sun_obj_commit_order() {
    g_golden_sun_obj_commit_order_count = 0;
    g_golden_sun_obj_commit_order_dropped = 0;
    g_golden_sun_obj_commit_order_frame = UINT64_MAX;
    g_golden_sun_obj_commit_order_sequence_in_frame = 0;
    g_golden_sun_obj_commit_order_last_by_route_slot = {};
}

void note_golden_sun_obj_commit_order(GoldenSunObjCommitOrderRoute route,
                                      std::uint32_t source, int slot,
                                      std::uint16_t attr0,
                                      std::uint16_t attr1,
                                      std::uint16_t attr2, bool x_valid,
                                      std::int16_t logical_x, bool y_valid,
                                      std::int16_t logical_y) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    if (slot >= static_cast<int>(kGoldenSunObjCommitOrderSlots)) return;
    // EC fires before the destination OAM slot is resolved (that only
    // becomes known at the later F0 write), so it reports slot=-1 --
    // truthfully "not yet assigned", never fabricated. Dedupe such entries
    // by a deterministic bucket derived from the source record address
    // instead of by slot; this is bookkeeping only; the reported event
    // still carries the real slot value (-1) unchanged.
    const std::size_t dedupe_index = slot >= 0
        ? static_cast<std::size_t>(slot)
        : static_cast<std::size_t>((source >> 3) %
                                    kGoldenSunObjCommitOrderSlots);
    const std::uint64_t frame = runtime_current_frame();
    if (frame != g_golden_sun_obj_commit_order_frame) {
        g_golden_sun_obj_commit_order_frame = frame;
        g_golden_sun_obj_commit_order_sequence_in_frame = 0;
    }
    const std::uint32_t sequence =
        g_golden_sun_obj_commit_order_sequence_in_frame++;
    auto& last = g_golden_sun_obj_commit_order_last_by_route_slot[
        static_cast<std::size_t>(route)][dedupe_index];
    // D4 fires only on a gated, successful commit (already low-volume), so
    // it keeps the fine-grained equality check (logs every real coordinate
    // change). EC/F0 are restricted to the shadow-candidate signature
    // (tile=0/shape=1/size=0) at their call sites, but a *moving* shadow's
    // raw ATTR0/ATTR1 (Y/X) still changes almost every frame, which would
    // defeat a fine-grained check the same way the unfiltered routes did --
    // this flooded the cap and stalled scene transitions in an earlier
    // pass. For EC/F0, dedupe coarsely on (route, dedupe_index) alone: log
    // only the first frame this object is seen in the epoch (or the first
    // frame after any absence), not every per-frame position update. That
    // is still enough to answer "does this route ever carry the shadow
    // signature, and where in write order" -- the fine per-frame offset
    // tracking that question also wants comes from D4's own logical X/Y,
    // already fully sampled.
    const bool unchanged = route == GoldenSunObjCommitOrderRoute::D4
        ? (last.used && last.route == route && last.slot == slot &&
           last.source == source && last.attr0 == attr0 &&
           last.attr1 == attr1 && last.attr2 == attr2 &&
           last.x_valid == x_valid && last.y_valid == y_valid &&
           last.logical_x == logical_x && last.logical_y == logical_y)
        : (last.used && last.route == route);
    GoldenSunObjCommitOrderEvent event;
    event.frame = frame;
    event.sequence = sequence;
    event.source = source;
    event.slot = slot;
    event.route = route;
    event.attr0 = attr0;
    event.attr1 = attr1;
    event.attr2 = attr2;
    event.logical_x = logical_x;
    event.logical_y = logical_y;
    event.x_valid = x_valid;
    event.y_valid = y_valid;
    event.used = true;
    last = event;
    if (unchanged) return;
    if (g_golden_sun_obj_commit_order_count >=
            kGoldenSunObjCommitOrderLimit) {
        ++g_golden_sun_obj_commit_order_dropped;
        return;
    }
    g_golden_sun_obj_commit_order_events[
        g_golden_sun_obj_commit_order_count++] = event;
}

void report_golden_sun_obj_commit_order(const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled() || !reason) return;
    for (std::size_t i = 0; i < g_golden_sun_obj_commit_order_count; ++i) {
        const auto& event = g_golden_sun_obj_commit_order_events[i];
        std::fprintf(
            stderr,
            "[wide-obj-commit-order] auth_epoch=%llu reason=%s frame=%llu "
            "sequence=%u route=%s source=0x%08x slot=%d attr0=0x%04x "
            "attr1=0x%04x attr2=0x%04x x_valid=%u logical_x=%d y_valid=%u "
            "logical_y=%d\n",
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            reason, static_cast<unsigned long long>(event.frame),
            event.sequence,
            golden_sun_obj_commit_order_route_name(event.route),
            event.source, event.slot, static_cast<unsigned>(event.attr0),
            static_cast<unsigned>(event.attr1),
            static_cast<unsigned>(event.attr2), event.x_valid ? 1u : 0u,
            static_cast<int>(event.logical_x), event.y_valid ? 1u : 0u,
            static_cast<int>(event.logical_y));
    }
    if (g_golden_sun_obj_commit_order_dropped != 0u) {
        std::fprintf(stderr,
                     "[wide-obj-commit-order-overflow] auth_epoch=%llu "
                     "reason=%s dropped=%llu\n",
                     static_cast<unsigned long long>(
                         g_golden_sun_field_auth_epoch),
                     reason, static_cast<unsigned long long>(
                         g_golden_sun_obj_commit_order_dropped));
    }
}

std::uint64_t g_golden_sun_field_epoch_map_writes = 0;
std::uint64_t g_golden_sun_field_epoch_raw_writes = 0;
std::uint64_t g_golden_sun_field_epoch_map_dma_writes = 0;
std::uint64_t g_golden_sun_field_epoch_raw_dma_writes = 0;
unsigned g_golden_sun_field_table_cpu_logs_in_epoch = 0;
unsigned g_golden_sun_field_table_dma_logs_in_epoch = 0;

bool golden_sun_field_table_overlap(std::uint32_t address,
                                    std::uint32_t size,
                                    std::uint32_t table_start,
                                    std::uint32_t table_end,
                                    std::uint64_t* out_bytes) {
    if (size == 0u || !out_bytes) return false;
    const std::uint64_t first = address;
    const std::uint64_t last = first + size;
    const std::uint64_t lo = std::max<std::uint64_t>(first, table_start);
    const std::uint64_t hi = std::min<std::uint64_t>(last, table_end);
    if (hi <= lo) return false;
    *out_bytes = hi - lo;
    return true;
}

void invalidate_golden_sun_palace_table_authorization() {
    g_golden_sun_palace_table_authorized = false;
    g_golden_sun_palace_table_invalidated = true;
    reset_golden_sun_palace_active_region();
}

void invalidate_golden_sun_palace_table_if_overlapping(
    std::uint32_t address, std::uint32_t size) {
    std::uint64_t overlap = 0;
    if (golden_sun_field_table_overlap(
            address, size, kGoldenSunFieldMapAddress,
            kGoldenSunFieldMapAddress + 0x10000u, &overlap) ||
        golden_sun_field_table_overlap(
            address, size, kGoldenSunFieldRawAddress,
            kGoldenSunFieldRawAddress + 0x8000u, &overlap)) {
        invalidate_golden_sun_palace_table_authorization();
    }
}

void record_golden_sun_field_table_write(std::uint32_t address,
                                         std::uint32_t size) {
    if (!golden_sun_wide_diagnostics_enabled() || size == 0u) return;
    const std::uint64_t frame = runtime_current_frame();
    const std::uint32_t pc = runtime_current_pc();
    const auto record = [&](const char* table_name,
                            std::uint32_t table_start,
                            std::uint32_t table_end,
                            GoldenSunFieldTableStats* stats,
                            std::uint64_t* epoch_writes) {
        std::uint64_t bytes = 0;
        if (!golden_sun_field_table_overlap(
                address, size, table_start, table_end, &bytes)) {
            return;
        }
        ++stats->writes;
        stats->bytes += bytes;
        if (stats->first_frame == UINT64_MAX) {
            stats->first_frame = frame;
            stats->first_pc = pc;
        }
        stats->last_frame = frame;
        stats->last_pc = pc;
        ++*epoch_writes;
        if (table_start == kGoldenSunFieldMapAddress) {
            record_golden_sun_field_producer(
                g_golden_sun_field_map_producers,
                &g_golden_sun_field_map_producer_overflow, pc, address,
                size, bytes, frame);
        } else {
            record_golden_sun_field_producer(
                g_golden_sun_field_raw_producers,
                &g_golden_sun_field_raw_producer_overflow, pc, address,
                size, bytes, frame);
        }
        if (g_golden_sun_field_table_cpu_logs_in_epoch >=
                kGoldenSunFieldTableCpuLogLimitPerEpoch) {
            return;
        }
        ++g_golden_sun_field_table_cpu_logs_in_epoch;
        std::fprintf(
            stderr,
            "[wide-field-table] frame=%llu auth_epoch=%llu table=%s "
            "writer_pc=0x%08x addr=0x%08x size=%u overlap=%llu reason=%s\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            table_name, pc, address, size,
            static_cast<unsigned long long>(bytes),
            golden_sun_wide_policy_reason_name(
                g_golden_sun_wide_policy_last_reason));
    };
    record("map", kGoldenSunFieldMapAddress,
           kGoldenSunFieldMapAddress + 0x10000u,
           &g_golden_sun_field_map_writes, &g_golden_sun_field_epoch_map_writes);
    record("raw", kGoldenSunFieldRawAddress,
           kGoldenSunFieldRawAddress + 0x8000u,
           &g_golden_sun_field_raw_writes, &g_golden_sun_field_epoch_raw_writes);
}

bool golden_sun_dma_destination_bounds(std::uint32_t destination,
                                       std::uint32_t bytes,
                                       std::uint16_t control,
                                       std::uint32_t* out_first,
                                       std::uint32_t* out_size) {
    if (!out_first || !out_size || bytes == 0u) return false;
    const std::uint32_t step = (control & 0x0400u) != 0u ? 4u : 2u;
    if (bytes < step || (bytes % step) != 0u) return false;
    const std::uint32_t dest_control = (control >> 5) & 3u;
    const std::uint64_t first = dest_control == 1u
        ? (destination >= bytes - step
              ? static_cast<std::uint64_t>(destination) - (bytes - step)
              : 0u)
        : destination;
    const std::uint64_t last = dest_control == 2u
        ? static_cast<std::uint64_t>(destination) + step
        : (dest_control == 1u
              ? static_cast<std::uint64_t>(destination) + step
              : static_cast<std::uint64_t>(destination) + bytes);
    if (last <= first || last > 0x100000000ull) return false;
    *out_first = static_cast<std::uint32_t>(first);
    *out_size = static_cast<std::uint32_t>(last - first);
    return true;
}

void golden_sun_wide_dma_descriptor_observer(
    int channel, std::uint32_t pc, std::uint32_t source,
    std::uint32_t destination, std::uint32_t bytes, std::uint16_t control,
    int start_mode) {
    // The descriptor observer runs immediately before the DMA transfer. At
    // this point the shadow is the complete image that is about to become
    // visible OAM, while later shadow writes must wait for the next handoff.
    // Publish only this exact measured 1024-byte transfer; partial/unrelated
    // DMAs must never make pending provenance visible.
    if (gsr::widescreen::golden_sun_obj_provenance_dma_handoff(
            source, destination, bytes, control)) {
        g_golden_sun_obj_visible_provenance =
            g_golden_sun_obj_pending_provenance;
    }
    std::uint32_t first = 0;
    std::uint32_t span = 0;
    if (!golden_sun_dma_destination_bounds(
            destination, bytes, control, &first, &span)) return;
    // A DMA copy is still a table write for Palace authorization, even when
    // diagnostics are disabled and no authored-cell bits are changed.
    invalidate_golden_sun_palace_table_if_overlapping(first, span);
    if (!golden_sun_wide_diagnostics_enabled()) return;
    const std::uint64_t frame = runtime_current_frame();
    const auto record = [&](const char* table_name,
                            std::uint32_t table_start,
                            std::uint32_t table_end,
                            GoldenSunFieldTableStats* stats,
                            std::uint64_t* epoch_writes) {
        std::uint64_t overlap = 0;
        if (!golden_sun_field_table_overlap(
                first, span, table_start, table_end, &overlap)) return;
        ++stats->writes;
        stats->bytes += overlap;
        if (stats->first_frame == UINT64_MAX) {
            stats->first_frame = frame;
            stats->first_pc = pc;
        }
        stats->last_frame = frame;
        stats->last_pc = pc;
        ++*epoch_writes;
        if (g_golden_sun_field_table_dma_logs_in_epoch >=
                kGoldenSunFieldTableDmaLogLimitPerEpoch) return;
        ++g_golden_sun_field_table_dma_logs_in_epoch;
        std::fprintf(
            stderr,
            "[wide-field-table-dma] frame=%llu auth_epoch=%llu table=%s "
            "writer_pc=0x%08x src=0x%08x dst=0x%08x size=%u "
            "overlap=%llu channel=%d start_mode=%d cnt_h=0x%04x "
            "reason=%s authored_cells=%u\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            table_name, pc, source, destination, bytes,
            static_cast<unsigned long long>(overlap), channel, start_mode,
            control, golden_sun_wide_policy_reason_name(
                         g_golden_sun_wide_policy_last_reason),
            g_golden_sun_field_authored.authored_count());
    };
    record("map", kGoldenSunFieldMapAddress,
           kGoldenSunFieldMapAddress + 0x10000u,
           &g_golden_sun_field_map_dma_writes,
           &g_golden_sun_field_epoch_map_dma_writes);
    record("raw", kGoldenSunFieldRawAddress,
           kGoldenSunFieldRawAddress + 0x8000u,
           &g_golden_sun_field_raw_dma_writes,
           &g_golden_sun_field_epoch_raw_dma_writes);
}

std::uint32_t golden_sun_field_table_crc(const std::uint8_t* ewram,
                                         std::size_t offset,
                                         std::size_t bytes) {
    return ewram ? gba::crc32(ewram + offset, bytes) : 0u;
}

bool refresh_golden_sun_palace_table_authorization() {
    if (!g_golden_sun_mode0_split_scroll) {
        g_golden_sun_palace_table_authorized = false;
        return false;
    }
    if (g_golden_sun_palace_table_invalidated ||
        g_golden_sun_palace_table_auth_attempted) {
        return g_golden_sun_palace_table_authorized;
    }
    // Fingerprint once after a complete clean split-scroll frame. Subsequent
    // scanlines rely on the CPU/DMA table-write invalidation observers; CRC is
    // deliberately not part of the per-scanline provider path.
    g_golden_sun_palace_table_auth_attempted = true;
    const gba::GbaBus* bus = gbarecomp::active_bus();
    if (!bus) {
        g_golden_sun_palace_table_authorized = false;
        return false;
    }
    const std::uint8_t* ewram = bus->ewram_ptr();
    const std::uint32_t map_crc = golden_sun_field_table_crc(
        ewram, kGoldenSunFieldMapOffset, kGoldenSunFieldMapBytes);
    const std::uint32_t raw_crc = golden_sun_field_table_crc(
        ewram, kGoldenSunFieldRawOffset, kGoldenSunFieldRawBytes);
    const bool fingerprint_match =
        gsr::widescreen::golden_sun_palace_table_fingerprint_matches(
            map_crc, raw_crc);
    g_golden_sun_palace_table_authorized = fingerprint_match;
    if (!fingerprint_match) {
        // A mismatch is evidence that this is not the measured Palace table.
        // Latch the failure for this scene/authentication epoch.
        g_golden_sun_palace_table_invalidated = true;
    }
    return g_golden_sun_palace_table_authorized;
}

bool prepare_golden_sun_palace_active_region(
    const gba::GbaBus* bus, const std::uint8_t* io) {
    if (!bus || !io || !g_golden_sun_mode0_split_scroll ||
        !g_golden_sun_palace_table_authorized) {
        return false;
    }
    const std::array<std::uint16_t, 6> scroll{{
        gsr::widescreen::read_io16(io, 0x14u),
        gsr::widescreen::read_io16(io, 0x16u),
        gsr::widescreen::read_io16(io, 0x18u),
        gsr::widescreen::read_io16(io, 0x1Au),
        gsr::widescreen::read_io16(io, 0x1Cu),
        gsr::widescreen::read_io16(io, 0x1Eu),
    }};
    if (!g_golden_sun_palace_region_scroll_valid ||
        g_golden_sun_palace_region_scroll != scroll) {
        g_golden_sun_palace_active_region.reset();
        g_golden_sun_palace_region_scroll = scroll;
        g_golden_sun_palace_region_scroll_valid = true;
        g_golden_sun_palace_region_build_attempted = false;
    }
    if (!g_golden_sun_palace_region_build_attempted) {
        g_golden_sun_palace_region_build_attempted = true;
        g_golden_sun_palace_active_region.build(
            g_golden_sun_wide_line_dispcnt, io, 0x20u, bus->ewram_ptr(),
            256u * 1024u, true);
    }
    return true;
}

void trace_golden_sun_field_map(const std::uint8_t* ewram) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        !ewram || g_golden_sun_wide_field_map_census.size() >= 16u) return;
    const std::uint64_t frame = runtime_current_frame();
    if (g_golden_sun_wide_field_map_trace_frame == frame) return;
    g_golden_sun_wide_field_map_trace_frame = frame;
    const std::uint32_t map_crc = golden_sun_field_table_crc(
        ewram, kGoldenSunFieldMapOffset, kGoldenSunFieldMapBytes);
    const std::uint32_t raw_crc = golden_sun_field_table_crc(
        ewram, kGoldenSunFieldRawOffset, kGoldenSunFieldRawBytes);
    for (const auto& census : g_golden_sun_wide_field_map_census) {
        if (census.auth_epoch == g_golden_sun_field_auth_epoch &&
            census.map_crc == map_crc && census.raw_crc == raw_crc) {
            return;
        }
    }
    g_golden_sun_wide_field_map_census.push_back(
        {g_golden_sun_field_auth_epoch, map_crc, raw_crc});
    std::array<std::uint16_t, 4096> counts{};
    for (std::size_t i = 0; i < 128u * 128u; ++i) {
        const std::size_t off = kGoldenSunFieldMapOffset + i * 4u;
        const std::uint16_t id = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(ewram[off]) |
             static_cast<std::uint16_t>(ewram[off + 1u] << 8)) & 0x0FFFu);
        ++counts[id];
    }
    std::array<std::uint16_t, 4> top_ids{};
    for (std::uint16_t id = 0; id < counts.size(); ++id) {
        for (std::size_t rank = 0; rank < top_ids.size(); ++rank) {
            if (counts[id] > counts[top_ids[rank]]) {
                for (std::size_t j = top_ids.size() - 1u; j > rank; --j)
                    top_ids[j] = top_ids[j - 1u];
                top_ids[rank] = id;
                break;
            }
        }
    }
    std::fprintf(stderr,
        "[wide-field-map] frame=%llu auth_epoch=%llu auth_cells=%u "
        "map_crc=%08x raw_crc=%08x top=%03x:%u,%03x:%u,%03x:%u,%03x:%u\n",
        frame, static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        g_golden_sun_field_authored.authored_count(), map_crc, raw_crc,
        top_ids[0], counts[top_ids[0]], top_ids[1], counts[top_ids[1]],
        top_ids[2], counts[top_ids[2]], top_ids[3], counts[top_ids[3]]);
}

void trace_golden_sun_wide_scene(std::uint16_t dispcnt,
                                 const std::uint8_t* io,
                                 unsigned flags) {
    if (!golden_sun_wide_diagnostics_enabled() || !g_ws_active || !io ||
        g_golden_sun_wide_scene_signatures.size() >= 64u)
        return;
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](std::uint16_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    // Deduplicate scene evidence by raster configuration and coarse camera
    // position. Raw scroll values change every frame while walking and used
    // to consume the entire 64-entry bound before a later scene was reached.
    mix(dispcnt);
    mix(static_cast<std::uint16_t>(flags));
    for (std::size_t off = 0x08u; off <= 0x0Eu; off += 2u)
        mix(gsr::widescreen::read_io16(io, off));
    for (std::size_t off = 0x14u; off <= 0x1Eu; off += 2u)
        mix(static_cast<std::uint16_t>(
            gsr::widescreen::read_io16(io, off) & ~0x001Fu));
    if (std::find(g_golden_sun_wide_scene_signatures.begin(),
                  g_golden_sun_wide_scene_signatures.end(), hash) !=
        g_golden_sun_wide_scene_signatures.end()) {
        return;
    }
    g_golden_sun_wide_scene_signatures.push_back(hash);
    std::fprintf(stderr,
        "[wide-scene] frame=%llu dispcnt=%04x flags=%x "
        "bgcnt=%04x/%04x/%04x/%04x scroll=%04x,%04x/%04x,%04x/%04x,%04x\n",
        runtime_current_frame(), dispcnt, flags,
        gsr::widescreen::read_io16(io, 0x08u),
        gsr::widescreen::read_io16(io, 0x0Au),
        gsr::widescreen::read_io16(io, 0x0Cu),
        gsr::widescreen::read_io16(io, 0x0Eu),
        gsr::widescreen::read_io16(io, 0x14u),
        gsr::widescreen::read_io16(io, 0x16u),
        gsr::widescreen::read_io16(io, 0x18u),
        gsr::widescreen::read_io16(io, 0x1Au),
        gsr::widescreen::read_io16(io, 0x1Cu),
        gsr::widescreen::read_io16(io, 0x1Eu));
}

void trace_golden_sun_wide_policy_sample(
    std::uint64_t frame, std::uint16_t dispcnt, const std::uint8_t* io,
    unsigned flags, GoldenSunWidePolicyReason reason) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        g_golden_sun_wide_policy_sample_end_frame == UINT64_MAX ||
        frame > g_golden_sun_wide_policy_sample_end_frame ||
        g_golden_sun_wide_policy_samples_in_window >=
            kGoldenSunWidePolicySampleLimit ||
        g_golden_sun_wide_policy_sample_frame == frame) {
        return;
    }
    g_golden_sun_wide_policy_sample_frame = frame;
    ++g_golden_sun_wide_policy_samples_in_window;
    ++g_golden_sun_wide_policy_sample_count;
    const auto read = [&](std::size_t offset) -> std::uint16_t {
        return io ? gsr::widescreen::read_io16(io, offset) : 0u;
    };
    const std::uint16_t bg1_hofs = read(0x14u);
    const std::uint16_t bg1_vofs = read(0x16u);
    const std::uint16_t bg2_hofs = read(0x18u);
    const std::uint16_t bg2_vofs = read(0x1Au);
    const std::uint16_t bg3_hofs = read(0x1Cu);
    const std::uint16_t bg3_vofs = read(0x1Eu);
    std::uint32_t map_crc = 0;
    std::uint32_t raw_crc = 0;
    if (const gba::GbaBus* bus = gbarecomp::active_bus()) {
        const std::uint8_t* ewram = bus->ewram_ptr();
        map_crc = golden_sun_field_table_crc(
            ewram, kGoldenSunFieldMapOffset, kGoldenSunFieldMapBytes);
        raw_crc = golden_sun_field_table_crc(
            ewram, kGoldenSunFieldRawOffset, kGoldenSunFieldRawBytes);
        // Reuse the existing bounded census/top-ID trace. It records no map
        // bytes and deduplicates by auth epoch plus both table CRCs.
        trace_golden_sun_field_map(ewram);
    }
    std::fprintf(
        stderr,
        "[wide-policy-sample] frame=%llu transition=%llu sample=%u "
        "reason=%s flags=%x dispcnt=%04x "
        "bgcnt=%04x/%04x/%04x/%04x "
        "raw_scroll=%04x,%04x/%04x,%04x/%04x,%04x "
        "effective_scroll=%03x,%03x/%03x,%03x/%03x,%03x "
        "auth_epoch=%llu auth_cells=%u map_crc=%08x raw_crc=%08x "
        "map_writes=%llu raw_writes=%llu epoch_map_writes=%llu "
        "epoch_raw_writes=%llu epoch_map_dma=%llu epoch_raw_dma=%llu\n",
        static_cast<unsigned long long>(frame),
        static_cast<unsigned long long>(g_golden_sun_wide_policy_sample_transition),
        g_golden_sun_wide_policy_samples_in_window,
        golden_sun_wide_policy_reason_name(reason), flags, dispcnt,
        read(0x08u), read(0x0Au), read(0x0Cu), read(0x0Eu),
        bg1_hofs, bg1_vofs, bg2_hofs, bg2_vofs, bg3_hofs, bg3_vofs,
        bg1_hofs & 0x01FFu, bg1_vofs & 0x01FFu,
        bg2_hofs & 0x01FFu, bg2_vofs & 0x01FFu,
        bg3_hofs & 0x01FFu, bg3_vofs & 0x01FFu,
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        g_golden_sun_field_authored.authored_count(), map_crc, raw_crc,
        static_cast<unsigned long long>(g_golden_sun_field_map_writes.writes),
        static_cast<unsigned long long>(g_golden_sun_field_raw_writes.writes),
        static_cast<unsigned long long>(g_golden_sun_field_epoch_map_writes),
        static_cast<unsigned long long>(g_golden_sun_field_epoch_raw_writes),
        static_cast<unsigned long long>(
            g_golden_sun_field_epoch_map_dma_writes),
        static_cast<unsigned long long>(
            g_golden_sun_field_epoch_raw_dma_writes));
}

void trace_golden_sun_wide_policy(std::uint16_t dispcnt,
                                  const std::uint8_t* io,
                                  unsigned flags,
                                  GoldenSunWidePolicyReason reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    const std::uint64_t frame = runtime_current_frame();
    const bool changed = !g_golden_sun_wide_policy_seen ||
        g_golden_sun_wide_policy_last_flags != flags ||
        g_golden_sun_wide_policy_last_reason != reason;
    if (changed) {
        const char* previous = g_golden_sun_wide_policy_seen
            ? golden_sun_wide_policy_reason_name(
                  g_golden_sun_wide_policy_last_reason)
            : "none";
        ++g_golden_sun_wide_policy_transition_count;
        g_golden_sun_wide_policy_last_flags = flags;
        g_golden_sun_wide_policy_last_reason = reason;
        g_golden_sun_wide_policy_seen = true;
        if (g_golden_sun_wide_policy_logged_transitions <
                kGoldenSunWidePolicyTransitionLimit) {
            ++g_golden_sun_wide_policy_logged_transitions;
            g_golden_sun_wide_policy_sample_transition =
                g_golden_sun_wide_policy_transition_count;
            g_golden_sun_wide_policy_sample_frame = UINT64_MAX;
            g_golden_sun_wide_policy_samples_in_window = 0;
            g_golden_sun_wide_policy_sample_end_frame =
                frame + kGoldenSunWidePolicySampleLimit - 1u;
            const auto read = [&](std::size_t offset) -> std::uint16_t {
                return io ? gsr::widescreen::read_io16(io, offset) : 0u;
            };
            std::fprintf(
                stderr,
                "[wide-policy] frame=%llu transition=%llu from=%s to=%s "
                "flags=%x dispcnt=%04x bgcnt=%04x/%04x/%04x/%04x "
                "raw_scroll=%04x,%04x/%04x,%04x/%04x,%04x "
                "effective_scroll=%03x,%03x/%03x,%03x/%03x,%03x "
                "auth_epoch=%llu\n",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(
                    g_golden_sun_wide_policy_transition_count), previous,
                golden_sun_wide_policy_reason_name(reason), flags, dispcnt,
                read(0x08u), read(0x0Au), read(0x0Cu), read(0x0Eu),
                read(0x14u), read(0x16u), read(0x18u), read(0x1Au),
                read(0x1Cu), read(0x1Eu), read(0x14u) & 0x01FFu,
                read(0x16u) & 0x01FFu, read(0x18u) & 0x01FFu,
                read(0x1Au) & 0x01FFu, read(0x1Cu) & 0x01FFu,
                read(0x1Eu) & 0x01FFu,
                static_cast<unsigned long long>(g_golden_sun_field_auth_epoch));
        } else {
            g_golden_sun_wide_policy_sample_end_frame = UINT64_MAX;
        }
    }
    trace_golden_sun_wide_policy_sample(
        frame, dispcnt, io, flags, reason);
}

void report_golden_sun_field_provider_trace(std::uint64_t frame,
                                            const char* reason);
void report_golden_sun_palace_margin_diagnostics(std::uint64_t frame,
                                                 const char* reason);
void report_golden_sun_obj_y_steady_state_diagnostics(std::uint64_t frame,
                                                     const char* reason);
void report_golden_sun_obj_y_transition_diagnostics(std::uint64_t frame,
                                                    const char* reason);

const char* golden_sun_field_margin_region(int hw_x, int screen_y,
                                           std::size_t* out_region) {
    std::size_t region = 3u; // bottom (the normal provider margin case)
    const char* name = "bottom";
    // Vertical rows span the whole output width, so classify them before the
    // horizontal sides; corners remain attributable to their vertical row.
    if (screen_y < 0) {
        region = 2u;
        name = "top";
    } else if (screen_y >= static_cast<int>(gsr::widescreen::kNativeHeight)) {
        region = 3u;
        name = "bottom";
    } else if (hw_x < 0) {
        region = 0u;
        name = "left";
    } else if (hw_x >= static_cast<int>(gsr::widescreen::kNativeWidth)) {
        region = 1u;
        name = "right";
    }
    if (out_region) *out_region = region;
    return name;
}

void record_golden_sun_field_map_id_attribution(
    int bg, int hw_x, int screen_y,
    const gsr::widescreen::GoldenSunFieldTilemapMetadata& metadata) {
    if (!golden_sun_wide_diagnostics_enabled() || bg < 0 || bg >= 4 ||
        !metadata.has_raw_entry) return;
    std::size_t region = 0;
    golden_sun_field_margin_region(hw_x, screen_y, &region);
    auto& entries = g_golden_sun_field_map_id_attribution[
        static_cast<std::size_t>(bg)][region];
    for (auto& entry : entries) {
        if (entry.replacements != 0u && entry.map_id == metadata.map_id) {
            ++entry.replacements;
            return;
        }
    }
    for (auto& entry : entries) {
        if (entry.replacements == 0u) {
            entry.map_id = metadata.map_id;
            entry.replacements = 1u;
            return;
        }
    }
    ++g_golden_sun_field_map_id_attribution_overflow[
        static_cast<std::size_t>(bg)][region];
}

void report_golden_sun_field_map_id_attribution(std::uint64_t frame,
                                                const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    static constexpr const char* kRegions[] = {
        "left", "right", "top", "bottom"};
    for (std::size_t bg = 0; bg < 4u; ++bg) {
        for (std::size_t region = 0; region < 4u; ++region) {
            const auto& entries = g_golden_sun_field_map_id_attribution[bg][region];
            bool any = g_golden_sun_field_map_id_attribution_overflow[bg][region] != 0u;
            for (const auto& entry : entries) any |= entry.replacements != 0u;
            if (!any) continue;
            std::fprintf(stderr,
                         "[wide-field-map-id] frame=%llu auth_epoch=%llu "
                         "reason=%s bg=%zu region=%s overflow=%llu ids=",
                         static_cast<unsigned long long>(frame),
                         static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                         reason, bg, kRegions[region],
                         static_cast<unsigned long long>(
                             g_golden_sun_field_map_id_attribution_overflow[bg][region]));
            bool first = true;
            for (const auto& entry : entries) {
                if (entry.replacements == 0u) continue;
                std::fprintf(stderr, "%s%03x:%llu", first ? "" : ",",
                             entry.map_id,
                             static_cast<unsigned long long>(entry.replacements));
                first = false;
            }
            std::fputc('\n', stderr);
        }
    }
}

void write_golden_sun_wide_scroll_trace_csv();

void report_golden_sun_wide_diagnostics_at_exit() {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    std::fprintf(stderr,
        "[wide-obj-experimental-cull-summary] logs=%u dropped=%llu\n",
        g_golden_sun_experimental_cull_logs,
        static_cast<unsigned long long>(
            g_golden_sun_experimental_cull_logs_dropped));
    std::fprintf(stderr,
        "[wide-obj-writer-summary] recognized=%llu identity_mismatch=%llu "
        "unknown_variant=%llu\n",
        static_cast<unsigned long long>(
            g_golden_sun_func1dc8_writer_recognized),
        static_cast<unsigned long long>(
            g_golden_sun_func1dc8_writer_identity_mismatch),
        static_cast<unsigned long long>(
            g_golden_sun_func1dc8_writer_unknown_variant));
    if (g_golden_sun_margin_diagnostic_callbacks != 0u) {
        std::fprintf(
            stderr,
            "[wide-margin-summary] callbacks=%llu logged=%llu\n",
            static_cast<unsigned long long>(
                g_golden_sun_margin_diagnostic_callbacks),
            static_cast<unsigned long long>(g_golden_sun_margin_diagnostic_logged));
        report_golden_sun_margin_diagnostic_sample(
            runtime_current_frame(), "summary", g_golden_sun_margin_diagnostic_total);
    }
    gba::vram_trace::OamShadowTraceStats oam_shadow{};
    gba::vram_trace::OamDmaTraceStats oam_dma{};
    gba::vram_trace::get_oam_shadow_trace_stats(&oam_shadow);
    gba::vram_trace::get_oam_dma_trace_stats(&oam_dma);
    std::fprintf(
        stderr,
        "[oam-shadow-summary] writes=%llu bytes=%llu slot_events=%llu "
        "dma_writes=%llu dma_bytes=%llu "
        "slot_overwrites=%llu unique_slots=%llu overwritten_slots=%llu "
        "records_dropped=%llu\n",
        static_cast<unsigned long long>(oam_shadow.write_calls),
        static_cast<unsigned long long>(oam_shadow.bytes),
        static_cast<unsigned long long>(oam_shadow.slot_write_events),
        static_cast<unsigned long long>(oam_shadow.dma_write_calls),
        static_cast<unsigned long long>(oam_shadow.dma_bytes),
        static_cast<unsigned long long>(oam_shadow.slot_overwrite_events),
        static_cast<unsigned long long>(oam_shadow.unique_slots),
        static_cast<unsigned long long>(oam_shadow.overwritten_slots),
        static_cast<unsigned long long>(oam_shadow.records_dropped));
    std::fprintf(
        stderr,
        "[oam-dma-summary] transfers=%llu bytes=%llu used_slots=%llu "
        "visible_slots=%llu nonzero_slots=%llu records_dropped=%llu "
        "last_src=0x%08x last_dst=0x%08x last_size=%u last_used=%u "
        "last_visible=%u last_nonzero=%u last_raw_x_ge_240=%u "
        "last_raw_y_ge_160=%u\n",
        static_cast<unsigned long long>(oam_dma.transfers),
        static_cast<unsigned long long>(oam_dma.bytes),
        static_cast<unsigned long long>(oam_dma.used_slot_total),
        static_cast<unsigned long long>(oam_dma.visible_slot_total),
        static_cast<unsigned long long>(oam_dma.nonzero_slot_total),
        static_cast<unsigned long long>(oam_dma.records_dropped),
        oam_dma.last_source, oam_dma.last_destination, oam_dma.last_size,
        oam_dma.last_used_slots, oam_dma.last_visible_slots,
        oam_dma.last_nonzero_slots, oam_dma.last_raw_x_ge_240,
        oam_dma.last_raw_y_ge_160);
    gba::vram_trace::OamAttr0TraceStats attr0{};
    gba::vram_trace::get_oam_attr0_trace_stats(&attr0);
    std::fprintf(stderr,
        "[oam-attr0-summary] post_copies=%llu candidates=%llu exact_192=%llu "
        "other=%llu unseen=%llu groups_dropped=%llu groups=",
        static_cast<unsigned long long>(attr0.post_copies),
        static_cast<unsigned long long>(attr0.candidate_slots),
        static_cast<unsigned long long>(attr0.exact_192_slots),
        static_cast<unsigned long long>(attr0.other_slots),
        static_cast<unsigned long long>(attr0.unseen_slots),
        static_cast<unsigned long long>(attr0.groups_dropped));
    for (std::uint32_t i = 0; i < attr0.group_count; ++i) {
        const auto& g = attr0.groups[i];
        const char* kind = g.kind == gba::vram_trace::OamAttr0WriterKind::Cpu
            ? "cpu" : (g.kind == gba::vram_trace::OamAttr0WriterKind::Dma
                ? "dma" : "unseen");
        std::fprintf(stderr, "%s%u:%s:0x%08x:%u..%u/%u:g%llu:c%llu",
            i == 0u ? "" : ";", static_cast<unsigned>(g.raw_y), kind,
            g.writer_pc, g.slot_first, g.slot_last, g.slot_count,
            static_cast<unsigned long long>(g.generation),
            static_cast<unsigned long long>(g.cycle));
    }
    std::fputc('\n', stderr);
    std::fprintf(
        stderr,
        "[wide-policy-summary] frame=%llu transitions=%llu logged=%llu "
        "samples=%llu auth_epoch=%llu final_reason=%s final_flags=%x\n",
        static_cast<unsigned long long>(runtime_current_frame()),
        static_cast<unsigned long long>(g_golden_sun_wide_policy_transition_count),
        static_cast<unsigned long long>(g_golden_sun_wide_policy_logged_transitions),
        static_cast<unsigned long long>(g_golden_sun_wide_policy_sample_count),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
        golden_sun_wide_policy_reason_name(g_golden_sun_wide_policy_last_reason),
        g_golden_sun_wide_policy_last_flags);
    report_golden_sun_field_provider_trace(runtime_current_frame(), "exit");
    report_golden_sun_palace_margin_diagnostics(runtime_current_frame(), "exit");
    report_golden_sun_obj_b328_diagnostics(runtime_current_frame(), "exit");
    report_golden_sun_obj_y_transition_diagnostics(runtime_current_frame(), "exit");
    report_golden_sun_obj_y_steady_state_diagnostics(runtime_current_frame(), "exit");
    report_golden_sun_field_map_id_attribution(runtime_current_frame(), "exit");
    report_golden_sun_field_producers("exit");
    report_golden_sun_obj_record_census("exit");
    report_golden_sun_obj_record_values("exit");
    report_golden_sun_obj_commit_order("exit");
    write_golden_sun_wide_scroll_trace_csv();
    std::fprintf(
        stderr,
        "[wide-field-table-summary] map_writes=%llu map_bytes=%llu "
        "map_first_frame=%llu map_first_pc=0x%08x map_last_frame=%llu "
        "map_last_pc=0x%08x raw_writes=%llu raw_bytes=%llu "
        "raw_first_frame=%llu raw_first_pc=0x%08x raw_last_frame=%llu "
        "raw_last_pc=0x%08x auth_cells=%u\n",
        static_cast<unsigned long long>(g_golden_sun_field_map_writes.writes),
        static_cast<unsigned long long>(g_golden_sun_field_map_writes.bytes),
        static_cast<unsigned long long>(g_golden_sun_field_map_writes.first_frame),
        g_golden_sun_field_map_writes.first_pc,
        static_cast<unsigned long long>(g_golden_sun_field_map_writes.last_frame),
        g_golden_sun_field_map_writes.last_pc,
        static_cast<unsigned long long>(g_golden_sun_field_raw_writes.writes),
        static_cast<unsigned long long>(g_golden_sun_field_raw_writes.bytes),
        static_cast<unsigned long long>(g_golden_sun_field_raw_writes.first_frame),
        g_golden_sun_field_raw_writes.first_pc,
        static_cast<unsigned long long>(g_golden_sun_field_raw_writes.last_frame),
        g_golden_sun_field_raw_writes.last_pc,
        g_golden_sun_field_authored.authored_count());
    std::fprintf(
        stderr,
        "[wide-field-table-dma-summary] map_writes=%llu map_bytes=%llu "
        "raw_writes=%llu raw_bytes=%llu epoch_map_writes=%llu "
        "epoch_raw_writes=%llu authored_cells=%u\n",
        static_cast<unsigned long long>(
            g_golden_sun_field_map_dma_writes.writes),
        static_cast<unsigned long long>(
            g_golden_sun_field_map_dma_writes.bytes),
        static_cast<unsigned long long>(
            g_golden_sun_field_raw_dma_writes.writes),
        static_cast<unsigned long long>(
            g_golden_sun_field_raw_dma_writes.bytes),
        static_cast<unsigned long long>(
            g_golden_sun_field_epoch_map_dma_writes),
        static_cast<unsigned long long>(
            g_golden_sun_field_epoch_raw_dma_writes),
        g_golden_sun_field_authored.authored_count());
}

void report_golden_sun_field_provider_trace(std::uint64_t frame,
                                             const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    for (std::size_t bg = 0; bg < g_golden_sun_field_provider_trace.size();
         ++bg) {
        const GoldenSunFieldProviderTrace& stat =
            g_golden_sun_field_provider_trace[bg];
        const std::int32_t min_x = stat.calls != 0u ? stat.min_x : 0;
        const std::int32_t max_x = stat.calls != 0u ? stat.max_x : 0;
        const std::int32_t min_y = stat.calls != 0u ? stat.min_y : 0;
        const std::int32_t max_y = stat.calls != 0u ? stat.max_y : 0;
        std::fprintf(
            stderr,
            "[wide-field-provider] frame=%llu auth_epoch=%llu reason=%s "
            "bg=%zu calls=%llu equal=%llu split=%llu replacements=%llu "
            "unavailable=%llu precondition=%llu boundary=%llu raw=%llu "
            "palace_region=%llu lookup_miss=%llu coord=%d..%d,%d..%d\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            reason, bg, static_cast<unsigned long long>(stat.calls),
            static_cast<unsigned long long>(stat.equal_scroll_calls),
            static_cast<unsigned long long>(stat.split_scroll_calls),
            static_cast<unsigned long long>(stat.replacements),
            static_cast<unsigned long long>(stat.unavailable),
            static_cast<unsigned long long>(stat.precondition_rejects),
            static_cast<unsigned long long>(stat.boundary_rejects),
            static_cast<unsigned long long>(stat.raw_unavailable),
            static_cast<unsigned long long>(stat.palace_region_rejects),
            static_cast<unsigned long long>(stat.lookup_misses), min_x,
            max_x, min_y, max_y);
    }
}

void report_golden_sun_palace_margin_diagnostics(std::uint64_t frame,
                                                 const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    static constexpr const char* kRegions[] = {
        "left", "right", "top", "bottom"};
    const auto& diagnostic = g_golden_sun_palace_margin_diagnostics;
    for (std::size_t bg = 0; bg < GoldenSunPalaceMarginDiagnostics::kBgCount;
         ++bg) {
        for (std::size_t region = 0;
             region < GoldenSunPalaceMarginDiagnostics::kRegionCount;
             ++region) {
            for (std::size_t outcome = 0;
                 outcome < GoldenSunPalaceMarginDiagnostics::kOutcomeCount;
                 ++outcome) {
                const std::uint64_t count =
                    diagnostic.counts[bg][region][outcome];
                if (count == 0u) continue;
                std::fprintf(
                    stderr,
                    "[wide-palace-margin] frame=%llu auth_epoch=%llu "
                    "reason=%s bg=%zu region=%s outcome=%s count=%llu\n",
                    static_cast<unsigned long long>(frame),
                    static_cast<unsigned long long>(
                        g_golden_sun_field_auth_epoch),
                    reason, bg + 1u, kRegions[region],
                    golden_sun_palace_margin_outcome_name(
                        static_cast<GoldenSunPalaceMarginOutcome>(outcome)),
                    static_cast<unsigned long long>(count));
            }
        }
    }
    for (std::size_t i = 0; i < diagnostic.sample_count; ++i) {
        const auto& sample = diagnostic.samples[i];
        static constexpr const char* kSampleRegions[] = {
            "left", "right", "top", "bottom"};
        std::fprintf(
            stderr,
            "[wide-palace-margin-sample] frame=%llu auth_epoch=%llu "
            "reason=%s bg=%u region=%s outcome=%s hw_x=%d screen_y=%d "
            "metadata=%u map_x=%u map_y=%u map_id=0x%03x\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            reason, static_cast<unsigned>(sample.bg),
            kSampleRegions[sample.region],
            golden_sun_palace_margin_outcome_name(sample.outcome),
            static_cast<int>(sample.hw_x), static_cast<int>(sample.screen_y),
            sample.metadata_valid ? 1u : 0u,
            static_cast<unsigned>(sample.map_x),
            static_cast<unsigned>(sample.map_y),
            static_cast<unsigned>(sample.map_id));
    }
    std::fprintf(stderr,
                 "[wide-palace-margin-summary] frame=%llu auth_epoch=%llu "
                 "reason=%s samples=%zu dropped=%llu\n",
                 static_cast<unsigned long long>(frame),
                 static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                 reason, diagnostic.sample_count,
                 static_cast<unsigned long long>(diagnostic.samples_dropped));
}

void report_golden_sun_obj_y_transition_diagnostics(std::uint64_t frame,
                                                   const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    for (std::size_t r = 0; r < static_cast<std::size_t>(
             GoldenSunObjYTransitionReason::Count); ++r) {
        for (std::size_t region = 0; region < static_cast<std::size_t>(
                 GoldenSunObjYTransitionRegion::Count); ++region) {
            for (std::size_t resolution = 0; resolution < static_cast<std::size_t>(
                     GoldenSunObjYTransitionResolution::Count); ++resolution) {
                const auto count = g_golden_sun_obj_y_transition_counts
                    [r][region][resolution];
                if (count == 0u) continue;
                std::fprintf(stderr,
                    "[wide-obj-y-transition-summary] frame=%llu "
                    "auth_epoch=%llu reason=%s resolution=%s region=%s "
                    "count=%llu\n",
                    static_cast<unsigned long long>(frame),
                    static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                    golden_sun_obj_y_transition_reason_name(
                        static_cast<GoldenSunObjYTransitionReason>(r)),
                    golden_sun_obj_y_transition_resolution_name(
                        static_cast<GoldenSunObjYTransitionResolution>(resolution)),
                    golden_sun_obj_y_transition_region_name(
                        static_cast<GoldenSunObjYTransitionRegion>(region)),
                    static_cast<unsigned long long>(count));
            }
        }
    }
    for (unsigned bucket = 0; bucket < kGoldenSunObjYTransitionBucketCount;
         ++bucket) {
        const auto transition_bucket =
            static_cast<GoldenSunObjYTransitionBucket>(bucket);
        for (unsigned i = 0;
             i < g_golden_sun_obj_y_transition_sample_counts[bucket]; ++i) {
        const auto& sample = g_golden_sun_obj_y_transition_samples[bucket][i];
        std::fprintf(stderr,
            "[wide-obj-y-transition-sample] bucket=%s frame=%llu "
            "auth_epoch=%llu "
            "slot=%d raw_y=%d canonical_y=%d output_y=%d resolution=%s "
            "reason=%s region=%s attr0=0x%04x attr1=0x%04x attr2=0x%04x "
            "expected_attr0=0x%04x expected_attr1=0x%04x expected_attr2=0x%04x "
            "expected_target=0x%08x provenance_frame=%llu "
            "provenance_epoch=%llu target=0x%08x "
            "writer_pc=0x%08x writer_generation=%llu\n",
            golden_sun_obj_y_transition_bucket_name(transition_bucket),
            static_cast<unsigned long long>(sample.frame),
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            sample.slot, sample.raw_y, sample.canonical_y, sample.output_y,
            golden_sun_obj_y_transition_resolution_name(sample.resolution),
            golden_sun_obj_y_transition_reason_name(sample.reason),
            golden_sun_obj_y_transition_region_name(sample.region),
            sample.attr0, sample.attr1, sample.attr2,
            sample.expected_attr0, sample.expected_attr1, sample.expected_attr2,
            sample.expected_target,
            static_cast<unsigned long long>(sample.provenance_frame),
            static_cast<unsigned long long>(sample.provenance_epoch),
            sample.target_address, sample.writer_pc,
            static_cast<unsigned long long>(sample.writer_generation));
        }
    }
    std::fprintf(stderr,
        "[wide-obj-y-transition] frame=%llu auth_epoch=%llu reason=%s "
        "raw_y=159..199 dedup=active-candidate-identity+slot-target "
        "active_candidate_canonical_samples=%u cap=%u dropped=%llu "
        "signed_bottom_samples=%u cap=%u dropped=%llu "
        "outcome_transition_samples=%u cap=%u dropped=%llu\n",
        static_cast<unsigned long long>(frame),
        static_cast<unsigned long long>(g_golden_sun_field_auth_epoch), reason,
        g_golden_sun_obj_y_transition_sample_counts[static_cast<std::size_t>(
            GoldenSunObjYTransitionBucket::ActiveCandidateCanonical)],
        kGoldenSunObjYTransitionSampleLimit,
        static_cast<unsigned long long>(g_golden_sun_obj_y_transition_samples_dropped[
            static_cast<std::size_t>(GoldenSunObjYTransitionBucket::ActiveCandidateCanonical)]),
        g_golden_sun_obj_y_transition_sample_counts[static_cast<std::size_t>(
            GoldenSunObjYTransitionBucket::SignedBottom)],
        kGoldenSunObjYTransitionSampleLimit,
        static_cast<unsigned long long>(g_golden_sun_obj_y_transition_samples_dropped[
            static_cast<std::size_t>(GoldenSunObjYTransitionBucket::SignedBottom)]),
        g_golden_sun_obj_y_transition_sample_counts[static_cast<std::size_t>(
            GoldenSunObjYTransitionBucket::OutcomeTransition)],
        kGoldenSunObjYTransitionSampleLimit,
        static_cast<unsigned long long>(g_golden_sun_obj_y_transition_samples_dropped[
            static_cast<std::size_t>(GoldenSunObjYTransitionBucket::OutcomeTransition)]));
}

void report_golden_sun_obj_y_steady_state_diagnostics(std::uint64_t frame,
                                                     const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    std::uint64_t total_calls = 0;
    std::uint64_t total_192 = 0;
    for (std::size_t slot = 0; slot < g_golden_sun_obj_y_slot_diagnostics.size(); ++slot) {
        const auto& stat = g_golden_sun_obj_y_slot_diagnostics[slot];
        if (stat.calls == 0u) continue;
        total_calls += stat.calls;
        total_192 += stat.raw_192;
        std::fprintf(stderr,
                     "[wide-obj-y-slot] frame=%llu auth_epoch=%llu reason=%s "
                     "slot=%zu calls=%llu raw_160_191=%llu raw_192=%llu "
                     "raw_193_199=%llu raw_200_255=%llu outcomes=",
                     static_cast<unsigned long long>(frame),
                     static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                     reason, slot, static_cast<unsigned long long>(stat.calls),
                     static_cast<unsigned long long>(stat.raw_160_191),
                     static_cast<unsigned long long>(stat.raw_192),
                     static_cast<unsigned long long>(stat.raw_193_199),
                     static_cast<unsigned long long>(stat.raw_200_255));
        bool first = true;
        for (std::size_t i = 0; i < stat.outcomes.size(); ++i) {
            if (stat.outcomes[i] == 0u) continue;
            std::fprintf(stderr, "%s%s:%llu", first ? "" : ",",
                         golden_sun_obj_y_outcome_name(
                             static_cast<GoldenSunObjYOutcome>(i)),
                         static_cast<unsigned long long>(stat.outcomes[i]));
            first = false;
        }
        std::fputc('\n', stderr);
    }
    std::fprintf(stderr,
                 "[wide-obj-y-summary] frame=%llu auth_epoch=%llu reason=%s "
                 "slots=%zu calls=%llu exact_192=%llu "
                 "alias_records=%u alias_dropped=%llu "
                 "edge_alias_activations=%llu "
                 "steady_state_only=1 transition_records_excluded=1\n",
                 static_cast<unsigned long long>(frame),
                 static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                 reason, g_golden_sun_obj_y_slot_diagnostics.size(),
                 static_cast<unsigned long long>(total_calls),
                 static_cast<unsigned long long>(total_192),
                 g_golden_sun_obj_y_alias_logs_in_epoch,
                 static_cast<unsigned long long>(
                     g_golden_sun_obj_y_alias_logs_dropped),
                 static_cast<unsigned long long>(
                     g_golden_sun_obj_y_edge_alias_activations));
}

// WIDE-01 parallax hypothesis capture (temporary): does each BG layer's
// scroll hold a constant offset/ratio to the gameplay layer within a room,
// observable live from hardware scroll registers alone? Gated by the same
// GBARECOMP_VRAM_MAP_TRACE toggle as the rest of this file; off by default,
// zero cost when disabled. One row per authentic visible frame (not per
// scanline -- the margin-policy callback fires once per raster line). A
// fixed-size ring bounds the CSV to at most kGoldenSunWideScrollTraceCap
// frames (~5.5 minutes at 60 fps) so a play session cannot grow this file
// without limit; oldest rows are dropped first.
constexpr std::size_t kGoldenSunWideScrollTraceCap = 20000u;

struct GoldenSunWideScrollTraceRow {
    std::uint64_t frame = 0;
    std::uint16_t dispcnt = 0;
    std::uint16_t bg0cnt = 0;
    std::uint16_t bg1cnt = 0;
    std::uint16_t bg2cnt = 0;
    std::uint16_t bg3cnt = 0;
    std::uint16_t bg0_hofs = 0;
    std::uint16_t bg0_vofs = 0;
    std::uint16_t bg1_hofs = 0;
    std::uint16_t bg1_vofs = 0;
    std::uint16_t bg2_hofs = 0;
    std::uint16_t bg2_vofs = 0;
    std::uint16_t bg3_hofs = 0;
    std::uint16_t bg3_vofs = 0;
    std::uint8_t margin_reason = 0;
    std::uint8_t split_reason = 0;
    std::uint8_t flags = 0;
    std::uint32_t map_crc = 0;
    std::uint32_t raw_crc = 0;
    std::uint64_t auth_epoch = 0;
};
std::array<GoldenSunWideScrollTraceRow, kGoldenSunWideScrollTraceCap>
    g_golden_sun_wide_scroll_trace{};
std::uint64_t g_golden_sun_wide_scroll_trace_count = 0;
std::uint64_t g_golden_sun_wide_scroll_trace_last_frame = UINT64_MAX;

// Record at most one row per frame, using the field-map census helper
// already used elsewhere in this file (golden_sun_field_table_crc over the
// same EWRAM offsets as trace_golden_sun_field_map / the Palace fingerprint)
// so this adds no new identity scheme.
void trace_golden_sun_wide_scroll_row(std::uint16_t dispcnt,
                                      const std::uint8_t* io,
                                      GoldenSunWidePolicyReason reason,
                                      GoldenSunWidePolicyReason split_reason,
                                      unsigned flags) {
    if (!golden_sun_wide_diagnostics_enabled() || !io) return;
    const std::uint64_t frame = runtime_current_frame();
    if (frame == g_golden_sun_wide_scroll_trace_last_frame) return;
    g_golden_sun_wide_scroll_trace_last_frame = frame;
    std::uint32_t map_crc = 0;
    std::uint32_t raw_crc = 0;
    if (const gba::GbaBus* bus = gbarecomp::active_bus()) {
        const std::uint8_t* ewram = bus->ewram_ptr();
        map_crc = golden_sun_field_table_crc(
            ewram, kGoldenSunFieldMapOffset, kGoldenSunFieldMapBytes);
        raw_crc = golden_sun_field_table_crc(
            ewram, kGoldenSunFieldRawOffset, kGoldenSunFieldRawBytes);
    }
    GoldenSunWideScrollTraceRow row{};
    row.frame = frame;
    row.dispcnt = dispcnt;
    row.bg0cnt = gsr::widescreen::read_io16(io, 0x08u);
    row.bg1cnt = gsr::widescreen::read_io16(io, 0x0Au);
    row.bg2cnt = gsr::widescreen::read_io16(io, 0x0Cu);
    row.bg3cnt = gsr::widescreen::read_io16(io, 0x0Eu);
    row.bg0_hofs = gsr::widescreen::read_io16(io, 0x10u);
    row.bg0_vofs = gsr::widescreen::read_io16(io, 0x12u);
    row.bg1_hofs = gsr::widescreen::read_io16(io, 0x14u);
    row.bg1_vofs = gsr::widescreen::read_io16(io, 0x16u);
    row.bg2_hofs = gsr::widescreen::read_io16(io, 0x18u);
    row.bg2_vofs = gsr::widescreen::read_io16(io, 0x1Au);
    row.bg3_hofs = gsr::widescreen::read_io16(io, 0x1Cu);
    row.bg3_vofs = gsr::widescreen::read_io16(io, 0x1Eu);
    row.margin_reason = static_cast<std::uint8_t>(reason);
    row.split_reason = static_cast<std::uint8_t>(split_reason);
    row.flags = static_cast<std::uint8_t>(flags);
    row.map_crc = map_crc;
    row.raw_crc = raw_crc;
    row.auth_epoch = g_golden_sun_field_auth_epoch;
    g_golden_sun_wide_scroll_trace[
        g_golden_sun_wide_scroll_trace_count % kGoldenSunWideScrollTraceCap] =
        row;
    ++g_golden_sun_wide_scroll_trace_count;
}

// Bounded ring flush, same discipline as hang_trace.csv in
// gbarecomp/src/runtime/runtime_bus_bridge.cpp: metadata-only rows written
// once at exit from the always-collected in-memory ring.
void write_golden_sun_wide_scroll_trace_csv() {
    if (!golden_sun_wide_diagnostics_enabled() ||
        g_golden_sun_wide_scroll_trace_count == 0u) return;
    FILE* f = std::fopen("wide_scroll_trace.csv", "w");
    if (!f) return;
    std::fprintf(f,
        "frame,dispcnt,bg0cnt,bg1cnt,bg2cnt,bg3cnt,"
        "bg0_hofs,bg0_vofs,bg1_hofs,bg1_vofs,bg2_hofs,bg2_vofs,"
        "bg3_hofs,bg3_vofs,margin_reason,split_reason,flags,"
        "map_crc,raw_crc,auth_epoch\n");
    const std::uint64_t count = std::min<std::uint64_t>(
        g_golden_sun_wide_scroll_trace_count, kGoldenSunWideScrollTraceCap);
    const std::uint64_t start = g_golden_sun_wide_scroll_trace_count > count
        ? g_golden_sun_wide_scroll_trace_count % kGoldenSunWideScrollTraceCap
        : 0u;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto& row = g_golden_sun_wide_scroll_trace[
            (start + i) % kGoldenSunWideScrollTraceCap];
        std::fprintf(f,
            "%llu,0x%04x,0x%04x,0x%04x,0x%04x,0x%04x,"
            "0x%04x,0x%04x,0x%04x,0x%04x,0x%04x,0x%04x,0x%04x,0x%04x,"
            "%s,%s,0x%x,0x%08x,0x%08x,%llu\n",
            static_cast<unsigned long long>(row.frame), row.dispcnt,
            row.bg0cnt, row.bg1cnt, row.bg2cnt, row.bg3cnt,
            row.bg0_hofs, row.bg0_vofs, row.bg1_hofs, row.bg1_vofs,
            row.bg2_hofs, row.bg2_vofs, row.bg3_hofs, row.bg3_vofs,
            golden_sun_wide_policy_reason_name(
                static_cast<GoldenSunWidePolicyReason>(row.margin_reason)),
            golden_sun_wide_policy_reason_name(
                static_cast<GoldenSunWidePolicyReason>(row.split_reason)),
            row.flags, row.map_crc, row.raw_crc,
            static_cast<unsigned long long>(row.auth_epoch));
    }
    std::fclose(f);
    std::fprintf(stderr,
        "[wide-scroll-trace] wrote wide_scroll_trace.csv (%llu of %llu "
        "frame rows, ring cap %zu)\n",
        static_cast<unsigned long long>(count),
        static_cast<unsigned long long>(g_golden_sun_wide_scroll_trace_count),
        kGoldenSunWideScrollTraceCap);
}

void maybe_report_golden_sun_cull_trace();

unsigned golden_sun_wide_margin_policy_callback(
    std::uint16_t dispcnt, const std::uint8_t* io) {
    maybe_report_golden_sun_cull_trace();
    const GoldenSunWidePolicyReason reason =
        gsr::widescreen::golden_sun_wide_margin_policy_reason(dispcnt, io);
    const GoldenSunWidePolicyReason split_reason =
        gsr::widescreen::golden_sun_mode0_split_scroll_policy_reason(
            dispcnt, io);
    const unsigned generic_flags =
        gsr::widescreen::golden_sun_wide_margin_policy(
        dispcnt, io);
    const bool split_row = split_reason ==
        GoldenSunWidePolicyReason::AuthorizedMode0SplitScroll;
    // The runtime calls this policy once for each authentic visible raster
    // line. Signed top/bottom rows are synthesized later from the latched
    // edge state and must not make a clean guest frame impossible to complete.
    const std::uint32_t expected_rows = gsr::widescreen::kNativeHeight;
    const bool split_frame_authorized =
        g_golden_sun_mode0_split_scroll_frame.observe(
            runtime_current_frame(), split_row, expected_rows);
    const unsigned flags = split_frame_authorized ? 0u : generic_flags;
    trace_golden_sun_wide_scene(dispcnt, io, flags);
    if (io) {
        std::memcpy(g_golden_sun_wide_line_io.data(), io,
                    g_golden_sun_wide_line_io.size());
        g_golden_sun_wide_line_io_valid = true;
        g_golden_sun_wide_line_dispcnt = dispcnt;
    } else {
        g_golden_sun_wide_line_io_valid = false;
        g_golden_sun_wide_line_dispcnt = 0;
    }
    // The BG X-provider has no IO arguments. Publish this per-scanline scene
    // bit immediately before the PPU invokes it; invalid/transition frames
    // therefore cannot inherit Mode 0 cutoff behavior.
    // Equal-scroll field/object authorization remains separate from the
    // complete-frame Palace split-scroll authorization. In particular, a
    // split-scroll frame must not inherit the equal-scroll object's culls.
    g_golden_sun_mode0_field =
        reason == GoldenSunWidePolicyReason::AuthorizedMode0;
    g_golden_sun_mode0_split_scroll = split_frame_authorized;
    if (!split_frame_authorized) {
        const bool had_palace_cache =
            g_golden_sun_palace_table_authorized ||
            g_golden_sun_palace_region_scroll_valid ||
            g_golden_sun_palace_region_build_attempted;
        g_golden_sun_palace_table_authorized = false;
        if (had_palace_cache) reset_golden_sun_palace_active_region();
    } else {
        refresh_golden_sun_palace_table_authorization();
    }
    // The metatile table's "authored" bitmap is keyed to this exact scene
    // signal. Clear it on every field/split/non-field transition so restored
    // contents cannot authorize a new scene.
    const GoldenSunFieldAuthScene auth_scene =
        g_golden_sun_mode0_field
            ? GoldenSunFieldAuthScene::EqualScroll
            : split_row ? GoldenSunFieldAuthScene::SplitScroll
                        : GoldenSunFieldAuthScene::None;
    if (auth_scene != g_golden_sun_field_auth_scene) {
        g_golden_sun_field_authored.reset();
        begin_golden_sun_field_auth_epoch();
        g_golden_sun_field_auth_scene = auth_scene;
    }
    // Func_b168 is measured in both the equal-scroll field and the two stable
    // Palace intervals. Keep object authorization narrower than generic Mode 0
    // by sharing only these authenticated map classes.
    g_golden_sun_expanded_obj_scene = g_golden_sun_mode0_field ||
        g_golden_sun_mode0_split_scroll;
    const GoldenSunWidePolicyReason effective_reason = split_frame_authorized
        ? split_reason : reason;
    trace_golden_sun_wide_policy(dispcnt, io, flags, effective_reason);
    trace_golden_sun_wide_scroll_row(dispcnt, io, reason, split_reason, flags);
    return flags;
}

int golden_sun_wide_tilemap_provider(int bg, int hw_x, int screen_y,
                                     std::uint16_t* out_entry) {
    const gba::GbaBus* bus = gbarecomp::active_bus();
    const bool equal_scroll_field = g_golden_sun_mode0_field;
    const bool split_scroll_palace = g_golden_sun_mode0_split_scroll &&
        g_golden_sun_palace_table_authorized;

    GoldenSunFieldProviderTrace* trace = nullptr;
    if (golden_sun_wide_diagnostics_enabled() && bg >= 0 && bg < 4) {
        trace = &g_golden_sun_field_provider_trace[
            static_cast<std::size_t>(bg)];
        ++trace->calls;
        if (equal_scroll_field) ++trace->equal_scroll_calls;
        else if (split_scroll_palace) ++trace->split_scroll_calls;
        trace->min_x = std::min(trace->min_x,
                                static_cast<std::int32_t>(hw_x));
        trace->max_x = std::max(trace->max_x,
                                static_cast<std::int32_t>(hw_x));
        trace->min_y = std::min(trace->min_y,
                                static_cast<std::int32_t>(screen_y));
        trace->max_y = std::max(trace->max_y,
                                static_cast<std::int32_t>(screen_y));
    }
    const auto reject = [&](bool precondition, bool boundary, bool raw,
                            bool lookup_miss) {
        if (trace) {
            ++trace->unavailable;
            if (precondition) ++trace->precondition_rejects;
            if (boundary) ++trace->boundary_rejects;
            if (raw) ++trace->raw_unavailable;
            if (lookup_miss) ++trace->lookup_misses;
        }
        return gba::kWsTilemapUnavailable;
    };
    const auto replace = [&] {
        if (trace) ++trace->replacements;
        return gba::kWsTilemapReplace;
    };
    if (!g_ws_active || (!equal_scroll_field && !split_scroll_palace) ||
        !g_golden_sun_wide_line_io_valid) {
        return reject(true, false, false, false);
    }
    // This is a presentation-time read of the already-active EWRAM image.
    // It does not use bus_read_* and therefore cannot mutate guest timing or
    // device state while the PPU is compositing the scanline.
    if (!bus) {
        return reject(true, false, false, false);
    }
    trace_golden_sun_field_map(bus->ewram_ptr());
    if (split_scroll_palace) {
        // The active-region cache is constructed only after the exact Palace
        // table fingerprint has authorized this scene.  A missing seed or a
        // stale cache therefore fails closed for every Palace layer.
        prepare_golden_sun_palace_active_region(
            bus, g_golden_sun_wide_line_io.data());
    }

    if (equal_scroll_field) {
        // BG3 remains the cross-layer boundary oracle for equal-scroll field
        // maps. Authored ownership is intentionally not required here because
        // restored tables may have no post-restore bitmap bits.
        std::uint16_t boundary_entry = 0;
        gsr::widescreen::GoldenSunFieldTilemapMetadata boundary_metadata;
        const bool boundary_resolved =
            gsr::widescreen::golden_sun_field_tilemap_entry(
                g_golden_sun_wide_line_dispcnt,
                g_golden_sun_wide_line_io.data(),
                g_golden_sun_wide_line_io.size(), bus->ewram_ptr(),
                256u * 1024u, 3, hw_x, screen_y, &boundary_entry,
                &boundary_metadata, nullptr);
        if (gsr::widescreen::golden_sun_field_atlas_unavailable(
                boundary_metadata)) {
            return reject(false, true, true, false);
        }

        if (bg == 3) {
            if (boundary_resolved) *out_entry = boundary_entry;
            if (boundary_resolved)
                record_golden_sun_field_map_id_attribution(
                    bg, hw_x, screen_y, boundary_metadata);
            return boundary_resolved ? replace()
                                     : reject(false, false, false, true);
        }
    }

    gsr::widescreen::GoldenSunFieldTilemapMetadata metadata;
    const bool resolved = gsr::widescreen::golden_sun_field_tilemap_entry(
                g_golden_sun_wide_line_dispcnt,
                g_golden_sun_wide_line_io.data(),
                g_golden_sun_wide_line_io.size(), bus->ewram_ptr(),
                256u * 1024u, bg, hw_x, screen_y, out_entry, &metadata,
                nullptr, false, split_scroll_palace);
    const auto record_palace = [&](GoldenSunPalaceMarginOutcome outcome) {
        if (split_scroll_palace)
            g_golden_sun_palace_margin_diagnostics.record(
                bg, hw_x, screen_y, outcome, metadata,
                resolved || metadata.has_raw_entry);
    };
    // Equal-scroll BG3 is the cross-layer boundary oracle above. Palace's
    // split-scroll class deliberately does not use BG3 for BG1/BG2; every
    // layer still rejects its own measured unavailable source and bad bounds.
    if (gsr::widescreen::golden_sun_field_atlas_unavailable(metadata)) {
        if (split_scroll_palace)
            record_palace(metadata.map_id == gsr::widescreen::kGoldenSunPalaceNoMapId
                              ? GoldenSunPalaceMarginOutcome::RejectedFill
                              : GoldenSunPalaceMarginOutcome::RejectedRaw);
        return reject(false, false, true, false);
    }
    if (split_scroll_palace &&
        !g_golden_sun_palace_active_region.reachable(
            bg, metadata.map_x, metadata.map_y)) {
        record_palace(metadata.map_id == gsr::widescreen::kGoldenSunPalaceNoMapId
                          ? GoldenSunPalaceMarginOutcome::RejectedFill
                          : GoldenSunPalaceMarginOutcome::RejectedMask);
        if (trace) ++trace->palace_region_rejects;
        return reject(false, true, false, false);
    }
    if (split_scroll_palace && resolved) {
        record_palace(g_golden_sun_palace_active_region.seeded(
                          bg, metadata.map_x, metadata.map_y)
                          ? GoldenSunPalaceMarginOutcome::AcceptedNativeSeed
                          : GoldenSunPalaceMarginOutcome::AcceptedConnected);
    } else if (split_scroll_palace) {
        record_palace(GoldenSunPalaceMarginOutcome::RejectedLookup);
    }
    if (resolved)
        record_golden_sun_field_map_id_attribution(
            bg, hw_x, screen_y, metadata);
    return resolved ? replace() : reject(false, false, false, true);
}

int golden_sun_wide_bg_x_provider(
    int bg, int output_x,
    int screen_y,  // Arrives in hardware space from the PPU: native rows are
                   // 0..159, margins are negative (top) or >=160 (bottom).
    int*) {
    // This provider is only for the expanded output. The runtime leaves
    // game-owned hooks installed across a live toggle, so avoid touching the
    // native/supersampled path while the view is back at 240 pixels.
    if (!g_ws_active) return 0;
    // golden_sun_suppress_bg0_margin expects canvas-space Y (0..extra_top +
    // 160 + extra_bottom, native window at [extra_top, extra_top+160)) to
    // match output_x's canvas space, so shift hardware-space screen_y here.
    const int canvas_y =
        screen_y + static_cast<int>(g_golden_sun_wide_extra_top);
    if (gsr::widescreen::golden_sun_suppress_bg0_margin(
            bg, output_x, canvas_y, g_golden_sun_wide_extra_left,
            g_golden_sun_wide_extra_right, g_golden_sun_wide_extra_top,
            g_golden_sun_wide_extra_bottom)) {
        return -1;
    }
    return 0;
}

int golden_sun_wide_obj_x_provider(int raw_x, int* out_x) {
    // The GBA OBJ X field is nine bits and normally decodes 0x100..0x1FF as
    // -256..-1.  Golden Sun's town field is already scene-authenticated by
    // the margin policy; reinterpret only the raw values needed to cover the
    // widened right edge.  Values outside that exact 24px envelope retain the
    // hardware signed decode, and non-field scenes never opt into this hook.
    if (!g_ws_active || !out_x ||
        !gsr::widescreen::golden_sun_field_obj_x_authorized(
            g_golden_sun_expanded_obj_scene, raw_x,
            g_golden_sun_wide_extra_right)) {
        return 0;
    }
    *out_x = raw_x;
    return 1;
}

void clear_golden_sun_obj_y_provenance() {
    g_golden_sun_obj_pending_provenance.fill({});
    g_golden_sun_obj_visible_provenance.fill({});
    g_golden_sun_obj_staging_provenance.fill({});
    g_golden_sun_obj_f0_contexts.fill({});
    g_golden_sun_obj_f0_context_frame = UINT64_MAX;
    g_golden_sun_obj_y_edge_alias_state.fill({});
    g_golden_sun_experimental_cull_last.fill({});
    g_golden_sun_experimental_cull_logs = 0;
    g_golden_sun_experimental_cull_logs_dropped = 0;
}

GoldenSunObjStagingProvenance* find_golden_sun_obj_staging(
    std::uint32_t staging_address) {
    for (auto& candidate : g_golden_sun_obj_staging_provenance) {
        if (candidate.valid && candidate.staging_address == staging_address)
            return &candidate;
    }
    return nullptr;
}

GoldenSunObjStagingProvenance* allocate_golden_sun_obj_staging(
    std::uint32_t staging_address) {
    if (auto* existing = find_golden_sun_obj_staging(staging_address))
        return existing;
    for (auto& candidate : g_golden_sun_obj_staging_provenance) {
        if (!candidate.valid) {
            candidate.staging_address = staging_address;
            return &candidate;
        }
    }
    // The guest normally uses only a small prefix of this bounded table. If
    // it ever fills, recycle the oldest entry rather than aliasing a record.
    auto* oldest = &g_golden_sun_obj_staging_provenance.front();
    for (auto& candidate : g_golden_sun_obj_staging_provenance) {
        if (candidate.frame < oldest->frame) oldest = &candidate;
    }
    *oldest = {};
    oldest->staging_address = staging_address;
    return oldest;
}

void record_golden_sun_obj_staging(std::uint32_t instruction_pc,
                                   bool x_axis, int logical_coordinate) {
    if (!golden_sun_expanded_obj_view_active()) return;
    auto* staging = allocate_golden_sun_obj_staging(g_cpu.R[7]);
    if (!staging) return;
    staging->valid = true;
    staging->frame = runtime_current_frame();
    staging->auth_epoch = g_golden_sun_field_auth_epoch;
    if (x_axis) {
        staging->x_valid = true;
        staging->logical_x = static_cast<std::int16_t>(logical_coordinate);
        staging->x_writer_branch_pc = instruction_pc;
    } else {
        staging->y_valid = true;
        staging->logical_y = static_cast<std::int16_t>(logical_coordinate);
        staging->y_writer_branch_pc = instruction_pc;
        if (instruction_pc == 0x0800B328u && logical_coordinate >= 160 &&
            logical_coordinate <= 199) {
            staging->y_correlation = capture_golden_sun_obj_y_correlation(
                logical_coordinate);
        } else {
            staging->y_correlation = {};
        }
    }
    if (golden_sun_wide_diagnostics_enabled() &&
        g_golden_sun_obj_y_provenance_logs_in_epoch <
            kGoldenSunObjYProvenanceLogLimitPerEpoch) {
        ++g_golden_sun_obj_y_provenance_logs_in_epoch;
        std::fprintf(stderr,
                     "[wide-obj-stage] frame=%llu auth_epoch=%llu "
                     "branch_pc=0x%08x axis=%c staging=0x%08x logical=%d\n",
                     static_cast<unsigned long long>(staging->frame),
                     static_cast<unsigned long long>(staging->auth_epoch),
                     instruction_pc, x_axis ? 'x' : 'y',
                     staging->staging_address, logical_coordinate);
    }
}

void clear_golden_sun_obj_staging(std::uint32_t staging_address) {
    if (auto* staging = find_golden_sun_obj_staging(staging_address))
        *staging = {};
}

bool read_golden_sun_obj_staging_attrs(std::uint32_t address,
                                       std::uint16_t* attr0,
                                       std::uint16_t* attr1,
                                       std::uint16_t* attr2) {
    constexpr std::uint32_t kIwramStart = 0x03000000u;
    constexpr std::uint32_t kIwramEnd = 0x03008000u;
    constexpr std::uint32_t kRecordBytes = 12u;
    if (!attr0 || !attr1 || !attr2 || (address & 3u) != 0u ||
        address < kIwramStart || address > kIwramEnd - kRecordBytes)
        return false;
    const std::uint32_t record_word1 = bus_read_u32(address + 4u);
    const std::uint32_t record_word2 = bus_read_u32(address + 8u);
    *attr0 = static_cast<std::uint16_t>(record_word1 & 0xFFFFu);
    *attr1 = static_cast<std::uint16_t>(record_word1 >> 16);
    *attr2 = static_cast<std::uint16_t>(record_word2 & 0xFFFFu);
    return true;
}

void golden_sun_obj_staging_handoff(std::uint32_t entry_pc) {
    // D4 loads {R6,R7,R8} from the staging record in R6, then its following
    // store writes R7/R8 to the OAM-shadow destination in R0.
    if (!golden_sun_func1dc8_writer_pc(
            entry_pc, gsr::Func1dc8WriterRoute::D4) ||
        !golden_sun_expanded_obj_view_active())
        return;
    const std::uint32_t staging_address = g_cpu.R[6];
    const int slot = gsr::widescreen::golden_sun_oam_shadow_slot(g_cpu.R[0]);
    std::uint16_t observed_attr0 = 0;
    std::uint16_t observed_attr1 = 0;
    std::uint16_t observed_attr2 = 0;
    const bool observed_attrs_valid = read_golden_sun_obj_staging_attrs(
        staging_address, &observed_attr0, &observed_attr1, &observed_attr2);
    record_golden_sun_b328_rejected_writer(
        GoldenSunObjB328WriterRoute::D4, staging_address, slot,
        observed_attr0, observed_attr1, observed_attr2,
        runtime_current_frame(), runtime_call_stack_depth(),
        golden_sun_obj_call_return_pc(runtime_call_stack_depth()), 0u);
    auto* staging = find_golden_sun_obj_staging(staging_address);
    if (!staging || !staging->valid || staging->auth_epoch !=
        g_golden_sun_field_auth_epoch ||
        staging->frame != runtime_current_frame()) return;
    if (slot < 0 || static_cast<std::size_t>(slot) >=
                         g_golden_sun_obj_pending_provenance.size()) return;
    auto& output = g_golden_sun_obj_pending_provenance[
        static_cast<std::size_t>(slot)];

    // The authenticated D4 entry executes `ldmia r6!, {r6,r7,r8}` before its
    // following store
    // R7/R8 as the two OAM words. Read that exact 12-byte IWRAM record at the
    // entry seam, with bounds/alignment checks, so the provenance cannot be
    // reused for a later slot occupant. Word 0 is the record's working value;
    // words 1/2 are ATTR0|ATTR1 and ATTR2|padding respectively.
    std::uint16_t expected_attr0 = observed_attr0;
    std::uint16_t expected_attr1 = observed_attr1;
    std::uint16_t expected_attr2 = observed_attr2;
    const bool oam_identity_valid = observed_attrs_valid;
    output.valid = staging->x_valid || staging->y_valid;
    output.x_valid = staging->x_valid;
    output.y_valid = staging->y_valid;
    output.logical_x = staging->logical_x;
    output.logical_y = staging->logical_y;
    output.frame = staging->frame;
    output.auth_epoch = staging->auth_epoch;
    output.writer_branch_pc = staging->y_writer_branch_pc;
    output.target_address = g_cpu.R[0];
    output.writer_generation = 0;
    output.oam_identity_valid = oam_identity_valid;
    output.expected_attr0 = expected_attr0;
    output.expected_attr1 = expected_attr1;
    output.expected_attr2 = expected_attr2;
    record_golden_sun_b328_handoff(
        staging_address, staging->frame, oam_identity_valid);
    note_golden_sun_obj_commit_order(
        GoldenSunObjCommitOrderRoute::D4, staging_address, slot,
        expected_attr0, expected_attr1, expected_attr2, staging->x_valid,
        staging->logical_x, staging->y_valid, staging->logical_y);
    if (oam_identity_valid) {
        trace_golden_sun_obj_y_correlation(
            staging->y_correlation, staging_address, slot, expected_attr0,
            expected_attr1, expected_attr2);
    }
    if (golden_sun_wide_diagnostics_enabled() &&
        g_golden_sun_obj_y_provenance_logs_in_epoch <
            kGoldenSunObjYProvenanceLogLimitPerEpoch) {
        ++g_golden_sun_obj_y_provenance_logs_in_epoch;
        std::fprintf(stderr,
                     "[wide-obj-handoff] frame=%llu auth_epoch=%llu "
                     "staging=0x%08x destination=0x%08x slot=%d "
                     "x_valid=%u logical_x=%d y_valid=%u logical_y=%d\n",
                     static_cast<unsigned long long>(runtime_current_frame()),
                     static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                     staging_address, g_cpu.R[0], slot,
                     staging->x_valid ? 1u : 0u,
                     static_cast<int>(staging->logical_x),
                     staging->y_valid ? 1u : 0u,
                     static_cast<int>(staging->logical_y));
    }
}

void trace_golden_sun_obj_y_attempt(std::uint32_t instruction_pc,
                                    const char* reason,
                                    std::uint32_t candidate_y,
                                    std::uint32_t address,
                                    bool address_valid, int slot) {
    if (!golden_sun_wide_diagnostics_enabled() ||
        g_golden_sun_obj_y_provenance_logs_in_epoch >=
            kGoldenSunObjYProvenanceLogLimitPerEpoch) return;
    ++g_golden_sun_obj_y_provenance_logs_in_epoch;
    std::fprintf(stderr,
                 "[wide-obj-y] frame=%llu auth_epoch=%llu reason=%s "
                 "branch_pc=0x%08x r7=0x%08x address=0x%08x "
                 "address_valid=%u slot=%d candidate_y=%u scene=%u "
                 "geometry=active:%u,top:%u,bottom:%u\n",
                 static_cast<unsigned long long>(runtime_current_frame()),
                 static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
                 reason, instruction_pc, g_cpu.R[7], address,
                 address_valid ? 1u : 0u, slot, candidate_y,
                 g_golden_sun_expanded_obj_scene ? 1u : 0u,
                 g_ws_active ? 1u : 0u, g_golden_sun_wide_extra_top,
                 g_golden_sun_wide_extra_bottom);
}

int golden_sun_obj_oam_truncated(int logical, int bits) {
    const int modulus = 1 << bits;
    int result = logical % modulus;
    if (result < 0) result += modulus;
    return result;
}

void record_golden_sun_obj_y_transition(
    int slot, int raw_y, int canonical_y, int output_y,
    GoldenSunObjYTransitionResolution resolution,
    GoldenSunObjYTransitionReason reason,
    const GoldenSunObjPlacementProvenance& provenance,
    std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    if (!golden_sun_wide_diagnostics_enabled() || raw_y < 159 ||
        raw_y > 199 || slot < 0 || static_cast<std::size_t>(slot) >=
            gsr::widescreen::kGoldenSunOamShadowSlotCount) return;
    const auto region = golden_sun_obj_y_transition_region(output_y);
    auto& count = g_golden_sun_obj_y_transition_counts[
        static_cast<std::size_t>(reason)][static_cast<std::size_t>(region)]
        [static_cast<std::size_t>(resolution)];
    ++count;

    // ATTR0=0 and the hardware disabled bit identify empty/disabled OAM;
    // 0x00C0 is the measured dormant entry. Do not spend reserved evidence
    // budget on those slots, while retaining the uncapped aggregate above.
    const bool affine = (attr0 & 0x0100u) != 0u;
    const bool disabled = !affine && (attr0 & 0x0200u) != 0u;
    if ((attr0 == 0u && attr1 == 0u && attr2 == 0u) ||
        attr0 == 0x00C0u || disabled)
        return;

    const std::uint32_t expected_target =
        gsr::widescreen::kGoldenSunOamShadowStart +
        static_cast<std::uint32_t>(slot) *
            gsr::widescreen::kGoldenSunOamShadowSlotBytes;
    GoldenSunObjYTransitionSample sample{};
    sample.valid = true;
    sample.frame = runtime_current_frame();
    sample.slot = slot;
    sample.raw_y = raw_y;
    sample.canonical_y = canonical_y;
    sample.output_y = output_y;
    sample.resolution = resolution;
    sample.reason = reason;
    sample.region = region;
    sample.attr0 = attr0;
    sample.attr1 = attr1;
    sample.attr2 = attr2;
    sample.expected_attr0 = provenance.expected_attr0;
    sample.expected_attr1 = provenance.expected_attr1;
    sample.expected_attr2 = provenance.expected_attr2;
    sample.expected_target = expected_target;
    sample.provenance_frame = provenance.frame;
    sample.provenance_epoch = provenance.auth_epoch;
    sample.target_address = provenance.target_address;
    sample.writer_pc = provenance.writer_branch_pc;
    sample.writer_generation = provenance.writer_generation;

    const GoldenSunObjYTransitionIdentity identity{
        true, slot, expected_target, attr0, attr1, attr2,
        provenance.expected_attr0, provenance.expected_attr1,
        provenance.expected_attr2};
    const auto identity_matches = [](const GoldenSunObjYTransitionIdentity& a,
                                     const GoldenSunObjYTransitionIdentity& b) {
        return a.valid && b.valid && a.slot == b.slot &&
            a.target == b.target && a.attr0 == b.attr0 &&
            a.attr1 == b.attr1 && a.attr2 == b.attr2 &&
            a.expected_attr0 == b.expected_attr0 &&
            a.expected_attr1 == b.expected_attr1 &&
            a.expected_attr2 == b.expected_attr2;
    };
    const auto append_sample = [&](GoldenSunObjYTransitionBucket bucket) {
        const auto bucket_index = static_cast<std::size_t>(bucket);
        auto& sample_count = g_golden_sun_obj_y_transition_sample_counts[
            bucket_index];
        if (sample_count >= kGoldenSunObjYTransitionSampleLimit) {
            ++g_golden_sun_obj_y_transition_samples_dropped[bucket_index];
            return;
        }
        g_golden_sun_obj_y_transition_samples[bucket_index][sample_count++] =
            sample;
    };

    // Buckets A and B are first-sample-per stable hardware identity. Their
    // independent caps ensure top-wrap candidates cannot starve signed-bottom
    // evidence.
    const auto append_first_identity = [&](GoldenSunObjYTransitionBucket bucket) {
        const auto seen_index = bucket ==
            GoldenSunObjYTransitionBucket::ActiveCandidateCanonical ? 0u : 1u;
        for (unsigned i = 0; i < g_golden_sun_obj_y_transition_seen_counts[
                 seen_index]; ++i) {
            if (identity_matches(g_golden_sun_obj_y_transition_seen[seen_index][i],
                                 identity)) return;
        }
        auto& seen_count = g_golden_sun_obj_y_transition_seen_counts[seen_index];
        if (seen_count < kGoldenSunObjYTransitionSampleLimit)
            g_golden_sun_obj_y_transition_seen[seen_index][seen_count++] = identity;
        append_sample(bucket);
    };

    if (resolution == GoldenSunObjYTransitionResolution::Canonical) {
        append_first_identity(
            GoldenSunObjYTransitionBucket::ActiveCandidateCanonical);
    } else if (resolution == GoldenSunObjYTransitionResolution::Signed &&
               region == GoldenSunObjYTransitionRegion::Bottom) {
        append_first_identity(GoldenSunObjYTransitionBucket::SignedBottom);
    }

    // Bucket C records changes between canonical fallback and signed-bottom
    // outcomes for the same slot/target. It intentionally does not define a
    // semantic NPC identity beyond measured hardware fields.
    if (resolution == GoldenSunObjYTransitionResolution::Canonical ||
        (resolution == GoldenSunObjYTransitionResolution::Signed &&
         region == GoldenSunObjYTransitionRegion::Bottom)) {
        bool previous_found = false;
        GoldenSunObjYTransitionOutcomeState* state = nullptr;
        for (auto& candidate : g_golden_sun_obj_y_transition_outcome_states) {
            if (candidate.valid && candidate.slot == slot &&
                candidate.target == expected_target) {
                state = &candidate;
                previous_found = true;
                break;
            }
        }
        if (!state) {
            for (auto& candidate : g_golden_sun_obj_y_transition_outcome_states) {
                if (!candidate.valid) {
                    candidate.valid = true;
                    candidate.slot = slot;
                    candidate.target = expected_target;
                    state = &candidate;
                    break;
                }
            }
        }
        if (state) {
            if (previous_found && state->resolution != resolution)
                append_sample(GoldenSunObjYTransitionBucket::OutcomeTransition);
            state->resolution = resolution;
        }
    }
}

bool golden_sun_expanded_obj_view_active() {
    return g_ws_active && gsr::widescreen::golden_sun_expanded_view_active(
        gsr::widescreen::kExpandedWidth, gsr::widescreen::kExpandedHeight,
        g_golden_sun_wide_extra_left, g_golden_sun_wide_extra_right,
        g_golden_sun_wide_extra_top, g_golden_sun_wide_extra_bottom);
}

int golden_sun_wide_obj_attr_x_provider(int oam_index,
                                        std::uint16_t attr0,
                                        std::uint16_t attr1,
                                        std::uint16_t attr2,
                                        int* out_x) {
    (void)attr0;
    if (!golden_sun_expanded_obj_view_active() || !out_x || oam_index < 0 ||
        static_cast<std::size_t>(oam_index) >=
            g_golden_sun_obj_visible_provenance.size()) return 0;
    const auto& provenance = g_golden_sun_obj_visible_provenance[
        static_cast<std::size_t>(oam_index)];
    const int raw_x = static_cast<int>(attr1 & 0x01FFu);
    if (!provenance.valid || !provenance.x_valid ||
        provenance.auth_epoch != g_golden_sun_field_auth_epoch ||
        !gsr::widescreen::golden_sun_obj_provenance_attrs_match(
            provenance.oam_identity_valid, provenance.expected_attr0,
            provenance.expected_attr1, provenance.expected_attr2,
            attr0, attr1, attr2) ||
        golden_sun_obj_oam_truncated(provenance.logical_x, 9) != raw_x) return 0;
    *out_x = provenance.logical_x;
    return 1;
}

int golden_sun_wide_obj_attr_y_provider(int oam_index,
                                        std::uint16_t attr0,
                                        std::uint16_t attr1,
                                        std::uint16_t attr2,
                                        int* out_y) {
    const std::uint32_t diag_pc = 0u;
    const int raw_y = static_cast<int>(attr0 & 0x00FFu);
    if (!golden_sun_expanded_obj_view_active()) {
        g_golden_sun_obj_y_edge_alias_state = {};
        trace_golden_sun_obj_y_attempt(diag_pc, "scene-disabled",
                                       attr0 & 0x00FFu, 0u, false, oam_index);
        return 0;
    }
    const auto record_obj = [&](GoldenSunObjYOutcome outcome) {
        record_golden_sun_obj_y_outcome(oam_index, raw_y, outcome);
    };
    if (!out_y) {
        record_obj(GoldenSunObjYOutcome::OutputUnavailable);
        trace_golden_sun_obj_y_attempt(diag_pc, "output-unavailable",
                                       attr0 & 0x00FFu, 0u, false, oam_index);
        return 0;
    }
    if (oam_index < 0 || static_cast<std::size_t>(oam_index) >=
                              g_golden_sun_obj_visible_provenance.size()) {
        trace_golden_sun_obj_y_attempt(diag_pc, "slot-rejected",
                                       attr0 & 0x00FFu, 0u, false, oam_index);
        return 0;
    }
    auto& provenance = g_golden_sun_obj_visible_provenance[
        static_cast<std::size_t>(oam_index)];
    const bool alias_candidate =
        gsr::widescreen::golden_sun_obj_y_alias_candidate(
            raw_y, provenance.logical_y);
    if (alias_candidate) {
        gba::vram_trace::OamAttr0Provenance attr_provenance{};
        if (gba::vram_trace::get_oam_attr0_provenance(
            static_cast<std::size_t>(oam_index), &attr_provenance)) {
            provenance.writer_generation = attr_provenance.generation;
        }
    }
    const bool provenance_epoch_matches =
        provenance.auth_epoch == g_golden_sun_field_auth_epoch;
    const bool provenance_raw_matches =
        golden_sun_obj_oam_truncated(provenance.logical_y, 8) == raw_y;
    const bool provenance_identity_matches =
        gsr::widescreen::golden_sun_obj_provenance_attrs_match(
            provenance.oam_identity_valid, provenance.expected_attr0,
            provenance.expected_attr1, provenance.expected_attr2,
            attr0, attr1, attr2);
    const bool signed_provenance_matches =
        provenance.valid && provenance.y_valid && provenance_epoch_matches &&
        provenance_raw_matches && provenance_identity_matches;
    const std::uint32_t expected_target =
        gsr::widescreen::kGoldenSunOamShadowStart +
        static_cast<std::uint32_t>(oam_index) *
            gsr::widescreen::kGoldenSunOamShadowSlotBytes;
    const bool affine = (attr0 & 0x0100u) != 0u;
    const bool sprite_disabled = !affine && (attr0 & 0x0200u) != 0u;
    const bool sprite_empty = attr0 == 0u && attr1 == 0u && attr2 == 0u;
    const bool sprite_dormant = attr0 == 0x00C0u;
    if (oam_index >= 0 && static_cast<std::size_t>(oam_index) <
                              g_golden_sun_obj_y_edge_alias_state.size()) {
        const auto sample = gsr::widescreen::GoldenSunObjYEdgeAliasSample{
            true, !(sprite_disabled || sprite_empty || sprite_dormant),
            signed_provenance_matches, oam_index, expected_target,
            g_golden_sun_field_auth_epoch, runtime_current_frame(), raw_y,
            raw_y >= 160 ? raw_y - 256 : raw_y, attr0, attr1, attr2};
        auto& alias_state = g_golden_sun_obj_y_edge_alias_state[
            static_cast<std::size_t>(oam_index)];
        const auto alias_step =
            gsr::widescreen::golden_sun_obj_y_edge_alias_step(
                alias_state, sample);
        alias_state = alias_step.state;
        if (alias_step.activated) {
            *out_y = -97;
            trace_golden_sun_obj_y_edge_alias(
                oam_index, expected_target, runtime_current_frame(),
                g_golden_sun_field_auth_epoch, attr0, attr1, attr2);
            return 1;
        }
    }
    if (golden_sun_wide_diagnostics_enabled() && raw_y >= 159 &&
        raw_y <= 199) {
        const int canonical_y = raw_y >= 160 ? raw_y - 256 : raw_y;
        const auto resolution = signed_provenance_matches
            ? GoldenSunObjYTransitionResolution::Signed
            : GoldenSunObjYTransitionResolution::Canonical;
        const auto reason = classify_golden_sun_obj_y_transition(
            provenance, runtime_current_frame(), g_golden_sun_field_auth_epoch,
            gsr::widescreen::kGoldenSunOamShadowStart +
                static_cast<std::uint32_t>(oam_index) *
                    gsr::widescreen::kGoldenSunOamShadowSlotBytes,
            raw_y, attr0, attr1, attr2);
        record_golden_sun_obj_y_transition(
            oam_index, raw_y, canonical_y,
            signed_provenance_matches ? provenance.logical_y : canonical_y,
            resolution, reason, provenance, attr0, attr1, attr2);
    }
    switch (gsr::widescreen::golden_sun_obj_y_resolution(
        signed_provenance_matches)) {
    case gsr::widescreen::GoldenSunObjYResolution::SignedProvenance:
        *out_y = provenance.logical_y;
        record_obj(GoldenSunObjYOutcome::AcceptedProvenance);
        trace_golden_sun_obj_y_jump(
            oam_index, raw_y, provenance.logical_y, provenance,
            attr0, attr1, attr2);
        trace_golden_sun_obj_y_alias(
            provenance.logical_y < 0 ? "accepted-negative" :
                                      "accepted-positive",
            oam_index, raw_y, provenance.logical_y,
            provenance, attr0, attr1, attr2, false);
        return 1;
    case gsr::widescreen::GoldenSunObjYResolution::Canonical:
        record_obj(GoldenSunObjYOutcome::ProvenanceMissing);
        if (provenance.valid && provenance.y_valid &&
            gsr::widescreen::golden_sun_obj_y_alias_candidate(
                raw_y, provenance.logical_y)) {
            const char* reason = !provenance_epoch_matches
                ? "canonical-provenance-stale-epoch"
                : !provenance_raw_matches
                ? "canonical-provenance-y-mismatch"
                : !provenance_identity_matches
                ? "canonical-provenance-oam-mismatch"
                : "canonical-provenance-rejected";
            trace_golden_sun_obj_y_alias(
                reason, oam_index, raw_y, provenance.logical_y, provenance,
                attr0, attr1, attr2, true);
        }
        return 0;
    }
    // Keep compilers that do not treat enum switches as exhaustive happy.
    record_obj(GoldenSunObjYOutcome::ProvenanceMissing);
    return 0;
}

struct GoldenSunCullTrace {
    std::uint32_t pc;
    // `calls` is the raw routed-PC count. It is intentionally recorded before
    // the strict 360x240 policy guard so inactive modes remain visible.
    std::uint64_t calls = 0;
    std::uint64_t bypasses = 0;
    std::uint64_t inactive_rejects = 0;
    std::int32_t min_coord = INT32_MAX;
    std::int32_t max_coord = INT32_MIN;
};

std::array<GoldenSunCullTrace, 10> g_golden_sun_cull_trace{{
    {0x0800B27Eu}, {0x0800B324u}, {0x0800B328u},
    {0x0800B3D2u}, {0x0800B3DCu}, {0x0800B3E6u}, {0x0800B3ECu},
    {0x0800C6FAu}, {0x0800C702u}, {0x0800C708u},
}};

std::uint64_t g_golden_sun_cull_last_report_frame = UINT64_MAX;

void report_golden_sun_cull_trace(std::uint64_t frame,
                                  const char* reason) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    // Emit every reviewed branch, including zeroes. A zero is evidence about
    // the raw route rather than an artifact of the strict policy guard.
    for (const auto& stat : g_golden_sun_cull_trace) {
        const std::int32_t min_coord = stat.calls != 0u ? stat.min_coord : 0;
        const std::int32_t max_coord = stat.calls != 0u ? stat.max_coord : 0;
        std::fprintf(
            stderr,
            "[wide-branch] frame=%llu auth_epoch=%llu reason=%s "
            "pc=0x%08x calls=%llu bypasses=%llu inactive_rejects=%llu "
            "coord=%d..%d\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(g_golden_sun_field_auth_epoch),
            reason, stat.pc,
            static_cast<unsigned long long>(stat.calls),
            static_cast<unsigned long long>(stat.bypasses),
            static_cast<unsigned long long>(stat.inactive_rejects), min_coord,
            max_coord);
    }
}

void begin_golden_sun_field_auth_epoch() {
    if (golden_sun_wide_diagnostics_enabled()) {
        report_golden_sun_cull_trace(runtime_current_frame(),
                                     "auth-epoch-end");
        report_golden_sun_field_provider_trace(runtime_current_frame(),
                                               "auth-epoch-end");
        report_golden_sun_palace_margin_diagnostics(runtime_current_frame(),
                                                    "auth-epoch-end");
        report_golden_sun_obj_b328_diagnostics(runtime_current_frame(),
                                               "auth-epoch-end");
        report_golden_sun_obj_y_transition_diagnostics(
            runtime_current_frame(), "auth-epoch-end");
        report_golden_sun_obj_y_steady_state_diagnostics(runtime_current_frame(),
                                                         "auth-epoch-end");
        report_golden_sun_field_map_id_attribution(runtime_current_frame(),
                                                   "auth-epoch-end");
        report_golden_sun_field_producers("auth-epoch-end");
        report_golden_sun_obj_record_census("auth-epoch-end");
        report_golden_sun_obj_record_values("auth-epoch-end");
        report_golden_sun_obj_commit_order("auth-epoch-end");
    }
    clear_golden_sun_obj_y_provenance();
    g_golden_sun_obj_y_provenance_logs_in_epoch = 0;
    reset_golden_sun_obj_y_alias_diagnostics();
    reset_golden_sun_field_producers();
    reset_golden_sun_obj_record_census();
    reset_golden_sun_obj_record_value_events();
    reset_golden_sun_obj_commit_order();
    g_golden_sun_field_provider_trace = {};
    g_golden_sun_palace_margin_diagnostics.reset();
    g_golden_sun_obj_y_slot_diagnostics = {};
    g_golden_sun_field_map_id_attribution = {};
    g_golden_sun_field_map_id_attribution_overflow = {};
    for (auto& stat : g_golden_sun_cull_trace) {
        const std::uint32_t pc = stat.pc;
        stat = {};
        stat.pc = pc;
    }
    ++g_golden_sun_field_auth_epoch;
    g_golden_sun_palace_table_authorized = false;
    g_golden_sun_palace_table_invalidated = false;
    g_golden_sun_palace_table_auth_attempted = false;
    reset_golden_sun_palace_active_region();
    g_golden_sun_field_epoch_map_writes = 0;
    g_golden_sun_field_epoch_raw_writes = 0;
    g_golden_sun_field_epoch_map_dma_writes = 0;
    g_golden_sun_field_epoch_raw_dma_writes = 0;
    g_golden_sun_field_table_cpu_logs_in_epoch = 0;
    g_golden_sun_field_table_dma_logs_in_epoch = 0;
    const bool vram_rearmed = gba::vram_trace::rearm_bounded_window();
    // OAM ATTR0 provenance is window-local just like the VRAM writer trace;
    // do not let a prior authenticated scene contaminate the next handoff.
    gba::vram_trace::reset_oam_trace_window();
    if (golden_sun_wide_diagnostics_enabled()) {
        std::fprintf(stderr,
                     "[wide-auth-epoch] frame=%llu auth_epoch=%llu "
                     "vram_trace_rearmed=%u\n",
                     static_cast<unsigned long long>(runtime_current_frame()),
                     static_cast<unsigned long long>(
                         g_golden_sun_field_auth_epoch),
                     vram_rearmed ? 1u : 0u);
        // CRASH-03 provenance: re-CRC the pool-LDM stack window at every
        // auth-epoch boundary so a stale-since-load window is visible in
        // session logs without retaining guest bytes.
        runtime_note_pool_ldm_epoch_crc();
    }
}

void maybe_report_golden_sun_cull_trace() {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    const std::uint64_t frame = runtime_current_frame();
    // This callback runs once per scanline.  The object-scene bit can change
    // between scanlines as DISPCNT transitions, so using it as an immediate
    // report trigger floods stderr during map transitions and stalls the
    // game thread.  Cull evidence is diagnostic only; sample it periodically.
    if (g_golden_sun_cull_last_report_frame == UINT64_MAX) {
        g_golden_sun_cull_last_report_frame = frame;
        return;
    }
    if (frame < g_golden_sun_cull_last_report_frame + 120u) return;
    report_golden_sun_cull_trace(frame, "periodic");
    g_golden_sun_cull_last_report_frame = frame;
}

void report_golden_sun_cull_trace_at_exit() {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    report_golden_sun_cull_trace(runtime_current_frame(), "exit");
}

void record_golden_sun_cull_trace(std::uint32_t pc) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    const int site = gsr::widescreen::golden_sun_viewport_branch_site_index(pc);
    if (site < 0 || static_cast<std::size_t>(site) >=
                         g_golden_sun_cull_trace.size()) {
        return;
    }
    GoldenSunCullTrace& stat = g_golden_sun_cull_trace[
        static_cast<std::size_t>(site)];
    ++stat.calls;
    std::int32_t coord = 0;
    if (pc == 0x0800B324u) coord = static_cast<std::int32_t>(g_cpu.R[4]);
    else if (pc == 0x0800B27Eu || pc == 0x0800B328u)
        coord = static_cast<std::int32_t>(g_cpu.R[6]);
    else if (pc == 0x0800B3D2u || pc == 0x0800B3DCu ||
             pc == 0x0800B3E6u || pc == 0x0800B3ECu)
        coord = static_cast<std::int32_t>(g_cpu.R[3]);
    else if (pc == 0x0800C6FAu)
        coord = static_cast<std::int32_t>(g_cpu.R[3]);
    else if (pc == 0x0800C702u || pc == 0x0800C708u)
        coord = static_cast<std::int32_t>(g_cpu.R[2]);
    stat.min_coord = std::min(stat.min_coord, coord);
    stat.max_coord = std::max(stat.max_coord, coord);
}

void record_golden_sun_cull_inactive_reject(std::uint32_t pc) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    const int site = gsr::widescreen::golden_sun_viewport_branch_site_index(pc);
    if (site < 0 || static_cast<std::size_t>(site) >=
                         g_golden_sun_cull_trace.size()) {
        return;
    }
    ++g_golden_sun_cull_trace[static_cast<std::size_t>(site)].inactive_rejects;
}

void record_golden_sun_cull_bypass(std::uint32_t pc) {
    if (!golden_sun_wide_diagnostics_enabled()) return;
    const int site = gsr::widescreen::golden_sun_viewport_branch_site_index(pc);
    if (site < 0 || static_cast<std::size_t>(site) >=
                         g_golden_sun_cull_trace.size()) {
        return;
    }
    ++g_golden_sun_cull_trace[static_cast<std::size_t>(site)].bypasses;
}

int golden_sun_wide_conditional_branch(std::uint32_t instruction_pc,
                                       std::uint32_t original_decision,
                                       std::uint32_t* out_decision) {
    record_golden_sun_cull_trace(instruction_pc);
    const std::uint32_t width = gsr::widescreen::kNativeWidth +
        g_golden_sun_wide_extra_left + g_golden_sun_wide_extra_right;
    const std::uint32_t height = gsr::widescreen::kNativeHeight +
        g_golden_sun_wide_extra_top + g_golden_sun_wide_extra_bottom;
    std::int32_t compared_operand = 0;
    if (instruction_pc == 0x0800B324u) {
        // Generated disassembly: B322 is `cmp r4,#239`; B324 consumes r4.
        compared_operand = static_cast<std::int32_t>(g_cpu.R[4]);
    } else if (instruction_pc == 0x0800B27Eu ||
               instruction_pc == 0x0800B328u) {
        // Generated disassembly: B27C/B326 are `cmp r6,#159`; both consume r6.
        compared_operand = static_cast<std::int32_t>(g_cpu.R[6]);
    } else if (instruction_pc == 0x0800B3D2u ||
               instruction_pc == 0x0800B3DCu ||
               instruction_pc == 0x0800B3E6u ||
               instruction_pc == 0x0800B3ECu ||
               instruction_pc == 0x0800C6FAu) {
        // Func_b388 and C6FA consume signed r3 at their reviewed branches.
        // C6FA's policy converts the preserved bit pattern back to unsigned.
        compared_operand = static_cast<std::int32_t>(g_cpu.R[3]);
    } else if (instruction_pc == 0x0800C702u ||
               instruction_pc == 0x0800C708u) {
        // Func_c62c's lower/upper Y routes consume signed r2.
        compared_operand = static_cast<std::int32_t>(g_cpu.R[2]);
    }
    record_golden_sun_signed_y_cull(instruction_pc, compared_operand);
    GoldenSunObjB328ParentClassification b328_classification =
        GoldenSunObjB328ParentClassification::NoParent;
    unsigned b328_mismatch_flags = 0u;
    if (instruction_pc == 0x0800B328u &&
        golden_sun_expanded_obj_view_active() && compared_operand >= 160 &&
        compared_operand <= 199) {
        const std::uint64_t frame = runtime_current_frame();
        const std::uint32_t call_depth = runtime_call_stack_depth();
        const std::uint32_t call_return_pc =
            golden_sun_obj_call_return_pc(call_depth);
        const auto classification = classify_golden_sun_b328_parent(
            g_cpu.R[7], frame, call_depth, call_return_pc);
        b328_classification = classification.classification;
        b328_mismatch_flags = classification.mismatch_flags;
        record_golden_sun_b328_classification(
            compared_operand, classification, g_cpu.R[7], frame, call_depth,
            call_return_pc);
        if (b328_classification == GoldenSunObjB328ParentClassification::NoParent) {
            record_golden_sun_b328_parentless_token(
                frame, g_cpu.R[7], compared_operand, call_depth,
                call_return_pc);
        }
    }
    bool overridden = g_ws_active &&
        gsr::widescreen::golden_sun_expanded_viewport_branch_override(
            instruction_pc, original_decision, width, height,
            g_golden_sun_wide_extra_left, g_golden_sun_wide_extra_right,
            g_golden_sun_wide_extra_top, g_golden_sun_wide_extra_bottom,
            compared_operand, out_decision);
    if (!overridden && instruction_pc == 0x0800B328u) {
        const std::uint64_t frame = runtime_current_frame();
        const std::uint32_t call_depth = runtime_call_stack_depth();
        const std::uint32_t call_return_pc =
            golden_sun_obj_call_return_pc(call_depth);
        const GoldenSunObjB27EParentRoute* parent =
            find_golden_sun_current_b27e_parent(
                g_cpu.R[7], frame, call_depth, call_return_pc);
        if (parent) {
            gsr::widescreen::GoldenSunObjB328ParentMatch match{};
            match.valid = parent->valid;
            match.staging_address = parent->staging_address;
            match.frame = parent->frame;
            match.call_depth = parent->call_depth;
            match.call_return_pc = parent->call_return_pc;
            match.original_decision = parent->original_decision;
            match.final_decision = parent->final_decision;
            match.overridden = parent->overridden;
            overridden =
                gsr::widescreen::golden_sun_b328_parent_override(
                    golden_sun_expanded_obj_view_active(), original_decision,
                    compared_operand, g_cpu.R[7], frame, call_depth,
                    call_return_pc, match, out_decision);
        }
    }
    if (instruction_pc == 0x0800B328u) {
        const std::uint32_t final_decision =
            overridden ? *out_decision : original_decision;
        trace_golden_sun_obj_y_cull_decision(
            original_decision, final_decision, overridden, compared_operand);
        if (gsr::widescreen::golden_sun_obj_y_branch_writes_oam(
                instruction_pc, original_decision, overridden,
                final_decision)) {
            record_golden_sun_b328_accepted_candidate(
                compared_operand, b328_classification, g_cpu.R[7],
                runtime_current_frame(), runtime_call_stack_depth(),
                golden_sun_obj_call_return_pc(runtime_call_stack_depth()));
        } else {
            record_golden_sun_b328_rejected_candidate(
                compared_operand, b328_classification, b328_mismatch_flags,
                g_cpu.R[7], runtime_current_frame(),
                runtime_call_stack_depth(),
                golden_sun_obj_call_return_pc(runtime_call_stack_depth()));
        }
    }
    if (golden_sun_expanded_obj_view_active() &&
        instruction_pc == 0x0800B27Eu) {
        const std::uint32_t final_decision =
            overridden ? *out_decision : original_decision;
        record_golden_sun_b27e_parent_route(
            original_decision, overridden, final_decision, compared_operand);
    }
    // B324/B328 are the final staging-record route. B27E is an earlier
    // decision point and does not itself write the record, so provenance must
    // not be attached there.
    const bool expanded_obj_view = golden_sun_expanded_obj_view_active();
    if (expanded_obj_view && instruction_pc == 0x0800B324u &&
        ((!overridden && !original_decision) ||
         (overridden && *out_decision == 0u))) {
        record_golden_sun_obj_staging(
            instruction_pc, true, compared_operand);
    }
    if (expanded_obj_view && instruction_pc == 0x0800B328u &&
        gsr::widescreen::golden_sun_obj_y_branch_writes_oam(
            instruction_pc, original_decision, overridden,
            overridden ? *out_decision : original_decision)) {
        record_golden_sun_obj_staging(
            instruction_pc, false, compared_operand);
    }
    // A rejected final Y branch means no staging record is written. Retire
    // the X candidate captured at B324 so a later OAM slot cannot inherit it.
    if (expanded_obj_view && instruction_pc == 0x0800B328u &&
        !gsr::widescreen::golden_sun_obj_y_branch_writes_oam(
            instruction_pc, original_decision, overridden,
            overridden ? *out_decision : original_decision)) {
        clear_golden_sun_obj_staging(g_cpu.R[7]);
    }
    if (!overridden) {
        if (instruction_pc == 0x0800B27Eu || instruction_pc == 0x0800B328u) {
            trace_golden_sun_obj_y_attempt(
                instruction_pc, g_ws_active ? "branch-rejected" : "scene-disabled",
                compared_operand < 0 ? 0u : static_cast<std::uint32_t>(compared_operand),
                0u, false, -1);
        }
        record_golden_sun_cull_inactive_reject(instruction_pc);
        return 0;
    }
    record_golden_sun_cull_bypass(instruction_pc);
    return 1;
}

void golden_sun_wide_ewram_write_observer(std::uint32_t address,
                                          std::uint32_t size) {
    // DMA has descriptor-level provenance and must not be misreported as a
    // CPU store or establish authored-cell identity one copied unit at a time.
    if (gba::vram_trace::dma_active()) return;
    invalidate_golden_sun_palace_table_if_overlapping(address, size);
    // A loaded savestate can contain a populated table before any post-load
    // CPU store repopulates the bitmap; the provider stays fail-closed until
    // current-epoch CPU ownership is observed.
    g_golden_sun_field_authored.mark_write(address, size);
    record_golden_sun_field_table_write(address, size);
}

void golden_sun_fast_iwram_write_observer(std::uint32_t address,
                                          std::uint32_t size,
                                          std::uint32_t phase) {
    if (phase == RUNTIME_FAST_IWRAM_WRITE_START) {
        gba::vram_trace::trace_oam_shadow_write(g_cpu.R[15], address, size);
        return;
    }
    gba::vram_trace::trace_oam_shadow_write_committed(
        g_cpu.R[15], address, size);
    // Fast generated IWRAM stores are already committed at this seam. Feed
    // the CRASH-03 observer as a post-write bus-equivalent event so partial
    // stores can be reconstructed from the live word.
    note_golden_sun_obj_record_write(g_cpu.R[15], address, size);
    note_golden_sun_obj_record_value(g_cpu.R[15], address, size);
    runtime_note_pool_ldm_bus_write(address, size);
}

void install_golden_sun_widescreen(std::uint32_t extra_left,
                                   std::uint32_t extra_right,
                                   std::uint32_t extra_top,
                                   std::uint32_t extra_bottom) {
    // run_game resets all game-owned PPU hooks before startup. This callback
    // is reached only after an expanded view has been authorized and applied.
    g_golden_sun_wide_extra_left = extra_left;
    g_golden_sun_wide_extra_right = extra_right;
    g_golden_sun_wide_extra_top = extra_top;
    g_golden_sun_wide_extra_bottom = extra_bottom;
    g_golden_sun_mode0_field = false;
    g_golden_sun_mode0_split_scroll = false;
    g_golden_sun_mode0_split_scroll_frame.reset();
    g_golden_sun_expanded_obj_scene = false;
    clear_golden_sun_obj_y_provenance();
    reset_golden_sun_func1dc8_writer_diagnostics();
    g_golden_sun_obj_y_provenance_logs_in_epoch = 0;
    reset_golden_sun_obj_y_alias_diagnostics();
    g_golden_sun_wide_line_io_valid = false;
    g_golden_sun_wide_line_dispcnt = 0;
    g_golden_sun_field_authored.reset();
    g_golden_sun_field_auth_scene = GoldenSunFieldAuthScene::None;
    g_golden_sun_wide_field_map_census.clear();
    g_golden_sun_wide_field_map_trace_frame = UINT64_MAX;
    g_golden_sun_field_auth_epoch = 0;
    g_golden_sun_palace_table_authorized = false;
    g_golden_sun_palace_table_invalidated = false;
    g_golden_sun_palace_table_auth_attempted = false;
    reset_golden_sun_palace_active_region();
    g_golden_sun_field_map_writes = {};
    g_golden_sun_field_raw_writes = {};
    g_golden_sun_field_map_dma_writes = {};
    g_golden_sun_field_raw_dma_writes = {};
    g_golden_sun_field_provider_trace = {};
    g_golden_sun_field_map_id_attribution = {};
    g_golden_sun_field_map_id_attribution_overflow = {};
    reset_golden_sun_field_producers();
    reset_golden_sun_obj_record_census();
    reset_golden_sun_obj_record_value_events();
    reset_golden_sun_obj_commit_order();
    g_golden_sun_field_epoch_map_writes = 0;
    g_golden_sun_field_epoch_raw_writes = 0;
    g_golden_sun_field_epoch_map_dma_writes = 0;
    g_golden_sun_field_epoch_raw_dma_writes = 0;
    g_golden_sun_field_table_cpu_logs_in_epoch = 0;
    g_golden_sun_field_table_dma_logs_in_epoch = 0;
    gba::vram_trace::reset_oam_trace_window();
    g_golden_sun_wide_policy_seen = false;
    g_golden_sun_wide_policy_last_flags = gsr::widescreen::kPillarboxAll;
    g_golden_sun_wide_policy_last_reason =
        GoldenSunWidePolicyReason::UnsupportedMode;
    g_golden_sun_wide_policy_transition_count = 0;
    g_golden_sun_wide_policy_logged_transitions = 0;
    g_golden_sun_wide_policy_sample_count = 0;
    g_golden_sun_wide_policy_sample_frame = UINT64_MAX;
    g_golden_sun_wide_policy_sample_end_frame = UINT64_MAX;
    g_golden_sun_wide_policy_sample_transition = 0;
    g_golden_sun_wide_policy_samples_in_window = 0;
    g_golden_sun_margin_diagnostic_callbacks = 0;
    g_golden_sun_margin_diagnostic_logged = 0;
    g_golden_sun_margin_diagnostic_last_log_frame = UINT64_MAX;
    g_golden_sun_margin_diagnostic_total = {};
    g_golden_sun_margin_diagnostic_last = {};
    static bool cull_report_armed = false;
    if (!cull_report_armed) {
        cull_report_armed = true;
        std::atexit(report_golden_sun_cull_trace_at_exit);
    }
    static bool wide_report_armed = false;
    if (!wide_report_armed) {
        wide_report_armed = true;
        std::atexit(report_golden_sun_wide_diagnostics_at_exit);
    }
    gba::g_ws_margin_policy =
        golden_sun_wide_margin_policy_callback;
    gba::g_ws_margin_diagnostics = golden_sun_wide_diagnostics_enabled()
        ? golden_sun_wide_margin_diagnostics_callback : nullptr;
    gba::g_ws_tilemap_provider = golden_sun_wide_tilemap_provider;
    gba::g_ws_bg_x_provider = golden_sun_wide_bg_x_provider;
    // The rich hooks consume signed logical coordinates captured before the
    // guest truncates them into OAM. Missing or stale visible provenance
    // always leaves canonical GBA wrapping in control.
    gba::g_ws_obj_attr_x_provider = golden_sun_wide_obj_attr_x_provider;
    gba::g_ws_obj_x_provider = nullptr;
    gba::g_ws_obj_y_provider = nullptr;
    gba::g_ws_obj_attr_y_provider = golden_sun_wide_obj_attr_y_provider;
    // Authored ownership is required by the provider in every expanded run;
    // diagnostics only controls logging, not the ownership contract.
    gba::g_ws_ewram_write_observer = golden_sun_wide_ewram_write_observer;
    g_runtime_fast_ewram_write_observer = golden_sun_wide_ewram_write_observer;
    g_runtime_fast_iwram_write_observer = golden_sun_wide_diagnostics_enabled()
        ? golden_sun_fast_iwram_write_observer : nullptr;
    gba::vram_trace::set_dma_descriptor_observer(
        golden_sun_wide_dma_descriptor_observer);
    gba::vram_trace::set_oam_shadow_write_observer(
        (golden_sun_wide_diagnostics_enabled() ||
         golden_sun_experimental_fixes_enabled())
            ? golden_sun_oam_shadow_write_observer : nullptr);
    gba::g_ws_bg_x_provider_layers = 0xFu; // BG0 + Mode0 field layers.
    g_runtime_thumb_alu_imm_override = nullptr;
    g_runtime_thumb_literal_override = nullptr;
    g_runtime_conditional_branch_override =
        golden_sun_wide_conditional_branch;
}

// Golden Sun-specific policy lives in the project runner. The reusable
// gbarecomp runtime only exposes generic memory-write callback seams.
const bool g_player_speed_diagnostic =
    std::getenv("GSR_PLAYER_SPEED_DIAGNOSTIC") != nullptr;
const bool g_player_speed_diagnostic_bypass =
    std::getenv("GSR_PLAYER_SPEED_DIAGNOSTIC_BYPASS") != nullptr;
extern "C" unsigned long long g_runtime_vblank_starts;
std::atomic<unsigned long long> g_player_speed_callback_calls{0};
std::atomic<unsigned long long> g_player_speed_exact_hits{0};
std::atomic<unsigned long long> g_player_speed_address_hits{0};
std::atomic<unsigned long long> g_player_speed_pc_hits{0};
std::atomic<std::uint32_t> g_player_speed_first_address_pc{0};
std::atomic<std::uint32_t> g_player_speed_first_pc_address{0};
std::atomic<long long> g_player_speed_requested_delta_sum{0};
std::atomic<long long> g_player_speed_applied_delta_sum{0};

constexpr std::uint32_t kEndpointXWriterPc = 0x0800DC36u;
constexpr std::uint32_t kEndpointYWriterPc = 0x0800DC3Au;
struct PlayerSpeedWindowStats {
    unsigned long long calls = 0;
    unsigned long long first_frame = 0;
    unsigned long long last_frame = 0;
    std::uint32_t addr = 0;
    std::uint32_t first_before = 0;
    std::uint32_t first_requested = 0;
    std::uint32_t first_applied = 0;
    std::uint32_t last_before = 0;
    std::uint32_t last_requested = 0;
    std::uint32_t last_applied = 0;
};
PlayerSpeedWindowStats g_player_speed_windows[2][4]{};

int player_speed_replay_window(unsigned long long frame) {
    // g_runtime_vblank_starts is session-relative; state1 resumes guest frame
    // 510330, while the replay's input rows use the absolute guest frame.
    if (frame >= 116u && frame < 427u) return 0;  // right walk
    if (frame >= 468u && frame < 656u) return 1;  // left + B
    return -1;
}

void record_player_speed_window(int window, int stage, std::uint32_t addr,
                                std::uint32_t before,
                                std::uint32_t requested,
                                std::uint32_t applied) {
    if (window < 0) return;
    PlayerSpeedWindowStats& stats = g_player_speed_windows[window][stage];
    if (stats.calls++ == 0) {
        stats.first_frame = g_runtime_vblank_starts;
        stats.addr = addr;
        stats.first_before = before;
        stats.first_requested = requested;
        stats.first_applied = applied;
    }
    stats.last_frame = g_runtime_vblank_starts;
    stats.last_before = before;
    stats.last_requested = requested;
    stats.last_applied = applied;
}

int player_speed_write_override(std::uint32_t pc, std::uint32_t addr,
                                std::uint32_t requested, std::uint32_t width,
                                std::uint32_t* out_value) {
    using namespace gsr::player_speed_cheat;
    const int replay_window = player_speed_replay_window(g_runtime_vblank_starts);
    if (g_player_speed_diagnostic) {
        const unsigned long long call = g_player_speed_callback_calls.fetch_add(
            1, std::memory_order_relaxed);
        if (call == 0) {
            std::fprintf(stderr,
                         "[cheat] PlayerWalkRun2x callback active: "
                         "first_pc=0x%08X first_addr=0x%08X width=%u\n",
                         pc, addr, width);
        }
        if (addr == kAccumulator && width == 4u) {
            const unsigned long long hit = g_player_speed_address_hits.fetch_add(
                1, std::memory_order_relaxed);
            if (hit == 0) {
                g_player_speed_first_address_pc.store(pc,
                                                      std::memory_order_relaxed);
            }
        }
        if (pc == kWriterPc) {
            const unsigned long long hit = g_player_speed_pc_hits.fetch_add(
                1, std::memory_order_relaxed);
            if (hit == 0) {
                g_player_speed_first_pc_address.store(addr,
                                                       std::memory_order_relaxed);
            }
        }
        if (width == 4u &&
            (pc == kEndpointXWriterPc || pc == kEndpointYWriterPc)) {
            static std::atomic<unsigned> target_logged{0};
            if (target_logged.exchange(1u, std::memory_order_relaxed) == 0u) {
                const std::uint32_t camera_base = addr -
                    (pc == kEndpointXWriterPc ? 0x08u : 0x10u);
                const std::uint32_t target = bus_read_u32(camera_base + 0x68u);
                std::fprintf(
                    stderr,
                    "[cheat] PlayerWalkRun2x target: camera=0x%08X "
                    "target=0x%08X live=%u coords=0x%08X/0x%08X/0x%08X\n",
                    camera_base, target, target ? bus_read_u32(target) : 0u,
                    target ? bus_read_u32(target + 0x08u) : 0u,
                    target ? bus_read_u32(target + 0x0Cu) : 0u,
                    target ? bus_read_u32(target + 0x10u) : 0u);
            }
            record_player_speed_window(
                replay_window, pc == kEndpointXWriterPc ? 2 : 3, addr,
                bus_read_u32(addr), requested, requested);
        }
    }
    if (!out_value || !matches(pc, addr, width)) return 0;
    const std::uint32_t before = bus_read_u32(addr);
    const std::uint32_t applied = g_player_speed_diagnostic_bypass
        ? requested
        : transform(before, requested,
                    static_cast<std::uint32_t>(
                        runtime_get_mem_write_override_enabled()));
    if (g_player_speed_diagnostic) {
        record_player_speed_window(
            replay_window, pc == kWriterXPc ? 0 : 1, addr, before, requested,
            applied);
        g_player_speed_exact_hits.fetch_add(1, std::memory_order_relaxed);
        g_player_speed_requested_delta_sum.fetch_add(
            static_cast<long long>(static_cast<std::int32_t>(requested - before)),
            std::memory_order_relaxed);
        g_player_speed_applied_delta_sum.fetch_add(
            static_cast<long long>(static_cast<std::int32_t>(applied - before)),
            std::memory_order_relaxed);
    }
    static std::atomic<unsigned> trace_logged{0};
    if (trace_logged.exchange(1u, std::memory_order_relaxed) == 0u) {
        std::fprintf(
            stderr,
            "[cheat] PlayerWalkRun2x observed: pc=0x%08X addr=0x%08X "
            "target=0x%08X before=0x%08X requested=0x%08X "
            "applied=0x%08X delta=0x%08X\n",
            pc, addr, addr, before, requested, applied,
            applied - before);
    }
    *out_value = applied;
    return 1;
}

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
    // main.toml's [[code_copy]] runtime_start=0x03000000 source_start=
    // 0x08000770 size=0x1400 DMA-installs a fixed ROM image that kDispatchTable
    // already has AOT entries for (gf_irq_handler_03000000 and friends). Page 0
    // of IWRAM (0x03000000..0x03000FFF) is registered for dirty-tracking by the
    // synth-template rows above, so that DMA copy itself sets the page-0 dirty
    // bit before any dispatch ever reaches it, discarding the AOT fn on first
    // touch. Split around the 0x03000828..0x030008CC synth-template slot,
    // whose content is not part of this fixed identity (three registered
    // variants above own it). SHA-1s are computed from the pinned ROM at
    // source_start (+offset for the second half); the image is dispatched in
    // both modes per goldensun.elf mapping symbols (e.g. gf_tfunc_03000658 is
    // THUMB, gf_afunc_03000088 is ARM), so each half is registered once per
    // mode over identical bytes.
    {0x03000000u, 0x03000828u, 0x08000770u, 0,
     "0fddefbcec607b67676b8f0de9bcad8322c6e061",
     "main_copy_03000000_pre_dc8_arm", nullptr, nullptr},
    {0x03000000u, 0x03000828u, 0x08000770u, 1,
     "0fddefbcec607b67676b8f0de9bcad8322c6e061",
     "main_copy_03000000_pre_dc8_thumb", nullptr, nullptr},
    {0x030008CCu, 0x03001400u, 0x0800103Cu, 0,
     "41e0eb7fdbbfdd4622513cb838239db8410e641a",
     "main_copy_03000000_post_dc8_arm", nullptr, nullptr},
    {0x030008CCu, 0x03001400u, 0x0800103Cu, 1,
     "41e0eb7fdbbfdd4622513cb838239db8410e641a",
     "main_copy_03000000_post_dc8_thumb", nullptr, nullptr},
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

struct GoldenSunFunc1dc8WriterCache {
    bool valid = false;
    bool image_verified = false;
    std::uint32_t base = 0;
    std::uint64_t generation = 0;
    std::array<unsigned int, 64> page_epochs{};
};
std::array<GoldenSunFunc1dc8WriterCache, 4>
    g_golden_sun_func1dc8_writer_cache{};

void reset_golden_sun_func1dc8_writer_diagnostics() {
    g_golden_sun_func1dc8_writer_cache.fill({});
    g_golden_sun_func1dc8_writer_recognized = 0;
    g_golden_sun_func1dc8_writer_identity_mismatch = 0;
    g_golden_sun_func1dc8_writer_unknown_variant = 0;
    g_golden_sun_func1dc8_writer_logs = 0;
    g_golden_sun_func1dc8_last_base = 0;
    g_golden_sun_func1dc8_last_generation = 0;
}

const char* golden_sun_func1dc8_writer_route_name(
    gsr::Func1dc8WriterRoute route) {
    switch (route) {
        case gsr::Func1dc8WriterRoute::D4: return "D4";
        case gsr::Func1dc8WriterRoute::EC: return "EC";
        case gsr::Func1dc8WriterRoute::F0: return "F0";
        case gsr::Func1dc8WriterRoute::Count: break;
    }
    return "unknown";
}

bool golden_sun_func1dc8_writer_pc(std::uint32_t pc,
                                   gsr::Func1dc8WriterRoute route) {
    const bool diagnostics = golden_sun_wide_diagnostics_enabled();
    // Preserve the previously proven fixed route when diagnostics are off;
    // identity work is strictly observational and must not affect gameplay.
    if (!diagnostics) {
        const std::uint32_t fixed =
            gsr::kFunc1dc8KnownFixedBase + gsr::writer_offset(route);
        return pc == fixed;
    }

    const std::uint32_t offset = gsr::writer_offset(route);
    if (offset == 0u || pc < offset) {
        ++g_golden_sun_func1dc8_writer_unknown_variant;
        return false;
    }
    const std::uint32_t base = pc - offset;
    if (base < 0x02000000u ||
        base + gsr::kFunc1dc8ImageSize > 0x04000000u ||
        (base & 3u) != 0u) {
        ++g_golden_sun_func1dc8_writer_unknown_variant;
        return false;
    }

    const RelocatableCodeImage* image = nullptr;
    for (const auto& candidate : kRelocatableCodeImages) {
        if (std::strcmp(candidate.name, "Func_1dc8_relocatable") == 0) {
            image = &candidate;
            break;
        }
    }
    if (!image) {
        ++g_golden_sun_func1dc8_writer_unknown_variant;
        return false;
    }

    auto& cache = g_golden_sun_func1dc8_writer_cache[
        static_cast<std::size_t>(route)];
    const bool current = cache.valid && cache.base == base &&
        gsr::ram_range_pages_current(
            base, base + *image->size, cache.page_epochs,
            ram_code_page_epoch);
    bool verified = false;
    if (current) {
        verified = cache.image_verified;
    } else {
        bool prefix_passed = false;
        verified = relocatable_resident_at(*image, base, &prefix_passed);
        cache = {};
        cache.valid = true;
        cache.image_verified = verified;
        cache.base = base;
        cache.generation = g_ram_write_epoch;
        gsr::ram_range_register_mask(
            base, base + *image->size, &g_ram_code_page_mask_iwram,
            &g_ram_code_page_mask_ewram_lo, &g_ram_code_page_mask_ewram_hi);
        gsr::save_ram_range_page_epochs(
            base, base + *image->size, cache.page_epochs,
            ram_code_page_epoch);
    }
    const gsr::Func1dc8WriterIdentity identity{
        verified, gsr::kFunc1dc8ImageKey, base, cache.generation};
    const auto resolved = gsr::resolve_func1dc8_writer(
        pc, route, gsr::kFunc1dc8ImageKey, cache.generation, identity);
    if (!resolved.recognized) {
        if (resolved.identity_mismatch)
            ++g_golden_sun_func1dc8_writer_identity_mismatch;
        else
            ++g_golden_sun_func1dc8_writer_unknown_variant;
        return false;
    }
    ++g_golden_sun_func1dc8_writer_recognized;
    g_golden_sun_func1dc8_last_base = resolved.base;
    g_golden_sun_func1dc8_last_generation = cache.generation;
    if (g_golden_sun_func1dc8_writer_logs < 64u) {
        ++g_golden_sun_func1dc8_writer_logs;
        std::fprintf(stderr,
            "[wide-obj-writer] frame=%llu route=%s pc=0x%08x base=0x%08x "
            "image_key=0x%08x generation=%llu verified=1\n",
            static_cast<unsigned long long>(runtime_current_frame()),
            golden_sun_func1dc8_writer_route_name(route), pc, resolved.base,
            gsr::kFunc1dc8ImageKey,
            static_cast<unsigned long long>(cache.generation));
    }
    return true;
}

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

RuntimeGuestFn try_relocatable_dispatch(std::uint32_t pc, int thumb) {
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
                       RuntimeGuestFn known_fn, bool from_hint) -> RuntimeGuestFn {
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
        if (offset >= size) return nullptr;
        RuntimeGuestFn fn = known_fn;
        if (fn == nullptr) {
            fn = lookup_entry(image.dispatch_table, *image.dispatch_table_len,
                              origin + offset, thumb);
        }
        if (fn == nullptr) return nullptr;
        bool prefix_passed = false;
        const bool resident =
            from_hint ? relocatable_resident_at_cached(index, image, base, pc,
                                                        &prefix_passed)
                      : relocatable_resident_at(image, base, &prefix_passed);
        if (!resident) {
            if (profiling && prefix_passed) ++image_stats.prefix_passes;
            return nullptr;
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
        return fn;
    };

    // Try every image's last verified base before doing any discovery work.
    // A later image's good hint must not sit behind an earlier image's scan.
    for (std::size_t index = 0; index < kRelocatableCodeImages.size();
         ++index) {
        const auto& image = kRelocatableCodeImages[index];
        if (thumb != image.thumb) continue;
        if (profiling) ++g_relocatable_profile_images[index].visits;
        const RelocatableBaseHint hint = g_relocatable_base_hint[index];
        if (hint.valid) {
            if (RuntimeGuestFn fn = attempt(index, hint.base, nullptr, true))
                return fn;
        }
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
        if (RuntimeGuestFn fn =
                attempt(it->image_index, base, it->entry->fn, false)) {
            return fn;
        }
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
                if (RuntimeGuestFn fn = attempt(index, base, entry.fn, false))
                    return fn;
            }
        }
    }
    return nullptr;
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

void golden_sun_function_entry_observer(std::uint32_t entry_pc) {
    golden_sun_obj_f0_entry_capture(entry_pc);
    golden_sun_obj_staging_handoff(entry_pc);
    blitter_function_entry(entry_pc);
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

// A resolved dynamic-RAM path has already performed its interpreter/fallback
// work. Returning this sentinel lets runtime_dispatch stop without falling
// through to static tables or reporting a second miss.
void verified_ram_dispatch_noop() {}

// The resolver returns a native function for runtime_dispatch to tail-transfer
// into. Keep the active-entry marker alive until the generated callee's guest
// return path retires it; a C++ RAII guard cannot span that tail transfer.
void verified_ram_mark_active(std::uint32_t pc, int thumb);

// Run an unexplained RAM PC: resolve healed native first, else bridge through
// the interpreter (which also enqueues the heal keyed by live bytes' CRC).
RuntimeGuestFn dispatch_dynamic_ram(std::uint32_t pc, int thumb) {
    blitter_shadow_dispatch(pc, thumb);
    // The builder reuses two measured IWRAM staging slots. The older
    // 0x030057e0..0x03005a64 image is just as generated as the newer
    // 0x03006000..0x03006500 image; leaving it on the interpreter path makes
    // every pool tail-jump pay the full bridge cost while its native shard is
    // already queued.
    const bool generated_blitter = !thumb &&
        ((pc >= 0x030057E0u && pc < 0x03005A64u) ||
         (pc >= 0x03006000u && pc < 0x03006500u));
    if (!dynamic_ram_probe_on()) {
        if (RuntimeGuestFn fn = gbarecomp::overlay_resolve(pc, thumb)) {
            verified_ram_mark_active(pc, thumb);
            return fn;
        }
        if (generated_blitter) {
            const auto outcome =
                gbarecomp::overlay_request_compile(pc, thumb != 0);
            if (outcome != gbarecomp::OverlayRequestOutcome::Failed) {
                if (RuntimeGuestFn fn =
                        gbarecomp::overlay_wait_resolve(
                            pc, thumb != 0, 10000u)) {
                    verified_ram_mark_active(pc, thumb);
                    return fn;
                }
            }
            std::fprintf(stderr,
                "GoldenSunRecomp: generated blitter 0x%08X failed to heal "
                "within 10 seconds; refusing unsafe interpreter bridge.\n",
                pc);
            std::abort();
        }
        runtime_dispatch_miss(pc | (thumb ? 1u : 0u));
        return &verified_ram_dispatch_noop;
    }
    const auto call_t0 = std::chrono::steady_clock::now();
    if (RuntimeGuestFn fn = gbarecomp::overlay_resolve(pc, thumb)) {
        verified_ram_mark_active(pc, thumb);
        const auto call_t1 = std::chrono::steady_clock::now();
        dynamic_ram_probe_record(pc,
            static_cast<std::uint64_t>(std::chrono::duration_cast<
                std::chrono::nanoseconds>(call_t1 - call_t0).count()),
            /*interp_ns=*/0, /*interp_insns=*/0, /*overlay_hit=*/true);
        return fn;
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
    return &verified_ram_dispatch_noop;
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

// ── TEMPORARY same-PC recursion probe (GSR_RECURSION_PROBE=1) ───────────────
// The guard below is intentionally behavior-preserving: this probe only counts
// hits that the guard was already going to bypass. It records sparse frame
// buckets rather than one line per event, so a runaway native loop cannot turn
// diagnostics into its own source of stutter. The frame key is the runtime's
// VBlank counter, which is the same guest-frame cadence used by frame phase
// telemetry. No guest bytes or addresses are written to this report.
bool g_recursion_probe_enabled = [] {
    const char* e = std::getenv("GSR_RECURSION_PROBE");
    return e != nullptr && e[0] != '\0' && e[0] != '0';
}();

struct RecursionProbeBucket {
    std::uint64_t frame = 0;
    std::uint64_t total_hits = 0;
    std::uint64_t fixed_hits = 0;
    std::uint64_t relocatable_hits = 0;
    std::uint64_t cache_entry_hits = 0;
    std::uint64_t uncached_hits = 0;
};

struct RecursionProbe {
    static constexpr std::size_t kBucketCount = 16384;
    std::array<RecursionProbeBucket, kBucketCount> buckets{};
    std::uint64_t bucket_count = 0;
    std::uint64_t dropped_buckets = 0;

    void record(std::uint64_t frame, bool fixed, bool relocatable,
                bool cache_entry) {
        RecursionProbeBucket* bucket = nullptr;
        if (bucket_count != 0) {
            RecursionProbeBucket& last =
                buckets[(bucket_count - 1u) % kBucketCount];
            if (last.frame == frame) bucket = &last;
        }
        if (bucket == nullptr) {
            if (bucket_count >= kBucketCount) ++dropped_buckets;
            bucket = &buckets[bucket_count % kBucketCount];
            *bucket = {};
            bucket->frame = frame;
            ++bucket_count;
        }
        ++bucket->total_hits;
        if (fixed) ++bucket->fixed_hits;
        if (relocatable) ++bucket->relocatable_hits;
        if (cache_entry) ++bucket->cache_entry_hits;
        if (!cache_entry) ++bucket->uncached_hits;
    }

    void dump() const {
        if (!g_recursion_probe_enabled || bucket_count == 0) return;
        const char* events = std::getenv("GBARECOMP_FRAME_EVENTS");
        std::string path = events && events[0]
            ? std::string(events) + ".recursion.csv" : std::string();
        std::FILE* f = path.empty() ? stderr : std::fopen(path.c_str(), "w");
        if (!f) return;
        std::fprintf(f,
            "frame,total_recursion_hits,fixed_hits,relocatable_hits,"
            "cache_entry_hits,uncached_hits\n");
        const std::uint64_t count =
            std::min<std::uint64_t>(bucket_count, kBucketCount);
        const std::uint64_t first = bucket_count - count;
        for (std::uint64_t i = 0; i < count; ++i) {
            const RecursionProbeBucket& bucket =
                buckets[(first + i) % kBucketCount];
            std::fprintf(f, "%llu,%llu,%llu,%llu,%llu,%llu\n",
                static_cast<unsigned long long>(bucket.frame),
                static_cast<unsigned long long>(bucket.total_hits),
                static_cast<unsigned long long>(bucket.fixed_hits),
                static_cast<unsigned long long>(bucket.relocatable_hits),
                static_cast<unsigned long long>(bucket.cache_entry_hits),
                static_cast<unsigned long long>(bucket.uncached_hits));
        }
        if (f != stderr) {
            std::fclose(f);
            std::fprintf(stderr,
                "[recursion-probe] dumped %llu buckets -> %s"
                " (dropped=%llu)\n",
                static_cast<unsigned long long>(count), path.c_str(),
                static_cast<unsigned long long>(dropped_buckets));
        } else {
            std::fprintf(stderr,
                "[recursion-probe] buckets=%llu dropped=%llu\n",
                static_cast<unsigned long long>(count),
                static_cast<unsigned long long>(dropped_buckets));
        }
        std::fflush(stderr);
    }
};

RecursionProbe g_recursion_probe;

void report_recursion_probe() { g_recursion_probe.dump(); }

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
thread_local std::array<std::uint32_t, kVerifiedRamActiveCapacity>
    g_verified_ram_active_call_depths{};
thread_local std::array<std::uint32_t, kVerifiedRamActiveCapacity>
    g_verified_ram_active_return_pcs{};
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

// Active verified-RAM entries are guest-call scoped, not C++-call scoped.
// runtime_dispatch tail-transfers to the returned function, so a local RAII
// guard would die before that function starts. The runtime return hook below
// retires these entries after generated BX-LR/cancel handling unwinds the
// guest call-return stack, preserving both cycle protection and tail calls.
struct VerifiedRamActiveGuard {
    bool self_dispatch = false;
    bool tail_redispatch = false;
    bool depth_exceeded = false;

    explicit VerifiedRamActiveGuard(std::uint64_t key) {
        for (std::size_t i = 0; i < g_verified_ram_active_depth; ++i) {
            if (g_verified_ram_active_keys[i] == key) {
                // Re-entering the same validated PC is ordinary guest control
                // flow, including MP2K's nested loop at 0x03000828. The guest
                // call stack preserves recursion semantics; keep this as a
                // native tail transfer. The prior crash came specifically
                // from overlay_try_dispatch's non-tail retry.
                tail_redispatch = true;
                return;
            }
        }
        if (g_verified_ram_active_depth >= kVerifiedRamActiveCapacity) {
            // A long top-level tail-dispatch chain can visit more than 64
            // distinct RAM entries without a guest return. Those entries are
            // not recursive host calls. Rotate the bounded diagnostic set;
            // never turn valid control flow into a multi-million-instruction
            // interpreter bridge merely because this set filled.
            g_verified_ram_active_depth = 0;
        }
    }
    VerifiedRamActiveGuard(const VerifiedRamActiveGuard&) = delete;
    VerifiedRamActiveGuard& operator=(const VerifiedRamActiveGuard&) = delete;
};

void verified_ram_dispatch_return_hook(std::uint32_t return_pc,
                                       std::uint32_t call_stack_depth) {
    // A guest return truncates the runtime call stack. Every marker created
    // below that new depth belongs to a callee that has now returned. Remove
    // by swap; ordering is only diagnostic/protection state, not guest state.
    for (std::size_t i = g_verified_ram_active_depth; i != 0;) {
        const std::size_t slot = i - 1u;
        const bool unwound =
            g_verified_ram_active_call_depths[slot] > call_stack_depth;
        const bool top_level_return =
            g_verified_ram_active_call_depths[slot] == 0u &&
            g_verified_ram_active_return_pcs[slot] == (return_pc & ~1u);
        if (!unwound && !top_level_return) {
            i = slot;
            continue;
        }
        --g_verified_ram_active_depth;
        if (slot != g_verified_ram_active_depth) {
            g_verified_ram_active_keys[slot] =
                g_verified_ram_active_keys[g_verified_ram_active_depth];
            g_verified_ram_active_call_depths[slot] =
                g_verified_ram_active_call_depths[g_verified_ram_active_depth];
            g_verified_ram_active_return_pcs[slot] =
                g_verified_ram_active_return_pcs[g_verified_ram_active_depth];
        }
        i = std::min(i, g_verified_ram_active_depth);
    }
}

void verified_ram_mark_active(std::uint32_t pc, int thumb) {
    if (g_verified_ram_active_depth >= kVerifiedRamActiveCapacity) return;
    const std::uint64_t key = verified_ram_key(pc, thumb);
    const std::uint32_t call_depth = runtime_call_stack_depth();
    g_verified_ram_active_keys[g_verified_ram_active_depth] = key;
    g_verified_ram_active_call_depths[g_verified_ram_active_depth] = call_depth;
    g_verified_ram_active_return_pcs[g_verified_ram_active_depth] =
        g_cpu.R[14] & ~1u;
    ++g_verified_ram_active_depth;
}

void verified_ram_dispatch_outer_boundary() {
    // The runner has regained control only after the current top-level guest
    // dispatch returned. This can be a scheduling yield/SWI rather than a
    // guest BX-LR, so no return hook is guaranteed to have fired. Nested
    // markers are retired by the call-return hook; clear the remaining
    // top-level markers here so the next outer resume is a fresh dispatch.
    if (runtime_call_stack_depth() == 0u)
        g_verified_ram_active_depth = 0;
}

RuntimeGuestFn verified_ram_dispatch(std::uint32_t pc, int thumb) {
    // A PC already classified as generated code short-circuits everything
    // below. This must come FIRST: the identity scan re-hashes the whole
    // containing candidate image (SHA-1 over ~1.2 KB) and then every
    // relocatable image at this base (~2.5 KB more) before it can conclude
    // what it already concluded the first time. Generated code is HOT — a
    // single fight dispatches these ~100,000 times — so paying that scan per
    const std::uint64_t key = verified_ram_key(pc, thumb);

    if (dynamic_ram_pc_repeat(pc)) return dispatch_dynamic_ram(pc, thumb);

    VerifiedRamActiveGuard active_guard(key);

    if (active_guard.self_dispatch) {
        // A verified-RAM native entry called back into runtime_dispatch for
        // the exact same pc/thumb it is currently executing, before
        // returning. Left alone this recurses through
        // runtime_dispatch -> verified_ram_dispatch -> fn() without bound
        // and overflows the host stack (this is the fix for that crash).
        // Evict the cache entry before using the interpreter fallback; do not
        // let the ordinary static/overlay path select this active body again.
        const auto self_cached = g_verified_ram_cache.find(key);
        if (g_recursion_probe_enabled) {
            const bool cache_entry = self_cached != g_verified_ram_cache.end();
            const bool fixed = cache_entry &&
                self_cached->second.identity_index < kTransientCodeImages.size();
            // The active guard also wraps try_relocatable_dispatch(). PIC
            // entries do not populate g_verified_ram_cache, so an uncached
            // same-key hit is the relocatable path; fixed native entries always
            // have their cache row until this guard evicts it below.
            const bool relocatable = !cache_entry;
            g_recursion_probe.record(runtime_current_frame(), fixed,
                                     relocatable, cache_entry);
        }
        if (!verified_ram_pc_in(g_verified_ram_self_dispatch_reported, pc)) {
            g_verified_ram_self_dispatch_reported.push_back(pc);
            std::fprintf(stderr,
                "GoldenSunRecomp: SELF-DISPATCH RECURSION at 0x%08X (%s) "
                "identity_index=%zu — a verified-RAM native entry "
                "re-dispatched its own pc before returning. Bridging this "
                "edge once through the interpreter instead of recursing "
                "without bound.\n",
                pc, thumb ? "thumb" : "arm",
                self_cached != g_verified_ram_cache.end()
                    ? self_cached->second.identity_index
                    : kNoVerifiedIdentity);
        }
        if (self_cached != g_verified_ram_cache.end())
            g_verified_ram_cache.erase(self_cached);
        // This dispatch was already handled by the active native entry.  A
        // null return would make runtime_dispatch continue into the static
        // table (and then Stage-2 on a miss), where a same-PC static entry can
        // re-enter the body that just declined.  Bridge the recursive edge
        // once through the interpreter and return a handled sentinel so the
        // dispatcher cannot select either lower tier.
        runtime_bridge_interpret(pc, thumb != 0, 0u, 0u);
        return &verified_ram_dispatch_noop;
    }
    if (active_guard.depth_exceeded) {
        // Not a same-pc self-dispatch, but this thread's nested
        // verified-RAM native dispatch depth ran past the cap — e.g. an
        // A->B->A cycle across distinct pcs. Refuse to recurse further and
        // bridge through the interpreter once. As with same-PC recursion,
        // returning null here would let runtime_dispatch select a static or
        // Stage-2 RAM body for the declined transfer.
        if (!verified_ram_pc_in(g_verified_ram_depth_cap_reported, pc)) {
            g_verified_ram_depth_cap_reported.push_back(pc);
            std::fprintf(stderr,
                "GoldenSunRecomp: VERIFIED-RAM DISPATCH DEPTH CAP (%zu) hit "
                "at pc=0x%08X (%s) — refusing to nest further and falling "
                "back to the interpreter.\n",
                kVerifiedRamActiveCapacity, pc, thumb ? "thumb" : "arm");
        }
        // runtime_dispatch_miss retries ready overlays first. At this point
        // that can re-enter an already-active body and bypass this depth cap.
        runtime_bridge_interpret(pc, thumb != 0, 0u, 0u);
        return &verified_ram_dispatch_noop;
    }

    const auto mark_active = [&] {
        if (!active_guard.tail_redispatch)
            verified_ram_mark_active(pc, thumb);
    };

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
                mark_active();
                return cached->second.fn;
            }
            // A fixed identity owns the ordinary static dispatch entry.
            return nullptr;
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
                    mark_active();
                    return cached->second.fn;
                }
                return nullptr;
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
            return nullptr;
        }
        if (void (*fn)(void) = lookup_variant(candidate, pc, thumb)) {
            cache_verified(VerifiedRamAction::Native, fn, candidate_index);
            mark_active();
            return fn;
        }
        if (dynamic_ram_pc_repeat(pc)) {
            return dispatch_dynamic_ram(pc, thumb);
        }
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
    if (RuntimeGuestFn fn = try_relocatable_dispatch(pc, thumb)) {
        mark_active();
        return fn;
    }

    if (!covered) return nullptr;
    if (dynamic_ram_pc_repeat(pc)) {
        return dispatch_dynamic_ram(pc, thumb);
    }
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

// The static kDispatchTable already has a fixed AOT entry for this
// transient-RAM pc, but runtime_ram_code_range_dirty() found a write
// recorded somewhere in its containing 4 KiB page since boot/load — the
// dirty bit is page-granular, not per-pc. verified_ram_dispatch() above
// already ran for this exact pc/thumb earlier in the same runtime_dispatch
// call, so its cache entry (if any) is guaranteed fresh here. A `Static`
// entry means it independently confirmed, via the registered identity's own
// SHA-1, that the reviewed ROM-sourced bytes at this pc still match live
// memory — the write that dirtied the page landed elsewhere in the page, not
// in this pc's own bytes — so the AOT entry is safe to use as-is.
int verified_ram_identity_confirmed(std::uint32_t pc, int thumb) {
    const auto cached = g_verified_ram_cache.find(verified_ram_key(pc, thumb));
    return cached != g_verified_ram_cache.end() &&
           cached->second.action == VerifiedRamAction::Static;
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
    // The field atlas source is now part of the evidence-backed widescreen
    // policy. Keep the payload-free producer trace opt-in for future source
    // investigations; set GBARECOMP_VRAM_MAP_TRACE=1 when needed.
    gba::vram_trace::set_default_enabled(false);
    if (g_recursion_probe_enabled) std::atexit(report_recursion_probe);

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
    options.function_entry_observer = golden_sun_function_entry_observer;
    options.max_view_width = 360;
    options.max_view_height = 240;
    options.frame_interpolation_available = true;
    options.enhanced_timing_available = true;
    options.native_renderer_available = true;
    options.max_resize_view_width = 240;
    options.resize_driven_view = false;
    options.extended_view_init = install_golden_sun_widescreen;
    options.launcher_region = "USA/Europe";
    options.launcher_game_config = "game.toml";
    options.launcher_expose_widescreen = true;
    options.launcher_expose_adaptive_view = false;
    options.widescreen_view_width = 288;
    options.widescreen_view_height = 160;
    options.launcher_aspect_labels = kGoldenSunAspectLabels;
    options.launcher_aspect_view_widths = kGoldenSunAspectWidths;
    options.launcher_aspect_view_heights = kGoldenSunAspectHeights;
    options.launcher_num_aspects = 3;
    g_runtime_ram_dispatch_hook = verified_ram_dispatch;
    g_runtime_ram_identity_confirmed_hook = verified_ram_identity_confirmed;
    g_runtime_call_return_hook = verified_ram_dispatch_return_hook;
    g_runtime_guest_step_boundary_hook = verified_ram_dispatch_outer_boundary;
    g_runtime_mem_write_override = player_speed_write_override;
    init_ram_code_page_masks();
    g_verified_ram_cache.clear();
    g_verified_identity_cache = {};
    {
        std::size_t static_aot_ranges = 0;
        for (const auto& image : kTransientCodeImages) {
            if (image.dispatch_table == nullptr) ++static_aot_ranges;
        }
        std::fprintf(stderr,
            "GoldenSunRecomp: %zu transient-RAM range(s) registered to defer "
            "to the static AOT dispatch table once their identity matches "
            "(no heal/JIT needed).\n",
            static_aot_ranges);
    }
    g_relocatable_identity_cache = {};
    const int result = gbarecomp::run_game(argc, argv, options);
    if (g_player_speed_diagnostic) {
        std::fprintf(
            stderr,
            "[cheat] PlayerWalkRunSpeed diagnostic: multiplier_final=%d "
            "callback_calls=%llu address_hits=%llu pc_hits=%llu "
            "exact_hits=%llu first_address_pc=0x%08X first_pc_address=0x%08X "
            "requested_delta_sum=%lld applied_delta_sum=%lld\n",
            runtime_get_mem_write_override_enabled(),
            g_player_speed_callback_calls.load(std::memory_order_relaxed),
            g_player_speed_address_hits.load(std::memory_order_relaxed),
            g_player_speed_pc_hits.load(std::memory_order_relaxed),
            g_player_speed_exact_hits.load(std::memory_order_relaxed),
            g_player_speed_first_address_pc.load(std::memory_order_relaxed),
            g_player_speed_first_pc_address.load(std::memory_order_relaxed),
            g_player_speed_requested_delta_sum.load(std::memory_order_relaxed),
            g_player_speed_applied_delta_sum.load(std::memory_order_relaxed));
        constexpr const char* kWindowNames[] = {"right", "left_b"};
        constexpr const char* kStageNames[] = {
            "run_limit", "walk_limit", "endpoint_x", "endpoint_y"};
        for (int window = 0; window < 2; ++window) {
            for (int stage = 0; stage < 4; ++stage) {
                const PlayerSpeedWindowStats& stats =
                    g_player_speed_windows[window][stage];
                std::fprintf(
                    stderr,
                    "[cheat] PlayerWalkRun2x downstream: bypass=%d "
                    "window=%s stage=%s calls=%llu frames=%llu/%llu "
                    "addr=0x%08X "
                    "first=0x%08X/0x%08X/0x%08X "
                    "last=0x%08X/0x%08X/0x%08X\n",
                    g_player_speed_diagnostic_bypass ? 1 : 0,
                    kWindowNames[window], kStageNames[stage], stats.calls,
                    stats.first_frame, stats.last_frame, stats.addr,
                    stats.first_before, stats.first_requested,
                    stats.first_applied, stats.last_before,
                    stats.last_requested, stats.last_applied);
            }
        }
    }
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
