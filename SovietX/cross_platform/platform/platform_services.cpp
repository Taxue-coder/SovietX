/**
 * @file platform_services.cpp
 * @brief SovietX 平台服务全局实例管理
 * 
 * 统一管理所有平台服务的 getter/setter。
 * 平台层在插件加载时调用 SetXxx() 注入具体实现。
 */

#include "platform_hook.h"
#include "platform_memory.h"
#include "platform_ui.h"
#include "platform_process.h"

namespace soviet {

// ============================================================
// PlatformHook
// ============================================================

static PlatformHook* g_hookInstance = nullptr;

PlatformHook* GetHook() {
    return g_hookInstance;
}

void SetHook(PlatformHook* hook) {
    g_hookInstance = hook;
}

// ============================================================
// PlatformMemory
// ============================================================

static PlatformMemory* g_memoryInstance = nullptr;

PlatformMemory* GetMemory() {
    return g_memoryInstance;
}

void SetMemory(PlatformMemory* memory) {
    g_memoryInstance = memory;
}

// ============================================================
// PlatformUI
// ============================================================

static PlatformUI* g_uiInstance = nullptr;

PlatformUI* GetUI() {
    return g_uiInstance;
}

void SetUI(PlatformUI* ui) {
    g_uiInstance = ui;
}

// ============================================================
// PlatformProcess
// ============================================================

static PlatformProcess* g_processInstance = nullptr;

PlatformProcess* GetProcess() {
    return g_processInstance;
}

void SetProcess(PlatformProcess* process) {
    g_processInstance = process;
}

} // namespace soviet
