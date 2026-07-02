//
//  ThemeHook.h
//  SovietExtension
//
//  Created by MustangYM on 2026/6/20.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
BOOL YMColorfulBlurBackgroundEnabled(void);
BOOL YMCarrierStyleIsDark(void);
@interface ThemeHook : NSObject
/// 启动背景图功能
+ (void)start;

/// 设置背景图路径
/// 也可以不调用，默认会从插件 bundle 或 ~/Pictures/WeChatBackground.jpg 读取
+ (void)setBackgroundImagePath:(NSString *)path;

/// 重新应用背景
+ (void)refreshAllQNSViews;
@end

NS_ASSUME_NONNULL_END
