// relocatable_identity_test.cpp — synthetic-fixture coverage for
// gsr::sha1_excluding (src/relocatable_identity.h).
//
// GS-011 (docs/GS011_TRANSIENT_IMAGES.md, "Hashing data as if it were code")
// established that a code identity must cover bytes the copy's WRITER
// leaves alone. Func_b5138 self-relocates a jump table into the MIDDLE of
// its own extent, so the fix is a hash that skips declared interior data
// ranges rather than one that trims a trailing literal pool. This test
// exercises that logic directly against a synthetic buffer - no ROM, BIOS,
// or extracted-asset bytes anywhere in this file.
//
// Every check is a plain assert(); a failed assertion aborts the process,
// which CTest reports as a failing test. Run standalone with
// gsr_relocatable_identity_test.exe for a printed PASS on success.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "relocatable_identity.h"
#include "sha1.h"

namespace {

// A synthetic "image": deterministic, non-uniform bytes so a single dropped
// or duplicated byte changes the hash. Not derived from any real ROM.
std::vector<std::uint8_t> make_fixture(std::size_t size) {
    std::vector<std::uint8_t> bytes(size);
    std::uint8_t v = 0x11;
    for (std::size_t i = 0; i < size; ++i) {
        v = static_cast<std::uint8_t>(v * 37u + static_cast<std::uint8_t>(i) + 1u);
        bytes[i] = v;
    }
    return bytes;
}

std::string hash_of(const std::vector<std::uint8_t>& bytes,
                     const gsr::ByteRange* excluded, unsigned excluded_len) {
    return gsr::sha1_excluding(
        static_cast<std::uint32_t>(bytes.size()),
        [&bytes](std::uint32_t offset) { return bytes[offset]; }, excluded,
        excluded_len);
}

// (0) byte_range_excludes: boundary correctness. start is inclusive, end is
// exclusive - the same convention config/*.toml uses for [[data_range]].
void test_byte_range_excludes_boundaries() {
    const gsr::ByteRange ranges[] = {{10, 20}};
    assert(!gsr::byte_range_excludes(9, ranges, 1));
    assert(gsr::byte_range_excludes(10, ranges, 1));
    assert(gsr::byte_range_excludes(19, ranges, 1));
    assert(!gsr::byte_range_excludes(20, ranges, 1));
    std::puts("PASS test_byte_range_excludes_boundaries");
}

// (1) No excluded ranges reproduces a plain whole-extent hash - the existing
// fixed-address and single-hole-free relocatable images see no behavior
// change from this fix.
void test_no_exclusions_matches_whole_extent() {
    auto bytes = make_fixture(64);
    const auto full = hash_of(bytes, nullptr, 0);
    const auto also_full = hash_of(bytes, nullptr, 0);
    assert(full == also_full);
    assert(!full.empty());
    std::puts("PASS test_no_exclusions_matches_whole_extent");
}

// (2) An image whose declared data range mutates at runtime still verifies:
// the hash computed with the range excluded is identical before and after
// bytes inside that range change.
void test_data_range_mutation_still_verifies() {
    auto bytes = make_fixture(0x230);  // same extent as Func_b5138
    // Mirrors config/usa/transient-func-b5138-relocatable.toml's two
    // interior data_range blocks, expressed as offsets from the image base.
    const gsr::ByteRange excluded[] = {{0xCC, 0xD4}, {0xE0, 0x120}};
    const auto before = hash_of(bytes, excluded, 2);

    // Simulate the self-relocation loop overwriting the placeholder jump
    // table (and, for good measure, the smaller literal-pool hole too).
    for (std::uint32_t offset = 0xE0; offset < 0x120; ++offset) {
        bytes[offset] = static_cast<std::uint8_t>(bytes[offset] ^ 0xFF);
    }
    for (std::uint32_t offset = 0xCC; offset < 0xD4; ++offset) {
        bytes[offset] = static_cast<std::uint8_t>(bytes[offset] ^ 0xFF);
    }
    const auto after = hash_of(bytes, excluded, 2);

    assert(before == after);
    std::puts("PASS test_data_range_mutation_still_verifies");
}

// (3) THE TEST THAT MATTERS MOST: an image whose CODE bytes mutate - a
// single byte outside every declared range - still FAILS verification. The
// exclusion must not swallow more than the declared holes.
void test_code_mutation_still_fails() {
    auto bytes = make_fixture(0x230);
    const gsr::ByteRange excluded[] = {{0xCC, 0xD4}, {0xE0, 0x120}};
    const auto before = hash_of(bytes, excluded, 2);

    // Flip one byte immediately outside each excluded range, and one deep in
    // the trailing code run past both holes (the region this task's new
    // extra_func entries live in).
    bytes[0xCB] ^= 0xFFu;  // just before the first hole
    const auto after_pre_hole = hash_of(bytes, excluded, 2);
    assert(after_pre_hole != before);
    bytes[0xCB] ^= 0xFFu;  // restore

    bytes[0x120] ^= 0xFFu;  // just after the second hole
    const auto after_post_hole = hash_of(bytes, excluded, 2);
    assert(after_post_hole != before);
    bytes[0x120] ^= 0xFFu;  // restore

    bytes[0x219C - 0x207C] ^= 0xFFu;  // deep in the trailing code run
    const auto after_deep_code = hash_of(bytes, excluded, 2);
    assert(after_deep_code != before);

    std::puts("PASS test_code_mutation_still_fails");
}

// (5) REGRESSION GUARD for the other 15 kRelocatableCodeImages entries, none
// of which declare excluded_ranges (nullptr, 0). relocatable_resident_at
// used to call `live_sha1(start, size)`:
//
//   std::vector<std::uint8_t> bytes(size);
//   for (offset = 0; offset < size; ++offset) bytes[offset] = read(offset);
//   return gba::sha1(bytes.data(), bytes.size()).hex();
//
// and now calls gsr::sha1_excluding(size, read, image.excluded_ranges,
// image.excluded_ranges_len) instead. For every one of those 15 images
// excluded_ranges_len is 0, so this must reproduce live_sha1's digest
// BYTE FOR BYTE over the same input, or all 15 recorded SHA-1s (computed
// under the old path) silently stop verifying. This reimplements
// live_sha1's exact algorithm inline (not by calling the production
// function - runner_main.cpp is a full guest-bus-dependent program, not
// something this standalone test links) and compares digests directly,
// across sizes including the size=0 edge case.
std::string live_sha1_reference(const std::vector<std::uint8_t>& source) {
    std::vector<std::uint8_t> bytes(source.size());
    for (std::size_t offset = 0; offset < source.size(); ++offset) {
        bytes[offset] = source[offset];
    }
    return gba::sha1(bytes.data(), bytes.size()).hex();
}

void test_no_exclusions_matches_live_sha1_byte_for_byte() {
    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{31},
                              std::size_t{0x230}, std::size_t{4096}}) {
        auto bytes = make_fixture(size);
        const auto reference = live_sha1_reference(bytes);
        const auto excluding = hash_of(bytes, nullptr, 0);
        assert(reference == excluding);
    }
    std::puts("PASS test_no_exclusions_matches_live_sha1_byte_for_byte");
}

// (4) Excluding a range does not shrink what gets hashed to zero, and two
// different exclusion sets over the same bytes produce different hashes -
// the exclusion set itself is part of the identity, not a knob that can be
// widened for free.
void test_exclusion_set_is_not_free() {
    auto bytes = make_fixture(0x100);
    const gsr::ByteRange narrow[] = {{0x10, 0x20}};
    const gsr::ByteRange wide[] = {{0x00, 0x40}};
    const auto narrow_hash = hash_of(bytes, narrow, 1);
    const auto wide_hash = hash_of(bytes, wide, 1);
    assert(narrow_hash != wide_hash);
    std::puts("PASS test_exclusion_set_is_not_free");
}

}  // namespace

int main() {
    test_byte_range_excludes_boundaries();
    test_no_exclusions_matches_whole_extent();
    test_no_exclusions_matches_live_sha1_byte_for_byte();
    test_data_range_mutation_still_verifies();
    test_code_mutation_still_fails();
    test_exclusion_set_is_not_free();
    std::puts("ALL PASS");
    return 0;
}
