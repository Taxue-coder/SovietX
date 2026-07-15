/**
 * @file injector.cpp
 * @brief SovietX Windows DLL injector and lifecycle launcher.
 */

#ifdef _WIN32

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr const wchar_t* kDllName = L"SovietX.dll";
constexpr const wchar_t* kWatcherMutexName = L"Local\\SovietXAutoInjectWatcher";
constexpr DWORD kRemoteWaitMilliseconds = 15000;

std::wstring GetExecutableDirectory() {
    std::vector<wchar_t> path(MAX_PATH);
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                          static_cast<DWORD>(path.size()));
        if (length == 0) return L".";
        if (length < path.size() - 1) {
            std::wstring result(path.data(), length);
            size_t separator = result.find_last_of(L"\\/");
            return separator == std::wstring::npos ? L"." : result.substr(0, separator);
        }
        if (path.size() >= 32768) return L".";
        path.resize(path.size() * 2);
    }
}

std::wstring GetDllFullPath() {
    return GetExecutableDirectory() + L"\\" + kDllName;
}

std::wstring GetFileName(const std::wstring& path) {
    size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}

uintptr_t GetRemoteModuleBase(DWORD pid, const std::wstring& moduleName);

DWORD FindWeixinHostProcess() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"Weixin.exe") == 0 &&
                GetRemoteModuleBase(entry.th32ProcessID, L"Weixin.dll") != 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

bool EnableDebugPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }

    LUID luid = {};
    TOKEN_PRIVILEGES privileges = {};
    bool success = LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid) != FALSE;
    if (success) {
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        success = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr) != FALSE &&
                  GetLastError() == ERROR_SUCCESS;
    }

    CloseHandle(token);
    return success;
}

uintptr_t GetRemoteModuleBase(DWORD pid, const std::wstring& moduleName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    uintptr_t base = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0 ||
                _wcsicmp(GetFileName(entry.szExePath).c_str(), moduleName.c_str()) == 0) {
                base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return base;
}

uintptr_t GetRemoteAddressForLocalProc(DWORD pid, FARPROC localProc) {
    HMODULE localModule = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(localProc), &localModule)) {
        return 0;
    }

    wchar_t modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(localModule, modulePath, MAX_PATH)) return 0;

    uintptr_t remoteModule = GetRemoteModuleBase(pid, GetFileName(modulePath));
    if (remoteModule == 0) return 0;

    uintptr_t offset = reinterpret_cast<uintptr_t>(localProc) -
                       reinterpret_cast<uintptr_t>(localModule);
    return remoteModule + offset;
}

bool RunRemoteFunction(HANDLE process, uintptr_t address, LPVOID parameter, DWORD* exitCode) {
    HANDLE thread = CreateRemoteThread(process, nullptr, 0,
                                       reinterpret_cast<LPTHREAD_START_ROUTINE>(address),
                                       parameter, 0, nullptr);
    if (!thread) {
        wprintf(L"[!] CreateRemoteThread failed (err=%lu)\n", GetLastError());
        return false;
    }

    DWORD wait = WaitForSingleObject(thread, kRemoteWaitMilliseconds);
    if (wait != WAIT_OBJECT_0) {
        wprintf(L"[!] Remote thread did not finish (result=%lu)\n", wait);
        CloseHandle(thread);
        return false;
    }

    DWORD result = ERROR_GEN_FAILURE;
    bool success = GetExitCodeThread(thread, &result) != FALSE;
    CloseHandle(thread);
    if (exitCode) *exitCode = result;
    return success;
}

bool GetExportRva(const std::wstring& dllPath, const char* exportName, uintptr_t* rva) {
    HMODULE module = LoadLibraryExW(dllPath.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!module) {
        wprintf(L"[!] Cannot inspect %ls (err=%lu)\n", dllPath.c_str(), GetLastError());
        return false;
    }

    FARPROC proc = GetProcAddress(module, exportName);
    if (!proc) {
        wprintf(L"[!] DLL does not export %S\n", exportName);
        FreeLibrary(module);
        return false;
    }

    *rva = reinterpret_cast<uintptr_t>(proc) - reinterpret_cast<uintptr_t>(module);
    FreeLibrary(module);
    return true;
}

