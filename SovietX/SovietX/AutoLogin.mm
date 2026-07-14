//
//  YMAutoLogin.m
//  SovietX
//
//  Created by MustangYM on 2026/6/26.
//
//  但我还是想说, 开源共产主义, 爱你们
//         -- MustangYM 2026-6-16
//

#import "AutoLogin.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <objc/message.h>
#import <stdatomic.h>
#import <stdbool.h>
#import <math.h>
#import "RevokePatch.h"

#pragma mark - 自动登录状态

static atomic_bool YMAutoLoginEnabled = ATOMIC_VAR_INIT(false);
static atomic_bool YMAutoLoginScannerStarted = ATOMIC_VAR_INIT(false);
static atomic_bool YMAutoLoginClickSucceeded = ATOMIC_VAR_INIT(false);
static atomic_bool YMAutoLoginReturnKeySent = ATOMIC_VAR_INIT(false);
static atomic_int YMAutoLoginClickAttemptCount = ATOMIC_VAR_INIT(0);

static BOOL YMIsAutoLoginEnabled(void) {
    return atomic_load(&YMAutoLoginEnabled);
}

#pragma mark - 自动登录实现

static NSString *YMAutoLoginTextFromObject(id object) {
    if (!object) {
        return @"";
    }

    NSMutableArray<NSString *> *parts = [NSMutableArray array];

    if ([object respondsToSelector:@selector(title)]) {
        NSString *title = nil;
        @try {
            title = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(title));
        } @catch (NSException *exception) {
            title = nil;
        }
        if (title.length > 0) {
            [parts addObject:title];
        }
    }

    if ([object respondsToSelector:@selector(alternateTitle)]) {
        NSString *title = nil;
        @try {
            title = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(alternateTitle));
        } @catch (NSException *exception) {
            title = nil;
        }
        if (title.length > 0) {
            [parts addObject:title];
        }
    }

    if ([object respondsToSelector:@selector(accessibilityLabel)]) {
        NSString *label = nil;
        @try {
            label = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(accessibilityLabel));
        } @catch (NSException *exception) {
            label = nil;
        }
        if (label.length > 0) {
            [parts addObject:label];
        }
    }

    if ([object respondsToSelector:@selector(accessibilityTitle)]) {
        NSString *title = nil;
        @try {
            title = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(accessibilityTitle));
        } @catch (NSException *exception) {
            title = nil;
        }
        if (title.length > 0) {
            [parts addObject:title];
        }
    }

    if ([object respondsToSelector:@selector(accessibilityValue)]) {
        id value = nil;
        @try {
            value = ((id (*)(id, SEL))objc_msgSend)(object, @selector(accessibilityValue));
        } @catch (NSException *exception) {
            value = nil;
        }
        if ([value isKindOfClass:[NSString class]] && [value length] > 0) {
            [parts addObject:(NSString *)value];
        }
    }

    if ([object respondsToSelector:@selector(accessibilityHelp)]) {
        NSString *help = nil;
        @try {
            help = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(accessibilityHelp));
        } @catch (NSException *exception) {
            help = nil;
        }
        if (help.length > 0) {
            [parts addObject:help];
        }
    }

    if ([object respondsToSelector:@selector(accessibilityRoleDescription)]) {
        NSString *roleDescription = nil;
        @try {
            roleDescription = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(accessibilityRoleDescription));
        } @catch (NSException *exception) {
            roleDescription = nil;
        }
        if (roleDescription.length > 0) {
            [parts addObject:roleDescription];
        }
    }

    return [parts componentsJoinedByString:@" "] ?: @"";
}

static BOOL YMAutoLoginViewVisible(NSView *view) {
    if (!view) {
        return NO;
    }

    if (view.hidden || view.alphaValue <= 0.01) {
        return NO;
    }

    NSWindow *window = view.window;
    if (window && !window.isVisible) {
        return NO;
    }

    return YES;
}

