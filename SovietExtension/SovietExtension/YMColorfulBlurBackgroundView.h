//
//  YMColorfulBlurBackgroundView.h
//  SovietExtension
//
//  Created by MustangYM on 2026/7/2.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/// 用 identifier 标记我们插入的动态渐变背景 view，避免使用 readonly 的 tag。
static NSString * const kYMColorfulBlurBackgroundViewIdentifier = @"SovietExtension.Misty.ColorfulBlurBackgroundView";
@interface YMColorfulBlurBackgroundView : NSView
@property (nonatomic, strong) CALayer *ym_colorRootLayer;
@property (nonatomic, assign) BOOL ym_hasBuiltLayers;
@property (nonatomic, assign) BOOL ym_lastDarkStyle;
@property (nonatomic, assign) NSSize ym_lastSize;
- (void)ym_updateColorfulBackgroundIfNeeded;
@end

NS_ASSUME_NONNULL_END
