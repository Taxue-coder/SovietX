/**
 * @file win_main.cpp
 * @brief SovietX Windows DLL 入口点 (DllMain)
 * 
 * 当 DLL 通过注入器加载到微信进程时，DllMain 被调用。
 * 在这里初始化所有模块。
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"
#include "../platform_hook.h"
#include "../platform_memory.h"
#include "../platform_ui.h"
#include "../platform_process.h"

#include <windows.h>
#include <process.h>
#include <string>

namespace soviet {

// 前向声明各模块初始化函数
extern void InitWindowsAntiRevoke();
extern void StartWindowsAutoLogin();
extern void InitWindowsTheme();
extern void ApplyMistyThemeToAllWindows();

// 外部声明的 Windows 平台实现类
extern PlatformHook* CreateWindowsHook();
extern PlatformMemory* CreateWindowsMemory();
extern PlatformConfig* CreateWindowsConfig();
extern PlatformProcess* CreateWindowsProcess();
extern PlatformUI* CreateWindowsUI();

} // namespace soviet

/**
 * 初始化线程（在后台线程中执行，避免阻塞 DllMain）。
 * 
 * DllMain 在 loader lock 下执行，不能做太多事（不能加载其他 DLL、
 * 不能创建同步对象等）。因此把主要初始化逻辑放到后台线程。
 */
static unsigned __stdcall InitThread(void* param) {
    using namespace soviet;

    Log("=== SovietX v%s (Windows) ===", kVersionString);
    Log("DLL loaded into process %lu", GetCurrentProcessId());

    // 1. 初始化配置系统
    PlatformConfig* cfg = CreateWindowsConfig();
    SetConfig(cfg);
    Log("Config system initialized");

    // 2. 初始化 Hook 框架
    PlatformHook* hook = CreateWindowsHook();
    SetHook(hook);
    HookResult hr = hook->Initialize();
    if (hr != HookResult::kSuccess) {
        Log("ERROR: Hook framework initialization failed!");
        return 1;
    }
    Log("Hook framework initialized");

    // 3. 初始化内存操作
    PlatformMemory* mem = CreateWindowsMemory();
    SetMemory(mem);
    Log("Memory system initialized");

    // 4. 初始化进程管理
    PlatformProcess* proc = CreateWindowsProcess();
    SetProcess(proc);

    std::string appPath = proc->GetHostAppPath();
    std::string appVer = proc->GetHostAppVersion();
    Log("Host app: %s (version %s)", appPath.c_str(), appVer.c_str());

    // 5. 初始化 UI（延迟，等微信窗口创建完成）
    Sleep(2000);
    PlatformUI* ui = CreateWindowsUI();
    SetUI(ui);
    ui->InitPluginMenu();
    Log("UI system initialized");

    // 6. 初始化各功能模块
    InitWindowsAntiRevoke();
    Log("Anti-revoke module initialized");

    StartWindowsAutoLogin();
    Log("Auto-login module started");

    InitWindowsTheme();
    ApplyMistyThemeToAllWindows();
    Log("Theme module initialized");

    Log("=== SovietX fully initialized ===");
    return 0;
}

/**
 * DLL 入口点。
 * 
 * DLL_PROCESS_ATTACH: DLL 被加载时调用 → 启动初始化线程
 * DLL_PROCESS_DETACH: DLL 被卸载时调用 → 清理资源
 * DLL_THREAD_ATTACH/DETACH: 新线程创建/销毁时调用（忽略）
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            // 禁止 DLL_THREAD_ATTACH/DETACH 通知（优化）
            DisableThreadLibraryCalls(hModule);

            // 初始化日志系统（在 DllMain 中只做最基本的事）
            {
                char tempPath[MAX_PATH];
                GetTempPathA(MAX_PATH, tempPath);
                std::string logPath = std::string(tempPath) + "SovietX.log";
                soviet::LogInit(logPath);
            }

            // 启动后台初始化线程
            uintptr_t threadHandle = _beginthreadex(
                NULL, 0, InitThread, NULL, 0, NULL);
            
            if (threadHandle) {
                CloseHandle((HANDLE)threadHandle);
            }
            break;
        }

        case DLL_PROCESS_DETACH: {
            soviet::Log("DLL_PROCESS_DETACH: Cleaning up...");

            // 清理平台服务
            soviet::PlatformHook* hook = soviet::GetHook();
            if (hook) {
                hook->Uninitialize();
                delete hook;
                soviet::SetHook(nullptr);
            }

            soviet::PlatformMemory* mem = soviet::GetMemory();
            if (mem) {
                delete mem;
                soviet::SetMemory(nullptr);
            }

            soviet::PlatformConfig* cfg = soviet::GetConfig();
            if (cfg) {
                cfg->Save();
                delete cfg;
                soviet::SetConfig(nullptr);
            }

            soviet::PlatformProcess* proc = soviet::GetProcess();
            if (proc) {
                delete proc;
                soviet::SetProcess(nullptr);
            }

            soviet::PlatformUI* ui = soviet::GetUI();
            if (ui) {
                delete ui;
                soviet::SetUI(nullptr);
            }

            soviet::Log("=== SovietX unloaded ===");
            break;
        }
    }

    return TRUE;
}

#endif // _WIN32
