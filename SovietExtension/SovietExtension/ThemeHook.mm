//
//  ThemeHook.m
//  SovietExtension
//
//  Created by MustangYM on 2026/6/20.
//

#import "ThemeHook.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreImage/CoreImage.h>
#import <objc/runtime.h>
#import <dlfcn.h>
#import <math.h>
#import "RevokePatch.h"
#import "MistyModeSettingsWindowController.h"
#import "MenuManager.h"
#import "YMColorfulBlurBackgroundView.h"

#pragma mark - 默认值
static const BOOL kYMDefaultMistyModeEnabled = NO;
static const CGFloat kYMDefaultQNSViewAlphaValue = 0.90f;
static const BOOL kYMDefaultWindowBackgroundBlurEnabled = YES;
static const int kYMDefaultWindowBackgroundBlurRadius = 10;
static const BOOL kYMDefaultBlurKeepAliveEnabled = YES;
static NSString * const kYMDefaultCarrierStyle = @"dark";

//手动调试出来这个效果
static const CGFloat kYMCarrierAlphaDark = 0.30f;
static const CGFloat kYMCarrierAlphaLight = 0.015f;

static const NSTimeInterval kYMBlurKeepAliveInterval = 2.0;

#pragma mark - 私有 CGS 接口声明

/// 这里不直接链接私有符号，而是运行时 dlsym，避免链接阶段报错。
typedef int YMCGSConnectionID;
typedef int YMCGSWindowID;
typedef int (*YMCGSMainConnectionIDFunc)(void);
typedef CGError (*YMCGSSetWindowBackgroundBlurRadiusFunc)(YMCGSConnectionID cid,
                                                          YMCGSWindowID wid,
                                                          int radius);

static YMCGSMainConnectionIDFunc gYMCGSMainConnectionID = NULL;
static YMCGSSetWindowBackgroundBlurRadiusFunc gYMCGSSetWindowBackgroundBlurRadius = NULL;
static BOOL gYMCGSBlurResolved = NO;
static BOOL gYMCGSBlurResolveFailedLogged = NO;

#pragma mark - 原始 IMP

typedef BOOL (*YMIsOpaqueIMP)(id self, SEL _cmd);
typedef void (*YMVoidIMP)(id self, SEL _cmd);
typedef void (*YMOneObjectIMP)(id self, SEL _cmd, id arg);

static YMIsOpaqueIMP gOrig_QNSView_isOpaque = NULL;
static YMVoidIMP gOrig_QNSView_viewDidMoveToWindow = NULL;
static YMVoidIMP gOrig_QNSView_viewDidMoveToSuperview = NULL;
static YMOneObjectIMP gOrig_QNSView_setLayer = NULL;

#pragma mark - 状态

static BOOL gYMStarted = NO;
static BOOL gYMQNSViewHookInstalled = NO;
static BOOL gYMBlurKeepAliveStarted = NO;

/// 防止处理背景过程中触发 setLayer / view move 后递归。
static BOOL gYMApplyingBlurBackground = NO;

/// 用于记录窗口上一次应用的 blur radius，避免重复刷日志。
static char kYMAppliedBlurRadiusAssociatedKey;
static char kYMAppliedBlurWindowNumberAssociatedKey;

#pragma mark - 配置读取

static void YMRegisterMistyThemeDefaults(void) {
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        kThemeMistyMode: @(kYMDefaultMistyModeEnabled),
        kThemeMistyQNSAlpha: @(kYMDefaultQNSViewAlphaValue),
        kThemeMistyWindowBlurEnabled: @(kYMDefaultWindowBackgroundBlurEnabled),
        kThemeMistyWindowBlurRadius: @(kYMDefaultWindowBackgroundBlurRadius),
        kThemeMistyColorful: @(NO),
        kThemeMistyCarrierStyle: kYMDefaultCarrierStyle,
        kThemeMistyKeepAlive: @(kYMDefaultBlurKeepAliveEnabled),
    }];
}

static BOOL YMBoolSetting(NSString *key, BOOL defaultValue) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:key] == nil) {
        return defaultValue;
    }
    return [defaults boolForKey:key];
}

static CGFloat YMFloatSetting(NSString *key, CGFloat defaultValue) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:key] == nil) {
        return defaultValue;
    }
    return (CGFloat)[defaults doubleForKey:key];
}