static BOOL YMAutoLoginTextLooksLikeLoginButton(NSString *text) {
    if (text.length == 0) {
        return NO;
    }

    NSString *lower = text.lowercaseString;

    //过滤一下咯
    NSArray<NSString *> *negative = @[
        @"切换账号",
        @"切換帳號",
        @"切換賬號",
        @"仅传输文件",
        @"僅傳輸文件",
        @"仅传文件",
        @"網絡代理",
        @"网络代理",
        @"代理设置",
        @"switch account",
        @"change account",
        @"file transfer",
        @"transfer files",
        @"proxy",
        @"network settings",
    ];

    for (NSString *word in negative) {
        if ([lower containsString:word.lowercaseString]) {
            return NO;
        }
    }

    //多语言
    NSArray<NSString *> *positive = @[
        @"进入微信",
        @"進入微信",
        @"进入 wechat",
        @"進入 wechat",
        @"enter wechat",
        @"enter weixin",
        @"log in",
        @"login",
        @"sign in",
        @"登录",
        @"登入",
        @"登錄",
        @"로그인",
        @"サインイン",
    ];

    for (NSString *word in positive) {
        if ([lower containsString:word.lowercaseString]) {
            return YES;
        }
    }

    return NO;
}

static NSRect YMAutoLoginAccessibilityFrameFromObject(id object) {
    if (!object || ![object respondsToSelector:@selector(accessibilityFrame)]) {
        return NSZeroRect;
    }

    NSRect frame = NSZeroRect;
    @try {
        frame = ((NSRect (*)(id, SEL))objc_msgSend)(object, @selector(accessibilityFrame));
    } @catch (NSException *exception) {
        frame = NSZeroRect;
    }

    if (isnan(frame.origin.x) || isnan(frame.origin.y) || isnan(frame.size.width) || isnan(frame.size.height)) {
        return NSZeroRect;
    }

    if (frame.size.width <= 1.0 || frame.size.height <= 1.0) {
        return NSZeroRect;
    }

    return frame;
}

static NSWindow *YMAutoLoginWindowContainingScreenPoint(NSPoint point) {
    NSArray<NSWindow *> *windows = [NSApp.windows copy];
    for (NSWindow *window in windows) {
        if (!window.isVisible) {
            continue;
        }
        if (NSPointInRect(point, window.frame)) {
            return window;
        }
    }

    for (NSWindow *window in windows) {
        if (!window.isVisible) {
            continue;
        }
        NSString *className = NSStringFromClass([window class]);
        if ([className containsString:@"QNSWindow"] || [window.title containsString:@"微信"] || [window.title.lowercaseString containsString:@"wechat"]) {
            return window;
        }
    }

    return nil;
}

static void YMAutoLoginSendCocoaMouseClick(NSWindow *window, NSPoint screenPoint, NSString *reason) {
    if (!window) {
        return;
    }

    NSPoint windowPoint = [window convertPointFromScreen:screenPoint];
    NSTimeInterval timestamp = [[NSProcessInfo processInfo] systemUptime];
    NSInteger windowNumber = window.windowNumber;

    NSEvent *mouseDown = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                                           location:windowPoint
                                      modifierFlags:0
                                          timestamp:timestamp
                                       windowNumber:windowNumber
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

    NSEvent *mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                         location:windowPoint
                                    modifierFlags:0
                                        timestamp:timestamp + 0.05
                                     windowNumber:windowNumber
                                          context:nil
                                      eventNumber:0
                                       clickCount:1
                                         pressure:0.0];

    if (mouseDown) {
        [window sendEvent:mouseDown];
    }
    if (mouseUp) {
        [window sendEvent:mouseUp];
    }

    YMLog(@"[AutoLogin] Cocoa mouse click sent reason=%@ window=%@ title=%@ screen=(%.1f, %.1f) window=(%.1f, %.1f)",
          reason ?: @"",
          NSStringFromClass([window class]),
          window.title ?: @"",
          screenPoint.x,
          screenPoint.y,
          windowPoint.x,
          windowPoint.y);
}