bool CallPluginExport(HANDLE process, DWORD pid, uintptr_t remoteDllBase,
                      const std::wstring& dllPath, const char* exportName) {
    uintptr_t rva = 0;
    if (!GetExportRva(dllPath, exportName, &rva)) return false;

    DWORD status = ERROR_GEN_FAILURE;
    if (!RunRemoteFunction(process, remoteDllBase + rva, nullptr, &status)) return false;
    if (status != ERROR_SUCCESS) {
        wprintf(L"[!] %S in PID %lu returned %lu\n", exportName, pid, status);
        return false;
    }
    return true;
}

bool InjectDLL(DWORD pid, const std::wstring& dllPath) {
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"[!] DLL not found: %ls\n", dllPath.c_str());
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        wprintf(L"[!] OpenProcess(%lu) failed (err=%lu)\n", pid, GetLastError());
        return false;
    }

    std::wstring dllName = GetFileName(dllPath);
    uintptr_t remoteDllBase = GetRemoteModuleBase(pid, dllName);
    if (remoteDllBase == 0) {
        size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
        LPVOID remotePath = VirtualAllocEx(process, nullptr, bytes,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remotePath) {
            wprintf(L"[!] VirtualAllocEx failed (err=%lu)\n", GetLastError());
            CloseHandle(process);
            return false;
        }

        SIZE_T written = 0;
        bool pathWritten = WriteProcessMemory(process, remotePath, dllPath.c_str(), bytes, &written) != FALSE &&
                           written == bytes;
        FARPROC localLoadLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        uintptr_t remoteLoadLibrary = localLoadLibrary
            ? GetRemoteAddressForLocalProc(pid, localLoadLibrary) : 0;
        DWORD ignoredExitCode = 0;
        bool loaded = pathWritten && remoteLoadLibrary != 0 &&
                      RunRemoteFunction(process, remoteLoadLibrary, remotePath, &ignoredExitCode);
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        if (!loaded) {
            wprintf(L"[!] Remote LoadLibraryW failed\n");
            CloseHandle(process);
            return false;
        }

        for (int attempt = 0; attempt < 20 && remoteDllBase == 0; ++attempt) {
            Sleep(100);
            remoteDllBase = GetRemoteModuleBase(pid, dllName);
        }
        if (remoteDllBase == 0) {
            wprintf(L"[!] DLL loaded but its module base was not found\n");
            CloseHandle(process);
            return false;
        }
    }

    bool started = CallPluginExport(process, pid, remoteDllBase, dllPath, "SovietX_Start");
    CloseHandle(process);
    if (started) {
        wprintf(L"[+] SovietX started in PID %lu\n", pid);
    }
    return started;
}

bool StopDLL(DWORD pid, const std::wstring& dllPath) {
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        wprintf(L"[!] OpenProcess(%lu) failed (err=%lu)\n", pid, GetLastError());
        return false;
    }

    uintptr_t remoteDllBase = GetRemoteModuleBase(pid, GetFileName(dllPath));
    if (remoteDllBase == 0) {
        wprintf(L"[!] SovietX is not loaded in PID %lu\n", pid);
        CloseHandle(process);
        return false;
    }

    bool stopped = CallPluginExport(process, pid, remoteDllBase, dllPath, "SovietX_Stop");
    CloseHandle(process);
    if (stopped) {
        wprintf(L"[+] SovietX stopped in PID %lu (DLL remains loaded until process exit)\n", pid);
    }
    return stopped;
}

void PrintUsage(const wchar_t* executable) {
    wprintf(L"SovietX Injector\n\n");
    wprintf(L"Usage:\n");
    wprintf(L"  %ls <PID>             Load and start in a specific process\n", executable);
    wprintf(L"  %ls --auto            Load and start in the first Weixin.exe process\n", executable);
    wprintf(L"  %ls --multi           Load and start in all Weixin.exe processes\n", executable);
    wprintf(L"  %ls --watch           Watch and inject every new Weixin host once\n", executable);
    wprintf(L"  %ls --status          Show the detected Weixin host process\n", executable);
    wprintf(L"  %ls --stop <PID>      Stop hooks and UI in a specific process\n", executable);
    wprintf(L"  %ls --stop --auto     Stop hooks and UI in the first Weixin.exe process\n", executable);
}

