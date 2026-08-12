// Golden Sun Recompiled click-to-play launcher.
//
// This launcher never copies or embeds the user's ROM. It selects the ROM,
// verifies the exact USA/Europe image, and starts the already-built runner.

#include <windows.h>
#include <bcrypt.h>
#include <commdlg.h>
#include <gdiplus.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr char kExpectedRomSha1[] =
    "5c4695205413df7db52b9a184815a07783999971";

std::wstring module_dir() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
        if (n == 0) return fs::current_path().wstring();
        if (n < buffer.size() - 1) {
            return fs::path(std::wstring(buffer.data(), n)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                      text.data(), static_cast<int>(text.size()),
                                      nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), out.data(), n);
    return out;
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                      static_cast<int>(text.size()), nullptr, 0,
                                      nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), n, nullptr, nullptr);
    return out;
}

std::string read_text(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
}

std::wstring read_json_string(const fs::path& path, const char* key) {
    const std::string text = read_text(path);
    const std::string marker = std::string("\"") + key + "\"";
    const std::size_t key_pos = text.find(marker);
    if (key_pos == std::string::npos) return {};
    const std::size_t colon = text.find(':', key_pos + marker.size());
    if (colon == std::string::npos) return {};
    const std::size_t quote = text.find('"', colon + 1);
    if (quote == std::string::npos) return {};

    std::string value;
    for (std::size_t i = quote + 1; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') break;
        if (c != '\\' || i + 1 >= text.size()) {
            value.push_back(c);
            continue;
        }
        const char escaped = text[++i];
        switch (escaped) {
        case '\\': value.push_back('\\'); break;
        case '"': value.push_back('"'); break;
        case '/': value.push_back('/'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        default: value.push_back(escaped); break;
        }
    }
    return utf8_to_wide(value);
}

std::wstring read_cached_path(const fs::path& path) {
    const std::string line = read_text(path);
    const std::size_t end = line.find_first_of("\r\n");
    return utf8_to_wide(line.substr(0, end));
}

void write_cached_path(const fs::path& path, const std::wstring& value) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file) file << wide_to_utf8(value) << '\n';
}

bool sha1_file(const fs::path& path, std::string* out) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;
    std::vector<UCHAR> object;
    std::vector<UCHAR> digest;
    bool ok = false;

    do {
        file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
        if (file == INVALID_HANDLE_VALUE) break;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM,
                                        nullptr, 0) < 0) break;

        DWORD object_len = 0;
        DWORD result_len = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&object_len),
                              sizeof(object_len), &result_len, 0) < 0) break;
        DWORD hash_len = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                              reinterpret_cast<PUCHAR>(&hash_len),
                              sizeof(hash_len), &result_len, 0) < 0) break;
        object.resize(object_len);
        digest.resize(hash_len);
        if (BCryptCreateHash(algorithm, &hash, object.data(), object_len,
                             nullptr, 0, 0) < 0) break;

        std::vector<UCHAR> buffer(1024 * 1024);
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                          &read, nullptr)) break;
            if (read == 0) {
                if (BCryptFinishHash(hash, digest.data(), hash_len, 0) < 0)
                    break;
                static constexpr char hex[] = "0123456789abcdef";
                out->clear();
                for (UCHAR byte : digest) {
                    out->push_back(hex[byte >> 4]);
                    out->push_back(hex[byte & 15]);
                }
                ok = true;
                break;
            }
            if (BCryptHashData(hash, buffer.data(), read, 0) < 0) break;
        }
    } while (false);

    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return ok;
}

std::wstring pick_file(const wchar_t* title, const wchar_t* filter,
                       const std::wstring& initial_directory = {}) {
    wchar_t buffer[32768] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
    dialog.lpstrTitle = title;
    dialog.lpstrInitialDir = initial_directory.empty()
        ? nullptr : initial_directory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return {};
    return buffer;
}

bool validate_rom(const std::wstring& path, std::wstring* error) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        *error = L"That file could not be opened.";
        return false;
    }
    std::string actual;
    if (!sha1_file(path, &actual)) {
        *error = L"The ROM could not be read.";
        return false;
    }
    if (actual != kExpectedRomSha1) {
        *error = L"That is not the required Golden Sun USA/Europe ROM.\n\n"
                 L"Expected SHA-1:\n";
        *error += utf8_to_wide(kExpectedRomSha1);
        *error += L"\n\nFound SHA-1:\n";
        *error += utf8_to_wide(actual);
        return false;
    }
    return true;
}

