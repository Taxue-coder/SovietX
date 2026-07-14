/**
 * @file re_helper.cpp
 * @brief SovietExtension 逆向工程辅助工具
 * 
 * 此工具用于辅助逆向 Windows 版微信，提取关键信息：
 *   1. PE 头信息（ImageBase、Section 布局）
 *   2. 搜索关键字符串（撤回、多开、退群等）
 *   3. 提取版本信息
 *   4. 搜索特征函数签名
 * 
 * 编译（Windows, MSVC）：
 *   cl /EHsc /O2 re_helper.cpp /Fe:re_helper.exe
 * 
 * 用法：
 *   re_helper.exe <path_to_WeChatWin.dll>
 *   re_helper.exe <path_to_WeChat.exe>
 */

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

// ============================================================
// PE 头分析
// ============================================================

struct PEInfo {
    uintptr_t imageBase;
    DWORD entryPoint;
    DWORD imageSize;
    WORD machine;
    bool is64bit;
    std::string versionString;
    std::string fileDescription;

    struct Section {
        std::string name;
        DWORD virtualAddress;
        DWORD virtualSize;
        DWORD rawSize;
        DWORD characteristics;
    };
    std::vector<Section> sections;
};

static bool ParsePE(const char* filePath, PEInfo& info) {
    FILE* fp = fopen(filePath, "rb");
    if (!fp) {
        printf("[!] Cannot open file: %s\n", filePath);
        return false;
    }

    // 读取 DOS Header
    IMAGE_DOS_HEADER dosHeader;
    fread(&dosHeader, sizeof(dosHeader), 1, fp);

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        printf("[!] Not a valid PE file (bad DOS signature)\n");
        fclose(fp);
        return false;
    }

    // 跳转到 PE Header
    fseek(fp, dosHeader.e_lfanew, SEEK_SET);

    DWORD peSignature;
    fread(&peSignature, sizeof(peSignature), 1, fp);
    if (peSignature != IMAGE_NT_SIGNATURE) {
        printf("[!] Not a valid PE file (bad NT signature)\n");
        fclose(fp);
        return false;
    }

    // 读取 File Header
    IMAGE_FILE_HEADER fileHeader;
    fread(&fileHeader, sizeof(fileHeader), 1, fp);
    info.machine = fileHeader.Machine;
    info.is64bit = (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);

    printf("[+] Machine: %s (0x%04X)\n",
           info.is64bit ? "x64 (AMD64)" : "x86",
           fileHeader.Machine);

    // 读取 Optional Header
    if (info.is64bit) {
        IMAGE_OPTIONAL_HEADER64 optHeader;
        fread(&optHeader, sizeof(optHeader), 1, fp);
        info.imageBase = (uintptr_t)optHeader.ImageBase;
        info.entryPoint = optHeader.AddressOfEntryPoint;
        info.imageSize = optHeader.SizeOfImage;
        printf("[+] ImageBase: 0x%llX\n", (unsigned long long)info.imageBase);
    } else {
        IMAGE_OPTIONAL_HEADER32 optHeader;
        fread(&optHeader, sizeof(optHeader), 1, fp);
        info.imageBase = (uintptr_t)optHeader.ImageBase;
        info.entryPoint = optHeader.AddressOfEntryPoint;
        info.imageSize = optHeader.SizeOfImage;
        printf("[+] ImageBase: 0x%X\n", (unsigned int)info.imageBase);
    }

    printf("[+] Entry Point RVA: 0x%08X\n", info.entryPoint);
    printf("[+] Image Size: 0x%08X (%.1f MB)\n",
           info.imageSize, info.imageSize / (1024.0 * 1024.0));

    // 读取 Section Headers
    printf("[+] Sections: %d\n", fileHeader.NumberOfSections);
    for (int i = 0; i < fileHeader.NumberOfSections; i++) {
        IMAGE_SECTION_HEADER section;
        fread(&section, sizeof(section), 1, fp);

        PEInfo::Section sec;
        sec.name = std::string((char*)section.Name,
                               strnlen((char*)section.Name, 8));
        sec.virtualAddress = section.VirtualAddress;
        sec.virtualSize = section.Misc.VirtualSize;
        sec.rawSize = section.SizeOfRawData;
        sec.characteristics = section.Characteristics;

        printf("    %-8s VA=0x%08X VS=0x%08X RS=0x%08X %s%s%s\n",
               sec.name.c_str(),
               sec.virtualAddress, sec.virtualSize, sec.rawSize,
               (sec.characteristics & 0x20000000) ? "EXEC " : "",
               (sec.characteristics & 0x40000000) ? "READ " : "",
               (sec.characteristics & 0x80000000) ? "WRITE" : "");

        info.sections.push_back(sec);
    }

    fclose(fp);
    return true;
}

// ============================================================
// 字符串搜索
// ============================================================

struct StringMatch {
    size_t fileOffset;
    uintptr_t rva;       // 相对虚拟地址
    uintptr_t va;        // 虚拟地址 (imageBase + rva)
    std::string context; // 匹配字符串上下文
};