int InjectAllInstances(const std::wstring& dllPath) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 1;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    int injected = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"Weixin.exe") == 0 &&
                GetRemoteModuleBase(entry.th32ProcessID, L"Weixin.dll") != 0 &&
                InjectDLL(entry.th32ProcessID, dllPath)) {
                ++injected;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return injected > 0 ? 0 : 1;
}

int WatchWeixinHosts(const std::wstring& dllPath) {
    HANDLE watcherMutex = CreateMutexW(nullptr, TRUE, kWatcherMutexName);
    if (!watcherMutex) {
        wprintf(L"[!] Cannot create watcher mutex (err=%lu)\n", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(watcherMutex);
        return 0;
    }

    std::set<DWORD> injectedPids;
    const std::wstring dllName = GetFileName(dllPath);

    wprintf(L"[*] SovietX watcher started\n");
    for (;;) {
        std::set<DWORD> currentPids;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry = {};
            entry.dwSize = sizeof(entry);
            if (Process32FirstW(snapshot, &entry)) {
                do {
                    if (_wcsicmp(entry.szExeFile, L"Weixin.exe") != 0 ||
                        GetRemoteModuleBase(entry.th32ProcessID, L"Weixin.dll") == 0) {
                        continue;
                    }

                    currentPids.insert(entry.th32ProcessID);
                    if (injectedPids.insert(entry.th32ProcessID).second &&
                        GetRemoteModuleBase(entry.th32ProcessID, dllName) == 0) {
                        wprintf(L"[*] New Weixin host PID %lu detected\n", entry.th32ProcessID);
                        InjectDLL(entry.th32ProcessID, dllPath);
                    }
                } while (Process32NextW(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }

        injectedPids.swap(currentPids);
        Sleep(1000);
    }
}

bool ParsePid(const wchar_t* text, DWORD* pid) {
    wchar_t* end = nullptr;
    unsigned long value = wcstoul(text, &end, 10);
    if (!text[0] || !end || *end != L'\0' || value == 0) return false;
    *pid = static_cast<DWORD>(value);
    return true;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    if (!EnableDebugPrivilege()) {
        wprintf(L"[*] SeDebugPrivilege unavailable; continuing with current process permissions\n");
    }

    const std::wstring dllPath = GetDllFullPath();
    if (_wcsicmp(argv[1], L"--watch") == 0) {
        return WatchWeixinHosts(dllPath);
    }

    if (_wcsicmp(argv[1], L"--status") == 0) {
        DWORD hostPid = FindWeixinHostProcess();
        if (hostPid == 0) {
            wprintf(L"[!] No Weixin.exe process with Weixin.dll loaded was found\n");
            return 1;
        }
        wprintf(L"[+] Weixin host PID: %lu\n", hostPid);
        return 0;
    }

    if (_wcsicmp(argv[1], L"--multi") == 0) {
        return InjectAllInstances(dllPath);
    }

    bool stop = _wcsicmp(argv[1], L"--stop") == 0;
    const wchar_t* pidArgument = stop ? (argc >= 3 ? argv[2] : nullptr) : argv[1];
    if (!pidArgument) {
        PrintUsage(argv[0]);
        return 1;
    }

    DWORD pid = 0;
    if (_wcsicmp(pidArgument, L"--auto") == 0) {
        pid = FindWeixinHostProcess();
    } else if (!ParsePid(pidArgument, &pid)) {
        wprintf(L"[!] Invalid PID: %ls\n", pidArgument);
        return 1;
    }

    if (pid == 0) {
        wprintf(L"[!] Weixin.exe was not found\n");
        return 1;
    }
    return (stop ? StopDLL(pid, dllPath) : InjectDLL(pid, dllPath)) ? 0 : 1;
}

#endif // _WIN32
