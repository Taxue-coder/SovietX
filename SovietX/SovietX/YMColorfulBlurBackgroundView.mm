//
//  YMColorfulBlurBackgroundView.m
//  SovietX
//
//  Created by MustangYM on 2026/7/2.
//

#import "YMColorfulBlurBackgroundView.h"
#import <CoreImage/CoreImage.h>
#import <QuartzCore/QuartzCore.h>
#import <math.h>
#import "ThemeHook.h"

static id YMCalibratedColor(CGFloat r, CGFloat g, CGFloat b, CGFloat a) {
    NSColor *color = [NSColor colorWithCalibratedRed:r green:g blue:b alpha:a];
    return (__bridge id)color.CGColor;
}

@implementation YMColorfulBlurBackgroundView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        self.layer.opaque = NO;
        self.layer.masksToBounds = YES;
        self.layer.backgroundColor = NSColor.clearColor.CGColor;
        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        self.identifier = kYMColorfulBlurBackgroundViewIdentifier;
        self.alphaValue = 1.0;
    }
    return self;
}

- (BOOL)isOpaque {
    return NO;
}

- (NSView *)hitTest:(NSPoint)point {
    // 背景 view 不拦截任何鼠标事件。
    return nil;
}

- (void)setFrame:(NSRect)frame {
    [super setFrame:frame];
    [self ym_updateColorfulBackgroundIfNeeded];
}

- (void)layout {
    [super layout];
    [self ym_updateColorfulBackgroundIfNeeded];
}

- (NSArray<NSValue *> *)ym_pathValuesForBlobIndex:(NSUInteger)index
                                             root:(CGRect)rootBounds
                                             size:(CGFloat)blobSize {
    CGFloat w = CGRectGetWidth(rootBounds);
    CGFloat h = CGRectGetHeight(rootBounds);

    NSArray<NSArray<NSNumber *> *> *points = nil;
    switch (index % 6) {
        case 0:
            points = @[@[@0.12, @0.18], @[@0.72, @0.10], @[@0.86, @0.66], @[@0.22, @0.82], @[@0.12, @0.18]];
            break;
        case 1:
            points = @[@[@0.84, @0.20], @[@0.24, @0.30], @[@0.16, @0.74], @[@0.76, @0.88], @[@0.84, @0.20]];
            break;
        case 2:
            points = @[@[@0.50, @0.08], @[@0.90, @0.42], @[@0.54, @0.92], @[@0.10, @0.48], @[@0.50, @0.08]];
            break;
        case 3:
            points = @[@[@0.20, @0.56], @[@0.48, @0.18], @[@0.88, @0.52], @[@0.50, @0.80], @[@0.20, @0.56]];
            break;
        case 4:
            points = @[@[@0.68, @0.72], @[@0.36, @0.86], @[@0.18, @0.36], @[@0.72, @0.22], @[@0.68, @0.72]];
            break;
        default:
            points = @[@[@0.36, @0.28], @[@0.64, @0.36], @[@0.82, @0.78], @[@0.28, @0.70], @[@0.36, @0.28]];
            break;
    }

    NSMutableArray<NSValue *> *values = [NSMutableArray arrayWithCapacity:points.count];
    for (NSArray<NSNumber *> *pair in points) {
        CGFloat x = pair[0].doubleValue * w;
        CGFloat y = pair[1].doubleValue * h;
        [values addObject:[NSValue valueWithPoint:NSMakePoint(x, y)]];
    }
    return values;
}

- (NSArray *)ym_colorsForBlobIndex:(NSUInteger)index dark:(BOOL)dark {
    if (dark) {
        NSArray *palette = @[
            YMCalibratedColor(0.22, 0.35, 1.00, 0.95),
            YMCalibratedColor(0.78, 0.22, 0.95, 0.90),
            YMCalibratedColor(0.04, 0.78, 0.86, 0.88),
            YMCalibratedColor(1.00, 0.42, 0.18, 0.82),
            YMCalibratedColor(0.30, 0.94, 0.56, 0.76),
            YMCalibratedColor(0.95, 0.20, 0.52, 0.82),
        ];
        id c0 = palette[index % palette.count];
        id c1 = palette[(index + 2) % palette.count];
        id c2 = palette[(index + 4) % palette.count];
        return @[c0, c1, c2, c0];
    } else {
        NSArray *palette = @[
            YMCalibratedColor(0.68, 0.82, 1.00, 0.80),
            YMCalibratedColor(1.00, 0.72, 0.92, 0.72),
            YMCalibratedColor(0.76, 1.00, 0.90, 0.70),
            YMCalibratedColor(1.00, 0.88, 0.58, 0.68),
            YMCalibratedColor(0.78, 0.72, 1.00, 0.72),
            YMCalibratedColor(0.62, 0.96, 1.00, 0.70),
        ];
        id c0 = palette[index % palette.count];
        id c1 = palette[(index + 2) % palette.count];
        id c2 = palette[(index + 4) % palette.count];
        return @[c0, c1, c2, c0];
    }
}

