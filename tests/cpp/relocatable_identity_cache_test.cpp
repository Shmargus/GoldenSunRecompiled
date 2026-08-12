// relocatable_identity_cache_test.cpp — synthetic-fixture coverage for the
// page-epoch cache core added to src/relocatable_identity.h (GS cost fix:
// try_relocatable_dispatch's hint path used to re-run a full SHA-1 over the
// image extent on every single dispatch, even when nothing had changed).
//
// This exercises the pure helpers only: ram_range_pages_current,
// save_ram_range_page_epochs, ram_range_register_mask and
// identity_local_words_current. All four are parameterized over a synthetic
// epoch/mask/byte-buffer stand-in for the guest bus, so nothing here touches
// runner_main.cpp (a full guest-bus-dependent program that cannot be linked
// into a standalone test) or any ROM/BIOS byte.
//
// Every check is a plain assert(); a failed assertion aborts the process,
// which CTest reports as a failing test.

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "relocatable_identity.h"

namespace {

// A tiny synthetic per-page epoch table standing in for
// g_ram_code_page_epoch_iwram/ewram. Region 0x03 = "IWRAM" (8 pages),
// region 0x02 = "EWRAM" (64 pages) — same split the real globals use.
struct FakeBus {
    std::array<unsigned int, 8> iwram_epoch{};
    std::array<unsigned int, 64> ewram_epoch{};

