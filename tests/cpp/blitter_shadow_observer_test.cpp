#include "blitter_shadow_observer.h"

#include <cassert>
#include <cstdint>

int main() {
    using namespace gsr::blitter_shadow;
    Observer observer;
    observer.builder_entry(0x2Eu, 7u, 7u, 3u, 3u);
    observer.allocator_entry(0x2Eu, 0x184u);

    const std::uint8_t body_a[] = {1, 2, 3, 4};
    const std::uint8_t body_b[] = {1, 2, 3, 5};
    const auto hash_a = Observer::bytes_hash(body_a, sizeof(body_a));
    const auto hash_b = Observer::bytes_hash(body_b, sizeof(body_b));
    assert(observer.generated_dispatch(0x03006220u, false, 0x03006220u,
                                       hash_a, 0x2Eu));
    assert(observer.generated_dispatch(0x03006220u, false, 0x03006220u,
                                       hash_a, 0x2Eu));
    assert(observer.generated_dispatch(0x03006220u, false, 0x03006220u,
                                       hash_b, 0x2Eu));
    assert(!observer.generated_dispatch(0x03006220u, true, 0x03006220u,
                                        hash_b, 0x2Eu));
    assert(!observer.generated_dispatch(0x03006400u, false, 0x03006220u,
                                        hash_b, 0x2Eu));

    observer.builder_entry(0x2Eu, 8u, 7u, 3u, 3u);
    const SlotStats* found = nullptr;
    for (const auto& s : observer.slots()) if (s.used && s.slot == 0x2Eu) found = &s;
    assert(found);
    assert(found->builder_calls == 2);
    assert(found->allocator_calls == 1);
    assert(found->requested_bytes == 0x184u);
    assert(found->descriptor_changes == 1);
    assert(found->arm_dispatches == 3);
    assert(found->wrong_mode_dispatches == 1);
    assert(found->fingerprint_count == 2);

    std::uint32_t addr = 0;
    assert(Observer::slot_address(0x2Eu, &addr));
    assert(addr == kSlotTable + 0x2Eu * 4u);
    assert(!Observer::slot_address(0x10000u, &addr));
    return 0;
}
