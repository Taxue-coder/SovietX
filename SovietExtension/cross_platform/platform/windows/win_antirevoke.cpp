/**
 * @file win_antirevoke.cpp
 * @brief SovietExtension Windows 防撤回 + 多开 + 退群监控实现
 * 
 * ★ 此文件为框架代码，具体 VM 地址和偏移量需要：
 *    1. 使用 IDA Pro / Ghidra 逆向 Windows 版微信
 *    2. 找到对应的函数地址和 MessageWrap 布局
 *    3. 填入 windows_profiles 数组
 * 
 * 逆向提示：
 * - 撤回消息：搜索 "RevokeMsg" / "撤回" 字符串的交叉引用
 * - 多开互斥：搜索 CreateMutex 调用或 Hook kernel32!CreateMutexW
 * - 退群处理：搜索 "chatroom_member" / "退出群聊" 字符串
 * - 消息删除：搜索 "DeleteMessages" / "DeleteMessage" 字符串
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/feature_flags.h"
#include "../../common/version_profile.h"
#include "../../common/revoke_parser.h"
#include "../../common/forward_notice.h"
#include "../../common/log.h"
#include "../platform_hook.h"
#include "../platform_memory.h"

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>

namespace soviet {

// ============================================================
// Windows 版微信版本适配配置
// ============================================================

/*
 * ★ TODO: 逆向 Windows 版微信后，填入以下地址
 * 
 * Windows 版微信的关键 DLL：
 * - WeChatWin.dll    : 主要业务逻辑
 * - WeChatUI.dll     : UI 相关
 * 
 * 逆向步骤：
 * 1. 用 IDA 打开 WeChatWin.dll
 * 2. 字符串搜索 "RevokeMsg" → 交叉引用找到撤回处理函数
 * 3. 字符串搜索 "DeleteMessage" → 找到消息删除函数
 * 4. 在撤回函数中分析 MessageWrap 的字段偏移
 * 5. 搜索 CreateMutexW 的调用者 → 找到多开检查逻辑
 */
static const WeChatProfile g_windowsProfiles[] = {
    // === 占位示例，逆向后替换 ===
    {
        .displayName = "Windows WeChat 3.9.x (TODO: fill after RE)",
        .platform = "windows",
        .bundleID = "WeChat",
        .shortVersion = "3.9",      // TODO: 确认版本号格式
        .buildVersion = "*",

        .hookMode = RevokeHookMode::kInline,
        .hookPointerVA = 0,          // TODO: 撤回处理函数 VA
        .rawMessageTemplateVA = 0,   // TODO
        .messageWrapFromRawVA = 0,   // TODO
        .messageWrapDestructVA = 0,  // TODO
        .insertPaySysMsgToSessionVA = 0, // TODO
        .revokeDeleteMessagesVA = 0, // TODO

        .revokeOriginCallsiteAfterQueryVA = 0,
        .revokeOriginCallsiteContinueVA = 0,
        .revokeOriginCallsiteZeroBranchVA = 0,
        .revokeOriginCallsiteCheckVA = 0,
        .revokeOriginCallsiteMode = RevokeCallsiteMode::kLegacy410,
        .revokeOriginOutWrapStackOffset = 0,
        .revokeOriginExtObjectStackOffset = 0,

        .multiOpenPreventInstanceVA = 0, // TODO: 多开互斥检查 VA
        .getMainProcessCountVA = 0,

        .groupExitDBApplyVA = 0,
        .groupExitFMessagePreVA = 0,
        .groupExitUpdateSessionCacheVA = 0,
        .groupExitMemberDataListVA = 0,
        .groupExitChatroomInfoOperatorVA = 0,

        .openURLWebViewKindVA = 0,  // TODO
        .sendMsgCGIVA = 0,          // TODO

        .layout = {
            .messageWrapSize = 0,    // TODO: 确认 Windows 版 MessageWrap 大小
            .remoteUserOrSessionOffset = 0, // TODO
            .selfUserOffset = 0,     // TODO
            .createTimeMsOffset = 0, // TODO
            .createTimeSecOffset = 0,// TODO
            .contentOffset = 0,      // TODO
            .messageTypeOffset = 0,  // TODO
            .msgSourceOffset = 0,    // TODO
        },
    },
};

static const size_t g_windowsProfilesCount = sizeof(g_windowsProfiles) / sizeof(g_windowsProfiles[0]);
static const WeChatProfile* g_activeProfile = nullptr;

// ============================================================
// 多开 Patch
// ============================================================

/**
 * 多开方案1：Hook CreateMutexW，拦截特定 Mutex 名。
 * 
 * Windows 版微信通常使用命名 Mutex 防止多开。
 * 常见 Mutex 名模式：
 *   - "_WeChat_App_Instance_Identity_Mutex_Name"
 *   - 或其他包含 "WeChat" 的字符串
 * 
 * Hook 后直接返回一个无效的 HANDLE，让微信认为没有重复实例。
 */
