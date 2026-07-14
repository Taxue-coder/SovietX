/**
 * @file win_process.cpp
 * @brief SovietExtension Windows 进程管理实现
 */

#ifdef _WIN32

#include "../platform_process.h"
#include "../../common/log.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

namespace soviet {

class WindowsProcess : public PlatformProcess {
public:
    void RestartHostApp() override {
        QuitHostApp();
        Sleep(1000);

        std::string path = GetHostAppPath();
        ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        Log("Host app restarted");
    }

    void QuitHostApp() override {
        // 发送 WM_CLOSE 到微信主窗口
        HWND hwnd = FindWindowW(NULL, L"\x5fae\x4fe1"); // "微信"
        if (hwnd) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            Log("WM_CLOSE sent to WeChat window");
        }

        // 备选：taskkill
        ExecuteCommand("taskkill /f /im WeChat.exe >nul 2>&1");
    }

    bool LaunchNewInstance() override {
        std::string path = GetHostAppPath();
        if (path.empty()) return false;

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        char cmdLine[MAX_PATH];
        snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", path.c_str());

        BOOL ok = CreateProcessA(path.c_str(), cmdLine, NULL, NULL, FALSE,
                                  0, NULL, NULL, &si, &pi);
        if (ok) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            Log("New WeChat instance launched");
            return true;
        }

        Log("Failed to launch new WeChat instance: %lu", GetLastError());
        return false;
    }

    std::string ExecuteCommand(const std::string& command) override {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return "";

        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        PROCESS_INFORMATION pi = {};
        char cmdBuf[4096];
        snprintf(cmdBuf, sizeof(cmdBuf), "cmd.exe /c %s", command.c_str());

        BOOL ok = CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(hWritePipe);

        if (!ok) {
            CloseHandle(hReadPipe);
            return "";
        }

        std::string output;
        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            output += buf;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(hReadPipe);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return output;
    }

    std::string GetHostAppPath() override {
        // 默认安装路径，实际应从注册表读取
        // HKLM\SOFTWARE\WOW6432Node\Tencent\WeChat -> InstallPath
        char path[MAX_PATH] = {};

        HKEY hKey;
        DWORD pathSize = sizeof(path);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                          "SOFTWARE\\WOW6432Node\\Tencent\\WeChat",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExA(hKey, "InstallPath", NULL, NULL,
                            reinterpret_cast<LPBYTE>(path), &pathSize);
            RegCloseKey(hKey);
        }

        if (path[0] == '\0') {
            // 备选默认路径
            snprintf(path, sizeof(path),
                     "C:\\Program Files (x86)\\Tencent\\WeChat\\WeChat.exe");
        } else {
            // 追加可执行文件名
            size_t len = strlen(path);
            if (len > 0 && path[len - 1] != '\\') {
                strcat(path, "\\");
            }
            strcat(path, "WeChat.exe");
        }

        return std::string(path);
    }

    std::string GetHostAppVersion() override {
        // 从 WeChat.exe 的 VS_FIXEDFILEINFO 读取
        std::string exePath = GetHostAppPath();
        DWORD dummy;
        DWORD size = GetFileVersionInfoSizeA(exePath.c_str(), &dummy);
        if (size == 0) return "";

        std::vector<uint8_t> data(size);
        if (!GetFileVersionInfoA(exePath.c_str(), 0, size, data.data())) return "";

        VS_FIXEDFILEINFO* info;
        UINT infoSize;
        if (!VerQueryValueA(data.data(), "\\", reinterpret_cast<void**>(&info), &infoSize)) {
            return "";
        }

        char ver[64];
        snprintf(ver, sizeof(ver), "%u.%u.%u",
                 HIWORD(info->dwFileVersionMS),
                 LOWORD(info->dwFileVersionMS),
                 HIWORD(info->dwFileVersionLS));
        return std::string(ver);
    }

    std::string GetHostAppBuildVersion() override {
        // Windows 版微信的 Build 号可能需要从文件版本或资源字符串读取
        // TODO: 逆向确认后实现
        return "";
    }

    bool OpenURLInSystemBrowser(const std::string& url) override {
        if (url.empty()) return false;
        INT_PTR result = reinterpret_cast<INT_PTR>(
            ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL));
        return result > 32;
    }
};

static WindowsProcess g_windowsProcess;

void InitWindowsProcess() {
    SetProcess(&g_windowsProcess);
}

} // namespace soviet

#endif // _WIN32
