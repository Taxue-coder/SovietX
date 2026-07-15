/**
 * @file win_ui.cpp
 * @brief Windows tray menu and persisted feature controls.
 */

#ifdef _WIN32

#include "../platform_ui.h"
#include "../platform_process.h"
#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <cmath>
#include <chrono>
#include <cstring>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace soviet {

void RefreshWindowsTheme();

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1001;
constexpr UINT kMenuAntiRevoke = 2001;
constexpr UINT kMenuMultiOpen = 2002;
constexpr UINT kMenuMistyMode = 2003;
constexpr UINT kMenuMistySettings = 2004;
constexpr UINT kMenuSettings = 2098;
constexpr wchar_t kWindowClass[] = L"SovietXTrayWindow";
constexpr wchar_t kMistySettingsClass[] = L"SovietXMistySettings";

constexpr int kControlOpacity = 3001;
constexpr int kControlBlur = 3002;
constexpr int kControlSpread = 3003;
constexpr int kControlDuration = 3004;
constexpr int kControlPrimaryColor = 3011;
constexpr int kControlSecondaryColor = 3012;
constexpr int kControlAccentColor = 3013;

struct MistySettingsState {
    HWND opacity = nullptr;
    HWND blur = nullptr;
    HWND spread = nullptr;
    HWND duration = nullptr;
    HWND opacityValue = nullptr;
    HWND blurValue = nullptr;
    HWND spreadValue = nullptr;
    HWND durationValue = nullptr;
    HWND colorButtons[3] = {};
    COLORREF colors[3] = {
        static_cast<COLORREF>(MistyDefaults::kColorPrimaryDefault),
        static_cast<COLORREF>(MistyDefaults::kColorSecondaryDefault),
        static_cast<COLORREF>(MistyDefaults::kColorAccentDefault),
    };
};

HWND CreateSlider(HWND parent, int id, int x, int y, int width, int minimum, int maximum, int value) {
    HWND slider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS,
                                  x, y, width, 26, parent,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  GetModuleHandleW(nullptr), nullptr);
    SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELONG(minimum, maximum));
    SendMessageW(slider, TBM_SETPOS, TRUE, value);
    return slider;
}

int SliderValue(HWND slider) {
    return static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
}

void UpdateSliderValues(MistySettingsState* state) {
    wchar_t value[64] = {};
    swprintf_s(value, L"%d%%", SliderValue(state->opacity));
    SetWindowTextW(state->opacityValue, value);
    swprintf_s(value, L"%d", SliderValue(state->blur));
    SetWindowTextW(state->blurValue, value);
    swprintf_s(value, L"%d", SliderValue(state->spread));
    SetWindowTextW(state->spreadValue, value);
    swprintf_s(value, L"%d s", SliderValue(state->duration));
    SetWindowTextW(state->durationValue, value);
}

void UpdateColorButton(MistySettingsState* state, int index) {
    wchar_t text[64] = {};
    swprintf_s(text, L"颜色 %d  #%02X%02X%02X", index + 1,
               GetRValue(state->colors[index]), GetGValue(state->colors[index]),
               GetBValue(state->colors[index]));
    SetWindowTextW(state->colorButtons[index], text);
}

void SaveMistySettings(MistySettingsState* state) {
    PlatformConfig* config = GetConfig();
    if (!config) return;

    config->SetDouble(kMistyColorfulOpacity, SliderValue(state->opacity) / 100.0);
    config->SetInt(kMistyWindowBlurRadius, SliderValue(state->blur));
    config->SetDouble(kMistyColorfulBlurRadius, SliderValue(state->spread));
    config->SetDouble(kMistyColorfulAnimationDuration, SliderValue(state->duration));
    config->SetInt(kMistyColorPrimary, static_cast<int>(state->colors[0]));
    config->SetInt(kMistyColorSecondary, static_cast<int>(state->colors[1]));
    config->SetInt(kMistyColorAccent, static_cast<int>(state->colors[2]));
    config->Save();
    RefreshWindowsTheme();
}

