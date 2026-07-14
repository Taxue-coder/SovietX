//
//  NSObject+MainHook.m
//  SovietX
//
//  Created by MustangYM on 2026/6/13.
//

#import "NSObject+MainHook.h"
#import "YMSwizzledHelper.h"
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#import <mach/mach.h>
#import <mach-o/dyld.h>
#import <mach-o/loader.h>
#import <libkern/OSCacheControl.h>
#import <unistd.h>

@implementation NSObject (MainHook)


+ (void)startHook
{
 
//    hookClassMethod(objc_getClass("NSRunningApplication"),
//                       @selector(runningApplicationsWithBundleIdentifier:),
//                       [self class],
//                       @selector(hook_runningApplicationsWithBundleIdentifier:));
    
}


@end