- (void)ym_rebuildColorfulLayersWithDarkStyle:(BOOL)dark {
    [self.ym_colorRootLayer removeFromSuperlayer];
    self.ym_colorRootLayer = nil;

    CGFloat opacity = YMColorfulBlurBackgroundOpacity();
    CGFloat blurRadius = MAX(0.0, YMColorfulBlurInternalBlurRadius());
    NSTimeInterval animationDuration = MAX(2.0, YMColorfulAnimationDuration());

    CGFloat inset = MAX(120.0, blurRadius * 2.0);
    CGRect rootFrame = NSInsetRect(self.bounds, -inset, -inset);

    CALayer *root = [CALayer layer];
    root.frame = rootFrame;
    root.masksToBounds = NO;
    root.opaque = NO;
    root.backgroundColor = NSColor.clearColor.CGColor;
    root.opacity = opacity;

    if (blurRadius > 0.0) {
        CIFilter *blur = [CIFilter filterWithName:@"CIGaussianBlur"];
        [blur setDefaults];
        [blur setValue:@(blurRadius) forKey:kCIInputRadiusKey];
        root.filters = @[blur];
    } else {
        root.filters = nil;
    }

    CGFloat maxSide = MAX(CGRectGetWidth(root.bounds), CGRectGetHeight(root.bounds));
    CGFloat baseSize = MAX(280.0, maxSide * 0.58);

    for (NSUInteger i = 0; i < 6; i++) {
        CGFloat blobSize = baseSize * (0.72 + 0.10 * (CGFloat)(i % 3));

        CALayer *blob = [CALayer layer];
        blob.bounds = CGRectMake(0, 0, blobSize, blobSize);
        blob.cornerRadius = blobSize / 2.0;
        blob.masksToBounds = YES;
        blob.opaque = NO;
        blob.backgroundColor = (__bridge CGColorRef)([self ym_colorsForBlobIndex:i dark:dark].firstObject);
        blob.opacity = 1.0;

        NSArray<NSValue *> *pathValues = [self ym_pathValuesForBlobIndex:i root:root.bounds size:blobSize];
        blob.position = pathValues.firstObject.pointValue;

        CAKeyframeAnimation *move = [CAKeyframeAnimation animationWithKeyPath:@"position"];
        move.values = pathValues;
        move.duration = animationDuration + (NSTimeInterval)i * 1.75;
        move.repeatCount = HUGE_VALF;
        move.autoreverses = YES;
        move.calculationMode = kCAAnimationCubicPaced;
        move.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        move.removedOnCompletion = NO;
        [blob addAnimation:move forKey:@"ym_colorful_move"];

        CAKeyframeAnimation *color = [CAKeyframeAnimation animationWithKeyPath:@"backgroundColor"];
        color.values = [self ym_colorsForBlobIndex:i dark:dark];
        color.duration = animationDuration * 1.15 + (NSTimeInterval)i;
        color.repeatCount = HUGE_VALF;
        color.autoreverses = YES;
        color.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        color.removedOnCompletion = NO;
        [blob addAnimation:color forKey:@"ym_colorful_color"];

        CABasicAnimation *scale = [CABasicAnimation animationWithKeyPath:@"transform.scale"];
        scale.fromValue = @(0.92 + 0.02 * (CGFloat)i);
        scale.toValue = @(1.18 - 0.015 * (CGFloat)i);
        scale.duration = animationDuration * 0.72 + (NSTimeInterval)i;
        scale.repeatCount = HUGE_VALF;
        scale.autoreverses = YES;
        scale.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        scale.removedOnCompletion = NO;
        [blob addAnimation:scale forKey:@"ym_colorful_scale"];

        [root addSublayer:blob];
    }

    [self.layer addSublayer:root];
    self.ym_colorRootLayer = root;
    self.ym_hasBuiltLayers = YES;
    self.ym_lastDarkStyle = dark;
    self.ym_lastSize = self.bounds.size;
    self.ym_lastOpacity = opacity;
    self.ym_lastInternalBlurRadius = blurRadius;
    self.ym_lastAnimationDuration = animationDuration;
}

- (void)ym_updateColorfulBackgroundIfNeeded {
    if (!self.layer) return;

    if (!YMColorfulBlurBackgroundEnabled()) {
        self.hidden = YES;
        return;
    }

    self.hidden = NO;

    BOOL dark = YMCarrierStyleIsDark();
    NSSize size = self.bounds.size;
    CGFloat opacity = YMColorfulBlurBackgroundOpacity();
    CGFloat blurRadius = YMColorfulBlurInternalBlurRadius();
    NSTimeInterval animationDuration = YMColorfulAnimationDuration();

    BOOL sizeChanged = fabs(size.width - self.ym_lastSize.width) > 2.0 ||
                       fabs(size.height - self.ym_lastSize.height) > 2.0;
    BOOL styleChanged = self.ym_hasBuiltLayers && self.ym_lastDarkStyle != dark;
    BOOL opacityChanged = self.ym_hasBuiltLayers && fabs(opacity - self.ym_lastOpacity) > 0.001;
    BOOL blurChanged = self.ym_hasBuiltLayers && fabs(blurRadius - self.ym_lastInternalBlurRadius) > 0.5;
    BOOL animationChanged = self.ym_hasBuiltLayers && fabs(animationDuration - self.ym_lastAnimationDuration) > 0.05;

    // 柔化半径和动画速度影响 layer/filter/animation，需要重建。
    // 透明度虽然可以直接改 root.opacity，但为了和设置状态完全一致，也一起走统一刷新。
    if (!self.ym_hasBuiltLayers || sizeChanged || styleChanged || opacityChanged || blurChanged || animationChanged) {
        [self ym_rebuildColorfulLayersWithDarkStyle:dark];
    } else {
        CGFloat inset = MAX(120.0, blurRadius * 2.0);
        self.ym_colorRootLayer.frame = NSInsetRect(self.bounds, -inset, -inset);
        self.ym_colorRootLayer.opacity = opacity;
    }
}

@end
