/**
 * @file forward_notice.cpp
 * @brief SovietExtension 撤回消息转发通知文本构建实现
 */

#include "forward_notice.h"
#include "string_utils.h"
#include "config.h"
#include "feature_flags.h"

#include <ctime>
#include <sstream>

namespace soviet {

static std::string GetCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

std::string BuildRevokeForwardNotice(const std::string& sessionText,
                                      uint32_t originType,
                                      const std::string& originRawContent,
                                      const std::string& revokerWxid,
                                      const std::string& revokerDisplayName,
                                      const std::string& roomName) {
    std::string sender;
    std::string clean = originRawContent;

    if (originType == 1) {
        clean = CleanContent(originRawContent, &sender);
        if (TextLooksUseless(clean)) {
            clean = "";
        }
    }

    // 截断过长内容
    if (UTF8Length(clean) > 1600) {
        clean = UTF8Truncate(clean, 1600) + "\xe2\x80\xa6"; // "…"
    }

    std::string contentDisplay = ContentDisplay(originType, clean);
    std::string revokerDisplay = RevokerDisplay(revokerDisplayName, revokerWxid, sender);

    std::ostringstream notice;
    notice << "--\xe6\x8b\xa6\xe6\x88\xaa\xe5\x88\xb0\xe4\xb8\x80\xe6\x9d\xa1\xe6\x92\xa4\xe5\x9b\x9e\xe6\xb6\x88\xe6\x81\xaf--\n";
    // "--拦截到一条撤回消息--\n"

    // 群聊显示群名
    if (sessionText.find("@chatroom") != std::string::npos) {
        std::string name = roomName.empty()
            ? "\xe6\x9c\xaa\xe7\x9f\xa5\xe7\xbe\xa4\xe8\x81\x8a" // "未知群聊"
            : roomName;
        notice << "\xe7\xbe\xa4\xe5\x90\x8d:" << name << "\n"; // "群名:"
    }

    notice << "\xe6\x92\xa4\xe5\x9b\x9e\xe4\xba\xba:" // "撤回人:"
           << (revokerDisplay.empty() ? "***" : revokerDisplay) << "\n";

    notice << "\xe5\x86\x85\xe5\xae\xb9:" // "内容:"
           << (contentDisplay.empty() ? "\xef\xbc\x88\xe7\xa9\xba\xef\xbc\x89" : contentDisplay); // "（空）"

    // 非文字消息只做提醒
    if (originType != 1) {
        notice << "\n(\xe9\x9d\x9e\xe6\x96\x87\xe5\xad\x97\xe6\xb6\x88\xe6\x81\xaf\xe5\x8f\xaa\xe5\x81\x9a\xe6\x8f\x90\xe9\x86\x92)";
        // "(非文字消息只做提醒)"
    }

    // 时间戳
    std::string timeStr = GetCurrentTimestamp();
    if (!timeStr.empty()) {
        notice << "\n" << timeStr;
    }

    return notice.str();
}

bool IsRevokeForwardEnabled() {
    PlatformConfig* cfg = GetConfig();
    if (!cfg) return false;
    return cfg->GetBool(kFeatureRevokeForwardToSelf,
                        FeatureDefaults::kRevokeForwardToSelfDefault);
}

} // namespace soviet
