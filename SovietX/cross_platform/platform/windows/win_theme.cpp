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

    void RenderFrame(double seconds) {
        const float width = static_cast<float>(m_width);
        const float height = static_cast<float>(m_height);
        const float centers[][2] = {
            {0.25f + 0.16f * std::sin(static_cast<float>(seconds * 0.55)),
             0.30f + 0.12f * std::cos(static_cast<float>(seconds * 0.42))},
            {0.68f + 0.14f * std::cos(static_cast<float>(seconds * 0.47)),
             0.64f + 0.15f * std::sin(static_cast<float>(seconds * 0.38))},
            {0.50f + 0.20f * std::sin(static_cast<float>(seconds * 0.31)),
             0.20f + 0.10f * std::cos(static_cast<float>(seconds * 0.62))},
        };
        const uint8_t colors[][3] = {
            {35, 190, 255},
            {190, 70, 255},
            {255, 145, 55},
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
                    float strength = 1.0f - (dx * dx + dy * dy) / 0.28f;
                    if (strength <= 0.0f) continue;
                    strength *= strength;
                    red += colors[index][0] * strength;
                    green += colors[index][1] * strength;
                    blue += colors[index][2] * strength;
                    energy += strength;
                }

                const uint8_t alpha = static_cast<uint8_t>(28.0f + (energy > 1.0f ? 1.0f : energy) * 42.0f);
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

        RenderFrame(seconds);
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
            if (host) {
                Update(host, static_cast<double>(GetTickCount64() - start) / 1000.0);
            } else {
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
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HBITMAP m_previousBitmap = nullptr;
    uint32_t* m_pixels = nullptr;
    int m_width = 0;
    int m_height = 0;
};

FlowingOverlay g_flowingOverlay;

} // namespace

void ApplyMistyThemeToAllWindows() {
    PlatformConfig* config = GetConfig();
    if (!config || !config->GetBool(kFeatureMistyMode, FeatureDefaults::kMistyModeDefault)) {
        g_flowingOverlay.Stop();
        return;
    }

    HWND host = FindMainWeChatWindow();
    if (host && config->GetBool(kFeatureMistyWindowBlur, FeatureDefaults::kMistyWindowBlurDefault)) {
        ApplyDwmBlur(host, config->GetInt(kMistyWindowBlurRadius,
                                          MistyDefaults::kWindowBlurRadiusDefault));
    }

    if (config->GetBool(kFeatureMistyColorful, FeatureDefaults::kMistyColorfulDefault)) {
        g_flowingOverlay.Start();
        Log("[Theme] Misty acrylic and flowing overlay enabled");
    }
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
