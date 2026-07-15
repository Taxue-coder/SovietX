/**
 * @file win_main.cpp
 * @brief SovietX Windows DLL lifecycle.
 *
 * DllMain deliberately performs no initialization under the loader lock. The
 * injector loads this DLL and then invokes SovietX_Start in a separate remote
 * thread. This makes failure handling deterministic and allows SovietX_Stop
 * to remove hooks before a process exits.
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/log.h"
#include "../platform_hook.h"
#include "../platform_memory.h"
#include "../platform_process.h"
#include "../platform_ui.h"

#include <windows.h>

#include <atomic>
#include <string>

namespace soviet {

extern void InitWindowsAntiRevoke();
extern void StartWindowsAutoLogin();
extern void InitWindowsTheme();
extern void ApplyMistyThemeToAllWindows();

extern PlatformHook* CreateWindowsHook();
extern PlatformMemory* CreateWindowsMemory();
extern PlatformConfig* CreateWindowsConfig();
extern PlatformProcess* CreateWindowsProcess();
extern PlatformUI* CreateWindowsUI();

namespace {

enum class PluginState : DWORD {
    kStopped = 0,
    kStarting = 1,
    kRunning = 2,
    kStopping = 3,
    kFailed = 4,
};

std::atomic<PluginState> g_state{PluginState::kStopped};

std::string GetLogPath() {
    char tempPath[MAX_PATH] = {};
    DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH) {
        return "SovietX.log";
    }
    return std::string(tempPath) + "SovietX.log";
}

void ReleasePlatformServices() {
    PlatformUI* ui = GetUI();
    if (ui) {
        delete ui;
        SetUI(nullptr);
    }

    PlatformHook* hook = GetHook();
    if (hook) {
        hook->Uninitialize();
        delete hook;
        SetHook(nullptr);
    }

    PlatformProcess* process = GetProcess();
    if (process) {
        delete process;
        SetProcess(nullptr);
    }

    PlatformMemory* memory = GetMemory();
    if (memory) {
        delete memory;
        SetMemory(nullptr);
    }

    PlatformConfig* config = GetConfig();
    if (config) {
        config->Save();
        delete config;
        SetConfig(nullptr);
    }
}

DWORD StartPlugin() {
    LogInit(GetLogPath());
    Log("=== SovietX v%s (Windows) ===", kVersionString);
    Log("Plugin start requested in PID %lu", GetCurrentProcessId());

    PlatformConfig* config = CreateWindowsConfig();
    if (!config) {
        Log("ERROR: Cannot create configuration service");
        return ERROR_OUTOFMEMORY;
    }
    SetConfig(config);

    PlatformHook* hook = CreateWindowsHook();
    if (!hook) {
        Log("ERROR: Cannot create hook service");
        return ERROR_OUTOFMEMORY;
    }
    SetHook(hook);
    if (hook->Initialize() != HookResult::kSuccess) {
        Log("ERROR: Hook framework initialization failed");
        return ERROR_DLL_INIT_FAILED;
    }

    PlatformMemory* memory = CreateWindowsMemory();
    PlatformProcess* process = CreateWindowsProcess();
    PlatformUI* ui = CreateWindowsUI();
    if (!memory || !process || !ui) {
        delete memory;
        delete process;
        delete ui;
        Log("ERROR: Cannot create one or more platform services");
        return ERROR_OUTOFMEMORY;
    }

    SetMemory(memory);
    SetProcess(process);
    SetUI(ui);

    Log("Host app: %s (file version %s, product version %s)",
        process->GetHostAppPath().c_str(),
        process->GetHostAppVersion().c_str(),
        process->GetHostAppBuildVersion().c_str());

    ui->InitPluginMenu();
    InitWindowsAntiRevoke();
    StartWindowsAutoLogin();
    InitWindowsTheme();
    ApplyMistyThemeToAllWindows();

    Log("=== SovietX initialized ===");
    return ERROR_SUCCESS;
}

} // namespace

extern "C" __declspec(dllexport) DWORD WINAPI SovietX_Start(LPVOID) {
    PluginState expected = PluginState::kStopped;
    if (!g_state.compare_exchange_strong(expected, PluginState::kStarting)) {
        return expected == PluginState::kRunning ? ERROR_SUCCESS : ERROR_BUSY;
    }

    DWORD result = StartPlugin();
    if (result == ERROR_SUCCESS) {
        g_state.store(PluginState::kRunning);
    } else {
        ReleasePlatformServices();
        g_state.store(PluginState::kFailed);
    }
    return result;
}

extern "C" __declspec(dllexport) DWORD WINAPI SovietX_Stop(LPVOID) {
    PluginState expected = PluginState::kRunning;
    if (!g_state.compare_exchange_strong(expected, PluginState::kStopping)) {
        return expected == PluginState::kStopped ? ERROR_SUCCESS : ERROR_BUSY;
    }

    ReleasePlatformServices();
    g_state.store(PluginState::kStopped);
    return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) DWORD WINAPI SovietX_GetStatus(LPVOID) {
    return static_cast<DWORD>(g_state.load());
}

} // namespace soviet

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

#endif // _WIN32