static NSInteger YMIntegerSetting(NSString *key, NSInteger defaultValue) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:key] == nil) {
        return defaultValue;
    }
    return [defaults integerForKey:key];
}

static NSString *YMStringSetting(NSString *key, NSString *defaultValue) {
    NSString *value = [[NSUserDefaults standardUserDefaults] stringForKey:key];
    if (value.length == 0) {
        return defaultValue;
    }
    return value;
}

static BOOL YMMistyModeEnabled(void) {
    return YMBoolSetting(kThemeMistyMode, kYMDefaultMistyModeEnabled);
}

static BOOL YMWindowBackgroundBlurEnabled(void) {
    if (!YMMistyModeEnabled()) return NO;
    return YMBoolSetting(kThemeMistyWindowBlurEnabled, kYMDefaultWindowBackgroundBlurEnabled);
}

static CGFloat YMQNSViewAlphaValue(void) {
    if (!YMMistyModeEnabled()) {
        return 1.0f;
    }

    CGFloat alpha = YMFloatSetting(kThemeMistyQNSAlpha, kYMDefaultQNSViewAlphaValue);
    alpha = MAX(0.20f, MIN(1.0f, alpha));
    return alpha;
}

static int YMWindowBackgroundBlurRadius(void) {
    if (!YMWindowBackgroundBlurEnabled()) {
        return 0;
    }

    NSInteger radius = YMIntegerSetting(kThemeMistyWindowBlurRadius, kYMDefaultWindowBackgroundBlurRadius);
    radius = MAX(0, MIN(100, radius));
    return (int)radius;
}

static BOOL YMBlurKeepAliveEnabled(void) {
    return YMBoolSetting(kThemeMistyKeepAlive, kYMDefaultBlurKeepAliveEnabled);
}

BOOL YMCarrierStyleIsDark(void) {
    NSString *style = YMStringSetting(kThemeMistyCarrierStyle, kYMDefaultCarrierStyle);
    return ![style isEqualToString:kThemeMistyCarrierStyleLight];
}

static CGFloat YMWindowBlurCarrierAlpha(void) {
    if (!YMWindowBackgroundBlurEnabled() || YMWindowBackgroundBlurRadius() <= 0) {
        return 0.0f;
    }

    return YMCarrierStyleIsDark() ? kYMCarrierAlphaDark : kYMCarrierAlphaLight;
}

#pragma mark - Colorful 风格动态渐变背景

BOOL YMColorfulBlurBackgroundEnabled(void) {
    if (!YMMistyModeEnabled()) return NO;
    if (!YMBoolSetting(kThemeMistyColorful, NO)) return NO;
    if (!YMWindowBackgroundBlurEnabled()) return NO;
    if (YMWindowBackgroundBlurRadius() <= 0) return NO;
    return YES;
}

static YMColorfulBlurBackgroundView *YMFindColorfulBlurBackgroundViewInContainer(NSView *container) {
    if (!container) return nil;

    for (NSView *subview in container.subviews) {
        if ([subview.identifier isEqualToString:kYMColorfulBlurBackgroundViewIdentifier] &&
            [subview isKindOfClass:[YMColorfulBlurBackgroundView class]]) {
            return (YMColorfulBlurBackgroundView *)subview;
        }
    }

    return nil;
}

static void YMEnsureColorfulBlurBackgroundInContainer(NSView *container, NSView *qnsView) {
    if (!container || !qnsView) return;

    YMColorfulBlurBackgroundView *colorfulView = YMFindColorfulBlurBackgroundViewInContainer(container);

    if (!YMColorfulBlurBackgroundEnabled()) {
        if (colorfulView) {
            [colorfulView removeFromSuperview];
        }
        return;
    }

    if (!colorfulView) {
        colorfulView = [[YMColorfulBlurBackgroundView alloc] initWithFrame:container.bounds];
        [container addSubview:colorfulView positioned:NSWindowBelow relativeTo:qnsView];
        YMLog(@"已插入 Colorful 动态渐变背景 view：%@", colorfulView);
    } else {
        colorfulView.frame = container.bounds;
        if (colorfulView.superview != container) {
            [colorfulView removeFromSuperview];
            [container addSubview:colorfulView positioned:NSWindowBelow relativeTo:qnsView];
        } else {
            [container addSubview:colorfulView positioned:NSWindowBelow relativeTo:qnsView];
        }
    }

    [colorfulView ym_updateColorfulBackgroundIfNeeded];
}

