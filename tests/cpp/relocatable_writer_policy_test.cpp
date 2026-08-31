#include "relocatable_writer_policy.h"

namespace {
bool check(bool value) { return value; }
}

int main() {
    using namespace gsr;
    constexpr std::uint32_t base_a = 0x0300387Cu;
    constexpr std::uint32_t base_b = 0x03005EE0u;
    const Func1dc8WriterIdentity valid_a{true, kFunc1dc8ImageKey, base_a,
                                         7u};
    const Func1dc8WriterIdentity valid_b{true, kFunc1dc8ImageKey, base_b,
                                         8u};
    const auto d4 = resolve_func1dc8_writer(
        base_a + kFunc1dc8D4Offset, Func1dc8WriterRoute::D4,
        kFunc1dc8ImageKey, 7u, valid_a);
    const auto ec = resolve_func1dc8_writer(
        base_b + kFunc1dc8EcOffset, Func1dc8WriterRoute::EC,
        kFunc1dc8ImageKey, 8u, valid_b);
    const auto f0 = resolve_func1dc8_writer(
        base_b + kFunc1dc8F0Offset, Func1dc8WriterRoute::F0,
        kFunc1dc8ImageKey, 8u, valid_b);
    const Func1dc8WriterIdentity wrong_hash{true, 0x9BB8u, base_b, 8u};
    const Func1dc8WriterIdentity stale_generation{true, kFunc1dc8ImageKey,
                                                  base_b, 7u};
    const auto collision = resolve_func1dc8_writer(
        base_b + kFunc1dc8F0Offset, Func1dc8WriterRoute::F0,
        kFunc1dc8ImageKey, 8u, wrong_hash);
    const auto stale = resolve_func1dc8_writer(
        base_b + kFunc1dc8F0Offset, Func1dc8WriterRoute::F0,
        kFunc1dc8ImageKey, 8u, stale_generation);
    const auto bad_offset = resolve_func1dc8_writer(
        base_b + 0x60u, Func1dc8WriterRoute::F0, kFunc1dc8ImageKey, 8u,
        valid_b);
    const auto unknown_base = resolve_func1dc8_writer(
        0x10000000u, Func1dc8WriterRoute::D4, kFunc1dc8ImageKey, 7u,
        valid_a);
    return check(d4.recognized && d4.base == base_a &&
                 d4.route == Func1dc8WriterRoute::D4 &&
                 ec.recognized && ec.base == base_b &&
                 f0.recognized && f0.offset == kFunc1dc8F0Offset &&
                 collision.identity_mismatch && !collision.recognized &&
                 stale.identity_mismatch && !stale.recognized &&
                 bad_offset.unknown_variant && !bad_offset.recognized &&
                 unknown_base.unknown_variant && !unknown_base.recognized)
        ? 0 : 1;
}
