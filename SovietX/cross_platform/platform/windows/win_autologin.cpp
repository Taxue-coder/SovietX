/**
 * @file win_autologin.cpp
 * @brief SovietX Windows 自动登录实现
 * 
 * 使用当前宿主进程的 Win32 窗口查找和 BM_CLICK 触发登录按钮。
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <string>
#include <atomic>
#include <thread>

namespace soviet {

static std::atomic_bool g_autoLoginClickSucceeded{false};
static std::atomic_int g_autoLoginAttemptCount{0};
static std::atomic_bool g_autoLoginScannerRunning{false};

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

        // Only use the control message. Do not move the physical mouse or
        // steal foreground focus from the user.
        SendMessage(hwnd, BM_CLICK, 0, 0);
        g_autoLoginClickSucceeded.store(true);
        Log("[AutoLogin] BM_CLICK sent");
        return FALSE;
    }

    return TRUE;
}

/**
 * 尝试查找并点击微信登录按钮。
 */
static bool TryClickLoginButton() {
    if (g_autoLoginClickSucceeded.load()) return true;

    HWND hWeChat = nullptr;
    const DWORD currentPid = GetCurrentProcessId();
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        DWORD ownerPid = 0;
        GetWindowThreadProcessId(hwnd, &ownerPid);
        if (ownerPid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hWeChat));

    if (!hWeChat) {
        Log("[AutoLogin] Host window not found (PID %lu)", currentPid);
        return false;
    }

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

    if (g_autoLoginScannerRunning.exchange(true)) {
        Log("[AutoLogin] scanner already running");
        return;
    }

    Log("[AutoLogin] scanner started");
    // Keep SovietX_Start short-lived because it is invoked by a remote thread.
    std::thread([]() {

    // 定时尝试点击（最多 12 次，间隔递增）
    for (int attempt = 0; attempt < 12; attempt++) {
        if (g_autoLoginClickSucceeded.load()) break;

        Sleep(350 + 300 * attempt);

        g_autoLoginAttemptCount.store(attempt + 1);
        bool ok = TryClickLoginButton();
        Log("[AutoLogin] attempt %d result=%s", attempt + 1, ok ? "OK" : "MISS");

        if (ok) break;
    }
    g_autoLoginScannerRunning.store(false);
    }).detach();
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
