/**
 * @file feature_flags.h
 * @brief SovietX 功能开关定义
 * 
 * 跨平台公共头文件，定义所有功能模块的开关键名和默认值。
 * 平台层通过 PlatformConfig 接口读写这些开关。
 */

#ifndef SOVIET_FEATURE_FLAGS_H
#define SOVIET_FEATURE_FLAGS_H

namespace soviet {

// 插件版本号
constexpr const char* kVersionString = "1.2.0";

// ============================================================
// 功能开关键名（用于配置持久化）
// ============================================================

// 阻止微信自动更新
constexpr const char* kFeatureAntiUpdate = "feature.anti_update";

// 消息防撤回
constexpr const char* kFeatureAntiRevoke = "feature.anti_revoke";

// 多开（仅在已验证的客户端版本中手动启用）
constexpr const char* kFeatureMultiOpen = "feature.multi_open";

// 退群监控
constexpr const char* kFeatureGroupExitMonitor = "feature.group_exit_monitor";

// 退群显示昵称
constexpr const char* kFeatureGroupExitNickname = "feature.group_exit_nickname";

// 撤回消息转发给自己（全设备同步）
constexpr const char* kFeatureRevokeForwardToSelf = "feature.revoke_forward_to_self";

// 使用系统浏览器打开链接
constexpr const char* kFeatureUseSystemBrowser = "feature.use_system_browser";

// ============================================================
// 功能开关默认值
// ============================================================

struct FeatureDefaults {
    static constexpr bool kAntiUpdateDefault = false;
    static constexpr bool kAntiRevokeDefault = false;
    static constexpr bool kMultiOpenDefault = false;
    static constexpr bool kGroupExitMonitorDefault = false;
    static constexpr bool kGroupExitNicknameDefault = false;
    static constexpr bool kRevokeForwardToSelfDefault = false;
    static constexpr bool kUseSystemBrowserDefault = false;
};

} // namespace soviet

#endif // SOVIET_FEATURE_FLAGS_H
