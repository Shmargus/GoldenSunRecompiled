#include "launcher_replay_policy.h"

#include <string>

int main() {
    using gsr::append_developer_replay_arguments;
    using gsr::parse_replay_frame_budget;
    using gsr::quote_windows_command_arg;

    if (quote_windows_command_arg(L"C:\\Golden Sun\\state 1") !=
        L"\"C:\\Golden Sun\\state 1\"") {
        return 1;
    }
    if (quote_windows_command_arg(L"C:\\path\\with\\") !=
        L"\"C:\\path\\with\\\\\"") {
        return 1;
    }
    std::uint32_t frames = 0;
    if (!parse_replay_frame_budget(L"900", &frames) || frames != 900 ||
        parse_replay_frame_budget(L"0", &frames) ||
        parse_replay_frame_budget(L"9x", &frames) ||
        parse_replay_frame_budget(L"2147483648", &frames)) {
        return 1;
    }

    std::wstring command = L"runner --bios \"bios\" --rom \"rom\"";
    if (!append_developer_replay_arguments(command, L"state 1", L"900") ||
        command != L"runner --bios \"bios\" --rom \"rom\" --load-state "
                   L"\"state 1\" --window --frames 900") {
        return 1;
    }
    if (append_developer_replay_arguments(command, L"", L"900")) return 1;
    return 0;
}