#pragma mark - 工具：判断 QNSView

static BOOL YMIsQNSView(NSView *view) {
    if (!view) return NO;

    Class qnsClass = NSClassFromString(@"QNSView");
    if (!qnsClass) return NO;

    return [view isKindOfClass:qnsClass];
}


#pragma mark - 工具：窗口过滤
static BOOL YMShouldSkipMistyEffectForWindow(NSWindow *window) {
    if (!window) {
        return YES;
    }

    NSString *className = NSStringFromClass(window.class);
    NSArray<NSString *> *blockedKeywords = @[
        @"Status",
        @"Menu",
        @"Popover",
        @"Tooltip",
        @"TouchBar"
    ];
    
    for (NSString *keyword in blockedKeywords) {
        if ([className rangeOfString:keyword options:NSCaseInsensitiveSearch].location != NSNotFound) {
            return YES;
        }
    }

    // AppKit 的菜单 / 状态栏相关窗口常见窗口层级。
    // 不用“非 NSNormalWindowLevel 一律跳过”，因为微信登录窗口在某些状态下可能不是普通 level。
    NSInteger level = window.level;
    if (level == NSMainMenuWindowLevel ||
        level == NSStatusWindowLevel ||
        level == NSPopUpMenuWindowLevel) {
        return YES;
    }

    NSRect frame = window.frame;

    // 顶部状态栏图标、菜单承载窗口一般非常小。
    // 这里阈值故意放低，避免把微信登录窗口误判掉。
    if (frame.size.width < 220.0 || frame.size.height < 120.0) {
        return YES;
    }

    return NO;
}

static BOOL YMShouldApplyMistyEffectForWindow(NSWindow *window) {
    return !YMShouldSkipMistyEffectForWindow(window);
}

#pragma mark - 工具：CGS 背景模糊

static BOOL YMResolveCGSBlurSymbolsIfNeeded(void) {
    if (gYMCGSBlurResolved) {
        return gYMCGSMainConnectionID && gYMCGSSetWindowBackgroundBlurRadius;
    }

    gYMCGSBlurResolved = YES;

    // 通常这两个符号可以从当前进程已经加载的 CoreGraphics / ApplicationServices 里找到。
    gYMCGSMainConnectionID = (YMCGSMainConnectionIDFunc)dlsym(RTLD_DEFAULT, "CGSMainConnectionID");
    gYMCGSSetWindowBackgroundBlurRadius = (YMCGSSetWindowBackgroundBlurRadiusFunc)dlsym(RTLD_DEFAULT, "CGSSetWindowBackgroundBlurRadius");

    if (!gYMCGSMainConnectionID || !gYMCGSSetWindowBackgroundBlurRadius) {
        void *handle = dlopen("/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices", RTLD_LAZY);
        if (handle) {
            if (!gYMCGSMainConnectionID) {
                gYMCGSMainConnectionID = (YMCGSMainConnectionIDFunc)dlsym(handle, "CGSMainConnectionID");
            }
            if (!gYMCGSSetWindowBackgroundBlurRadius) {
                gYMCGSSetWindowBackgroundBlurRadius = (YMCGSSetWindowBackgroundBlurRadiusFunc)dlsym(handle, "CGSSetWindowBackgroundBlurRadius");
            }
        }
    }

    if (!gYMCGSMainConnectionID || !gYMCGSSetWindowBackgroundBlurRadius) {
        void *handle = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_LAZY);
        if (handle) {
            if (!gYMCGSMainConnectionID) {
                gYMCGSMainConnectionID = (YMCGSMainConnectionIDFunc)dlsym(handle, "CGSMainConnectionID");
            }
            if (!gYMCGSSetWindowBackgroundBlurRadius) {
                gYMCGSSetWindowBackgroundBlurRadius = (YMCGSSetWindowBackgroundBlurRadiusFunc)dlsym(handle, "CGSSetWindowBackgroundBlurRadius");
            }
        }
    }

    BOOL ok = gYMCGSMainConnectionID && gYMCGSSetWindowBackgroundBlurRadius;
    if (!ok && !gYMCGSBlurResolveFailedLogged) {
        gYMCGSBlurResolveFailedLogged = YES;
        YMLog(@"CGS 背景模糊接口解析失败，当前系统可能不可用：CGSMainConnectionID=%p CGSSetWindowBackgroundBlurRadius=%p",
              gYMCGSMainConnectionID,
              gYMCGSSetWindowBackgroundBlurRadius);
    }

    return ok;
}

