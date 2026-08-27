// Pure root-launcher policy for the optional WIDE-01 diagnostics.
// No Win32/UI or runtime dependency.
#pragma once

namespace gsr {

struct LauncherWidescreenDiagnosticsPolicy {
    bool enabled = false;
    const wchar_t* environment_value = L"0";
};

// Requested acceptance traces are opt-in per launcher session, never a
// persistent normal-gameplay setting.
constexpr bool launcher_widescreen_diagnostics_default() {
    return false;
}

// The launcher owns this choice. Always return an explicit child value so an
// inherited GBARECOMP_VRAM_MAP_TRACE cannot silently enable a normal launch.
constexpr LauncherWidescreenDiagnosticsPolicy
resolve_launcher_widescreen_diagnostics_policy(bool requested) {
    return requested
        ? LauncherWidescreenDiagnosticsPolicy{true, L"1"}
        : LauncherWidescreenDiagnosticsPolicy{false, L"0"};
}

}  // namespace gsr
