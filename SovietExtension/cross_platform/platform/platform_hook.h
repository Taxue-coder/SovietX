/**
 * @file platform_hook.h
 * @brief SovietX 平台 Hook 抽象接口
 * 
 * 封装 inline hook / 方法替换等平台相关操作。
 * - macOS: ObjC Runtime method_setImplementation + Mach-O inline patch
 * - Windows: MinHook / Detours inline hook
 */

#ifndef SOVIET_PLATFORM_HOOK_H
#define SOVIET_PLATFORM_HOOK_H

#include <cstdint>
#include <cstddef>

namespace soviet {

/**
 * Hook 操作结果
 */
enum class HookResult {
    kSuccess = 0,
    kTargetNotFound,
    kAlreadyHooked,
    kPermissionDenied,
    kUnknownError,
};

/**
 * 平台 Hook 抽象接口。
 * 平台层（macOS/Windows）各自实现。
 */
class PlatformHook {
public:
    virtual ~PlatformHook() = default;

    /**
     * 安装 inline hook。
     * @param target 目标函数地址
     * @param detour Hook 函数地址
     * @param original 输出：原始函数指针（用于调用原函数）
     * @return Hook 结果
     */
    virtual HookResult InstallInlineHook(void* target, void* detour, void** original) = 0;

    /**
     * 移除 inline hook，恢复原始函数。
     * @param target 目标函数地址
     * @return Hook 结果
     */
    virtual HookResult RemoveHook(void* target) = 0;

    /**
     * 在指定地址写入跳转指令（raw patch）。
     * 用于 macOS 的 Mach-O inline patch 或 Windows 的 JMP patch。
     * @param targetAddr 目标地址（已加 ASLR slide）
     * @param jumpAddr 跳转目标地址
     * @param originalBytes 输出：被覆盖前的原始字节（用于恢复）
     * @param originalBytesSize 原始字节缓冲区大小
     * @return 实际写入的字节数，失败返回 0
     */
    virtual size_t WriteJumpPatch(uintptr_t targetAddr,
                                   uintptr_t jumpAddr,
                                   uint8_t* originalBytes,
                                   size_t originalBytesSize) = 0;

    /**
     * 恢复被 patch 的原始字节。
     */
    virtual HookResult RestoreBytes(uintptr_t targetAddr,
                                     const uint8_t* originalBytes,
                                     size_t size) = 0;

    /**
     * 初始化 Hook 引擎（如 MH_Initialize）。
     * 应在插件加载时调用一次。
     */
    virtual HookResult Initialize() = 0;

    /**
     * 清理所有 Hook（如 MH_Uninitialize）。
     * 应在插件卸载时调用。
     */
    virtual void Uninitialize() = 0;
};

/**
 * 获取全局 Hook 实例（由平台层注入）。
 */
PlatformHook* GetHook();
void SetHook(PlatformHook* hook);

} // namespace soviet

#endif // SOVIET_PLATFORM_HOOK_H
