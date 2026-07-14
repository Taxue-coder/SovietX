/**
 * @file message_types.h
 * @brief SovietExtension 微信消息类型常量和 MessageWrap 布局定义
 * 
 * 从 RevokePatch.mm 中提取的跨平台公共数据结构。
 * 不同平台的偏移量由 version_profile.h 中的配置管理。
 */

#ifndef SOVIET_MESSAGE_TYPES_H
#define SOVIET_MESSAGE_TYPES_H

#include <cstdint>
#include <cstddef>

namespace soviet {

// ============================================================
// 微信消息类型常量
// ============================================================

enum class MessageType : uint32_t {
    kText           = 1,     // 文本消息
    kImage          = 3,     // 图片
    kVoice          = 34,    // 语音
    kVideo          = 43,    // 视频
    kEmoji          = 47,    // 表情包
    kLocation       = 48,    // 位置
    kFileLink       = 49,    // 文件/链接/卡片
    kSystemMessage  = 10000, // 系统消息（撤回、红包等）
};

// ============================================================
// MessageWrap 字段布局描述
// ============================================================

/**
 * MessageWrap 内存布局描述符。
 * 不同微信版本、不同平台（macOS/Windows）的偏移量可能不同。
 * 由逆向分析确认后填入 version_profile.h 的适配配置中。
 */
struct MessageWrapLayout {
    size_t messageWrapSize;           // MessageWrap 总大小（字节）
    size_t remoteUserOrSessionOffset; // 对方/当前会话 字符串偏移
    size_t selfUserOffset;            // 当前登录账号 字符串偏移
    size_t createTimeMsOffset;        // 毫秒级创建时间偏移
    size_t createTimeSecOffset;       // 秒级创建时间偏移
    size_t contentOffset;             // 消息内容/XML 字符串偏移
    size_t messageTypeOffset;         // 消息类型 uint32_t 偏移（可选，0 表示未适配）
    size_t msgSourceOffset;           // msgSource XML 偏移（可选，0 表示未适配）
};

// ============================================================
// 撤回消息 Hook 模式
// ============================================================

/**
 * 防撤回 Hook 的三种模式（macOS ARM64 特有）。
 * Windows x64 版本统一使用 Inline 模式。
 */
enum class RevokeHookMode {
    kPointer     = 0,  // 写函数指针（如 off_91EAD20）
    kInline      = 1,  // Patch 局部 callsite，保留原消息内容
    kInlineEntry = 2,  // 直接 inline hook 函数入口
};

/**
 * 撤回 callsite 的版本分支。
 * 不同版本覆盖的指令不同，stub 按此走不同分支。
 */
enum class RevokeCallsiteMode {
    kLegacy410 = 0,  // 4.1.9/4.1.10 风格
    kV411      = 1,  // 4.1.11+ 风格
};

// ============================================================
// 群成员 UI 数据结构（退群监控用）
// ============================================================

/**
 * GetAllMemberDataList 返回的成员 UI 数据。
 * 平台无关的抽象描述，具体内存布局由平台层处理。
 */
struct ChatroomMemberUIData {
    char memberID[64];     // wxid 或 memberID
    char displayName[64];  // 群昵称
    char extraName[64];    // 备注名
    int32_t type;          // 成员类型
    uint8_t noContact;     // 是否非联系人
};

} // namespace soviet

#endif // SOVIET_MESSAGE_TYPES_H
