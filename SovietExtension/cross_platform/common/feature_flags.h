/**
 * @file feature_flags.h
 * @brief SovietExtension 功能开关定义
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

// 退群监控
constexpr const char* kFeatureGroupExitMonitor = "feature.group_exit_monitor";

// 退群显示昵称
constexpr const char* kFeatureGroupExitNickname = "feature.group_exit_nickname";

// 撤回消息转发给自己（全设备同步）
constexpr const char* kFeatureRevokeForwardToSelf = "feature.revoke_forward_to_self";

// 使用系统浏览器打开链接
constexpr const char* kFeatureUseSystemBrowser = "feature.use_system_browser";

// 自动登录
constexpr const char* kFeatureAutoLogin = "feature.auto_login";

// 迷离模式（主题模糊）
constexpr const char* kFeatureMistyMode = "feature.misty_mode";

// 迷离模式 - 窗口模糊开关
constexpr const char* kFeatureMistyWindowBlur = "feature.misty_window_blur";

// 迷离模式 - 流光氛围开关
constexpr const char* kFeatureMistyColorful = "feature.misty_colorful";

// ============================================================
// 功能开关默认值
// ============================================================

struct FeatureDefaults {
    static constexpr bool kAntiUpdateDefault = false;
    static constexpr bool kAntiRevokeDefault = false;
    static constexpr bool kGroupExitMonitorDefault = false;
    static constexpr bool kGroupExitNicknameDefault = false;
    static constexpr bool kRevokeForwardToSelfDefault = false;
    static constexpr bool kUseSystemBrowserDefault = false;
    static constexpr bool kAutoLoginDefault = false;
    static constexpr bool kMistyModeDefault = false;
    static constexpr bool kMistyWindowBlurDefault = true;
    static constexpr bool kMistyColorfulDefault = false;
};

// ============================================================
// 迷离模式参数键名和默认值
// ============================================================

constexpr const char* kMistyQNSAlpha = "misty.qns_alpha";
constexpr const char* kMistyWindowBlurRadius = "misty.window_blur_radius";
constexpr const char* kMistyColorfulOpacity = "misty.colorful_opacity";
constexpr const char* kMistyColorfulBlurRadius = "misty.colorful_blur_radius";
constexpr const char* kMistyColorfulAnimationDuration = "misty.colorful_animation_duration";

struct MistyDefaults {
    static constexpr double kQNSAlphaDefault = 0.90;
    static constexpr int kWindowBlurRadiusDefault = 10;
    static constexpr double kColorfulOpacityDefault = 0.42;
    static constexpr double kColorfulBlurRadiusDefault = 70.0;
    static constexpr double kColorfulAnimationDurationDefault = 10.0;
};

} // namespace soviet

#endif // SOVIET_FEATURE_FLAGS_H
