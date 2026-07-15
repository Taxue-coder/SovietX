/**
 * @file win_process.cpp
 * @brief SovietX Windows 进程管理实现
 */

#ifdef _WIN32

#include "../platform_process.h"
#include "../../common/log.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>

namespace soviet {

static std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                   static_cast<int>(value.size()),
                                   nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return L"";

    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                   static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return L"";

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}

static std::wstring CurrentExecutablePath() {
    std::vector<wchar_t> path(MAX_PATH);
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                          static_cast<DWORD>(path.size()));
        if (length == 0) return L"";
        if (length < path.size() - 1) {
            return std::wstring(path.data(), length);
        }
        if (path.size() >= 32768) return L"";
        path.resize(path.size() * 2);
    }
}

static std::string ReadFixedFileVersion(const std::wstring& path, bool productVersion) {
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) return "";

    std::vector<uint8_t> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return "";

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &infoSize) ||
        !info || infoSize < sizeof(*info)) {
        return "";
    }

    DWORD high = productVersion ? info->dwProductVersionMS : info->dwFileVersionMS;
    DWORD low = productVersion ? info->dwProductVersionLS : info->dwFileVersionLS;
    char version[64] = {};
    snprintf(version, sizeof(version), "%u.%u.%u.%u",
             HIWORD(high), LOWORD(high), HIWORD(low), LOWORD(low));
    return version;
}

class WindowsProcess : public PlatformProcess {
public:
    bool LaunchNewInstance() override {
        std::wstring path = Utf8ToWide(GetHostAppPath());
        if (path.empty()) return false;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        std::wstring cmdLine = L"\"" + path + L"\"";
        std::vector<wchar_t> mutableCommand(cmdLine.begin(), cmdLine.end());
        mutableCommand.push_back(L'\0');
        BOOL ok = CreateProcessW(path.c_str(), mutableCommand.data(), NULL, NULL, FALSE,
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
        return WideToUtf8(CurrentExecutablePath());
    }

    std::string GetHostAppVersion() override {
        return ReadFixedFileVersion(CurrentExecutablePath(), false);
    }

    std::string GetHostAppBuildVersion() override {
        return ReadFixedFileVersion(CurrentExecutablePath(), true);
    }

    bool OpenURLInSystemBrowser(const std::string& url) override {
        if (url.empty()) return false;
        std::wstring wideUrl = Utf8ToWide(url);
        if (wideUrl.empty()) return false;
        INT_PTR result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(NULL, L"open", wideUrl.c_str(), NULL, NULL, SW_SHOWNORMAL));
        return result > 32;
    }
};

static WindowsProcess g_windowsProcess;

void InitWindowsProcess() {
    SetProcess(&g_windowsProcess);
}

// 工厂函数
PlatformProcess* CreateWindowsProcess() {
    return new WindowsProcess();
}

} // namespace soviet

#endif // _WIN32