std::wstring choose_rom(const fs::path& root) {
    const fs::path cache = root / L"local" / L"launcher-rom.txt";
    const std::wstring cached = read_cached_path(cache);
    std::wstring initial_directory;
    if (!cached.empty()) initial_directory = fs::path(cached).parent_path();
    for (;;) {
        const std::wstring candidate = pick_file(
            L"Select the Golden Sun USA/Europe ROM",
            L"Golden Sun ROM (*.gba)\0*.gba\0All files (*.*)\0*.*\0\0",
            initial_directory);
        if (candidate.empty()) return {};

        std::wstring error;
        if (validate_rom(candidate, &error)) {
            write_cached_path(cache, candidate);
            return candidate;
        }
        MessageBoxW(nullptr, error.c_str(), L"Golden Sun Recompiled",
                    MB_OK | MB_ICONERROR);
    }
}

int run_game(const fs::path& root, const std::wstring& rom,
             const std::wstring& bios) {
    const fs::path game = root / L"build" / L"gs011" /
                          L"GoldenSunRecomp.exe";
    if (!fs::is_regular_file(game)) {
        MessageBoxW(nullptr,
                    L"GoldenSunRecomp.exe was not found. Run the build first.",
                    L"Golden Sun Recompiled", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!fs::is_regular_file(bios)) {
        MessageBoxW(nullptr,
                    L"The configured GBA BIOS was not found.\n\nEdit "
                    L"config\\local.json before launching.",
                    L"Golden Sun Recompiled", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::create_directories(root / L"recomp_cache");
    // Golden Sun's Camelot intro overflows the Windows host stack with the
    // present-in-place path. Use the stable frame-boundary unwind path here.
    SetEnvironmentVariableW(L"GBARECOMP_PRESENT_IN_PLACE", L"0");
    // The menu/overworld reaches mutable generated RAM code. The runtime
    // verifies its CRC on every native entry and re-heals on any change.
    SetEnvironmentVariableW(L"GBARECOMP_SELFHEAL_RAM", L"1");
    // The native MP2K path is still probationary; keep canonical GBA audio.
    SetEnvironmentVariableW(L"GBARECOMP_AUDIO_NATIVE", L"0");
    // Keep the canonical GBA left/right Direct Sound buses instead of the
    // legacy faithful mono host route. This does not enable native MP2K.
    SetEnvironmentVariableW(L"GBARECOMP_AUDIO_STEREO", L"1");
    SetEnvironmentVariableW(L"GBARECOMP_HEAL_CACHE",
                            (root / L"recomp_cache").c_str());

    std::wstring command = L"\"" + game.wstring() + L"\" --bios \"" +
                           bios + L"\" --rom \"" + rom + L"\"";
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(game.c_str(), mutable_command.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, root.c_str(), &startup, &process)) {
        MessageBoxW(nullptr, L"Windows could not start the recompiled game.",
                    L"Golden Sun Recompiled", MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}

constexpr int kPickRomButton = 1001;
constexpr int kQuitButton = 1002;

fs::path g_launcher_root;
std::wstring g_launcher_bios;
std::unique_ptr<Gdiplus::Image> g_splash_image;
HFONT g_button_font = nullptr;

void layout_buttons(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int button_width = 190;
    const int button_height = 48;
    const int gap = 18;
    const int total_width = button_width * 2 + gap;
    const int x = std::max<int>(0, static_cast<int>((client.right - total_width) / 2));
    const int y = std::max<int>(0, static_cast<int>(client.bottom - button_height - 28));
    HWND pick = GetDlgItem(window, kPickRomButton);
    HWND quit = GetDlgItem(window, kQuitButton);
    if (pick) MoveWindow(pick, x, y, button_width, button_height, TRUE);
    if (quit) MoveWindow(quit, x + button_width + gap, y,
                         button_width, button_height, TRUE);
}

void paint_splash(HWND window, HDC dc) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.Clear(Gdiplus::Color(255, 13, 17, 15));

    if (g_splash_image && g_splash_image->GetLastStatus() == Gdiplus::Ok &&
        width > 0 && height > 0) {
        const auto image_width = static_cast<float>(g_splash_image->GetWidth());
        const auto image_height = static_cast<float>(g_splash_image->GetHeight());
        const float scale = std::max(width / image_width, height / image_height);
        const int draw_width = static_cast<int>(image_width * scale);
        const int draw_height = static_cast<int>(image_height * scale);
        const int draw_x = (width - draw_width) / 2;
        const int draw_y = (height - draw_height) / 2;

        Gdiplus::ColorMatrix dim = {
            {{0.52f, 0.00f, 0.00f, 0.00f, 0.00f},
             {0.00f, 0.52f, 0.00f, 0.00f, 0.00f},
             {0.00f, 0.00f, 0.52f, 0.00f, 0.00f},
             {0.00f, 0.00f, 0.00f, 1.00f, 0.00f},
             {0.00f, 0.00f, 0.00f, 0.00f, 1.00f}}};
        Gdiplus::ImageAttributes attributes;
        attributes.SetColorMatrix(&dim);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(g_splash_image.get(),
                           Gdiplus::Rect(draw_x, draw_y, draw_width, draw_height),
                           0, 0, g_splash_image->GetWidth(),
                           g_splash_image->GetHeight(), Gdiplus::UnitPixel,
                           &attributes);
    }

    Gdiplus::SolidBrush bottom_overlay(Gdiplus::Color(145, 0, 0, 0));
    graphics.FillRectangle(&bottom_overlay, 0, std::max(0, height - 132),
                           width, 132);
}

LRESULT CALLBACK launcher_window_proc(HWND window, UINT message,
                                      WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowExW(0, L"BUTTON", L"Pick ROM",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                        0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(kPickRomButton),
                        GetModuleHandleW(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Quit",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                        0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(kQuitButton),
                        GetModuleHandleW(nullptr), nullptr);
        layout_buttons(window);
        return 0;
    }
    case WM_SIZE:
        layout_buttons(window);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        paint_splash(window, dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(l_param);
        if (!item || !g_button_font) return FALSE;
        const bool quit = item->CtlID == kQuitButton;
        const COLORREF fill = quit ? RGB(92, 42, 37) : RGB(180, 127, 35);
        const COLORREF border = quit ? RGB(202, 120, 95) : RGB(255, 220, 112);
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(item->hDC, &item->rcItem, brush);
        DeleteObject(brush);
        HBRUSH border_brush = CreateSolidBrush(border);
        FrameRect(item->hDC, &item->rcItem, border_brush);
        DeleteObject(border_brush);
        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, RGB(255, 246, 215));
        HFONT old_font = static_cast<HFONT>(SelectObject(item->hDC, g_button_font));
        RECT text_rect = item->rcItem;
        if (item->itemState & ODS_SELECTED) OffsetRect(&text_rect, 1, 1);
        DrawTextW(item->hDC, quit ? L"Quit" : L"Pick ROM", -1, &text_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item->hDC, old_font);
        if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
        return TRUE;
    }
    case WM_COMMAND:
        if (HIWORD(w_param) != BN_CLICKED) return 0;
        if (LOWORD(w_param) == kQuitButton) {
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(w_param) == kPickRomButton) {
            const std::wstring rom = choose_rom(g_launcher_root);
            if (!rom.empty() && run_game(g_launcher_root, rom, g_launcher_bios) == 0) {
                DestroyWindow(window);
            }
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

int show_launcher(const fs::path& root, const std::wstring& bios) {
    Gdiplus::GdiplusStartupInput startup_input;
    ULONG_PTR gdiplus_token = 0;
    if (Gdiplus::GdiplusStartup(&gdiplus_token, &startup_input, nullptr) !=
        Gdiplus::Ok) {
        MessageBoxW(nullptr, L"The splash screen could not start.",
                    L"Golden Sun Recompiled", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_launcher_root = root;
    g_launcher_bios = bios;
    g_splash_image = std::make_unique<Gdiplus::Image>(
        (root / L"gssplash.jpg").c_str());
    g_button_font = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    const wchar_t class_name[] = L"GoldenSunRecompiledLauncherWindow";
    WNDCLASSW window_class{};
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpfnWndProc = launcher_window_proc;
    window_class.lpszClassName = class_name;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    RegisterClassW(&window_class);

    RECT desired{0, 0, 720, 760};
    AdjustWindowRectEx(&desired, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                 WS_MINIMIZEBOX, FALSE, WS_EX_APPWINDOW);
    HWND window = CreateWindowExW(
        WS_EX_APPWINDOW, class_name, L"Golden Sun Recompiled",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, desired.right - desired.left,
        desired.bottom - desired.top, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!window) {
        if (g_button_font) DeleteObject(g_button_font);
        g_button_font = nullptr;
        g_splash_image.reset();
        Gdiplus::GdiplusShutdown(gdiplus_token);
        MessageBoxW(nullptr, L"The launcher window could not be created.",
                    L"Golden Sun Recompiled", MB_OK | MB_ICONERROR);
        return 1;
    }

    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(window, nullptr,
                 (screen_width - (desired.right - desired.left)) / 2,
                 (screen_height - (desired.bottom - desired.top)) / 2, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_button_font) DeleteObject(g_button_font);
    g_button_font = nullptr;
    g_splash_image.reset();
    UnregisterClassW(class_name, GetModuleHandleW(nullptr));
    Gdiplus::GdiplusShutdown(gdiplus_token);
    return static_cast<int>(message.wParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const fs::path root = module_dir();
    const std::wstring bios = read_json_string(root / L"config" / L"local.json",
                                               "bios");
    if (bios.empty()) {
        MessageBoxW(nullptr,
                    L"No BIOS path is configured in config\\local.json.",
                    L"Golden Sun Recompiled", MB_OK | MB_ICONERROR);
        return 1;
    }
    SetProcessDPIAware();
    return show_launcher(root, bios);
}
