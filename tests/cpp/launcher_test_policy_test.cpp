#include "launcher_test_policy.h"

namespace {

bool require(bool condition) {
    return condition;
}

}  // namespace

int main() {
    constexpr gsr::LauncherTestDefaults defaults =
        gsr::launcher_test_defaults();
    return require(!defaults.master && defaults.self_heal_ram &&
                   !defaults.cost_probe && !defaults.present_cadence &&
                   !defaults.blitter_shadow && !defaults.recursion_probe &&
                   !defaults.ram_churn_probe && !defaults.oam_shadow_trace &&
                   gsr::launcher_test_variable_enabled(
                       defaults.master, true, defaults.self_heal_ram) &&
                   !gsr::launcher_test_variable_enabled(
                       defaults.master, false, true) &&
                   gsr::launcher_test_variable_enabled(true, false, true) &&
                   !gsr::launcher_test_variable_enabled(true, false, false))
        ? 0
        : 1;
}