static void YMApplyWindowBackgroundBlur(NSWindow *window) {
    if (!window) return;

    NSInteger windowNumber = window.windowNumber;
    if (windowNumber <= 0) {
        // windowNumber 为 0 通常说明窗口还没真正进入 WindowServer，稍后 refresh 会再应用。
        return;
    }

    int radius = YMWindowBackgroundBlurRadius();

    // 如果 radius 为 0，也要调用一次 CGS，把旧 blur 关掉。
    if (!YMResolveCGSBlurSymbolsIfNeeded()) {
        return;
    }

    NSNumber *oldRadius = objc_getAssociatedObject(window, &kYMAppliedBlurRadiusAssociatedKey);
    NSNumber *oldWindowNumber = objc_getAssociatedObject(window, &kYMAppliedBlurWindowNumberAssociatedKey);
    if (oldRadius && oldWindowNumber &&
        oldRadius.intValue == radius &&
        oldWindowNumber.integerValue == windowNumber) {
        return;
    }

    YMCGSConnectionID cid = gYMCGSMainConnectionID();
    CGError err = gYMCGSSetWindowBackgroundBlurRadius(cid, (YMCGSWindowID)windowNumber, radius);

    objc_setAssociatedObject(window,
                             &kYMAppliedBlurRadiusAssociatedKey,
                             @(radius),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(window,
                             &kYMAppliedBlurWindowNumberAssociatedKey,
                             @(windowNumber),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    YMLog(@"已应用窗口背景模糊：window=%@ windowNumber=%ld radius=%d err=%d",
          window,
          (long)windowNumber,
          radius,
          err);
}

#pragma mark - Cocoa 层：窗口与 View 透明 / blur carrier

static NSColor *YMWindowBlurCarrierColor(void) {
    if (!YMWindowBackgroundBlurEnabled() || YMWindowBackgroundBlurRadius() <= 0) {
        return [NSColor clearColor];
    }

    CGFloat alpha = MAX(0.0, MIN(1.0, YMWindowBlurCarrierAlpha()));
    CGFloat white = YMCarrierStyleIsDark() ? 0.0 : 1.0;

    return [NSColor colorWithCalibratedWhite:white alpha:alpha];
}

static void YMMakeWindowTransparent(NSWindow *window) {
    if (!window) return;

    window.opaque = NO;

    // 不再插入额外 overlay view。
    // 这里用 window 自己的背景色作为 blur carrier。
    // 深色模式内部 alpha = 0.3；浅色模式内部 alpha = 0.015。
    window.backgroundColor = YMWindowBlurCarrierColor();

    // 保留阴影，整体观感更像系统窗口。
    // window.hasShadow = YES;

    YMApplyWindowBackgroundBlur(window);
}

static void YMMakeViewTransparent(NSView *view) {
    if (!view) return;

    view.wantsLayer = YES;
    view.layer.opaque = NO;
    view.layer.backgroundColor = NSColor.clearColor.CGColor;

    if ([view respondsToSelector:@selector(setAlphaValue:)]) {
        // 这里不要设成 0，否则整个 Qt 内容都会不可见。
        view.alphaValue = YMQNSViewAlphaValue();
    }
}

static void YMMakeContainerBlurCarrier(NSView *container) {
    if (!container) return;

    container.wantsLayer = YES;
    container.layer.opaque = NO;

    // 不用额外 view，而是让 contentView / QNSView 父容器自己提供 blur carrier。
    // 这样 QNSView 内部完全透明的区域，例如最左侧工具栏，也能看到 WindowServer blur。
    container.layer.backgroundColor = YMWindowBlurCarrierColor().CGColor;
}

static void YMInstallBlurBackgroundBehindQNSView(NSView *qnsView) {
    if (!YMIsQNSView(qnsView)) return;
    if (gYMApplyingBlurBackground) return;

    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            YMInstallBlurBackgroundBehindQNSView(qnsView);
        });
        return;
    }

    gYMApplyingBlurBackground = YES;

    @try {
        NSWindow *window = qnsView.window;
        if (!window) return;

        // 只跳过 macOS 顶部状态栏图标、菜单、popover 等附属窗口。
        // 不跳过登录窗口，避免登录界面的迷离效果失效。
        if (!YMShouldApplyMistyEffectForWindow(window)) {
            return;
        }

        NSView *container = qnsView.superview ?: window.contentView;
        if (!container) return;

        // QNSView 必须已经是 container 的直接子 view，否则不要强行处理。
        if (qnsView.superview != container) {
            return;
        }

        YMMakeWindowTransparent(window);
        YMMakeContainerBlurCarrier(container);
        YMEnsureColorfulBlurBackgroundInContainer(container, qnsView);
        YMMakeViewTransparent(qnsView);
    }
    @finally {
        gYMApplyingBlurBackground = NO;
    }
}

