/**
 * @file win_ui.cpp
 * @brief SovietExtension Windows UI 实现
 * 
 * 使用 Win32 API 创建系统托盘图标和右键菜单。
 */

#ifdef _WIN32

#include "../platform_ui.h"
#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <map>

namespace soviet {

// 自定义消息
#define WM_TRAYICON  (WM_USER + 1)
#define ID_TRAYICON  1001

// 菜单项 ID 范围
#define ID_MENU_BASE 2000

class WindowsUI : public PlatformUI {
public:
    void InitPluginMenu() override {
        // 创建隐藏窗口用于接收托盘消息
        WNDCLASSA wc = {};
        wc.lpfnWndProc = TrayWndProc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "SovietTrayWindow";
        RegisterClassA(&wc);

        m_hWnd = CreateWindowA("SovietTrayWindow", "", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, GetModuleHandleA(NULL), NULL);
        if (!m_hWnd) {
            Log("Failed to create tray message window");
            return;
        }

        // 添加托盘图标
        NOTIFYICONDATAA nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hWnd;
        nid.uID = ID_TRAYICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        strncpy(nid.szTip, "SovietExtension", sizeof(nid.szTip) - 1);
        Shell_NotifyIconA(NIM_ADD, &nid);

        Log("Tray icon created");
    }

    DialogResult ShowConfirmDialog(const std::string& title,
                                    const std::string& message,
                                    const std::string& confirmText,
                                    const std::string& cancelText) override {
        int result = MessageBoxA(NULL, message.c_str(), title.c_str(),
                                  MB_OKCANCEL | MB_ICONWARNING);
        return (result == IDOK) ? DialogResult::kConfirm : DialogResult::kCancel;
    }

    void ShowSettingsWindow() override {
        // TODO: 创建设置对话框
        MessageBoxA(NULL, "Settings window not yet implemented.",
                     "SovietExtension", MB_OK | MB_ICONINFORMATION);
    }

    void UpdateMenuItemState(const std::string& key, MenuItemState state) override {
        m_menuStates[key] = state;
    }

    MenuItemState GetMenuItemState(const std::string& key) override {
        auto it = m_menuStates.find(key);
        if (it != m_menuStates.end()) return it->second;
        return MenuItemState::kOff;
    }

    bool IsDarkMode() override {
        // 检查 Windows 深色模式注册表设置
        // HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize -> AppsUseLightTheme
        HKEY hKey;
        DWORD value = 1;
        DWORD size = sizeof(value);
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
                          "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExA(hKey, "AppsUseLightTheme", NULL, NULL,
                            reinterpret_cast<LPBYTE>(&value), &size);
            RegCloseKey(hKey);
        }
        return value == 0; // 0 = Dark, 1 = Light
    }

private:
    static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_TRAYICON && lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
            return 0;
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    static void ShowTrayMenu(HWND hwnd) {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        // 读取配置状态
        PlatformConfig* cfg = GetConfig();
        bool antiRevoke = cfg ? cfg->GetBool(kFeatureAntiRevoke, false) : false;
        bool multiOpen = true;

        AppendMenuA(hMenu, MF_STRING | (antiRevoke ? MF_CHECKED : 0),
                    ID_MENU_BASE + 1, "Anti-Revoke / \xe6\xb6\x88\xe6\x81\xaf\xe9\x98\xb2\xe6\x92\xa4\xe5\x9b\x9e");
        AppendMenuA(hMenu, MF_STRING, ID_MENU_BASE + 2,
                    "Multi-Open / \xe5\xa4\x9a\xe5\xbc\x80");
        AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuA(hMenu, MF_STRING, ID_MENU_BASE + 99,
                    "Restart WeChat / \xe9\x87\x8d\xe5\x90\xaf\xe5\xbe\xae\xe4\xbf\xa1");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);

        UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                   pt.x, pt.y, 0, hwnd, NULL);

        if (cmd == ID_MENU_BASE + 99 && GetProcess()) {
            GetProcess()->RestartHostApp();
        }

        DestroyMenu(hMenu);
        PostMessage(hwnd, WM_NULL, 0, 0);
    }

    HWND m_hWnd = NULL;
    std::map<std::string, MenuItemState> m_menuStates;
};

static WindowsUI g_windowsUI;

void InitWindowsUI() {
    SetUI(&g_windowsUI);
}

// 工厂函数
PlatformUI* CreateWindowsUI() {
    return new WindowsUI();
}

} // namespace soviet

#endif // _WIN32
