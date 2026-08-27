#include "launcher_session_id.h"

#include <string>

int main() {
    if (gsr::session_log_id_from_path(
            L"C:\\Golden Sun Recompiled\\logs\\session_20260823_110044.log") !=
        L"session_20260823_110044") {
        return 1;
    }
    if (gsr::session_log_id_from_path(L"session_without_log.txt") !=
        std::wstring()) {
        return 1;
    }
    if (gsr::session_log_id_from_path(L"logs\\session.log.bak") !=
        std::wstring()) {
        return 1;
    }
    return 0;
}
