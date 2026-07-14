/**
 * @file revoke_parser.h
 * @brief SovietExtension 撤回消息解析公共逻辑
 * 
 * 从 RevokePatch.mm 中提取的跨平台撤回消息处理逻辑。
 * 负责从 MessageWrap 内存中读取撤回消息的关键字段。
 */

#ifndef SOVIET_REVOKE_PARSER_H
#define SOVIET_REVOKE_PARSER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include "message_types.h"

namespace soviet {

/**
 * 从 MessageWrap 内存地址读取指定偏移的 std::string 内容。
 * 使用平台层提供的 SafeReadMemory 读取，不直接解引用。
 * 
 * @param wrapBase MessageWrap 基地址
 * @param offset 字段偏移
 * @param readMemory 平台层提供的内存读取回调
 * @return 读取到的字符串，失败返回空
 */
std::string ReadStdStringFromWrap(uintptr_t wrapBase,
                                   size_t offset,
                                   bool (*readMemory)(uintptr_t addr, void* buf, size_t size));

/**
 * 从 MessageWrap 内存地址读取 uint32_t 值。
 */
bool ReadUInt32FromWrap(uintptr_t wrapBase,
                         size_t offset,
                         uint32_t* outValue,
                         bool (*readMemory)(uintptr_t addr, void* buf, size_t size));

/**
 * 撤回消息解析结果。
 * 从一条撤回系统消息中提取出的结构化信息。
 */
struct RevokeMessageInfo {
    std::string sessionText;      // 当前会话标识
    std::string selfUser;         // 当前登录账号
    std::string senderWxid;       // 消息发送者 wxid
    std::string content;          // 原始消息内容
    uint32_t    originType;       // 原始消息类型
    uint64_t    createTimeMs;     // 毫秒级创建时间
    uint32_t    createTimeSec;    // 秒级创建时间
    bool        valid;            // 解析是否成功
};

/**
 * 从撤回消息的 rawWrap 中解析出结构化信息。
 * 
 * @param rawWrapAddr rawWrap 的内存地址
 * @param layout MessageWrap 布局描述
 * @param readMemory 平台层内存读取回调
 * @return 解析结果
 */
RevokeMessageInfo ParseRevokeMessage(uintptr_t rawWrapAddr,
                                      const MessageWrapLayout& layout,
                                      bool (*readMemory)(uintptr_t addr, void* buf, size_t size));

/**
 * 构造撤回保留提示文本（灰色提示）。
 * 类似微信原生的 "xxx撤回了一条消息"，但保留原消息内容。
 * 
 * @param info 解析后的撤回消息信息
 * @param showContent 是否显示原消息内容
 * @return 提示文本
 */
std::string BuildRevokeRetentionNotice(const RevokeMessageInfo& info, bool showContent);

} // namespace soviet

#endif // SOVIET_REVOKE_PARSER_H