static BOOL YMAutoLoginSendQuartzMouseClick(NSPoint screenPoint, NSString *reason) {
    CGPoint point = CGPointMake(screenPoint.x, screenPoint.y);

    CGEventRef move = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, point, kCGMouseButtonLeft);
    CGEventRef mouseDown = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, point, kCGMouseButtonLeft);
    CGEventRef mouseUp = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, point, kCGMouseButtonLeft);

    if (!mouseDown || !mouseUp) {
        if (move) CFRelease(move);
        if (mouseDown) CFRelease(mouseDown);
        if (mouseUp) CFRelease(mouseUp);
        YMLog(@"[AutoLogin] Quartz mouse click failed: CGEventCreateMouseEvent returned NULL reason=%@ point=(%.1f, %.1f)",
              reason ?: @"",
              screenPoint.x,
              screenPoint.y);
        return NO;
    }

    if (move) {
        CGEventPost(kCGHIDEventTap, move);
        CGEventPostToPid(getpid(), move);
    }
    CGEventPost(kCGHIDEventTap, mouseDown);
    CGEventPostToPid(getpid(), mouseDown);
    CGEventPost(kCGHIDEventTap, mouseUp);
    CGEventPostToPid(getpid(), mouseUp);

    if (move) CFRelease(move);
    CFRelease(mouseDown);
    CFRelease(mouseUp);

    YMLog(@"[AutoLogin] Quartz mouse click sent reason=%@ point=(%.1f, %.1f)",
          reason ?: @"",
          screenPoint.x,
          screenPoint.y);
    return YES;
}

static BOOL YMAutoLoginTryClickAccessibilityFrame(id object,
                                                  NSString *role,
                                                  NSString *text,
                                                  const char *source) {
    NSRect frame = YMAutoLoginAccessibilityFrameFromObject(object);
    if (NSIsEmptyRect(frame)) {
        YMLog(@"[AutoLogin] AX frame unavailable source=%s class=%@ role=%@ text=%@",
              source ?: "",
              NSStringFromClass([object class]),
              role ?: @"",
              text ?: @"");
        return NO;
    }

    NSPoint center = NSMakePoint(NSMidX(frame), NSMidY(frame));
    NSWindow *targetWindow = YMAutoLoginWindowContainingScreenPoint(center);

    [NSApp activateIgnoringOtherApps:YES];
    if (targetWindow) {
        [targetWindow makeKeyAndOrderFront:nil];
    }

    YMLog(@"[AutoLogin] try physical click AX object source=%s class=%@ role=%@ text=%@ frame=(%.1f, %.1f, %.1f, %.1f) center=(%.1f, %.1f) targetWindow=%@ title=%@",
          source ?: "",
          NSStringFromClass([object class]),
          role ?: @"",
          text ?: @"",
          frame.origin.x,
          frame.origin.y,
          frame.size.width,
          frame.size.height,
          center.x,
          center.y,
          targetWindow ? NSStringFromClass([targetWindow class]) : @"NULL",
          targetWindow.title ?: @"");

    if (targetWindow) {
        YMAutoLoginSendCocoaMouseClick(targetWindow, center, @"AXFrame center");
    }

    BOOL quartzOK = YMAutoLoginSendQuartzMouseClick(center, @"AXFrame center");
    return targetWindow != nil || quartzOK;
}

