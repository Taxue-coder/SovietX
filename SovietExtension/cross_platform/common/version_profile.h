/**
 * @file version_profile.h
 * @brief SovietExtension 微信版本适配配置结构体
 * 
 * 每个微信版本对应一组 VM 地址和参数偏移。
 * 平台层在插件加载时匹配当前版本，选择对应的 Profile。
 * 
 * ★ 适配新版微信时，只需在此结构体的数组中新增一条记录。
 * ★ 尽量用最少的 VM 地址实现功能，在注释中记录逆向思路。
 */

#ifndef SOVIET_VERSION_PROFILE_H
#define SOVIET_VERSION_PROFILE_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "message_types.h"

namespace soviet {

/**
 * 微信版本适配配置。
 * 所有 VM 地址均为静态地址（IDA/Ghidra 中看到），
 * 运行时地址 = ASLR Slide + 静态地址。
 */
struct WeChatProfile {
    const char* displayName;     // 对外展示版本，如 "Mac WeChat 4.1.11.23 arm64 / 269079"
    const char* platform;        // 平台标识："macos" 或 "windows"
    const char* bundleID;        // Bundle ID，如 "com.tencent.xinWeChat"
    const char* shortVersion;    // 短版本号，如 "4.1.11"
    const char* buildVersion;    // Build 号，如 "269079"

    // ============================================================
    // 防撤回相关地址
    // ============================================================

    RevokeHookMode hookMode;
    uintptr_t hookPointerVA;                    // 撤回处理函数入口/指针
    uintptr_t rawMessageTemplateVA;             // 撤回 MessageWrap 模板
    uintptr_t messageWrapFromRawVA;             // MessageWrap 构造函数
    uintptr_t messageWrapDestructVA;            // MessageWrap 析构函数
    uintptr_t insertPaySysMsgToSessionVA;       // 系统消息插入函数
    uintptr_t revokeDeleteMessagesVA;           // DeleteMessages 函数入口

    // 撤回 callsite 相关（Inline 模式用）
    uintptr_t revokeOriginCallsiteAfterQueryVA;
    uintptr_t revokeOriginCallsiteContinueVA;
    uintptr_t revokeOriginCallsiteZeroBranchVA;
    uintptr_t revokeOriginCallsiteCheckVA;
    RevokeCallsiteMode revokeOriginCallsiteMode;
    size_t revokeOriginOutWrapStackOffset;
    size_t revokeOriginExtObjectStackOffset;

    // ============================================================
    // 多开相关地址
    // ============================================================

    uintptr_t multiOpenPreventInstanceVA;       // 多开互斥检查函数
    uintptr_t getMainProcessCountVA;            // 进程数量检测函数

    // ============================================================
    // 退群监控相关地址
    // ============================================================

    uintptr_t groupExitDBApplyVA;               // chatroom_member DB apply
    uintptr_t groupExitFMessagePreVA;           // InsertFMessageToSessionPre
    uintptr_t groupExitUpdateSessionCacheVA;    // UpdateSessionCache
    uintptr_t groupExitMemberDataListVA;        // GetAllMemberDataList
    uintptr_t groupExitChatroomInfoOperatorVA;  // chatroom_manager::operator()

    // ============================================================
    // 系统浏览器相关地址
    // ============================================================

    uintptr_t openURLWebViewKindVA;             // GetUrlWebViewKind

    // ============================================================
    // 消息发送相关地址
    // ============================================================

    uintptr_t sendMsgCGIVA;                     // SendMsg CGI dispatcher

    // ============================================================
    // MessageWrap 布局
    // ============================================================

    MessageWrapLayout layout;
};

/**
 * 在 Profile 数组中查找匹配当前版本的配置。
 * @param profiles Profile 数组
 * @param count 数组长度
 * @param shortVersion 当前微信短版本号
 * @param buildVersion 当前微信 Build 号
 * @return 匹配的 Profile 指针，未找到返回 nullptr
 */
inline const WeChatProfile* FindProfile(const WeChatProfile* profiles,
                                         size_t count,
                                         const char* shortVersion,
                                         const char* buildVersion) {
    if (!profiles || !shortVersion || !buildVersion) {
        return nullptr;
    }

    for (size_t i = 0; i < count; i++) {
        const auto& p = profiles[i];
        // 精确匹配 shortVersion + buildVersion
        bool shortMatch = (p.shortVersion[0] == '*' && p.shortVersion[1] == '\0') ||
                          (strcmp(p.shortVersion, shortVersion) == 0);
        bool buildMatch = (p.buildVersion[0] == '*' && p.buildVersion[1] == '\0') ||
                          (strcmp(p.buildVersion, buildVersion) == 0);
        if (shortMatch && buildMatch) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace soviet

#endif // SOVIET_VERSION_PROFILE_H