static void SearchStrings(const char* filePath, const PEInfo& info,
                          const std::vector<std::string>& keywords) {
    FILE* fp = fopen(filePath, "rb");
    if (!fp) return;

    // 读取整个文件到内存
    fseek(fp, 0, SEEK_END);
    size_t fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<uint8_t> data(fileSize);
    fread(data.data(), 1, fileSize, fp);
    fclose(fp);

    printf("\n=== String Search Results ===\n\n");

    // 找到 .text section 的映射信息（用于计算 RVA）
    // 简化处理：使用第一个可执行 section
    DWORD textVA = 0, textRaw = 0, textRawSize = 0;
    for (auto& sec : info.sections) {
        if (sec.characteristics & 0x20000000) { // EXEC
            textVA = sec.virtualAddress;
            textRaw = 0; // 简化：假设 raw offset ≈ VA（实际需要用 section table 转换）
            textRawSize = sec.rawSize;
            break;
        }
    }

    // 搜索 ASCII 字符串
    for (auto& keyword : keywords) {
        printf("--- Searching for: \"%s\" ---\n", keyword.c_str());
        int count = 0;

        for (size_t i = 0; i + keyword.length() <= fileSize; i++) {
            if (memcmp(data.data() + i, keyword.c_str(), keyword.length()) == 0) {
                // 提取上下文（前后各 20 字节）
                size_t ctxStart = (i > 20) ? i - 20 : 0;
                size_t ctxEnd = std::min(i + keyword.length() + 20, fileSize);

                // 简单打印偏移
                printf("  [FOUND] Offset=0x%08zX (%zu)\n", i, i);

                // 打印上下文（可打印字符）
                printf("    Context: ");
                for (size_t j = ctxStart; j < ctxEnd; j++) {
                    char c = (char)data[j];
                    if (c >= 0x20 && c < 0x7F) {
                        putchar(c);
                    } else {
                        putchar('.');
                    }
                }
                printf("\n");

                count++;
                if (count >= 10) {
                    printf("    ... (showing first 10 matches)\n");
                    break;
                }
            }
        }

        if (count == 0) {
            printf("  [NOT FOUND]\n");
        }
        printf("\n");
    }

    // 搜索 UTF-16LE 字符串
    printf("--- UTF-16LE Search ---\n");
    const wchar_t* wKeywords[] = {
        L"RevokeMsg",
        L"DeleteMessage",
        L"WeChat",
        L"chatroom_member",
        L"CreateMutex",
        nullptr
    };

    for (int k = 0; wKeywords[k]; k++) {
        size_t kwLen = wcslen(wKeywords[k]);
        size_t kwBytes = kwLen * 2;
        int count = 0;

        printf("  Searching UTF-16: \"");
        // 打印 ASCII 部分
        for (size_t i = 0; i < kwLen; i++) putchar((char)wKeywords[k][i]);
        printf("\"\n");

        for (size_t i = 0; i + kwBytes <= fileSize; i += 2) {
            if (memcmp(data.data() + i, wKeywords[k], kwBytes) == 0) {
                printf("    [FOUND] Offset=0x%08zX\n", i);
                count++;
                if (count >= 5) break;
            }
        }

        if (count == 0) printf("    [NOT FOUND]\n");
    }
}

// ============================================================
// 版本信息提取
// ============================================================

static void ExtractVersionInfo(const char* filePath) {
    printf("\n=== Version Info ===\n\n");

    DWORD handle;
    DWORD size = GetFileVersionInfoSizeA(filePath, &handle);
    if (size == 0) {
        printf("[!] GetFileVersionInfoSize failed\n");
        return;
    }

    std::vector<BYTE> buffer(size);
    if (!GetFileVersionInfoA(filePath, handle, size, buffer.data())) {
        printf("[!] GetFileVersionInfo failed\n");
        return;
    }

    // 提取固定版本信息
    VS_FIXEDFILEINFO* fixedInfo;
    UINT fixedLen;
    if (VerQueryValueA(buffer.data(), "\\", (LPVOID*)&fixedInfo, &fixedLen)) {
        printf("File Version:    %d.%d.%d.%d\n",
               HIWORD(fixedInfo->dwFileVersionMS),
               LOWORD(fixedInfo->dwFileVersionMS),
               HIWORD(fixedInfo->dwFileVersionLS),
               LOWORD(fixedInfo->dwFileVersionLS));
        printf("Product Version: %d.%d.%d.%d\n",
               HIWORD(fixedInfo->dwProductVersionMS),
               LOWORD(fixedInfo->dwProductVersionMS),
               HIWORD(fixedInfo->dwProductVersionLS),
               LOWORD(fixedInfo->dwProductVersionLS));
    }

    // 提取字符串版本信息
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    };

    LANGANDCODEPAGE* translate;
    UINT translateLen;
    if (VerQueryValueA(buffer.data(), "\\VarFileInfo\\Translation",
                       (LPVOID*)&translate, &translateLen)) {
        char query[256];
        snprintf(query, sizeof(query),
                 "\\StringFileInfo\\%04x%04x\\FileVersion",
                 translate->wLanguage, translate->wCodePage);

        char* value;
        UINT valueLen;
        if (VerQueryValueA(buffer.data(), query, (LPVOID*)&value, &valueLen)) {
            printf("FileVersion (str): %s\n", value);
        }

        snprintf(query, sizeof(query),
                 "\\StringFileInfo\\%04x%04x\\ProductVersion",
                 translate->wLanguage, translate->wCodePage);
        if (VerQueryValueA(buffer.data(), query, (LPVOID*)&value, &valueLen)) {
            printf("ProductVersion (str): %s\n", value);
        }

        snprintf(query, sizeof(query),
                 "\\StringFileInfo\\%04x%04x\\FileDescription",
                 translate->wLanguage, translate->wCodePage);
        if (VerQueryValueA(buffer.data(), query, (LPVOID*)&value, &valueLen)) {
            printf("FileDescription: %s\n", value);
        }
    }
}

