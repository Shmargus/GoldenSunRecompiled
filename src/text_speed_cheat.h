#pragma once

#include <cstdint>

// Golden Sun USA/Europe metadata belongs to the project runner, not the
// reusable ARMv4T runtime. These identities were measured from the generated
// image and the hash-verified ROM (FACTS.md, 2026-09-10).
namespace gsr::text_speed_cheat {

// The dialogue text processor 0x080168F4 keeps a per-call token allowance in
// the stack slot [sp+0x20]. Four sites write it:
//
//   0x08016920  seed from the table at 0x0807380B, indexed by the Message
//               speed byte at 0x0200044C -- {1, 1, 10} for Slow/Normal/Fast.
//   0x08016942  replacement with 5*clamp(halfword(0x03001CD0), 0, 2)+3, taken
//               only when the byte at r8+0xEA5 is nonzero (the path battle
//               text uses; measured at 8 there).
//   0x08016EDC  forced stop on a space token (r7 == 0x20), when that same
//               r8+0xEA5 byte is zero.
//   0x08016A34, 0x08016D72  forced stops on the box/line-break paths.
//
// Session 20260910_225128 measured that the Fast allowance is never spent:
// all 156 invocations ended at 0x08016EDC, the deepest countdown being
// 10 -> 5. Fast therefore draws one WORD per call, not ten tokens, and
// raising the table value alone changes nothing.
//
// Instant text overrides exactly two of those writes: the seed, and the
// space stop. The line-break and box stops at 0x08016A34 / 0x08016D72 are
// deliberately left alone, so page waits and "press A to continue" keep
// their original behaviour.
constexpr std::uint32_t kBudgetSeedPc = 0x08016920u;
constexpr std::uint32_t kWordStopPc = 0x08016EDCu;

// One page of dialogue is far below this. The counter is decremented once per
// parsed token and the slot is a full word, so a large value simply means the
// parser reaches its own exit before the allowance runs out.
constexpr std::uint32_t kInstantBudget = 255u;

// Message speed lives at 0x02000240 + 0x20C; 0 = Slow, 1 = Normal, 2 = Fast.
// Instant text applies only on Fast, so the in-game option still selects
// Slow / Normal / Instant rather than being overridden everywhere.
constexpr std::uint32_t kMessageSpeedAddress = 0x0200044Cu;
constexpr std::uint32_t kMessageSpeedFast = 2u;

constexpr bool matches(std::uint32_t pc, std::uint32_t width) {
    return width == 4u && (pc == kBudgetSeedPc || pc == kWordStopPc);
}

}  // namespace gsr::text_speed_cheat