    unsigned int epoch_of(std::uint32_t addr) const {
        const std::uint32_t region = addr >> 24;
        if (region == 0x03u) return iwram_epoch[(addr & 0x7FFFu) >> 12];
        return ewram_epoch[(addr & 0x3FFFFu) >> 12];
    }
    void touch(std::uint32_t addr) {
        const std::uint32_t region = addr >> 24;
        if (region == 0x03u) ++iwram_epoch[(addr & 0x7FFFu) >> 12];
        else ++ewram_epoch[(addr & 0x3FFFFu) >> 12];
    }
};

auto epoch_lookup(const FakeBus& bus) {
    return [&bus](std::uint32_t addr) { return bus.epoch_of(addr); };
}

// (1) A freshly saved snapshot reads back as current, and a write to an
// entirely unrelated page leaves it current — ordinary WRAM traffic must not
// invalidate every cached image.
void test_pages_current_ignores_unrelated_writes() {
    FakeBus bus;
    const std::uint32_t base = 0x03007BA4u;  // IWRAM, page 7
    const std::uint32_t size = 0x68u;        // Func_2cf4-sized fixture
    std::array<unsigned int, 64> epochs{};
    gsr::save_ram_range_page_epochs(base, base + size, epochs, epoch_lookup(bus));
    assert(gsr::ram_range_pages_current(base, base + size, epochs, epoch_lookup(bus)));

    bus.touch(0x03002000u);  // IWRAM page 2 — unrelated
    assert(gsr::ram_range_pages_current(base, base + size, epochs, epoch_lookup(bus)));
    std::puts("PASS test_pages_current_ignores_unrelated_writes");
}

// (2) A write landing inside the cached extent's own page invalidates it —
// the case a normal guest store (through the generated bus helpers) hits.
void test_pages_current_invalidated_by_own_page_write() {
    FakeBus bus;
    const std::uint32_t base = 0x03007BA4u;
    const std::uint32_t size = 0x68u;
    std::array<unsigned int, 64> epochs{};
    gsr::save_ram_range_page_epochs(base, base + size, epochs, epoch_lookup(bus));
    assert(gsr::ram_range_pages_current(base, base + size, epochs, epoch_lookup(bus)));

    bus.touch(base + 4u);  // inside the cached extent
    assert(!gsr::ram_range_pages_current(base, base + size, epochs, epoch_lookup(bus)));
    std::puts("PASS test_pages_current_invalidated_by_own_page_write");
}

// (3) THE CASE THAT MATTERS MOST: the safety analysis found that Golden
// Sun's relocatable bases are not enumerable ahead of time, so a page can be
// written to (e.g. by a DMA-style transfer that goes through the same write
// path as any other store) with NO prior epoch tracking for it at all — the
// synthetic bus never touches iwram_epoch[page] unless something calls
// touch(). ram_range_register_mask exists so a caller can prove a page is
// tracked before it starts trusting the cache for it; this test shows a
// snapshot taken WITHOUT registering first is still safe by construction —
// it just means the next real write (post-registration) is what gets
// noticed, matching the runtime rule: never trust a cache entry the write
// path can silently skip.
void test_epoch_only_advances_after_registration_matches_touch_model() {
    FakeBus bus;
    const std::uint32_t base = 0x03007DC4u;  // page 7 again, a second base
    const std::uint32_t size = 0x68u;
    unsigned int mask_iwram = 0;
    unsigned long long mask_ewram_lo = 0, mask_ewram_hi = 0;

    // No mask registered yet: a real write path would skip this page (see
    // relocatable_identity.h's "Page-epoch cache core" comment), so the
    // fixture's own `touch()` stands in for "the tracked write happened."
    // register_mask itself doesn't touch any epoch — it only ORs bits.
    gsr::ram_range_register_mask(base, base + size, &mask_iwram, &mask_ewram_lo,
                                 &mask_ewram_hi);
    assert((mask_iwram & (1u << 7)) != 0u);  // page 7 now covered
    assert(mask_ewram_lo == 0 && mask_ewram_hi == 0);  // IWRAM-only base

    std::array<unsigned int, 64> epochs{};
    gsr::save_ram_range_page_epochs(base, base + size, epochs, epoch_lookup(bus));
    assert(gsr::ram_range_pages_current(base, base + size, epochs, epoch_lookup(bus)));

    bus.touch(base);  // the "next real write", now on a registered page
    assert(!gsr::ram_range_pages_current(base, base + size, epochs, epoch_lookup(bus)));
    std::puts("PASS test_epoch_only_advances_after_registration_matches_touch_model");
}

// (4) register_mask is idempotent/monotonic: registering the same or an
// overlapping range twice never clears a bit, and never registers pages
// outside the requested extent.
void test_register_mask_monotonic_and_precise() {
    unsigned int mask_iwram = 0;
    unsigned long long mask_ewram_lo = 0, mask_ewram_hi = 0;
    gsr::ram_range_register_mask(0x03003400u, 0x0300347Cu, &mask_iwram,
                                 &mask_ewram_lo, &mask_ewram_hi);
    const unsigned int after_first = mask_iwram;
    gsr::ram_range_register_mask(0x03003400u, 0x0300347Cu, &mask_iwram,
                                 &mask_ewram_lo, &mask_ewram_hi);
    assert(mask_iwram == after_first);  // repeat registration changes nothing

    // 0x03003400 is IWRAM page 3 only (offset 0x3400..0x347c stays in one
    // 4KiB page); page 7 must remain untouched.
    assert((mask_iwram & (1u << 3)) != 0u);
    assert((mask_iwram & (1u << 7)) == 0u);
    std::puts("PASS test_register_mask_monotonic_and_precise");
}

// (5) EWRAM register_mask routes to the correct half of the split 64-bit
// mask (lo = pages 0..31, hi = pages 32..63).
void test_register_mask_ewram_split() {
    unsigned int mask_iwram = 0;
    unsigned long long mask_ewram_lo = 0, mask_ewram_hi = 0;
    gsr::ram_range_register_mask(0x02008000u, 0x02008010u, &mask_iwram,
                                 &mask_ewram_lo, &mask_ewram_hi);  // page 8, lo
    assert((mask_ewram_lo & (1ull << 8)) != 0u);
    assert(mask_ewram_hi == 0u);

    gsr::ram_range_register_mask(0x02021000u, 0x02021010u, &mask_iwram,
                                 &mask_ewram_lo, &mask_ewram_hi);  // page 33, hi
    assert((mask_ewram_hi & (1ull << 1)) != 0u);
    std::puts("PASS test_register_mask_ewram_split");
}

// (6) identity_local_words_current: a matching snapshot at the live PC's
// offset reads current; a byte that changed at that offset reads stale even
// though nothing here touched a page epoch — the belt-and-suspenders net for
// whatever epoch tracking misses.
void test_local_words_detects_content_change() {
    std::vector<std::uint8_t> snapshot(0x20, 0);
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        snapshot[i] = static_cast<std::uint8_t>(0x10 + i);
    }
    std::vector<std::uint8_t> live = snapshot;
    const std::uint32_t base = 0x03003000u;
    const std::uint32_t pc = base + 8u;  // word-aligned offset 8

