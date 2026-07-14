/**
 * @file string_utils.h
 * @brief SovietExtension 字符串工具函数
 * 
 * 从 ForwardToSelfPatch.mm 中提取的跨平台字符串校验逻辑。
 * 纯 C++ 实现，不依赖 NSString / ObjC。
 */

#ifndef SOVIET_STRING_UTILS_H
#define SOVIET_STRING_UTILS_H

#include <string>
#include <cstdint>

namespace soviet {

/**
 * 去除字符串首尾空白字符。
 */
std::string TrimString(const std::string& value);

/**
 * 判断字符串是否看起来像一个微信账号 ID。
 * - wxid_xxx 格式
 * - 或 5~128 位字母数字下划线横线
 * - 不包含 @chatroom、空格、换行、尖括号
 */
bool LooksLikeAccountID(const std::string& value);

/**
 * 判断 selfId 是否安全（不会误发给别人）。
 * @param selfId 疑似当前登录账号
 * @param sessionText 当前会话标识
 * @param revokerWxid 撤回人 wxid
 * @param revokerDisplayName 撤回人显示名
 */
bool LooksLikeSafeSelfID(const std::string& selfId,
                          const std::string& sessionText,
                          const std::string& revokerWxid,
                          const std::string& revokerDisplayName);

/**
 * 判断文本是否是内置表情符号（如 [微笑]、[大哭]）。
 */
bool TextIsBuiltinEmoji(const std::string& text);

/**
 * 判断文本是否是无效内容（"暂不支持该内容"等）。
 */
bool TextLooksUseless(const std::string& text);

/**
 * 群聊消息格式为 "wxid_xxx:\n内容"，拆出发送者和正文。
 * @param rawContent 原始内容
 * @param senderOut 输出发送者（可为 nullptr）
 * @return 清洗后的正文
 */
std::string CleanContent(const std::string& rawContent, std::string* senderOut);

/**
 * 根据消息类型返回内容展示文本。
 */
std::string ContentDisplay(uint32_t type, const std::string& cleanContent);

/**
 * 撤回人显示名优先级：displayName > wxid > sender > "***"
 */
std::string RevokerDisplay(const std::string& displayName,
                            const std::string& wxid,
                            const std::string& sender);

/**
 * UTF-8 字符串长度（字符数，非字节数）。
 */
size_t UTF8Length(const std::string& str);

/**
 * 截断 UTF-8 字符串到指定字符数。
 */
std::string UTF8Truncate(const std::string& str, size_t maxChars);

} // namespace soviet

#endif // SOVIET_STRING_UTILS_H
