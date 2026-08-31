#include <cwchar>
#include <string>

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
    const bool policy_ok = !gsr::launcher_widescreen_diagnostics_default() &&
                   !disabled.enabled &&
                       std::wcscmp(disabled.environment_value, L"0") == 0 &&
                   enabled.enabled &&
                       std::wcscmp(enabled.environment_value, L"1") == 0 &&
                   !gsr::resolve_launcher_input_record_policy(
                            false, L"", false, L"logs\\unused.input").enabled &&
                   gsr::resolve_launcher_input_record_policy(
                            true, L"", false,
                            L"logs\\session_20260829_221432.input").enabled &&
                   gsr::resolve_launcher_input_record_policy(
                            true, L"", false,
                            L"logs\\session_20260829_221432.input").path ==
                       L"logs\\session_20260829_221432.input" &&
                   gsr::resolve_launcher_input_record_policy(
                            false, L"C:\\work dir\\capture.input", false,
                            L"logs\\unused.input").explicit_path &&
                   gsr::resolve_launcher_input_record_policy(
                            false, L"C:\\work dir\\capture.input", false,
                            L"logs\\unused.input").path ==
                       L"C:\\work dir\\capture.input" &&
                   !gsr::resolve_launcher_input_record_policy(
                            true, L"", true, L"logs\\unused.input").enabled &&
                   gsr::resolve_launcher_input_record_policy(
                            true, L"capture.input", true,
                            L"logs\\unused.input").replay_conflict &&
                   gsr::automatic_input_record_path(
                          L"logs\\session_20260829_221432.log") ==
                       L"logs\\session_20260829_221432.input" &&
                   gsr::automatic_input_record_path(
                          L"logs\\session_20260829_221432.log", 1) ==
                       L"logs\\session_20260829_221432_1.input" &&
                   gsr::automatic_input_record_path(L"capture.txt").empty() &&
                   gsr::choose_unique_input_record_path(
                       L"logs\\session_20260829_221432.log",
                       [](const std::wstring& path) {
                           return path.ends_with(L".input") &&
                                  !path.ends_with(L"_2.input");
                       }, 3) ==
                       L"logs\\session_20260829_221432_2.input";
    return require(policy_ok) ? 0 : 1;
}