LRESULT CALLBACK MistySettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* state = reinterpret_cast<MistySettingsState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_HSCROLL:
            if (state) UpdateSliderValues(state);
            return 0;
        case WM_COMMAND: {
            if (!state) break;
            const int command = LOWORD(wParam);
            int colorIndex = -1;
            if (command == kControlPrimaryColor) colorIndex = 0;
            if (command == kControlSecondaryColor) colorIndex = 1;
            if (command == kControlAccentColor) colorIndex = 2;
            if (colorIndex >= 0) {
                COLORREF customColors[16] = {};
                CHOOSECOLORW chooser = {};
                chooser.lStructSize = sizeof(chooser);
                chooser.hwndOwner = hwnd;
                chooser.rgbResult = state->colors[colorIndex];
                chooser.lpCustColors = customColors;
                chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&chooser)) {
                    state->colors[colorIndex] = chooser.rgbResult;
                    UpdateColorButton(state, colorIndex);
                }
                return 0;
            }
            if (command == IDOK) {
                SaveMistySettings(state);
                DestroyWindow(hwnd);
                return 0;
            }
            if (command == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_NCDESTROY:
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowMistySettingsWindow() {
    HWND existing = FindWindowW(kMistySettingsClass, nullptr);
    if (existing) {
        ShowWindow(existing, SW_SHOWNORMAL);
        SetForegroundWindow(existing);
        return;
    }

    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = MistySettingsWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kMistySettingsClass;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

    auto* state = new MistySettingsState();
    PlatformConfig* config = GetConfig();
    if (config) {
        state->colors[0] = static_cast<COLORREF>(config->GetInt(kMistyColorPrimary, state->colors[0]));
        state->colors[1] = static_cast<COLORREF>(config->GetInt(kMistyColorSecondary, state->colors[1]));
        state->colors[2] = static_cast<COLORREF>(config->GetInt(kMistyColorAccent, state->colors[2]));
    }

    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, kMistySettingsClass, L"迷离模式参数",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 460, 370,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), state);
    if (!window) {
        delete state;
        return;
    }

    CreateWindowW(L"STATIC", L"不透明度", WS_CHILD | WS_VISIBLE, 24, 24, 90, 20, window, nullptr, nullptr, nullptr);
    state->opacity = CreateSlider(window, kControlOpacity, 120, 18, 230, 5, 45,
                                  static_cast<int>(std::lround(config ? config->GetDouble(kMistyColorfulOpacity, MistyDefaults::kColorfulOpacityDefault) * 100.0 : MistyDefaults::kColorfulOpacityDefault * 100.0)));
    state->opacityValue = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 370, 24, 60, 20, window, nullptr, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"毛玻璃强度", WS_CHILD | WS_VISIBLE, 24, 76, 90, 20, window, nullptr, nullptr, nullptr);
    state->blur = CreateSlider(window, kControlBlur, 120, 70, 230, 1, 30,
                               config ? config->GetInt(kMistyWindowBlurRadius, MistyDefaults::kWindowBlurRadiusDefault) : MistyDefaults::kWindowBlurRadiusDefault);
    state->blurValue = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 370, 76, 60, 20, window, nullptr, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"流光范围", WS_CHILD | WS_VISIBLE, 24, 128, 90, 20, window, nullptr, nullptr, nullptr);
    state->spread = CreateSlider(window, kControlSpread, 120, 122, 230, 35, 120,
                                 static_cast<int>(std::lround(config ? config->GetDouble(kMistyColorfulBlurRadius, MistyDefaults::kColorfulBlurRadiusDefault) : MistyDefaults::kColorfulBlurRadiusDefault)));
    state->spreadValue = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 370, 128, 60, 20, window, nullptr, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"流动周期", WS_CHILD | WS_VISIBLE, 24, 180, 90, 20, window, nullptr, nullptr, nullptr);
    state->duration = CreateSlider(window, kControlDuration, 120, 174, 230, 4, 30,
                                   static_cast<int>(std::lround(config ? config->GetDouble(kMistyColorfulAnimationDuration, MistyDefaults::kColorfulAnimationDurationDefault) : MistyDefaults::kColorfulAnimationDurationDefault)));
    state->durationValue = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 370, 180, 60, 20, window, nullptr, nullptr, nullptr);

    const int colorIds[] = {kControlPrimaryColor, kControlSecondaryColor, kControlAccentColor};
    for (int index = 0; index < 3; ++index) {
        state->colorButtons[index] = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                   24 + index * 136, 235, 126, 30, window,
                                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(colorIds[index])),
                                                   GetModuleHandleW(nullptr), nullptr);
        UpdateColorButton(state, index);
    }
    CreateWindowW(L"BUTTON", L"应用", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                  260, 295, 80, 28, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                  350, 295, 80, 28, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);

    UpdateSliderValues(state);
    ShowWindow(window, SW_SHOWNORMAL);
    SetForegroundWindow(window);
}

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
        if (!m_hWnd) {
            Log("Tray UI did not initialize");
        }
    }

    DialogResult ShowConfirmDialog(const std::string& title,
                                   const std::string& message,
                                   const std::string&,
                                   const std::string&) override {
        std::wstring wideTitle(title.begin(), title.end());
        std::wstring wideMessage(message.begin(), message.end());
        int result = MessageBoxW(m_hWnd, wideMessage.c_str(), wideTitle.c_str(),
                                 MB_OKCANCEL | MB_ICONWARNING);
        return result == IDOK ? DialogResult::kConfirm : DialogResult::kCancel;
    }

    void ShowSettingsWindow() override {
        PlatformConfig* config = GetConfig();
        if (!config) return;

        wchar_t message[512] = {};
        swprintf_s(message,
                   L"SovietX 设置会保存到用户配置文件。\n\n"
                   L"消息防撤回：%s\n微信多开：%s\n迷离模式：%s\n\n"
                   L"迷离参数可从托盘菜单即时调整。",
                   config->GetBool(kFeatureAntiRevoke, FeatureDefaults::kAntiRevokeDefault) ? L"开启" : L"关闭",
                   config->GetBool(kFeatureMultiOpen, FeatureDefaults::kMultiOpenDefault) ? L"开启" : L"关闭",
                   config->GetBool(kFeatureMistyMode, FeatureDefaults::kMistyModeDefault) ? L"开启" : L"关闭");
        MessageBoxW(m_hWnd, message, L"SovietX 设置状态", MB_OK | MB_ICONINFORMATION);
    }

    void UpdateMenuItemState(const std::string& key, MenuItemState state) override {
        std::lock_guard<std::mutex> lock(m_menuMutex);
        m_menuStates[key] = state;
    }

    MenuItemState GetMenuItemState(const std::string& key) override {
        std::lock_guard<std::mutex> lock(m_menuMutex);
        auto iterator = m_menuStates.find(key);
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
        Log("Tray UI initialized");

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

        if (threadId != 0) {
            PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        }
        m_thread.join();
    }

    static void AppendFeatureMenuItem(HMENU menu, UINT command, const wchar_t* text,
                                      const char* key, bool defaultValue) {
        PlatformConfig* config = GetConfig();
        bool enabled = config && config->GetBool(key, defaultValue);
        AppendMenuW(menu, MF_STRING | (enabled ? MF_CHECKED : 0), command, text);
    }

    void ToggleFeature(const char* key, bool defaultValue) {
        PlatformConfig* config = GetConfig();
        if (!config) return;
        bool enabled = !config->GetBool(key, defaultValue);
        config->SetBool(key, enabled);
        config->Save();
        if (std::strcmp(key, kFeatureMistyMode) == 0) {
            RefreshWindowsTheme();
        }
        Log("Configuration changed: %s=%d", key, enabled ? 1 : 0);
    }

    void ShowTrayMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        AppendFeatureMenuItem(menu, kMenuAntiRevoke, L"消息防撤回", kFeatureAntiRevoke,
                              FeatureDefaults::kAntiRevokeDefault);
        AppendFeatureMenuItem(menu, kMenuMultiOpen, L"微信多开", kFeatureMultiOpen,
                              FeatureDefaults::kMultiOpenDefault);
        AppendFeatureMenuItem(menu, kMenuMistyMode, L"迷离模式", kFeatureMistyMode,
                              FeatureDefaults::kMistyModeDefault);
        AppendMenuW(menu, MF_STRING, kMenuMistySettings, L"迷离参数...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuSettings, L"查看设置状态");

        POINT point = {};
        GetCursorPos(&point);
        SetForegroundWindow(m_hWnd);
        UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
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
            case kMenuMistyMode:
                ToggleFeature(kFeatureMistyMode, FeatureDefaults::kMistyModeDefault);
                break;
            case kMenuMistySettings:
                ShowMistySettingsWindow();
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
