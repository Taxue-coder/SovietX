//
//  AntiUpdate.m
//  SovietX
//
//  Created by MustangYM on 2026/6/13.
//

#import "AntiUpdate.h"
#import <objc/runtime.h>
#import <objc/message.h>
#import <Foundation/Foundation.h>
#import "RevokePatch.h"

void YMDisableSparkleAutoUpdateDefaults(void) {
    NSString *bundleID = [[NSBundle mainBundle] bundleIdentifier] ?: @"";
    if (![bundleID isEqualToString:@"com.tencent.xinWeChat"]) {
        return;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    [defaults setBool:NO forKey:@"SUEnableAutomaticChecks"];
    [defaults setBool:NO forKey:@"SUAutomaticallyUpdate"];
    [defaults setBool:NO forKey:@"SUAllowsAutomaticUpdates"];
    [defaults setBool:NO forKey:@"SUSendProfileInfo"];

    // 拉长检查间隔，作为额外保险。
    [defaults setDouble:60 * 60 * 24 * 365 * 20 forKey:@"SUScheduledCheckInterval"];
    [defaults setDouble:60 * 60 * 24 * 365 * 20 forKey:@"SUScheduledImpatientCheckInterval"];

    [defaults synchronize];

    YMLog(@"Sparkle defaults disabled");
}

static void YMNoopVoidMethod(id self, SEL _cmd) {
    YMLog(@"blocked Sparkle method: %@ %@", NSStringFromClass([self class]), NSStringFromSelector(_cmd));
}

static BOOL YMReturnNoBoolMethod(id self, SEL _cmd) {
    YMLog(@"blocked Sparkle bool method: %@ %@", NSStringFromClass([self class]), NSStringFromSelector(_cmd));
    return NO;
}

static void YMSwizzleInstanceMethodToNoop(Class cls, SEL sel, IMP newIMP, const char *types) {
    if (!cls || !sel) {
        return;
    }

    Method m = class_getInstanceMethod(cls, sel);
    if (!m) {
        YMLog(@"Sparkle method not found: %@ %@", NSStringFromClass(cls), NSStringFromSelector(sel));
        return;
    }

    const char *typeEncoding = method_getTypeEncoding(m);
    method_setImplementation(m, newIMP);

    YMLog(@"Sparkle method hooked: %@ %@ types=%s",
          NSStringFromClass(cls),
          NSStringFromSelector(sel),
          typeEncoding ?: types ?: "");
}

void YMDisableSparkleByRuntimeHook(void) {
    YMLog(@"try disable Sparkle by runtime hook");

    // Sparkle 1.x
    Class SUUpdater = NSClassFromString(@"SUUpdater");
    if (SUUpdater) {
        YMLog(@"found SUUpdater");

        YMSwizzleInstanceMethodToNoop(SUUpdater,
                                      NSSelectorFromString(@"checkForUpdates:"),
                                      (IMP)YMNoopVoidMethod,
                                      "v@:@");

        YMSwizzleInstanceMethodToNoop(SUUpdater,
                                      NSSelectorFromString(@"checkForUpdatesInBackground"),
                                      (IMP)YMNoopVoidMethod,
                                      "v@:");

        YMSwizzleInstanceMethodToNoop(SUUpdater,
                                      NSSelectorFromString(@"resetUpdateCycle"),
                                      (IMP)YMNoopVoidMethod,
                                      "v@:");
    }

    // Sparkle 2.x
    Class SPUUpdater = NSClassFromString(@"SPUUpdater");
    if (SPUUpdater) {
        YMLog(@"found SPUUpdater");

        YMSwizzleInstanceMethodToNoop(SPUUpdater,
                                      NSSelectorFromString(@"startUpdater:"),
                                      (IMP)YMReturnNoBoolMethod,
                                      "B@:^@");

        YMSwizzleInstanceMethodToNoop(SPUUpdater,
                                      NSSelectorFromString(@"checkForUpdates"),
                                      (IMP)YMNoopVoidMethod,
                                      "v@:");

        YMSwizzleInstanceMethodToNoop(SPUUpdater,
                                      NSSelectorFromString(@"checkForUpdatesInBackground"),
                                      (IMP)YMNoopVoidMethod,
                                      "v@:");

        YMSwizzleInstanceMethodToNoop(SPUUpdater,
                                      NSSelectorFromString(@"resetUpdateCycle"),
                                      (IMP)YMNoopVoidMethod,
                                      "v@:");
    }

    Class SPUStandardUpdaterController = NSClassFromString(@"SPUStandardUpdaterController");
    if (SPUStandardUpdaterController) {
        YMLog(@"found SPUStandardUpdaterController");
    }
}

