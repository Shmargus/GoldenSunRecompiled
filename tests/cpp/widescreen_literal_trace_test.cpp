#include <cstdio>

#include "widescreen_literal_trace.h"

int main() {
    using namespace gsr::widescreen;

    if (golden_sun_literal_trace_index(kFieldListXUpperLiteralPc) != 0 ||
        golden_sun_literal_trace_index(kFieldListYLowerLiteralPc) != 1 ||
        golden_sun_literal_trace_index(0x0800C700u) != -1) {
        std::fprintf(stderr, "literal trace PC indexing failed\n");
        return 1;
    }

    GoldenSunLiteralTraceStats x{};
    x.pc = kFieldListXUpperLiteralPc;
    golden_sun_literal_trace_call(x, 0x012FFFFEu);
    golden_sun_literal_trace_compare_reject(x);
    golden_sun_literal_trace_call(x, 0x01200000u);
    golden_sun_literal_trace_override(x);
    if (x.calls != 2u || x.compare_rejects != 1u || x.overrides != 1u ||
        x.gate_rejects != 0u || x.min_original != 0x01200000u ||
        x.max_original != 0x012FFFFEu) {
        std::fprintf(stderr, "C6F2 literal trace counters failed\n");
        return 1;
    }

    GoldenSunLiteralTraceStats y{};
    y.pc = kFieldListYLowerLiteralPc;
    golden_sun_literal_trace_call(y, 0xFFE00000u);
    golden_sun_literal_trace_gate_reject(y);
    golden_sun_literal_trace_override(y);
    if (y.calls != 1u || y.gate_rejects != 1u || y.overrides != 1u ||
        y.compare_rejects != 0u || y.min_original != 0xFFE00000u ||
        y.max_original != 0xFFE00000u) {
        std::fprintf(stderr, "C6FE literal trace counters failed\n");
        return 1;
    }

    std::puts("widescreen_literal_trace_test: PASS");
    return 0;
}
