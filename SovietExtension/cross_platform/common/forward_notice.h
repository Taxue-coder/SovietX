/**
 * @file forward_notice.h
 * @brief SovietExtension 撤回消息转发通知文本构建
 * 
 * 从 ForwardToSelfPatch.mm 中提取的跨平台转发逻辑。
 * 纯 C++ 实现，不依赖 NSString / ObjC。
 */

#ifndef SOVIET_FORWARD_NOTICE_H
#define SOVIET_FORWARD_NOTICE_H

#include <string>
#include <cstdint>

namespace soviet {

/**
 * 构建撤回消息转发通知文本。
 * 
 * 格式示例：
 * --拦截到一条撤回消息--
 * 群名:xxx群聊
 * 撤回人:张三
 * 内容:你好
 * 2026-07-14 10:30:00
 * 
 * @param sessionText 当前会话标识（群聊则含 @chatroom）
 * @param originType 原始消息类型
 * @param originRawContent 原始消息内容
 * @param revokerWxid 撤回人 wxid
 * @param revokerDisplayName 撤回人显示名
 * @param roomName 群名（私聊传空字符串）
 * @return 格式化后的通知文本
 */
std::string BuildRevokeForwardNotice(const std::string& sessionText,
                                      uint32_t originType,
                                      const std::string& originRawContent,
                                      const std::string& revokerWxid,
                                      const std::string& revokerDisplayName,
                                      const std::string& roomName);

/**
 * 检查撤回转发功能是否启用。
 * 平台层需要实现具体的门控检查（NSUserDefaults / 注册表 / 文件哨兵）。
 * 默认实现只检查配置。
 */
bool IsRevokeForwardEnabled();

} // namespace soviet

#endif // SOVIET_FORWARD_NOTICE_H
