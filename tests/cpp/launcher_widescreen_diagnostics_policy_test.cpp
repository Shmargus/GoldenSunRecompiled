#include <cwchar>

#include "launcher_widescreen_diagnostics_policy.h"

namespace {

bool require(bool condition) {
    return condition;
}

}  // namespace

int main() {
    const auto disabled =
        gsr::resolve_launcher_widescreen_diagnostics_policy(false);
    const auto enabled =
        gsr::resolve_launcher_widescreen_diagnostics_policy(true);
    return require(!gsr::launcher_widescreen_diagnostics_default() &&
                   !disabled.enabled &&
                       std::wcscmp(disabled.environment_value, L"0") == 0 &&
                   enabled.enabled &&
                       std::wcscmp(enabled.environment_value, L"1") == 0)
        ? 0
        : 1;
}
