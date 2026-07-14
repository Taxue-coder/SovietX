/**
 * @file win_hook.cpp
 * @brief SovietExtension Windows Hook 实现
 * 
 * 基于 MinHook 实现 inline hook。
 * x64 跳转使用 14 字节 JMP：FF 25 00 00 00 00 + 8字节绝对地址。
 */

#ifdef _WIN32

#include "../platform_hook.h"
#include "../../common/log.h"

#include <MinHook.h>
#include <windows.h>

namespace soviet {

class WindowsHook : public PlatformHook {
public:
    HookResult Initialize() override {
        MH_STATUS status = MH_Initialize();
        if (status == MH_OK) {
            Log("MinHook initialized successfully");
            return HookResult::kSuccess;
        }
        Log("MinHook initialization failed: %s", MH_StatusToString(status));
        return HookResult::kUnknownError;
    }

    void Uninitialize() override {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("MinHook uninitialized");
    }

    HookResult InstallInlineHook(void* target, void* detour, void** original) override {
        if (!target || !detour) {
            return HookResult::kTargetNotFound;
        }

        MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status != MH_OK) {
            Log("MH_CreateHook failed: %s (target=%p)", MH_StatusToString(status), target);
            return HookResult::kUnknownError;
        }

        status = MH_EnableHook(target);
        if (status != MH_OK) {
            Log("MH_EnableHook failed: %s (target=%p)", MH_StatusToString(status), target);
            return HookResult::kUnknownError;
        }

        Log("Inline hook installed: target=%p detour=%p original=%p", target, detour, original ? *original : nullptr);
        return HookResult::kSuccess;
    }

    HookResult RemoveHook(void* target) override {
        MH_STATUS status = MH_DisableHook(target);
        if (status != MH_OK) {
            return HookResult::kUnknownError;
        }
        status = MH_RemoveHook(target);
        return (status == MH_OK) ? HookResult::kSuccess : HookResult::kUnknownError;
    }

    size_t WriteJumpPatch(uintptr_t targetAddr,
                           uintptr_t jumpAddr,
                           uint8_t* originalBytes,
                           size_t originalBytesSize) override {
        if (targetAddr == 0 || jumpAddr == 0) return 0;

        // x64 absolute JMP: FF 25 00 00 00 00 [8-byte address]
        const size_t patchSize = 14;
        if (originalBytesSize < patchSize) return 0;

        // 保存原始字节
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Log("VirtualProtect failed for WriteJumpPatch at %p", reinterpret_cast<void*>(targetAddr));
            return 0;
        }

        memcpy(originalBytes, reinterpret_cast<void*>(targetAddr), patchSize);

        // 写入 JMP 指令
        uint8_t* dst = reinterpret_cast<uint8_t*>(targetAddr);
        dst[0] = 0xFF;
        dst[1] = 0x25;
        *reinterpret_cast<uint32_t*>(dst + 2) = 0; // RIP-relative offset = 0
        *reinterpret_cast<uintptr_t*>(dst + 6) = jumpAddr;

        VirtualProtect(reinterpret_cast<void*>(targetAddr), patchSize, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), patchSize);

        Log("WriteJumpPatch: %p -> %p (%zu bytes)", reinterpret_cast<void*>(targetAddr), reinterpret_cast<void*>(jumpAddr), patchSize);
        return patchSize;
    }

    HookResult RestoreBytes(uintptr_t targetAddr, const uint8_t* originalBytes, size_t size) override {
        if (targetAddr == 0 || !originalBytes || size == 0) {
            return HookResult::kUnknownError;
        }

        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return HookResult::kPermissionDenied;
        }

        memcpy(reinterpret_cast<void*>(targetAddr), originalBytes, size);
        VirtualProtect(reinterpret_cast<void*>(targetAddr), size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), size);

        return HookResult::kSuccess;
    }
};

static WindowsHook g_windowsHook;

void InitWindowsHook() {
    SetHook(&g_windowsHook);
}

// 工厂函数
PlatformHook* CreateWindowsHook() {
    return new WindowsHook();
}

} // namespace soviet

#endif // _WIN32
