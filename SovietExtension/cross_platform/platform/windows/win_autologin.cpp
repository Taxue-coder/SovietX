/**
 * @file win_autologin.cpp
 * @brief SovietExtension Windows 自动登录实现
 * 
 * 使用 Win32 窗口查找 + SendInput 模拟点击登录按钮。
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <string>
#include <atomic>

namespace soviet {

static std::atomic_bool g_autoLoginClickSucceeded{false};
static std::atomic_int g_autoLoginAttemptCount{0};

/**
 * 判断窗口文本是否像登录按钮。
 * 多语言支持。
 */
static bool TextLooksLikeLoginButton(const std::wstring& text) {
    if (text.empty()) return false;

    // 转小写
    std::wstring lower = text;
    for (auto& c : lower) c = towlower(c);

    // 排除项
    const wchar_t* negatives[] = {
        L"\x5207\x6362\x8d26\x53f7",  // "切换账号"
        L"switch account",
        L"file transfer",
        L"proxy",
        nullptr
    };
    for (int i = 0; negatives[i]; i++) {
        if (lower.find(negatives[i]) != std::wstring::npos) return false;
    }

    // 正向匹配
    const wchar_t* positives[] = {
        L"\x8fdb\x5165\x5fae\x4fe1",  // "进入微信"
        L"\x767b\x5f55",               // "登录"
        L"\x767b\x5165",               // "登入"
        L"enter wechat",
        L"log in",
        L"login",
        L"sign in",
        nullptr
    };
    for (int i = 0; positives[i]; i++) {
        if (lower.find(positives[i]) != std::wstring::npos) return true;
    }

    return false;
}

/**
 * 枚举子窗口查找登录按钮并点击。
 */
static BOOL CALLBACK EnumChildForLogin(HWND hwnd, LPARAM lParam) {
    if (g_autoLoginClickSucceeded.load()) return FALSE;

    wchar_t text[256];
    GetWindowTextW(hwnd, text, 256);

    if (!TextLooksLikeLoginButton(text)) return TRUE;

    // 找到登录按钮
    if (IsWindowVisible(hwnd) && IsWindowEnabled(hwnd)) {
        Log("[AutoLogin] Found login button: hwnd=%p text=%ls", hwnd, text);

        // 方案1：发送 BM_CLICK
        SendMessage(hwnd, BM_CLICK, 0, 0);
        g_autoLoginClickSucceeded.store(true);
        Log("[AutoLogin] BM_CLICK sent");

        // 方案2：物理点击（更可靠，Qt 控件可能需要）
        RECT rect;
        GetWindowRect(hwnd, &rect);
        int cx = (rect.left + rect.right) / 2;
        int cy = (rect.top + rect.bottom) / 2;

        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;
        inputs[0].mi.dx = cx * 65535 / GetSystemMetrics(SM_CXSCREEN);
        inputs[0].mi.dy = cy * 65535 / GetSystemMetrics(SM_CYSCREEN);

        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

        SendInput(2, inputs, sizeof(INPUT));
        Log("[AutoLogin] Physical click sent at (%d, %d)", cx, cy);

        return FALSE;
    }

    return TRUE;
}

/**
 * 尝试查找并点击微信登录按钮。
 */
static bool TryClickLoginButton() {
    if (g_autoLoginClickSucceeded.load()) return true;

    // 查找微信窗口
    HWND hWeChat = FindWindowW(NULL, L"\x5fae\x4fe1"); // "微信"
    if (!hWeChat || !IsWindowVisible(hWeChat)) {
        // 尝试通过类名查找
        hWeChat = FindWindowA("Qt5QWindowIcon", NULL);
    }

    if (!hWeChat) {
        Log("[AutoLogin] WeChat window not found");
        return false;
    }

    // 前置窗口
    SetForegroundWindow(hWeChat);
    ShowWindow(hWeChat, SW_RESTORE);

    // 枚举子窗口
    EnumChildWindows(hWeChat, EnumChildForLogin, 0);

    return g_autoLoginClickSucceeded.load();
}

/**
 * 启动自动登录扫描器。
 */
void StartWindowsAutoLogin() {
    PlatformConfig* cfg = GetConfig();
    if (!cfg) return;

    bool enabled = cfg->GetBool(kFeatureAutoLogin, FeatureDefaults::kAutoLoginDefault);
    if (!enabled) {
        Log("[AutoLogin] disabled, skip");
        return;
    }

    g_autoLoginClickSucceeded.store(false);
    g_autoLoginAttemptCount.store(0);

    Log("[AutoLogin] scanner started");

    // 定时尝试点击（最多 12 次，间隔递增）
    for (int attempt = 0; attempt < 12; attempt++) {
        if (g_autoLoginClickSucceeded.load()) break;

        Sleep(350 + 300 * attempt);

        g_autoLoginAttemptCount.store(attempt + 1);
        bool ok = TryClickLoginButton();
        Log("[AutoLogin] attempt %d result=%s", attempt + 1, ok ? "OK" : "MISS");

        if (ok) break;
    }
}

/**
 * 重置自动登录状态。
 */
void ResetWindowsAutoLogin() {
    g_autoLoginClickSucceeded.store(false);
    g_autoLoginAttemptCount.store(0);
}

} // namespace soviet

#endif // _WIN32
