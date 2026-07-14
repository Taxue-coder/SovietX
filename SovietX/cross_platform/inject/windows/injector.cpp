/**
 * @file injector.cpp
 * @brief SovietX Windows DLL 注入器
 * 
 * 使用 CreateRemoteThread + LoadLibraryA 将 SovietX.dll 注入到微信进程。
 * 
 * 用法：
 *   SovietInjector.exe [WeChat_PID]
 *   或自动查找微信进程：SovietInjector.exe --auto
 * 
 * 编译：
 *   cl /EHsc /O2 injector.cpp /Fe:SovietInjector.exe
 *   (需要 Windows SDK)
 */

#ifdef _WIN32

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

// ============================================================
// 常量
// ============================================================

static const char* kDllName = "SovietX.dll";

// ============================================================
// 工具函数
// ============================================================

/**
 * 获取当前可执行文件所在目录。
 */
static std::string GetExeDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    if (pos != std::string::npos) {
        return s.substr(0, pos);
    }
    return ".";
}

/**
 * 拼接 DLL 完整路径。
 */
static std::string GetDllFullPath() {
    std::string dir = GetExeDirectory();
    return dir + "\\" + kDllName;
}

/**
 * 通过进程名查找 PID（不区分大小写）。
 */
static DWORD FindProcessByName(const char* name) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32First(hSnap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return pid;
}

/**
 * 启用 SeDebugPrivilege（需要管理员权限）。
 * 允许注入到其他进程。
 */
static bool EnableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        printf("[!] OpenProcessToken failed (err=%lu)\n", GetLastError());
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(hToken);

    if (!ok || err != ERROR_SUCCESS) {
        printf("[!] AdjustTokenPrivileges failed (err=%lu). Run as Administrator.\n", err);
        return false;
    }

    return true;
}

// ============================================================
// 注入核心
// ============================================================

/**
 * 将 DLL 注入到目标进程。
 * 
 * 步骤：
 * 1. OpenProcess 获取目标进程句柄
 * 2. VirtualAllocEx 在目标进程分配内存
 * 3. WriteProcessMemory 写入 DLL 路径字符串
 * 4. CreateRemoteThread 调用 LoadLibraryA 加载 DLL
 * 5. 等待线程完成
 * 
 * @param pid 目标进程 PID
 * @param dllPath DLL 完整路径
 * @return 是否成功
 */
static bool InjectDLL(DWORD pid, const std::string& dllPath) {
    printf("[*] Target PID: %lu\n", pid);
    printf("[*] DLL path: %s\n", dllPath.c_str());

    // 检查 DLL 是否存在
    DWORD attr = GetFileAttributesA(dllPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("[!] DLL not found: %s\n", dllPath.c_str());
        return false;
    }

    // 1. 打开目标进程
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (!hProcess) {
        printf("[!] OpenProcess failed (err=%lu). Try running as Administrator.\n", GetLastError());
        return false;
    }
    printf("[+] Process opened\n");

    // 2. 在目标进程分配内存（存放 DLL 路径）
    size_t pathLen = dllPath.length() + 1;
    LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, pathLen,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        printf("[!] VirtualAllocEx failed (err=%lu)\n", GetLastError());
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] Remote memory allocated at %p (%zu bytes)\n", remoteMem, pathLen);

    // 3. 写入 DLL 路径
    SIZE_T written;
    if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathLen, &written)) {
        printf("[!] WriteProcessMemory failed (err=%lu)\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] DLL path written (%zu bytes)\n", written);

    // 4. 获取 LoadLibraryA 地址
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        printf("[!] GetModuleHandle kernel32 failed\n");
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    FARPROC loadLib = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!loadLib) {
        printf("[!] GetProcAddress LoadLibraryA failed\n");
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] LoadLibraryA at %p\n", loadLib);

    // 5. 创建远程线程
    HANDLE hThread = CreateRemoteThread(
        hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)loadLib,
        remoteMem, 0, NULL);

    if (!hThread) {
        printf("[!] CreateRemoteThread failed (err=%lu)\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] Remote thread created, waiting for LoadLibrary...\n");

    // 6. 等待线程完成（最多 10 秒）
    DWORD waitResult = WaitForSingleObject(hThread, 10000);
    if (waitResult == WAIT_TIMEOUT) {
        printf("[!] Timeout waiting for LoadLibrary\n");
        TerminateThread(hThread, 1);
    } else if (waitResult == WAIT_OBJECT_0) {
        // 获取 LoadLibrary 返回值（HMODULE，0 表示失败）
        DWORD exitCode;
        GetExitCodeThread(hThread, &exitCode);
        if (exitCode == 0) {
            printf("[!] LoadLibrary returned NULL in target process (DLL load failed)\n");
            printf("    Check DLL dependencies and architecture (x64)\n");
        } else {
            printf("[+] DLL loaded successfully! Module handle: 0x%08X\n", exitCode);
        }
    }

    // 清理
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return (waitResult == WAIT_OBJECT_0);
}

// ============================================================
// 用法与入口
// ============================================================

static void PrintUsage(const char* exe) {
    printf("SovietX DLL Injector for Windows\n");
    printf("=========================================\n\n");
    printf("Usage:\n");
    printf("  %s <PID>           Inject into specific process\n", exe);
    printf("  %s --auto          Auto-find WeChat.exe process\n", exe);
    printf("  %s --multi         Inject into all WeChat.exe instances\n", exe);
    printf("\nRequirements:\n");
    printf("  - Run as Administrator\n");
    printf("  - %s must be in the same directory\n", kDllName);
    printf("  - Target: WeChat.exe (x64)\n");
}

/**
 * 注入到所有 WeChat.exe 实例（多开支持）。
 */
static int InjectAllInstances() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        printf("[!] CreateToolhelp32Snapshot failed\n");
        return 1;
    }

    std::string dllPath = GetDllFullPath();
    int injected = 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, "WeChat.exe") == 0) {
                printf("\n--- Injecting PID %lu ---\n", pe.th32ProcessID);
                if (InjectDLL(pe.th32ProcessID, dllPath)) {
                    injected++;
                }
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    if (injected == 0) {
        printf("\n[!] No WeChat.exe instances found or injection failed\n");
        return 1;
    }

    printf("\n[+] Injected into %d WeChat instance(s)\n", injected);
    return 0;
}

int main(int argc, char* argv[]) {
    printf("SovietX Injector v1.0\n");
    printf("============================\n\n");

    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    // 启用调试权限
    if (!EnableDebugPrivilege()) {
        printf("[!] Failed to enable SeDebugPrivilege. Run as Administrator.\n");
        return 1;
    }
    printf("[+] SeDebugPrivilege enabled\n\n");

    std::string dllPath = GetDllFullPath();
    printf("[*] DLL: %s\n\n", dllPath.c_str());

    if (strcmp(argv[1], "--auto") == 0) {
        DWORD pid = FindProcessByName("WeChat.exe");
        if (pid == 0) {
            printf("[!] WeChat.exe not found. Is WeChat running?\n");
            return 1;
        }
        return InjectDLL(pid, dllPath) ? 0 : 1;
    }

    if (strcmp(argv[1], "--multi") == 0) {
        return InjectAllInstances();
    }

    // 指定 PID
    DWORD pid = (DWORD)atol(argv[1]);
    if (pid == 0) {
        printf("[!] Invalid PID: %s\n", argv[1]);
        return 1;
    }

    return InjectDLL(pid, dllPath) ? 0 : 1;
}

#endif // _WIN32
