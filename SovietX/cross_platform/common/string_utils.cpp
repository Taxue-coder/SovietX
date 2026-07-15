/**
 * @file string_utils.cpp
 * @brief SovietX 字符串工具函数实现
 * 
 * 从 ForwardToSelfPatch.mm 提取并改写为纯 C++ 实现。
 */

#include "string_utils.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace soviet {

std::string TrimString(const std::string& value) {
    if (value.empty()) return "";

    auto start = value.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";

    auto end = value.find_last_not_of(" \t\n\r\f\v");
    return value.substr(start, end - start + 1);
}

bool LooksLikeAccountID(const std::string& value) {
    std::string text = TrimString(value);
    if (text.empty() || text.length() > 128) {
        return false;
    }

    // 不能包含群标识、换行、空格、尖括号
    if (text.find("@chatroom") != std::string::npos ||
        text.find('\n') != std::string::npos ||
        text.find(' ') != std::string::npos ||
        text.find('<') != std::string::npos ||
        text.find('>') != std::string::npos) {
        return false;
    }

    // wxid_ 前缀
    if (text.substr(0, 5) == "wxid_") {
        return true;
    }

    // 兼容老微信号 / 自定义微信号
    static const std::regex kAccountPattern("^[A-Za-z0-9_\\-]{5,128}$");
    return std::regex_match(text, kAccountPattern);
}

bool LooksLikeSafeSelfID(const std::string& selfId,
                          const std::string& sessionText,
                          const std::string& revokerWxid,
                          const std::string& revokerDisplayName) {
    std::string value = TrimString(selfId);
    std::string session = TrimString(sessionText);
    std::string revoker = TrimString(revokerWxid);

    if (!LooksLikeAccountID(value)) {
        return false;
    }

    // 不能是群
    if (value.find("@chatroom") != std::string::npos) {
        return false;
    }

    // 私聊场景：如果目标等于当前会话，极可能是误把对方当自己
    if (!session.empty() && value == session) {
        return false;
    }

    // 如果 revoker 不是"你"，但 selfId 等于 revoker，高度可疑
    bool displayMeansMe = (revokerDisplayName == "\xe4\xbd\xa0"); // "你"
    if (!revoker.empty() && value == revoker && !displayMeansMe) {
        return false;
    }

    return true;
}

bool TextIsBuiltinEmoji(const std::string& text) {
    std::string value = TrimString(text);
    if (value.length() < 3 || value.length() > 32) {
        return false;
    }
    return value.front() == '[' && value.back() == ']' &&
           value.find('\n') == std::string::npos;
}

bool TextLooksUseless(const std::string& text) {
    if (text.empty()) return false;
    return text.find("\xe6\x9a\x82\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81") != std::string::npos || // "暂不支持该内容"
           text.find("\xe8\xaf\xb7\xe5\x9c\xa8\xe6\x89\x8b\xe6\x9c\xba") != std::string::npos;    // "请在手机上查看"
}

std::string CleanContent(const std::string& rawContent, std::string* senderOut) {
    if (senderOut) *senderOut = "";
    if (rawContent.empty()) return "";

    std::string text = TrimString(rawContent);

    // 群聊消息格式： "wxid_xxx:\n内容"
    auto pos = text.find(":\n");
    if (pos != std::string::npos && pos > 0) {
        std::string prefix = TrimString(text.substr(0, pos));
        std::string body = TrimString(text.substr(pos + 2));
        if (!prefix.empty() && senderOut) *senderOut = prefix;
        if (!body.empty()) return body;
    }

    return text;
}

std::string ContentDisplay(uint32_t type, const std::string& cleanContent) {
    switch (type) {
        case 1:
            return (TextIsBuiltinEmoji(cleanContent) && !cleanContent.empty())
                ? cleanContent
                : (!cleanContent.empty() ? cleanContent : "\xef\xbc\x88\xe7\xa9\xba\xef\xbc\x89"); // "（空）"
        case 3:  return "[\xe5\x9b\xbe\xe7\x89\x87]";        // "[图片]"
        case 34: return "[\xe8\xaf\xad\xe9\x9f\xb3]";        // "[语音]"
        case 43: return "[\xe8\xa7\x86\xe9\xa2\x91]";        // "[视频]"
        case 47: return "[\xe8\xa1\xa8\xe6\x83\x85\xe5\x8c\x85]"; // "[表情包]"
        case 48: return "[\xe4\xbd\x8d\xe7\xbd\xae]";        // "[位置]"
        case 49: return "[\xe6\x96\x87\xe4\xbb\xb6/\xe9\x93\xbe\xe6\x8e\xa5/\xe5\x8d\xa1\xe7\x89\x87]"; // "[文件/链接/卡片]"
        default: return "[" + std::to_string(type) + "]";
    }
}

std::string RevokerDisplay(const std::string& displayName,
                            const std::string& wxid,
                            const std::string& sender) {
    if (!displayName.empty()) return displayName;
    if (!wxid.empty()) return wxid;
    if (!sender.empty()) return sender;
    return "***";
}

size_t UTF8Length(const std::string& str) {
    size_t len = 0;
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1; // invalid byte, skip
        len++;
    }
    return len;
}

std::string UTF8Truncate(const std::string& str, size_t maxChars) {
    size_t bytePos = 0;
    size_t charCount = 0;

    for (size_t i = 0; i < str.size() && charCount < maxChars; ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t charBytes;
        if (c < 0x80) charBytes = 1;
        else if ((c & 0xE0) == 0xC0) charBytes = 2;
        else if ((c & 0xF0) == 0xE0) charBytes = 3;
        else if ((c & 0xF8) == 0xF0) charBytes = 4;
        else charBytes = 1;

        if (i + charBytes > str.size()) break;
        i += charBytes;
        bytePos = i;
        charCount++;
    }

    return str.substr(0, bytePos);
}

} // namespace soviet