typedef HANDLE(WINAPI* pfnCreateMutexW)(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR);
static pfnCreateMutexW g_origCreateMutexW = nullptr;

static HANDLE WINAPI HookedCreateMutexW(LPSECURITY_ATTRIBUTES attr, BOOL ownInitial, LPCWSTR name) {
    if (name && wcsstr(name, L"WeChat")) {
        Log("[MultiOpen] Intercepted WeChat mutex: %ls", name);
        // 返回一个假的 handle，让微信以为没重复
        return CreateEventW(NULL, FALSE, FALSE, NULL);
    }
    return g_origCreateMutexW(attr, ownInitial, name);
}

/**
 * 安装多开 Patch。
 */
static bool InstallMultiOpenPatch() {
    PlatformHook* hook = GetHook();
    if (!hook) return false;

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) return false;

    void* target = GetProcAddress(hKernel32, "CreateMutexW");
    if (!target) return false;

    HookResult result = hook->InstallInlineHook(target, (void*)HookedCreateMutexW, (void**)&g_origCreateMutexW);
    if (result == HookResult::kSuccess) {
        Log("[MultiOpen] CreateMutexW hooked successfully");
        return true;
    }

    Log("[MultiOpen] CreateMutexW hook failed");
    return false;
}

// ============================================================
// 防撤回 Patch
// ============================================================

/**
 * 撤回消息 Hook 函数（框架）。
 * 
 * TODO: 逆向后填入正确的函数签名和参数。
 * 当前仅作为结构示例。
 */
static void* g_origRevokeHandler = nullptr;

static int64_t HookedRevokeHandler(int64_t a1, int64_t a2) {
    // TODO: 逆向确认后实现
    // 1. 从参数中获取 rawWrap 地址
    // 2. 使用 revoke_parser.h 的 ParseRevokeMessage 解析
    // 3. 如果启用转发，调用 forward_notice.h 的 BuildRevokeForwardNotice
    // 4. 不调用原始的删除消息逻辑（即阻止撤回生效）

    Log("[AntiRevoke] Revoke handler called (TODO: implement after RE)");

    // 暂时调用原函数
    if (g_origRevokeHandler) {
        return ((int64_t(*)(int64_t, int64_t))g_origRevokeHandler)(a1, a2);
    }
    return 0;
}

/**
 * 安装防撤回 Patch。
 */
static bool InstallAntiRevokePatch() {
    if (!g_activeProfile || g_activeProfile->hookPointerVA == 0) {
        Log("[AntiRevoke] No profile or hookPointerVA is 0, skip");
        return false;
    }

    PlatformMemory* mem = GetMemory();
    PlatformHook* hook = GetHook();
    if (!mem || !hook) return false;

    uintptr_t runtimeAddr = mem->ToRuntimeAddress(g_activeProfile->hookPointerVA);
    if (runtimeAddr == 0) {
        Log("[AntiRevoke] Runtime address is 0");
        return false;
    }

    HookResult result = hook->InstallInlineHook(
        reinterpret_cast<void*>(runtimeAddr),
        (void*)HookedRevokeHandler,
        &g_origRevokeHandler);

    if (result == HookResult::kSuccess) {
        Log("[AntiRevoke] Hook installed at %p", reinterpret_cast<void*>(runtimeAddr));
        return true;
    }

    Log("[AntiRevoke] Hook installation failed");
    return false;
}

// ============================================================
// 初始化入口
// ============================================================

/**
 * 初始化 Windows 防撤回模块。
 * 在 DllMain 或插件入口中调用。
 */
void InitWindowsAntiRevoke() {
    PlatformProcess* proc = GetProcess();
    if (!proc) {
        Log("[AntiRevoke] PlatformProcess not available");
        return;
    }

    std::string shortVer = proc->GetHostAppVersion();
    std::string buildVer = proc->GetHostAppBuildVersion();

    Log("[AntiRevoke] Detected WeChat version: %s build: %s",
        shortVer.c_str(), buildVer.c_str());

    g_activeProfile = FindProfile(g_windowsProfiles, g_windowsProfilesCount,
                                   shortVer.c_str(), buildVer.c_str());

    if (!g_activeProfile) {
        Log("[AntiRevoke] No matching profile found for this WeChat version");
        Log("[AntiRevoke] Available profiles: %zu", g_windowsProfilesCount);
        // 尝试用通配符匹配
        g_activeProfile = FindProfile(g_windowsProfiles, g_windowsProfilesCount, shortVer.c_str(), "*");
    }

    if (g_activeProfile) {
        Log("[AntiRevoke] Active profile: %s", g_activeProfile->displayName);
    }

    PlatformConfig* cfg = GetConfig();

    // 多开（始终启用，不需要配置开关）
    InstallMultiOpenPatch();

    // 防撤回
    if (cfg && cfg->GetBool(kFeatureAntiRevoke, FeatureDefaults::kAntiRevokeDefault)) {
        InstallAntiRevokePatch();
    }

    // TODO: 退群监控、系统浏览器等模块
}

} // namespace soviet

#endif // _WIN32
