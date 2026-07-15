/**
 * @file win_theme.cpp
 * @brief Windows acrylic and click-through flowing-color theme.
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <dwmapi.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

namespace soviet {

namespace {

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attribute;
    PVOID Data;
    SIZE_T SizeOfData;
};

enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19,
};

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

SetWindowCompositionAttributeFn g_setWindowCompositionAttribute = nullptr;
constexpr wchar_t kOverlayClassName[] = L"SovietXFlowingOverlay";

struct ThemeOptions {
    int blurRadius = MistyDefaults::kWindowBlurRadiusDefault;
    float opacity = static_cast<float>(MistyDefaults::kColorfulOpacityDefault);
    float spread = static_cast<float>(MistyDefaults::kColorfulBlurRadiusDefault);
    float animationDuration = static_cast<float>(MistyDefaults::kColorfulAnimationDurationDefault);
    COLORREF colors[3] = {
        static_cast<COLORREF>(MistyDefaults::kColorPrimaryDefault),
        static_cast<COLORREF>(MistyDefaults::kColorSecondaryDefault),
        static_cast<COLORREF>(MistyDefaults::kColorAccentDefault),
    };
};

ThemeOptions ReadThemeOptions() {
    ThemeOptions options;
    PlatformConfig* config = GetConfig();
    if (!config) return options;

    options.blurRadius = std::clamp(
        config->GetInt(kMistyWindowBlurRadius, options.blurRadius), 1, 30);
    options.opacity = std::clamp(static_cast<float>(
        config->GetDouble(kMistyColorfulOpacity, options.opacity)), 0.05f, 0.45f);
    options.spread = std::clamp(static_cast<float>(
        config->GetDouble(kMistyColorfulBlurRadius, options.spread)), 35.0f, 120.0f);
    options.animationDuration = std::clamp(static_cast<float>(
        config->GetDouble(kMistyColorfulAnimationDuration, options.animationDuration)), 4.0f, 30.0f);

    const char* keys[] = {kMistyColorPrimary, kMistyColorSecondary, kMistyColorAccent};
    for (int index = 0; index < 3; ++index) {
        const int value = config->GetInt(keys[index], static_cast<int>(options.colors[index]));
        if (value >= 0 && value <= 0x00FFFFFF) {
            options.colors[index] = static_cast<COLORREF>(value);
        }
    }
    return options;
}

void ResolveCompositionAttributeFunc() {
    if (g_setWindowCompositionAttribute) return;

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        g_setWindowCompositionAttribute = reinterpret_cast<SetWindowCompositionAttributeFn>(
            GetProcAddress(user32, "SetWindowCompositionAttribute"));
    }
}

bool IsHostWindow(HWND hwnd, DWORD processId) {
    DWORD ownerProcessId = 0;
    GetWindowThreadProcessId(hwnd, &ownerProcessId);
    if (ownerProcessId != processId || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;

    wchar_t className[256] = {};
    GetClassNameW(hwnd, className, 256);
    return wcsstr(className, L"Qt") != nullptr;
}

bool ContainsQrLoginText(const wchar_t* text) {
    return text && (wcsstr(text, L"\x626B\x7801\x767B\x5F55") != nullptr ||
                    wcsstr(text, L"scan qr") != nullptr ||
                    wcsstr(text, L"Scan QR") != nullptr ||
                    wcsstr(text, L"\x4EC5\x4F20\x8F93\x6587\x4EF6") != nullptr);
}

bool IsQrLoginWindow(HWND hwnd) {
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 256);
    if (ContainsQrLoginText(title)) return true;

    bool hasQrLoginText = false;
    EnumChildWindows(hwnd, [](HWND child, LPARAM value) -> BOOL {
        auto* found = reinterpret_cast<bool*>(value);
        wchar_t text[256] = {};
        GetWindowTextW(child, text, 256);
        if (ContainsQrLoginText(text)) {
            *found = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hasQrLoginText));
    if (hasQrLoginText) return true;

    RECT rect = {};
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    return width <= 720 && height <= 980 && height * 100 >= width * 116;
}

HWND FindMainWeChatWindow() {
    struct SearchState {
        DWORD processId;
        HWND window;
        LONG64 area;
    } state{GetCurrentProcessId(), nullptr, 0};

    EnumWindows([](HWND hwnd, LPARAM value) -> BOOL {
        auto* state = reinterpret_cast<SearchState*>(value);
        if (!IsHostWindow(hwnd, state->processId)) return TRUE;

        RECT rect = {};
        GetWindowRect(hwnd, &rect);
        const LONG64 width = rect.right - rect.left;
        const LONG64 height = rect.bottom - rect.top;
        const LONG64 area = width * height;
        if (width > 200 && height > 120 && area > state->area) {
            state->window = hwnd;
            state->area = area;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&state));

    return state.window;
}

void ApplyDwmBlur(HWND hwnd, int blurRadius) {
    if (!hwnd || blurRadius <= 0) return;

    DWM_BLURBEHIND blurBehind = {};
    blurBehind.dwFlags = DWM_BB_ENABLE;
    blurBehind.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &blurBehind);

    ResolveCompositionAttributeFunc();
    if (!g_setWindowCompositionAttribute) {
        Log("[Theme] Acrylic API is not available");
        return;
    }

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.AccentFlags = 2;
    accent.GradientColor = 0x9A202030;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &accent;
    data.SizeOfData = sizeof(accent);
    g_setWindowCompositionAttribute(hwnd, &data);
}

void ClearDwmBlur(HWND hwnd) {
    if (!hwnd) return;

    DWM_BLURBEHIND blurBehind = {};
    blurBehind.dwFlags = DWM_BB_ENABLE;
    DwmEnableBlurBehindWindow(hwnd, &blurBehind);

    ResolveCompositionAttributeFunc();
    if (!g_setWindowCompositionAttribute) return;

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_DISABLED;
    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &accent;
    data.SizeOfData = sizeof(accent);
    g_setWindowCompositionAttribute(hwnd, &data);
}

class FlowingOverlay {
public:
    void Start() {
        if (m_running.exchange(true)) return;
        if (m_thread.joinable()) m_thread.join();
        m_thread = std::thread(&FlowingOverlay::Run, this);
    }

    void Stop() {
        m_running.store(false);
        if (m_thread.joinable()) m_thread.join();
    }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCHITTEST) return HTTRANSPARENT;
        if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    bool CreateOverlayWindow() {
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = kOverlayClassName;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            Log("[Theme] Failed to register flowing overlay class: %lu", GetLastError());
            return false;
        }

        m_window = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            kOverlayClassName, L"", WS_POPUP, 0, 0, 0, 0,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!m_window) {
            Log("[Theme] Failed to create flowing overlay: %lu", GetLastError());
            return false;
        }
        return true;
    }

    void ReleaseSurface() {
        if (m_dc && m_previousBitmap) SelectObject(m_dc, m_previousBitmap);
        if (m_bitmap) DeleteObject(m_bitmap);
        if (m_dc) DeleteDC(m_dc);
        m_dc = nullptr;
        m_bitmap = nullptr;
        m_previousBitmap = nullptr;
        m_pixels = nullptr;
        m_width = 0;
        m_height = 0;
    }

    bool EnsureSurface(int width, int height) {
        if (width == m_width && height == m_height && m_pixels) return true;
        ReleaseSurface();

        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        m_dc = CreateCompatibleDC(nullptr);
        if (!m_dc) {
            Log("[Theme] Failed to create flowing overlay device context");
            return false;
        }
        m_bitmap = CreateDIBSection(m_dc, &info, DIB_RGB_COLORS,
                                    reinterpret_cast<void**>(&m_pixels), nullptr, 0);
        if (!m_dc || !m_bitmap || !m_pixels) {
            ReleaseSurface();
            Log("[Theme] Failed to allocate flowing overlay surface");
            return false;
        }

        m_previousBitmap = static_cast<HBITMAP>(SelectObject(m_dc, m_bitmap));
        m_width = width;
        m_height = height;
        return true;
    }

    void RenderFrame(double seconds, const ThemeOptions& options) {
        const float width = static_cast<float>(m_width);
        const float height = static_cast<float>(m_height);
        const float phase = static_cast<float>(seconds) * 10.0f / options.animationDuration;
        const float blobRadius = 0.12f + options.spread / 550.0f;
        const float radiusSquared = blobRadius * blobRadius;
        const float centers[][2] = {
            {0.25f + 0.16f * std::sin(phase * 0.55f),
             0.30f + 0.12f * std::cos(phase * 0.42f)},
            {0.68f + 0.14f * std::cos(phase * 0.47f),
             0.64f + 0.15f * std::sin(phase * 0.38f)},
            {0.50f + 0.20f * std::sin(phase * 0.31f),
             0.20f + 0.10f * std::cos(phase * 0.62f)},
        };

        for (int y = 0; y < m_height; ++y) {
            const float normalizedY = static_cast<float>(y) / height;
            for (int x = 0; x < m_width; ++x) {
                const float normalizedX = static_cast<float>(x) / width;
                float red = 6.0f;
                float green = 12.0f;
                float blue = 22.0f;
                float energy = 0.0f;

                for (int index = 0; index < 3; ++index) {
                    const float dx = normalizedX - centers[index][0];
                    const float dy = normalizedY - centers[index][1];
                    float strength = 1.0f - (dx * dx + dy * dy) / radiusSquared;
                    if (strength <= 0.0f) continue;
                    strength *= strength;
                    red += GetRValue(options.colors[index]) * strength;
                    green += GetGValue(options.colors[index]) * strength;
                    blue += GetBValue(options.colors[index]) * strength;
                    energy += strength;
                }

                const float normalizedEnergy = energy > 1.0f ? 1.0f : energy;
                const uint8_t alpha = static_cast<uint8_t>(options.opacity * 255.0f *
                                                            (0.45f + normalizedEnergy * 0.55f));
                const uint8_t r = static_cast<uint8_t>(red > 255.0f ? 255.0f : red);
                const uint8_t g = static_cast<uint8_t>(green > 255.0f ? 255.0f : green);
                const uint8_t b = static_cast<uint8_t>(blue > 255.0f ? 255.0f : blue);
                m_pixels[y * m_width + x] = (static_cast<uint32_t>(alpha) << 24) |
                                          (static_cast<uint32_t>(b) << 16) |
                                          (static_cast<uint32_t>(g) << 8) |
                                          static_cast<uint32_t>(r);
            }
        }
    }

    void Update(HWND host, double seconds) {
        RECT rect = {};
        if (!GetWindowRect(host, &rect)) return;

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 200 || height <= 120 || !EnsureSurface(width, height)) return;

        if (host != m_owner) {
            SetWindowLongPtrW(m_window, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(host));
            m_owner = host;
        }

        RenderFrame(seconds, ReadThemeOptions());
        POINT destination{rect.left, rect.top};
        SIZE size{width, height};
        POINT source{0, 0};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        HDC screen = GetDC(nullptr);
        UpdateLayeredWindow(m_window, screen, &destination, &size, m_dc, &source,
                            0, &blend, ULW_ALPHA);
        ReleaseDC(nullptr, screen);
        ShowWindow(m_window, SW_SHOWNOACTIVATE);
    }

    void Run() {
        if (!CreateOverlayWindow()) {
            m_running.store(false);
            return;
        }

        const ULONGLONG start = GetTickCount64();
        while (m_running.load()) {
            HWND host = FindMainWeChatWindow();
            if (host && !IsQrLoginWindow(host)) {
                m_suppressedQrHost = nullptr;
                Update(host, static_cast<double>(GetTickCount64() - start) / 1000.0);
            } else {
                if (host && host != m_suppressedQrHost) {
                    ClearDwmBlur(host);
                    m_suppressedQrHost = host;
                    Log("[Theme] QR login window detected; visual effects are disabled");
                }
                ShowWindow(m_window, SW_HIDE);
            }
            Sleep(50);
        }

        ReleaseSurface();
        DestroyWindow(m_window);
        m_window = nullptr;
        m_owner = nullptr;
    }

    std::atomic_bool m_running{false};
    std::thread m_thread;
    HWND m_window = nullptr;
    HWND m_owner = nullptr;
    HWND m_suppressedQrHost = nullptr;
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HBITMAP m_previousBitmap = nullptr;
    uint32_t* m_pixels = nullptr;
    int m_width = 0;
    int m_height = 0;
};

FlowingOverlay g_flowingOverlay;

} // namespace

void RefreshWindowsTheme() {
    PlatformConfig* config = GetConfig();
    if (!config || !config->GetBool(kFeatureMistyMode, FeatureDefaults::kMistyModeDefault)) {
        g_flowingOverlay.Stop();
        ClearDwmBlur(FindMainWeChatWindow());
        return;
    }

    HWND host = FindMainWeChatWindow();
    if (!host || IsQrLoginWindow(host)) {
        g_flowingOverlay.Stop();
        ClearDwmBlur(host);
        Log("[Theme] QR login window detected; visual effects are disabled");
        return;
    }

    const ThemeOptions options = ReadThemeOptions();
    if (config->GetBool(kFeatureMistyWindowBlur, FeatureDefaults::kMistyWindowBlurDefault)) {
        ApplyDwmBlur(host, options.blurRadius);
    }

    g_flowingOverlay.Start();
    Log("[Theme] Misty acrylic and flowing overlay enabled");
}

void ApplyMistyThemeToAllWindows() {
    RefreshWindowsTheme();
}

void InitWindowsTheme() {
    ResolveCompositionAttributeFunc();
    Log("[Theme] Windows theme module initialized");
}

void ShutdownWindowsTheme() {
    g_flowingOverlay.Stop();
}

} // namespace soviet

#endif // _WIN32