static BOOL YMAutoLoginTryPressObject(id object,
                                      NSString *role,
                                      NSString *text,
                                      const char *source) {
    if (!object) {
        return NO;
    }

    BOOL sentAXAction = NO;

    if ([object respondsToSelector:@selector(accessibilityPerformPress)]) {
        BOOL ok = NO;
        @try {
            ok = ((BOOL (*)(id, SEL))objc_msgSend)(object, @selector(accessibilityPerformPress));
        } @catch (NSException *exception) {
            ok = NO;
        }

        YMLog(@"[AutoLogin] accessibilityPerformPress source=%s class=%@ role=%@ text=%@ result=%@",
              source ?: "",
              NSStringFromClass([object class]),
              role ?: @"",
              text ?: @"",
              ok ? @"OK" : @"FAIL");

        if (ok) {
            sentAXAction = YES;
        }
    }

    if ([object respondsToSelector:@selector(accessibilityPerformAction:)]) {
        @try {
            ((void (*)(id, SEL, NSString *))objc_msgSend)(object,
                                                          @selector(accessibilityPerformAction:),
                                                          NSAccessibilityPressAction);
            sentAXAction = YES;
            YMLog(@"[AutoLogin] accessibilityPerformAction AXPress source=%s class=%@ role=%@ text=%@ sent",
                  source ?: "",
                  NSStringFromClass([object class]),
                  role ?: @"",
                  text ?: @"");
        } @catch (NSException *exception) {
            YMLog(@"[AutoLogin] accessibilityPerformAction AXPress source=%s class=%@ role=%@ text=%@ exception=%@",
                  source ?: "",
                  NSStringFromClass([object class]),
                  role ?: @"",
                  text ?: @"",
                  exception ?: @"");
        }
    }

    BOOL physicalClickOK = YMAutoLoginTryClickAccessibilityFrame(object, role, text, source);
    YMLog(@"[AutoLogin] press summary source=%s class=%@ role=%@ text=%@ axAction=%@ physicalClick=%@",
          source ?: "",
          NSStringFromClass([object class]),
          role ?: @"",
          text ?: @"",
          sentAXAction ? @"YES" : @"NO",
          physicalClickOK ? @"YES" : @"NO");

    return physicalClickOK;
}


static NSString *YMAutoLoginAccessibilityRoleFromObject(id object) {
    if (!object || ![object respondsToSelector:@selector(accessibilityRole)]) {
        return @"";
    }

    NSString *role = nil;
    @try {
        role = ((NSString *(*)(id, SEL))objc_msgSend)(object, @selector(accessibilityRole));
    } @catch (NSException *exception) {
        role = nil;
    }

    return role ?: @"";
}

static NSArray *YMAutoLoginAccessibilityChildrenFromObject(id object) {
    if (!object || ![object respondsToSelector:@selector(accessibilityChildren)]) {
        return @[];
    }

    id children = nil;
    @try {
        children = ((id (*)(id, SEL))objc_msgSend)(object, @selector(accessibilityChildren));
    } @catch (NSException *exception) {
        children = nil;
    }

    if ([children isKindOfClass:[NSArray class]]) {
        return (NSArray *)children;
    }

    if (children) {
        return @[children];
    }

    return @[];
}

static BOOL YMAutoLoginTryPressAccessibilityObject(id object,
                                                   NSMutableArray<NSString *> *debugItems,
                                                   NSMutableSet<NSValue *> *visited,
                                                   NSUInteger depth) {
    if (!object || depth > 18) {
        return NO;
    }

    NSValue *key = [NSValue valueWithNonretainedObject:object];
    if ([visited containsObject:key]) {
        return NO;
    }
    [visited addObject:key];

    NSString *role = YMAutoLoginAccessibilityRoleFromObject(object);
    NSString *text = YMAutoLoginTextFromObject(object);

    if (debugItems.count < 80 && (role.length > 0 || text.length > 0)) {
        [debugItems addObject:[NSString stringWithFormat:@"AX %@ role=%@ text=%@",
                               NSStringFromClass([object class]),
                               role ?: @"",
                               text ?: @""]];
    }

    BOOL roleLooksPressable = [role isEqualToString:NSAccessibilityButtonRole] ||
                              [role isEqualToString:NSAccessibilityRadioButtonRole] ||
                              [role isEqualToString:NSAccessibilityCheckBoxRole] ||
                              [role.lowercaseString containsString:@"button"];

    if (roleLooksPressable && YMAutoLoginTextLooksLikeLoginButton(text)) {
        YMLog(@"[AutoLogin] found login AX object class=%@ role=%@ text=%@",
              NSStringFromClass([object class]),
              role ?: @"",
              text ?: @"");
        if (YMAutoLoginTryPressObject(object, role, text, "AX")) {
            return YES;
        }
    }

    NSArray *children = YMAutoLoginAccessibilityChildrenFromObject(object);
    for (id child in children) {
        if (YMAutoLoginTryPressAccessibilityObject(child, debugItems, visited, depth + 1)) {
            return YES;
        }
    }

    return NO;
}