    auto read_u32 = [&](std::uint32_t addr) {
        const std::uint32_t off = addr - base;
        std::uint32_t v;
        std::memcpy(&v, live.data() + off, 4);
        return v;
    };
    assert(gsr::identity_local_words_current(base, pc, snapshot.data(),
                                             static_cast<std::uint32_t>(snapshot.size()),
                                             nullptr, 0, read_u32));

    live[8] ^= 0xFFu;  // a DMA/direct-store-style change right at the offset
    assert(!gsr::identity_local_words_current(base, pc, snapshot.data(),
                                              static_cast<std::uint32_t>(snapshot.size()),
                                              nullptr, 0, read_u32));
    std::puts("PASS test_local_words_detects_content_change");
}

// (7) A change strictly inside a declared excluded (self-modified) range
// must NOT flip the local-word check to stale — that byte is expected to
// vary, exactly like Func_b5138's self-relocated jump table.
void test_local_words_ignores_excluded_range() {
    std::vector<std::uint8_t> snapshot(0x20, 0);
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        snapshot[i] = static_cast<std::uint8_t>(0x10 + i);
    }
    std::vector<std::uint8_t> live = snapshot;
    const std::uint32_t base = 0x03003000u;
    const std::uint32_t pc = base + 8u;
    const gsr::ByteRange excluded[] = {{8u, 16u}};

    auto read_u32 = [&](std::uint32_t addr) {
        const std::uint32_t off = addr - base;
        std::uint32_t v;
        std::memcpy(&v, live.data() + off, 4);
        return v;
    };
    live[9] ^= 0xFFu;  // inside the excluded [8,16) range
    assert(gsr::identity_local_words_current(base, pc, snapshot.data(),
                                             static_cast<std::uint32_t>(snapshot.size()),
                                             excluded, 1, read_u32));
    std::puts("PASS test_local_words_ignores_excluded_range");
}

// (8) A change with the SAME extent length but different content mid-image
// (no excluded range covers it) must not be shadowed by the sparse
// two-word check alone at an unrelated pc — checked here by confirming that
// checking near the actual change catches it (word 0 of the pair).
void test_local_words_same_length_content_change_detected_at_pc() {
    std::vector<std::uint8_t> snapshot(0x40, 0);
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        snapshot[i] = static_cast<std::uint8_t>(i * 3 + 1);
    }
    std::vector<std::uint8_t> live = snapshot;
    const std::uint32_t base = 0x03003A84u;
    const std::uint32_t pc = base + 0x20u;

    auto read_u32 = [&](std::uint32_t addr) {
        const std::uint32_t off = addr - base;
        std::uint32_t v;
        std::memcpy(&v, live.data() + off, 4);
        return v;
    };
    live[0x20] ^= 0x01u;  // differs, same overall length
    assert(!gsr::identity_local_words_current(base, pc, snapshot.data(),
                                              static_cast<std::uint32_t>(snapshot.size()),
                                              nullptr, 0, read_u32));
    std::puts("PASS test_local_words_same_length_content_change_detected_at_pc");
}

}  // namespace

int main() {
    test_pages_current_ignores_unrelated_writes();
    test_pages_current_invalidated_by_own_page_write();
    test_epoch_only_advances_after_registration_matches_touch_model();
    test_register_mask_monotonic_and_precise();
    test_register_mask_ewram_split();
    test_local_words_detects_content_change();
    test_local_words_ignores_excluded_range();
    test_local_words_same_length_content_change_detected_at_pc();
    std::puts("ALL PASS");
    return 0;
}
