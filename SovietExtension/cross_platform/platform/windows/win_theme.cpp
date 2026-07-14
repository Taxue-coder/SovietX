/**
 * @file win_theme.cpp
 * @brief SovietX Windows 主题模糊效果实现
 * 
 * 使用 DWM API 和 SetWindowCompositionAttribute 实现窗口模糊/透明。
 * Win10: Acrylic Blur / DWM Blur Behind
 * Win11: Mica 材质
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <dwmapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

namespace soviet {

// 未公开的 API：SetWindowCompositionAttribute
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
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5,
    ACCENT_INVALID_STATE = 6,
};

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

static pfnSetWindowCompositionAttribute g_SetWindowCompositionAttribute = nullptr;

static void ResolveCompositionAttributeFunc() {
    if (g_SetWindowCompositionAttribute) return;

    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        g_SetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)
            GetProcAddress(hUser32, "SetWindowCompositionAttribute");
    }

    if (g_SetWindowCompositionAttribute) {
        Log("SetWindowCompositionAttribute resolved");
    } else {
        Log("SetWindowCompositionAttribute not available (older Windows?)");
    }
}

/**
 * 对指定 HWND 应用 DWM 模糊效果。
 */
bool ApplyDwmBlur(HWND hwnd, int blurRadius) {
    if (!hwnd) return false;

    // 方案1：DwmEnableBlurBehindWindow（Vista+）
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = (blurRadius > 0) ? TRUE : FALSE;
    bb.hRgnBlur = NULL;

    HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    if (SUCCEEDED(hr)) {
        Log("DwmEnableBlurBehindWindow: hwnd=%p enabled=%d", hwnd, blurRadius > 0);
    }

    // 方案2：SetWindowCompositionAttribute + Acrylic（Win10 1803+）
    ResolveCompositionAttributeFunc();
    if (g_SetWindowCompositionAttribute && blurRadius > 0) {
        ACCENT_POLICY accent = {};
        accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
        // GradientColor: AABBGGRR 格式
        // 半透明深色背景
        accent.GradientColor = 0xCC1A1A1A;
        accent.AccentFlags = 2;

        WINDOWCOMPOSITIONATTRIBDATA data = {};
        data.Attribute = WCA_ACCENT_POLICY;
        data.Data = &accent;
        data.SizeOfData = sizeof(accent);

        g_SetWindowCompositionAttribute(hwnd, &data);
        Log("Acrylic blur applied: hwnd=%p", hwnd);
    }

    return true;
}

/**
 * 查找微信所有窗口并应用模糊效果。
 */
void ApplyMistyThemeToAllWindows() {
    PlatformConfig* cfg = GetConfig();
    if (!cfg) return;

    bool mistyEnabled = cfg->GetBool(kFeatureMistyMode, FeatureDefaults::kMistyModeDefault);
    if (!mistyEnabled) return;

    bool blurEnabled = cfg->GetBool(kFeatureMistyWindowBlur, FeatureDefaults::kMistyWindowBlurDefault);
    if (!blurEnabled) return;

    int blurRadius = cfg->GetInt(kMistyWindowBlurRadius, MistyDefaults::kWindowBlurRadiusDefault);

    // 枚举所有微信窗口
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);

        std::wstring cls(className);
        // 微信 Qt 窗口类名通常包含 "Qt" 或窗口标题为"微信"
        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        std::wstring ttl(title);

        bool isWeChatWindow = (cls.find(L"Qt") != std::wstring::npos) ||
                              (ttl.find(L"\x5fae\x4fe1") != std::wstring::npos); // "微信"

        if (isWeChatWindow && IsWindowVisible(hwnd)) {
            RECT rect;
            GetWindowRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            // 跳过太小的窗口（菜单、tooltip 等）
            if (width > 200 && height > 120) {
                int radius = *reinterpret_cast<int*>(lParam);
                ApplyDwmBlur(hwnd, radius);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&blurRadius));
}

/**
 * 初始化 Windows 主题模块。
 */
void InitWindowsTheme() {
    ResolveCompositionAttributeFunc();
    Log("Windows theme module initialized");
}

} // namespace soviet

#endif // _WIN32