static BOOL YMAutoLoginTrySendReturnKeyNow(void) {
    if (atomic_exchange(&YMAutoLoginReturnKeySent, true)) {
        return NO;
    }

    if (![NSThread isMainThread]) {
        __block BOOL ok = NO;
        dispatch_sync(dispatch_get_main_queue(), ^{
            ok = YMAutoLoginTrySendReturnKeyNow();
        });
        return ok;
    }

    NSWindow *targetWindow = [NSApp keyWindow];
    if (!targetWindow || !targetWindow.isVisible) {
        for (NSWindow *window in [NSApp.windows copy]) {
            if (!window.isVisible) {
                continue;
            }
            NSString *className = NSStringFromClass([window class]);
            if ([className containsString:@"QNSWindow"] || [window.title containsString:@"微信"]) {
                targetWindow = window;
                break;
            }
        }
    }

    if (targetWindow) {
        [NSApp activateIgnoringOtherApps:YES];
        [targetWindow makeKeyAndOrderFront:nil];
    }

    CGEventRef keyDown = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)36, true);  // Return
    CGEventRef keyUp = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)36, false);
    if (!keyDown || !keyUp) {
        if (keyDown) CFRelease(keyDown);
        if (keyUp) CFRelease(keyUp);
        YMLog(@"[AutoLogin] send Return key failed: CGEventCreateKeyboardEvent returned NULL");
        return NO;
    }

    CGEventPostToPid(getpid(), keyDown);
    CGEventPostToPid(getpid(), keyUp);
    CFRelease(keyDown);
    CFRelease(keyUp);

    YMLog(@"[AutoLogin] send Return key fallback to login window=%@ title=%@",
          targetWindow ? NSStringFromClass([targetWindow class]) : @"NULL",
          targetWindow.title ?: @"");
    return YES;
}

static BOOL YMAutoLoginTryPressView(NSView *view, NSMutableArray<NSString *> *debugItems, NSUInteger depth) {
    if (!view || depth > 12) {
        return NO;
    }

    if (!YMAutoLoginViewVisible(view)) {
        return NO;
    }

    NSString *text = YMAutoLoginTextFromObject(view);
    if (debugItems.count < 40 && text.length > 0) {
        [debugItems addObject:[NSString stringWithFormat:@"%@ text=%@ hidden=%d",
                               NSStringFromClass([view class]),
                               text,
                               view.hidden ? 1 : 0]];
    }

    if ([view isKindOfClass:[NSButton class]]) {
        NSButton *button = (NSButton *)view;
        if (button.enabled && YMAutoLoginTextLooksLikeLoginButton(text)) {
            YMLog(@"[AutoLogin] performClick login button class=%@ text=%@",
                  NSStringFromClass([button class]),
                  text ?: @"");
            [button performClick:nil];
            return YES;
        }
    }

    if (YMAutoLoginTextLooksLikeLoginButton(text)) {
        NSString *role = YMAutoLoginAccessibilityRoleFromObject(view);
        if (YMAutoLoginTryPressObject(view, role, text, "NSView")) {
            return YES;
        }
    }

    NSArray<NSView *> *subviews = [view.subviews copy];
    for (NSView *subview in subviews) {
        if (YMAutoLoginTryPressView(subview, debugItems, depth + 1)) {
            return YES;
        }
    }

    return NO;
}

