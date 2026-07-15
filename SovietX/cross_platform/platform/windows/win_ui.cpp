/**
 * @file win_ui.cpp
 * @brief Windows tray menu and persisted feature controls.
 */

#ifdef _WIN32

#include "../platform_ui.h"
#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <shellapi.h>

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace soviet {

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1001;
constexpr UINT kMenuAntiRevoke = 2001;
constexpr UINT kMenuMultiOpen = 2002;
constexpr UINT kMenuSettings = 2098;
constexpr wchar_t kWindowClass[] = L"SovietXTrayWindow";

} // namespace

class WindowsUI final : public PlatformUI {
public:
    ~WindowsUI() override {
        Shutdown();
    }

    void InitPluginMenu() override {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_thread.joinable()) return;

        m_ready = false;
        m_thread = std::thread(&WindowsUI::RunMessageLoop, this);
        m_readyCondition.wait_for(lock, std::chrono::seconds(5), [this] { return m_ready; });
        if (!m_hWnd) Log("Tray UI did not initialize");
    }

    DialogResult ShowConfirmDialog(const std::string& title,
                                   const std::string& message,
                                   const std::string&,
                                   const std::string&) override {
        std::wstring wideTitle(title.begin(), title.end());
        std::wstring wideMessage(message.begin(), message.end());
        const int result = MessageBoxW(m_hWnd, wideMessage.c_str(), wideTitle.c_str(),
                                       MB_OKCANCEL | MB_ICONWARNING);
        return result == IDOK ? DialogResult::kConfirm : DialogResult::kCancel;
    }

    void ShowSettingsWindow() override {
        PlatformConfig* config = GetConfig();
        if (!config) return;

        wchar_t message[384] = {};
        swprintf_s(message,
                   L"SovietX 设置会保存到用户配置文件。\n\n"
                   L"消息防撤回：%s\n微信多开：%s",
                   config->GetBool(kFeatureAntiRevoke, FeatureDefaults::kAntiRevokeDefault) ? L"开启" : L"关闭",
                   config->GetBool(kFeatureMultiOpen, FeatureDefaults::kMultiOpenDefault) ? L"开启" : L"关闭");
        MessageBoxW(m_hWnd, message, L"SovietX 设置状态", MB_OK | MB_ICONINFORMATION);
    }

    void UpdateMenuItemState(const std::string& key, MenuItemState state) override {
        std::lock_guard<std::mutex> lock(m_menuMutex);
        m_menuStates[key] = state;
    }

    MenuItemState GetMenuItemState(const std::string& key) override {
        std::lock_guard<std::mutex> lock(m_menuMutex);
        const auto iterator = m_menuStates.find(key);
        return iterator == m_menuStates.end() ? MenuItemState::kOff : iterator->second;
    }

    bool IsDarkMode() override {
        HKEY key = nullptr;
        DWORD value = 1;
        DWORD size = sizeof(value);
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          0, KEY_READ, &key) == ERROR_SUCCESS) {
            RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size);
            RegCloseKey(key);
        }
        return value == 0;
    }

private:
    static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }

        auto* self = reinterpret_cast<WindowsUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && message == kTrayMessage && lParam == WM_RBUTTONUP) {
            self->ShowTrayMenu();
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void RunMessageLoop() {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = TrayWindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ready = true;
            m_readyCondition.notify_all();
            return;
        }

        HWND window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0,
                                      HWND_MESSAGE, nullptr, instance, this);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_hWnd = window;
            m_threadId = GetCurrentThreadId();
            m_ready = true;
        }
        m_readyCondition.notify_all();
        if (!window) return;

        NOTIFYICONDATAW icon = {};
        icon.cbSize = sizeof(icon);
        icon.hWnd = window;
        icon.uID = kTrayIconId;
        icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        icon.uCallbackMessage = kTrayMessage;
        icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(icon.szTip, L"SovietX");
        Shell_NotifyIconW(NIM_ADD, &icon);

        MSG message = {};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        Shell_NotifyIconW(NIM_DELETE, &icon);
        DestroyWindow(window);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_hWnd = nullptr;
        m_threadId = 0;
    }

    void Shutdown() {
        DWORD threadId = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_thread.joinable()) return;
            threadId = m_threadId;
        }
        if (threadId != 0) PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        m_thread.join();
    }

    static void AppendFeatureMenuItem(HMENU menu, UINT command, const wchar_t* text,
                                      const char* key, bool defaultValue) {
        PlatformConfig* config = GetConfig();
        const bool enabled = config && config->GetBool(key, defaultValue);
        AppendMenuW(menu, MF_STRING | (enabled ? MF_CHECKED : 0), command, text);
    }

    void ToggleFeature(const char* key, bool defaultValue) {
        PlatformConfig* config = GetConfig();
        if (!config) return;
        const bool enabled = !config->GetBool(key, defaultValue);
        config->SetBool(key, enabled);
        config->Save();
        Log("Configuration changed: %s=%d", key, enabled ? 1 : 0);
    }

    void ShowTrayMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        AppendFeatureMenuItem(menu, kMenuAntiRevoke, L"消息防撤回", kFeatureAntiRevoke,
                              FeatureDefaults::kAntiRevokeDefault);
        AppendFeatureMenuItem(menu, kMenuMultiOpen, L"微信多开", kFeatureMultiOpen,
                              FeatureDefaults::kMultiOpenDefault);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuSettings, L"查看设置状态");

        POINT point = {};
        GetCursorPos(&point);
        SetForegroundWindow(m_hWnd);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                            point.x, point.y, 0, m_hWnd, nullptr);
        DestroyMenu(menu);
        PostMessageW(m_hWnd, WM_NULL, 0, 0);

        switch (command) {
            case kMenuAntiRevoke:
                ToggleFeature(kFeatureAntiRevoke, FeatureDefaults::kAntiRevokeDefault);
                break;
            case kMenuMultiOpen:
                ToggleFeature(kFeatureMultiOpen, FeatureDefaults::kMultiOpenDefault);
                break;
            case kMenuSettings:
                ShowSettingsWindow();
                break;
            default:
                break;
        }
    }

    HWND m_hWnd = nullptr;
    DWORD m_threadId = 0;
    bool m_ready = false;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_readyCondition;
    std::map<std::string, MenuItemState> m_menuStates;
    std::mutex m_menuMutex;
};

PlatformUI* CreateWindowsUI() {
    return new WindowsUI();
}

} // namespace soviet

#endif // _WIN32