static void YMInstallBlurBackgroundBehindQNSViewAsync(NSView *qnsView) {
    if (!qnsView) return;

    __weak NSView *weakView = qnsView;

    dispatch_async(dispatch_get_main_queue(), ^{
        NSView *strongView = weakView;
        if (!strongView) return;

        YMInstallBlurBackgroundBehindQNSView(strongView);
    });
}

static void YMRefreshViewTree(NSView *view) {
    if (!view) return;

    if (YMIsQNSView(view)) {
        YMInstallBlurBackgroundBehindQNSView(view);
        return;
    }

    NSArray<NSView *> *subviews = [view.subviews copy];
    for (NSView *subview in subviews) {
        YMRefreshViewTree(subview);
    }
}

static void YMRefreshAllWindows(void) {
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            YMRefreshAllWindows();
        });
        return;
    }

    YMRegisterMistyThemeDefaults();

    for (NSWindow *window in NSApp.windows) {
        if (!YMShouldApplyMistyEffectForWindow(window)) {
            continue;
        }

        YMMakeWindowTransparent(window);

        // window.contentView 本身也设成 blur carrier，保证还没找到 QNSView 时也能先有承载面。
        YMMakeContainerBlurCarrier(window.contentView);

        YMRefreshViewTree(window.contentView);
    }
}

static void YMBlurKeepAliveTick(void) {
    if (!gYMBlurKeepAliveStarted) return;

    if (YMBlurKeepAliveEnabled()) {
        YMRefreshAllWindows();
    }

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kYMBlurKeepAliveInterval * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        YMBlurKeepAliveTick();
    });
}

static void YMStartBlurKeepAliveIfNeeded(void) {
    if (gYMBlurKeepAliveStarted) return;

    gYMBlurKeepAliveStarted = YES;

    dispatch_async(dispatch_get_main_queue(), ^{
        YMBlurKeepAliveTick();
    });
}

#pragma mark - Runtime Hook：QNSView

static BOOL YM_QNSView_isOpaque(id self, SEL _cmd) {
    if ([self isKindOfClass:[NSView class]]) {
        NSWindow *window = ((NSView *)self).window;
        if (window && !YMShouldApplyMistyEffectForWindow(window)) {
            if (gOrig_QNSView_isOpaque) {
                return gOrig_QNSView_isOpaque(self, _cmd);
            }
            return YES;
        }
    }

    return NO;
}

static void YM_QNSView_viewDidMoveToWindow(id self, SEL _cmd) {
    if (gOrig_QNSView_viewDidMoveToWindow) {
        gOrig_QNSView_viewDidMoveToWindow(self, _cmd);
    }

    if ([self isKindOfClass:[NSView class]]) {
        YMInstallBlurBackgroundBehindQNSViewAsync((NSView *)self);
    }
}

static void YM_QNSView_viewDidMoveToSuperview(id self, SEL _cmd) {
    if (gOrig_QNSView_viewDidMoveToSuperview) {
        gOrig_QNSView_viewDidMoveToSuperview(self, _cmd);
    }

    if ([self isKindOfClass:[NSView class]]) {
        YMInstallBlurBackgroundBehindQNSViewAsync((NSView *)self);
    }
}

