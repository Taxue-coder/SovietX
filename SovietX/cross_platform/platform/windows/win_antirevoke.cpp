/**
 * @file win_antirevoke.cpp
 * @brief SovietX Windows 防撤回 + 多开 + 退群监控实现
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
#include "../platform_process.h"

#include <windows.h>
#include <intrin.h>
#include <string>
#include <vector>
#include <atomic>
#include <cstring>

namespace soviet {

// ============================================================
// Windows 版微信版本适配配置
// ============================================================

/*
 * ★ TODO: 逆向 Windows 版微信后，填入以下地址
 * 
 * Windows 版微信的关键 DLL：
 * - Weixin.dll       : 主要业务逻辑
 * - WeChatUI.dll     : UI 相关
 * 
 * 逆向步骤：
 * 1. 用 IDA 打开 Weixin.dll
 * 2. 字符串搜索 "RevokeMsg" → 交叉引用找到撤回处理函数
 * 3. 字符串搜索 "DeleteMessage" → 找到消息删除函数
 * 4. 在撤回函数中分析 MessageWrap 的字段偏移
 * 5. 搜索 CreateMutexW 的调用者 → 找到多开检查逻辑
 */
static const uint8_t kWindowsRevokeTypeCheckEntry[] = {
    0x48, 0x83, 0x79, 0x10, 0x09, 0x75, 0x26, 0x48,
    0x83, 0x79, 0x18, 0x10, 0x72, 0x03, 0x48, 0x8B,
};
static const WeChatProfile g_windowsProfiles[] = {
    {
        .displayName = "Windows WeChat 4.1.11.54 x64",
        .platform = "windows",
        .bundleID = "WeChat",
        .shortVersion = "4.1.11.54",
        .buildVersion = "*",

        .moduleName = "Weixin.dll",
        .moduleTimeDateStamp = 0x6A4F29C8,
        .moduleImageSize = 0x0B4D0000,
        .hookExpectedBytes = kWindowsRevokeTypeCheckEntry,
        .hookExpectedBytesSize = sizeof(kWindowsRevokeTypeCheckEntry),

        .hookMode = RevokeHookMode::kInline,
        .hookPointerVA = 0x180A22D30,
        .revokeTypeCheckReturnVA = 0x1822EA0D4,
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
    {
        .displayName = "Windows WeChat 4.1.12.21 x64",
        .platform = "windows",
        .bundleID = "WeChat",
        .shortVersion = "4.1.12.21",
        .buildVersion = "*",

        .moduleName = "Weixin.dll",
        .moduleTimeDateStamp = 0x6A59DC20,
        .moduleImageSize = 0x0B6CB000,
        .hookExpectedBytes = kWindowsRevokeTypeCheckEntry,
        .hookExpectedBytesSize = sizeof(kWindowsRevokeTypeCheckEntry),

        .hookMode = RevokeHookMode::kInline,
        .hookPointerVA = 0x180A39100,
        .revokeTypeCheckReturnVA = 0x18232C414,
        .rawMessageTemplateVA = 0,
        .messageWrapFromRawVA = 0,
        .messageWrapDestructVA = 0,
        .insertPaySysMsgToSessionVA = 0,
        .revokeDeleteMessagesVA = 0,

        .revokeOriginCallsiteAfterQueryVA = 0,
        .revokeOriginCallsiteContinueVA = 0,
        .revokeOriginCallsiteZeroBranchVA = 0,
        .revokeOriginCallsiteCheckVA = 0,
        .revokeOriginCallsiteMode = RevokeCallsiteMode::kLegacy410,
        .revokeOriginOutWrapStackOffset = 0,
        .revokeOriginExtObjectStackOffset = 0,

        .multiOpenPreventInstanceVA = 0,
        .getMainProcessCountVA = 0,

        .groupExitDBApplyVA = 0,
        .groupExitFMessagePreVA = 0,
        .groupExitUpdateSessionCacheVA = 0,
        .groupExitMemberDataListVA = 0,
        .groupExitChatroomInfoOperatorVA = 0,

        .openURLWebViewKindVA = 0,
        .sendMsgCGIVA = 0,

        .layout = {
            .messageWrapSize = 0,
            .remoteUserOrSessionOffset = 0,
            .selfUserOffset = 0,
            .createTimeMsOffset = 0,
            .createTimeSecOffset = 0,
            .contentOffset = 0,
            .messageTypeOffset = 0,
            .msgSourceOffset = 0,
        },
    },
};

static const size_t g_windowsProfilesCount = sizeof(g_windowsProfiles) / sizeof(g_windowsProfiles[0]);
static const WeChatProfile* g_activeProfile = nullptr;

static bool ValidateActiveModule() {
    if (!g_activeProfile || !g_activeProfile->moduleName ||
        g_activeProfile->moduleTimeDateStamp == 0 ||
        g_activeProfile->moduleImageSize == 0) {
        Log("[Profile] Active module fingerprint is incomplete");
        return false;
    }

    HMODULE module = GetModuleHandleA(g_activeProfile->moduleName);
    if (!module) {
        Log("[Profile] Required module not loaded: %s", g_activeProfile->moduleName);
        return false;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        Log("[Profile] Invalid DOS header for %s", g_activeProfile->moduleName);
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.TimeDateStamp != g_activeProfile->moduleTimeDateStamp ||
        nt->OptionalHeader.SizeOfImage != g_activeProfile->moduleImageSize) {
        Log("[Profile] PE fingerprint mismatch for %s", g_activeProfile->moduleName);
        return false;
    }

    return true;
}

static bool ValidateActiveProfile() {
    if (!ValidateActiveModule()) return false;

    if (g_activeProfile->hookPointerVA == 0 ||
        g_activeProfile->revokeTypeCheckReturnVA == 0 ||
        !g_activeProfile->hookExpectedBytes ||
        g_activeProfile->hookExpectedBytesSize == 0) {
        Log("[Profile] %s anti-revoke entry is not verified; feature stays disabled",
            g_activeProfile->shortVersion);
        return false;
    }

    PlatformMemory* memory = GetMemory();
    if (!memory) return false;

    uintptr_t runtimeAddress = memory->ToRuntimeAddress(g_activeProfile->hookPointerVA);
    std::vector<uint8_t> actual(g_activeProfile->hookExpectedBytesSize);
    if (runtimeAddress == 0 ||
        !memory->SafeRead(runtimeAddress, actual.data(), actual.size()) ||
        std::memcmp(actual.data(), g_activeProfile->hookExpectedBytes, actual.size()) != 0) {
        Log("[Profile] Hook entry bytes mismatch; refusing to install anti-revoke hook");
        return false;
    }

    return true;
}

// ============================================================
// 多开 Patch
// ============================================================

/**
 * 多开方案1：Hook CreateMutexW，拦截特定 Mutex 名。
 * 
 * Windows 版微信通常使用命名 Mutex 防止多开。
 * 4.1.11.54 x64 的实际 Mutex 名：
 *   - "_WeChat_Win_User_Identity_Mutex_Name"
 * 
 * Hook 后返回一个新的未命名事件并清除 ERROR_ALREADY_EXISTS，令客户端将其
 * 视为新建的互斥对象。
 */
typedef HANDLE(WINAPI* pfnCreateMutexW)(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR);
static pfnCreateMutexW g_origCreateMutexW = nullptr;

static HANDLE WINAPI HookedCreateMutexW(LPSECURITY_ATTRIBUTES attr, BOOL ownInitial, LPCWSTR name) {
    if (name && wcsstr(name, L"_WeChat_Win_User_Identity_Mutex_Name")) {
        Log("[MultiOpen] Intercepted WeChat mutex: %ls", name);
        HANDLE handle = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (handle) SetLastError(ERROR_SUCCESS);
        return handle;
    }
    return g_origCreateMutexW ? g_origCreateMutexW(attr, ownInitial, name) : NULL;
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
#if 0 // Superseded placeholder hook retained only for historical context.
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
#endif

/**
 * 安装防撤回 Patch。
 */
// Supported Windows 4.1.x profiles use MSVC x64 std::string sysmsg fields.
constexpr size_t kMsvcStringInlineCapacity = 15;
constexpr size_t kMaxSysMsgTypeLength = 64;

struct MsvcStringView {
    union {
        char inlineBuffer[16];
        uintptr_t heapBuffer;
    } storage;
    size_t length;
    size_t capacity;
};

static bool ReadMsvcString(uintptr_t address, std::string* outType) {
    if (address == 0 || !outType) return false;

    PlatformMemory* memory = GetMemory();
    if (!memory) return false;

    MsvcStringView value = {};
    if (!memory->SafeRead(address, &value, sizeof(value)) ||
        value.length == 0 || value.length > kMaxSysMsgTypeLength) {
        return false;
    }

    const char* source = value.capacity <= kMsvcStringInlineCapacity
        ? value.storage.inlineBuffer
        : reinterpret_cast<const char*>(value.storage.heapBuffer);
    if (!source) return false;

    std::string type(value.length, '\0');
    if (source == value.storage.inlineBuffer) {
        std::memcpy(type.data(), source, value.length);
    } else if (!memory->SafeRead(reinterpret_cast<uintptr_t>(source), type.data(), type.size())) {
        return false;
    }

    *outType = type;
    return true;
}

// The helper is shared by several sysmsg types. Restrict the detour to the
// callsite that checks `revokemsg` in the matched profile's dispatcher.
using RevokeTypeCheckFn = int (*)(void*);
static RevokeTypeCheckFn g_origRevokeTypeCheck = nullptr;
static std::atomic<uint64_t> g_blockedRevokeCount{0};

static int HookedRevokeTypeCheck(void* typeString) {
    PlatformMemory* memory = GetMemory();
    const uintptr_t expectedReturn = memory
        ? memory->ToRuntimeAddress(g_activeProfile
            ? g_activeProfile->revokeTypeCheckReturnVA
            : 0)
        : 0;

    std::string type;
    const uintptr_t returnAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (returnAddress == expectedReturn &&
        ReadMsvcString(reinterpret_cast<uintptr_t>(typeString), &type) &&
        type == "revokemsg") {
        const uint64_t count = ++g_blockedRevokeCount;
        Log("[AntiRevoke] Blocked revokemsg at verified dispatcher branch (count=%llu)",
            static_cast<unsigned long long>(count));
        return 0;
    }

    return g_origRevokeTypeCheck ? g_origRevokeTypeCheck(typeString) : 0;
}

static bool InstallAntiRevokePatch() {
    if (!ValidateActiveProfile()) {
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
        reinterpret_cast<void*>(HookedRevokeTypeCheck),
        reinterpret_cast<void**>(&g_origRevokeTypeCheck));

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
        return;
    }

    if (g_activeProfile) {
        Log("[AntiRevoke] Active profile: %s", g_activeProfile->displayName);
    }

    PlatformConfig* cfg = GetConfig();

    if (cfg && cfg->GetBool(kFeatureMultiOpen, FeatureDefaults::kMultiOpenDefault)) {
        if (ValidateActiveModule()) {
            InstallMultiOpenPatch();
        } else {
            Log("[MultiOpen] profile verification failed; feature stays disabled");
        }
    } else {
        Log("[MultiOpen] disabled by configuration");
    }

    // 防撤回
    if (cfg && cfg->GetBool(kFeatureAntiRevoke, FeatureDefaults::kAntiRevokeDefault)) {
        InstallAntiRevokePatch();
    }

    // TODO: 退群监控、系统浏览器等模块
}

} // namespace soviet

#endif // _WIN32