// ============================================================
// 导出函数列表
// ============================================================

static void ListExports(const char* filePath) {
    printf("\n=== Export Table ===\n\n");

    HMODULE hModule = LoadLibraryExA(filePath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hModule) {
        printf("[!] LoadLibraryExA failed (err=%lu)\n", GetLastError());
        return;
    }

    // 获取导出表
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)hModule + dosHeader->e_lfanew);

    IMAGE_DATA_DIRECTORY exportDir;
    if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        exportDir = ((IMAGE_NT_HEADERS64*)ntHeaders)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        exportDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    }

    if (exportDir.VirtualAddress == 0) {
        printf("[!] No export table found\n");
        FreeLibrary(hModule);
        return;
    }

    IMAGE_EXPORT_DIRECTORY* exportTable =
        (IMAGE_EXPORT_DIRECTORY*)((BYTE*)hModule + exportDir.VirtualAddress);

    DWORD* nameRVAs = (DWORD*)((BYTE*)hModule + exportTable->AddressOfNames);

    printf("Exported functions: %lu\n\n", exportTable->NumberOfNames);

    for (DWORD i = 0; i < exportTable->NumberOfNames && i < 100; i++) {
        char* name = (char*)((BYTE*)hModule + nameRVAs[i]);
        printf("  [%3lu] %s\n", i, name);
    }

    if (exportTable->NumberOfNames > 100) {
        printf("  ... and %lu more\n", exportTable->NumberOfNames - 100);
    }

    FreeLibrary(hModule);
}

// ============================================================
// 主入口
// ============================================================

int main(int argc, char* argv[]) {
    printf("SovietExtension Reverse Engineering Helper\n");
    printf("==========================================\n\n");

    if (argc < 2) {
        printf("Usage: %s <path_to_dll_or_exe>\n\n", argv[0]);
        printf("Examples:\n");
        printf("  %s \"C:\\Program Files\\Tencent\\WeChat\\WeChatWin.dll\"\n", argv[0]);
        printf("  %s \"C:\\Program Files\\Tencent\\WeChat\\WeChat.exe\"\n", argv[0]);
        return 1;
    }

    const char* filePath = argv[1];
    printf("[*] Target: %s\n\n", filePath);

    // 1. 版本信息
    ExtractVersionInfo(filePath);

    // 2. PE 头分析
    printf("\n=== PE Header Analysis ===\n\n");
    PEInfo info;
    if (!ParsePE(filePath, info)) {
        return 1;
    }

    // 3. 导出函数
    ListExports(filePath);

    // 4. 关键字符串搜索
    std::vector<std::string> keywords = {
        // 撤回相关
        "RevokeMsg", "revokemsg", "revoke",
        "DeleteMessage", "DeleteMessages",
        "AddRevokeMsg", "SysCmdAction",

        // 多开相关
        "WeChat_App_Instance", "_WeChat",
        "CreateMutex", "Mutex",

        // 退群相关
        "chatroom_member", "ExitChatRoom", "QuitChatRoom",
        "DeleteMember",

        // 消息发送
        "SendMsg", "sendMsg", "TextMessage",
        "cgi-bin", "micromsg",

        // 更新相关
        "AutoUpdate", "CheckUpdate", "WeChatUpdate",

        // URL/浏览器
        "ShellExecute", "openUrl", "WebView",

        // 登录相关
        "AutoAuth", "LoginBtn", "EnterWeChat",
    };

    SearchStrings(filePath, info, keywords);

    printf("\n=== Summary ===\n\n");
    printf("Next steps:\n");
    printf("1. Open the DLL in IDA Pro / Ghidra\n");
    printf("2. Use the offsets found above to navigate to key functions\n");
    printf("3. Analyze MessageWrap structure layout in memory\n");
    printf("4. Fill in the WeChatProfile struct in win_antirevoke.cpp\n");
    printf("5. Set hookMode based on whether inline hook or JMP patch is needed\n");

    return 0;
}

#else
// 非 Windows 平台的 stub
#include <cstdio>
int main() {
    printf("This tool only runs on Windows.\n");
    return 1;
}
#endif // _WIN32