static void YM_QNSView_setLayer(id self, SEL _cmd, id layer) {
    if (gOrig_QNSView_setLayer) {
        gOrig_QNSView_setLayer(self, _cmd, layer);
    }

    if ([self isKindOfClass:[NSView class]]) {
        NSView *view = (NSView *)self;
        NSWindow *window = view.window;

        if (!window || YMShouldApplyMistyEffectForWindow(window)) {
            YMMakeViewTransparent(view);
            YMInstallBlurBackgroundBehindQNSViewAsync(view);
        }
    }
}

/// 只替换一次，避免 oldImp 被覆盖成自己的 hook，导致无限递归。
static void YMReplaceInstanceMethodOnce(Class cls, SEL sel, IMP newImp, IMP *oldImpStorage) {
    Method method = class_getInstanceMethod(cls, sel);
    if (!method) {
        YMLog(@"找不到方法：%@ %@", NSStringFromClass(cls), NSStringFromSelector(sel));
        return;
    }

    IMP currentImp = method_getImplementation(method);

    if (currentImp == newImp) {
        YMLog(@"方法已经 hook，跳过：%@ %@", NSStringFromClass(cls), NSStringFromSelector(sel));
        return;
    }

    IMP oldImp = method_setImplementation(method, newImp);

    // 只在第一次保存原始 IMP。
    if (oldImpStorage && *oldImpStorage == NULL) {
        *oldImpStorage = oldImp;
    }

    YMLog(@"已 hook 方法：%@ %@", NSStringFromClass(cls), NSStringFromSelector(sel));
}

static void YMInstallQNSViewHooks(void) {
    if (gYMQNSViewHookInstalled) {
        return;
    }

    Class qnsClass = NSClassFromString(@"QNSView");
    if (!qnsClass) {
        YMLog(@"QNSView 类不存在，稍后重试");
        return;
    }

    gYMQNSViewHookInstalled = YES;

    YMReplaceInstanceMethodOnce(qnsClass,
                                @selector(isOpaque),
                                (IMP)YM_QNSView_isOpaque,
                                (IMP *)&gOrig_QNSView_isOpaque);

    YMReplaceInstanceMethodOnce(qnsClass,
                                @selector(viewDidMoveToWindow),
                                (IMP)YM_QNSView_viewDidMoveToWindow,
                                (IMP *)&gOrig_QNSView_viewDidMoveToWindow);

    YMReplaceInstanceMethodOnce(qnsClass,
                                @selector(viewDidMoveToSuperview),
                                (IMP)YM_QNSView_viewDidMoveToSuperview,
                                (IMP *)&gOrig_QNSView_viewDidMoveToSuperview);

    YMReplaceInstanceMethodOnce(qnsClass,
                                @selector(setLayer:),
                                (IMP)YM_QNSView_setLayer,
                                (IMP *)&gOrig_QNSView_setLayer);
}

#pragma mark - 启动入口

@implementation ThemeHook

+ (void)start {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    BOOL mistyEnabled = [defaults boolForKey:kThemeMistyMode];
    if (!mistyEnabled) {
        return;
    }
    
    
    if (gYMStarted) {
        return;
    }

    gYMStarted = YES;

    YMLog(@"start");

    [MenuManager shareInstance].hasLoadMistyHook = YES;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        YMRegisterMistyThemeDefaults();
        YMInstallQNSViewHooks();
        YMRefreshAllWindows();
        YMStartBlurKeepAliveIfNeeded();

        // 微信 / Qt 有些窗口和 layer 会延后创建，所以延迟再刷几次。
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            YMInstallQNSViewHooks();
            YMRefreshAllWindows();
        });

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            YMInstallQNSViewHooks();
            YMRefreshAllWindows();
        });
    });
}

+ (void)setBackgroundImagePath:(NSString *)path {
    // 当前版本已经从背景图 / NSVisualEffectView 切换为 WindowServer 可调背景模糊。
    // 保留这个接口只是为了兼容旧调用，避免外部代码调用时报错。
    (void)path;
    YMRefreshAllWindows();
}

+ (void)refreshAllQNSViews {
    YMRegisterMistyThemeDefaults();
    YMRefreshAllWindows();
    YMStartBlurKeepAliveIfNeeded();
}

@end

__attribute__((constructor))
static void YMWeChatAntiRevokePatchEntry(void) {
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [ThemeHook start];
        });
    }
}
