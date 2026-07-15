/**
 * @file win_memory.cpp
 * @brief SovietX Windows 内存操作实现
 * 
 * 基于 Win32 API 实现进程内内存读写。
 * ASLR slide 通过 Weixin.dll 的实际加载地址计算。
 */

#ifdef _WIN32

#include "../platform_memory.h"
#include "../../common/log.h"

#include <windows.h>
#include <psapi.h>
#include <fstream>

namespace soviet {

static bool ReadPreferredImageBaseFromDisk(HMODULE module, uintptr_t* imageBase) {
    if (!module || !imageBase) return false;

    char modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameA(module, modulePath, MAX_PATH)) return false;

    std::ifstream input(modulePath, std::ios::binary);
    if (!input) return false;

    IMAGE_DOS_HEADER dos = {};
    input.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!input || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) return false;

    input.seekg(dos.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS64 nt = {};
    input.read(reinterpret_cast<char*>(&nt), sizeof(nt));
    if (!input || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return false;
    }

    *imageBase = static_cast<uintptr_t>(nt.OptionalHeader.ImageBase);
    return *imageBase != 0;
}

class WindowsMemory : public PlatformMemory {
public:
    bool SafeRead(uintptr_t address, void* buffer, size_t size) override {
        if (address == 0 || !buffer || size == 0) return false;

        // 使用 ReadProcessMemory 或直接 try/catch (SEH)
        __try {
            memcpy(buffer, reinterpret_cast<const void*>(address), size);
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool SafeWrite(uintptr_t address, const void* data, size_t size) override {
        if (address == 0 || !data || size == 0) return false;

        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }

        memcpy(reinterpret_cast<void*>(address), data, size);
        VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

        return true;
    }

    bool ReadPointer(uintptr_t address, uintptr_t* value) override {
        return SafeRead(address, value, sizeof(uintptr_t));
    }

    bool ReadUInt32(uintptr_t address, uint32_t* value) override {
        return SafeRead(address, value, sizeof(uint32_t));
    }

    bool SetProtection(uintptr_t address, size_t size, MemoryProtection protection) override {
        DWORD newProtect;
        switch (protection) {
            case MemoryProtection::kReadWrite:
                newProtect = PAGE_READWRITE;
                break;
            case MemoryProtection::kReadExecute:
                newProtect = PAGE_EXECUTE_READ;
                break;
            case MemoryProtection::kReadWriteExecute:
                newProtect = PAGE_EXECUTE_READWRITE;
                break;
            default:
                return false;
        }

        DWORD oldProtect;
        return VirtualProtect(reinterpret_cast<void*>(address), size, newProtect, &oldProtect) != 0;
    }

    void FlushInstructionCache(uintptr_t address, size_t size) override {
        ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
    }

    uintptr_t GetModuleSlide() override {
        if (m_hasCachedSlide) return m_cachedSlide;

        // 获取 Weixin.dll 的实际加载基址
        HMODULE hMod = GetModuleHandleA("Weixin.dll");
        if (!hMod) {
            // 备选：获取主模块
            hMod = GetModuleHandleA(nullptr);
        }

        if (!hMod) {
            Log("Failed to get module handle for ASLR slide calculation");
            return 0;
        }

        // 读取 PE 头获取默认 ImageBase
        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
            return 0;
        }

        uintptr_t actualBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
        uintptr_t preferredBase = 0;
        // Weixin rewrites ImageBase in its mapped PE header. Read the on-disk
        // header so static profile VAs remain valid after ASLR relocation.
        if (!ReadPreferredImageBaseFromDisk(hMod, &preferredBase)) {
            Log("Failed to read preferred image base from the module on disk");
            return 0;
        }

        m_cachedSlide = actualBase - preferredBase;
        m_hasCachedSlide = true;
        Log("Module slide: actual=%p preferred=%p slide=0x%llx",
            reinterpret_cast<void*>(actualBase),
            reinterpret_cast<void*>(preferredBase),
            static_cast<unsigned long long>(m_cachedSlide));
        return m_cachedSlide;

        // PE header: 读取 OptionalHeader.ImageBase
        IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
        IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<uint8_t*>(hMod) + dosHeader->e_lfanew);

        uintptr_t defaultBase = ntHeaders->OptionalHeader.ImageBase;

        m_cachedSlide = actualBase - defaultBase;
        Log("Module slide: actual=%p default=%p slide=0x%llx",
            reinterpret_cast<void*>(actualBase),
            reinterpret_cast<void*>(defaultBase),
            static_cast<unsigned long long>(m_cachedSlide));

        return m_cachedSlide;
    }

    uintptr_t ToRuntimeAddress(uintptr_t staticVA) override {
        if (staticVA == 0) return 0;
        GetModuleSlide();
        if (!m_hasCachedSlide) return 0;
        return GetModuleSlide() + staticVA;
    }

private:
    uintptr_t m_cachedSlide = 0;
    bool m_hasCachedSlide = false;
};

static WindowsMemory g_windowsMemory;

void InitWindowsMemory() {
    SetMemory(&g_windowsMemory);
}

// 工厂函数
PlatformMemory* CreateWindowsMemory() {
    return new WindowsMemory();
}

} // namespace soviet

#endif // _WIN32