static BOOL YMAutoLoginTryPressLoginButtonNow(void) {
    if (atomic_load(&YMAutoLoginClickSucceeded)) {
        return YES;
    }

    if (![NSThread isMainThread]) {
        __block BOOL ok = NO;
        dispatch_sync(dispatch_get_main_queue(), ^{
            ok = YMAutoLoginTryPressLoginButtonNow();
        });
        return ok;
    }

    NSMutableArray<NSString *> *debugItems = [NSMutableArray array];
    NSArray<NSWindow *> *windows = [NSApp.windows copy];

    for (NSWindow *window in windows) {
        if (!window.isVisible) {
            continue;
        }

        NSView *contentView = window.contentView;
        if (!contentView) {
            continue;
        }

        if (debugItems.count < 40) {
            [debugItems addObject:[NSString stringWithFormat:@"window=%@ title=%@",
                                   NSStringFromClass([window class]),
                                   window.title ?: @""]];
        }

        if (YMAutoLoginTryPressView(contentView, debugItems, 0)) {
            atomic_store(&YMAutoLoginClickSucceeded, true);
            return YES;
        }

        NSMutableSet<NSValue *> *visited = [NSMutableSet set];
        if (YMAutoLoginTryPressAccessibilityObject(contentView, debugItems, visited, 0)) {
            atomic_store(&YMAutoLoginClickSucceeded, true);
            return YES;
        }

        if (YMAutoLoginTryPressAccessibilityObject(window, debugItems, visited, 0)) {
            atomic_store(&YMAutoLoginClickSucceeded, true);
            return YES;
        }
    }

    YMLog(@"[AutoLogin] login button not found. windows=%lu candidates=%@",
          (unsigned long)windows.count,
          [debugItems componentsJoinedByString:@" | "] ?: @"");

    if (YMAutoLoginTrySendReturnKeyNow()) {
        YMLog(@"[AutoLogin] fallback Return key sent after button miss, but keep retrying because Return may be ignored by Qt login page");
        return NO;
    }

    return NO;
}

static void YMAutoLoginScheduleLoginClick(void) {
    if (!YMIsAutoLoginEnabled()) {
        return;
    }

    if (atomic_load(&YMAutoLoginClickSucceeded)) {
        return;
    }

    int oldAttempt = atomic_fetch_add(&YMAutoLoginClickAttemptCount, 1);
    if (oldAttempt >= 12) {
        YMLog(@"[AutoLogin] scanner reached max attempts, stop");
        return;
    }

    // 启动初期 QNSWindow / QMacAccessibilityElement 创建有先后顺序，
    // 第一轮稍微等一下，后面逐步拉开间隔。
    NSTimeInterval delay = 0.35 + 0.30 * oldAttempt;

    YMLog(@"[AutoLogin] schedule scanner attempt=%d delay=%.2f",
          oldAttempt + 1,
          delay);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        if (!YMIsAutoLoginEnabled() || atomic_load(&YMAutoLoginClickSucceeded)) {
            return;
        }

        BOOL ok = YMAutoLoginTryPressLoginButtonNow();
        YMLog(@"[AutoLogin] scanner attempt result=%@", ok ? @"OK" : @"MISS");

        if (!ok && !atomic_load(&YMAutoLoginClickSucceeded) && atomic_load(&YMAutoLoginClickAttemptCount) < 12) {
            YMAutoLoginScheduleLoginClick();
        }
    });
}

static void YMStartAutoLoginScanner(void) {
    if (!YMIsAutoLoginEnabled()) {
        YMLog(@"auto login disabled, skip");
        return;
    }

    if (atomic_exchange(&YMAutoLoginScannerStarted, true)) {
        YMLog(@"[AutoLogin] scanner already started, skip");
        return;
    }

    atomic_store(&YMAutoLoginClickSucceeded, false);
    atomic_store(&YMAutoLoginReturnKeySent, false);
    atomic_store(&YMAutoLoginClickAttemptCount, 0);

    YMLog(@"[AutoLogin] start AX scanner without WeChat internal hook");
    YMAutoLoginScheduleLoginClick();
}


@implementation AutoLogin

+ (void)startWithEnabled:(BOOL)enabled {
    atomic_store(&YMAutoLoginEnabled, enabled ? true : false);

    if (!enabled) {
        YMLog(@"auto login disabled, skip");
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        YMStartAutoLoginScanner();
    });
}

+ (void)reset {
    atomic_store(&YMAutoLoginScannerStarted, false);
    atomic_store(&YMAutoLoginClickSucceeded, false);
    atomic_store(&YMAutoLoginReturnKeySent, false);
    atomic_store(&YMAutoLoginClickAttemptCount, 0);
}

@end
