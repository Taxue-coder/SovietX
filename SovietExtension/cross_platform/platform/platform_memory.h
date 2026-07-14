/**
 * @file platform_memory.h
 * @brief SovietExtension 平台内存操作抽象接口
 * 
 * 封装进程内安全内存读写和内存保护修改。
 * - macOS: vm_read_overwrite / vm_protect / sys_icache_invalidate
 * - Windows: ReadProcessMemory / VirtualProtect / FlushInstructionCache
 */

#ifndef SOVIET_PLATFORM_MEMORY_H
#define SOVIET_PLATFORM_MEMORY_H

#include <cstdint>
#include <cstddef>

namespace soviet {

/**
 * 内存保护标志
 */
enum class MemoryProtection {
    kReadWrite,       // 可读可写
    kReadExecute,     // 可读可执行
    kReadWriteExecute // 可读可写可执行
};

/**
 * 平台内存操作抽象接口。
 */
class PlatformMemory {
public:
    virtual ~PlatformMemory() = default;

    /**
     * 安全读取当前进程内存。
     * @param address 源地址
     * @param buffer 目标缓冲区
     * @param size 读取字节数
     * @return 是否成功读取指定字节数
     */
    virtual bool SafeRead(uintptr_t address, void* buffer, size_t size) = 0;

    /**
     * 安全写入当前进程内存。
     * 自动处理内存保护修改和指令缓存刷新。
     * @param address 目标地址
     * @param data 源数据
     * @param size 写入字节数
     * @return 是否成功
     */
    virtual bool SafeWrite(uintptr_t address, const void* data, size_t size) = 0;

    /**
     * 读取一个指针大小的值。
     */
    virtual bool ReadPointer(uintptr_t address, uintptr_t* value) = 0;

    /**
     * 读取一个 uint32_t 值。
     */
    virtual bool ReadUInt32(uintptr_t address, uint32_t* value) = 0;

    /**
     * 修改内存保护属性。
     * @param address 目标地址
     * @param size 区域大小
     * @param protection 新的保护属性
     * @return 是否成功
     */
    virtual bool SetProtection(uintptr_t address, size_t size, MemoryProtection protection) = 0;

    /**
     * 刷新指令缓存（代码修改后必须调用）。
     * @param address 修改起始地址
     * @param size 修改区域大小
     */
    virtual void FlushInstructionCache(uintptr_t address, size_t size) = 0;

    /**
     * 获取主模块的 ASLR slide。
     * - macOS: wechat.dylib 的 slide
     * - Windows: WeChatWin.dll 的 ImageBase - 默认 ImageBase
     */
    virtual uintptr_t GetModuleSlide() = 0;

    /**
     * 将静态 VM 地址转换为运行时地址。
     * @return GetModuleSlide() + staticVA，staticVA 为 0 时返回 0
     */
    virtual uintptr_t ToRuntimeAddress(uintptr_t staticVA) = 0;
};

/**
 * 获取全局内存操作实例（由平台层注入）。
 */
PlatformMemory* GetMemory();
void SetMemory(PlatformMemory* memory);

} // namespace soviet

#endif // SOVIET_PLATFORM_MEMORY_H
