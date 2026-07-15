/**
 * @file revoke_parser.cpp
 * @brief SovietX 撤回消息解析实现
 */

#include "revoke_parser.h"

#include <cstring>
#include <sstream>

namespace soviet {

/**
 * 从内存中读取一个 libc++ std::string 对象的内容。
 * 
 * libc++ std::string 布局（64 位，24 字节）：
 *   - 短字符串（SSO）：[flags(1)][padding(22)][data(1)] 或直接内联
 *   - 长字符串：[pointer(8)][size(8)][capacity(8)]
 * 
 * 实际上 libc++ 的 SSO 判断在第 1 个字节的最低位：
 *   bit0 == 0 → 短字符串，数据内联在 offset 1 开始
 *   bit0 == 1 → 长字符串，data pointer 在 offset 8/16
 * 
 * 注意：这里使用安全的内存读取，不直接解引用指针。
 */
std::string ReadStdStringFromWrap(uintptr_t wrapBase,
                                   size_t offset,
                                   bool (*readMemory)(uintptr_t addr, void* buf, size_t size)) {
    if (!readMemory || wrapBase == 0) return "";

    uintptr_t strAddr = wrapBase + offset;

    // 读取 24 字节的 std::string 对象
    uint8_t raw[24] = {0};
    if (!readMemory(strAddr, raw, sizeof(raw))) {
        return "";
    }

    // libc++ short string: 第一个字节的最低位 == 0
    // 短字符串数据从 offset 1 开始，长度为 (raw[0] >> 1)
    uint8_t firstByte = raw[0];

    if ((firstByte & 0x01) == 0) {
        // 短字符串（SSO）
        size_t len = firstByte >> 1;
        if (len == 0 || len > 22) return "";
        return std::string(reinterpret_cast<const char*>(raw + 1), len);
    } else {
        // 长字符串：指针在 offset 8 或 16（取决于 endianness）
        // little-endian: size 在 offset 8, pointer 在 offset 16
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, raw + 16, sizeof(uintptr_t));

        size_t strLen = 0;
        memcpy(&strLen, raw + 8, sizeof(size_t));

        if (dataPtr == 0 || strLen == 0 || strLen > 65536) {
            return "";
        }

        // 安全读取字符串内容
        std::string result(strLen, '\0');
        if (!readMemory(dataPtr, &result[0], strLen)) {
            return "";
        }

        return result;
    }
}

bool ReadUInt32FromWrap(uintptr_t wrapBase,
                         size_t offset,
                         uint32_t* outValue,
                         bool (*readMemory)(uintptr_t addr, void* buf, size_t size)) {
    if (!readMemory || !outValue || wrapBase == 0) return false;

    uint32_t val = 0;
    if (!readMemory(wrapBase + offset, &val, sizeof(val))) {
        return false;
    }

    *outValue = val;
    return true;
}

RevokeMessageInfo ParseRevokeMessage(uintptr_t rawWrapAddr,
                                      const MessageWrapLayout& layout,
                                      bool (*readMemory)(uintptr_t addr, void* buf, size_t size)) {
    RevokeMessageInfo info;
    info.valid = false;
    info.originType = 0;
    info.createTimeMs = 0;
    info.createTimeSec = 0;

    if (rawWrapAddr == 0 || !readMemory) {
        return info;
    }

    // 读取各字段
    info.sessionText = ReadStdStringFromWrap(rawWrapAddr, layout.remoteUserOrSessionOffset, readMemory);
    info.selfUser = ReadStdStringFromWrap(rawWrapAddr, layout.selfUserOffset, readMemory);
    info.content = ReadStdStringFromWrap(rawWrapAddr, layout.contentOffset, readMemory);

    // 读取创建时间
    if (layout.createTimeMsOffset > 0) {
        uint64_t ms = 0;
        readMemory(rawWrapAddr + layout.createTimeMsOffset, &ms, sizeof(ms));
        info.createTimeMs = ms;
    }

    if (layout.createTimeSecOffset > 0) {
        uint32_t sec = 0;
        readMemory(rawWrapAddr + layout.createTimeSecOffset, &sec, sizeof(sec));
        info.createTimeSec = sec;
    }

    // 读取消息类型（如果布局中有定义）
    if (layout.messageTypeOffset > 0) {
        ReadUInt32FromWrap(rawWrapAddr, layout.messageTypeOffset, &info.originType, readMemory);
    }

    info.valid = !info.sessionText.empty() || !info.content.empty();
    return info;
}

std::string BuildRevokeRetentionNotice(const RevokeMessageInfo& info, bool showContent) {
    std::ostringstream notice;

    // "[防撤回] 原消息已保留"
    notice << "[\xe9\x98\xb2\xe6\x92\xa4\xe5\x9b\x9e] "
           << "\xe5\x8e\x9f\xe6\xb6\x88\xe6\x81\xaf\xe5\xb7\xb2\xe4\xbf\x9d\xe7\x95\x99";

    if (showContent && !info.content.empty()) {
        notice << ": ";
        // 截断过长内容
        std::string display = info.content;
        if (display.length() > 200) {
            display = display.substr(0, 200) + "...";
        }
        notice << display;
    }

    return notice.str();
}

} // namespace soviet
