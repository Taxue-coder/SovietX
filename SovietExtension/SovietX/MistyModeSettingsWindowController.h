//  MistyModeSettingsWindowController.h
//  SovietX
//
//  Created by MustangYM on 2026/7/2.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

#pragma mark - 主题模式 / 迷离模式 UserDefaults Keys
static NSString *kThemeMistyMode = @"kThemeMistyMode1.SOVIET";
static NSString *kThemeMistyQNSAlpha = @"kThemeMistyQNSAlpha1.SOVIET";
static NSString *kThemeMistyWindowBlurEnabled = @"kThemeMistyWindowBlurEnabled1.SOVIET";
static NSString *kThemeMistyWindowBlurRadius = @"kThemeMistyWindowBlurRadius1.SOVIET";
static NSString *kThemeMistyCarrierStyle = @"kThemeMistyCarrierStyle1.SOVIET";
static NSString *kThemeMistyCarrierStyleDark = @"dark";
static NSString *kThemeMistyCarrierStyleLight = @"light";
static NSString *kThemeMistyKeepAlive = @"kThemeMistyKeepAlive1.SOVIET";
static NSString *kThemeMistyColorful = @"kThemeMistyColorful.SOVIET";
static NSString *kThemeMistyColorfulOpacity = @"kThemeMistyColorfulOpacity.SOVIET";
static NSString *kThemeMistyColorfulBlurRadius = @"kThemeMistyColorfulBlurRadius.SOVIET";
static NSString *kThemeMistyColorfulAnimationDuration = @"kThemeMistyColorfulAnimationDuration.SOVIET";

/// 迷离模式设置窗口。
/// MenuManager 只负责打开这个窗口，具体 UI、读写配置、立即应用效果都在这里处理。
@interface MistyModeSettingsWindowController : NSWindowController

/// 点击「确定」并保存成功后回调，MenuManager 用它来给菜单打勾。
@property (nonatomic, copy, nullable) void (^confirmHandler)(BOOL isOpen);

+ (void)registerDefaults;
- (void)showWindowCentered;

@end

NS_ASSUME_NONNULL_END
//
