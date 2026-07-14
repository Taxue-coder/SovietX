//
//  YMColorfulBlurBackgroundView.h
//  SovietX
//
//  Created by MustangYM on 2026/7/2.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/// 用 identifier 标记我们插入的动态渐变背景 view，避免使用 readonly 的 tag。
static NSString * const kYMColorfulBlurBackgroundViewIdentifier = @"SovietX.Misty.ColorfulBlurBackgroundView";

@interface YMColorfulBlurBackgroundView : NSView
@property (nonatomic, strong, nullable) CALayer *ym_colorRootLayer;
@property (nonatomic, assign) BOOL ym_hasBuiltLayers;
@property (nonatomic, assign) BOOL ym_lastDarkStyle;
@property (nonatomic, assign) NSSize ym_lastSize;
@property (nonatomic, assign) CGFloat ym_lastOpacity;
@property (nonatomic, assign) CGFloat ym_lastInternalBlurRadius;
@property (nonatomic, assign) NSTimeInterval ym_lastAnimationDuration;
- (void)ym_updateColorfulBackgroundIfNeeded;
@end

NS_ASSUME_NONNULL_END
