//
//  YMColorfulBlurBackgroundView.m
//  SovietExtension
//
//  Created by MustangYM on 2026/7/2.
//

#import "YMColorfulBlurBackgroundView.h"
#import <CoreImage/CoreImage.h>
#import <QuartzCore/QuartzCore.h>
#import "ThemeHook.h"

/// Colorful 背景整体透明度。
static const CGFloat kYMColorfulOpacityDark = 0.42f;
static const CGFloat kYMColorfulOpacityLight = 0.26f;

/// Colorful 自身的柔化半径。
static const CGFloat kYMColorfulInternalBlurRadius = 70.0f;

/// 动态渐变动画速度。数值越大越慢。
static const NSTimeInterval kYMColorfulAnimationDuration = 10.0;

static CGColorRef YMCalibratedColor(CGFloat r, CGFloat g, CGFloat b, CGFloat a) {
    return [NSColor colorWithCalibratedRed:r green:g blue:b alpha:a].CGColor;
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
        // rootLayer 比 view 大，这里直接使用 root 坐标系即可。
        [values addObject:[NSValue valueWithPoint:NSMakePoint(x, y)]];
    }
    return values;
}

- (NSArray *)ym_colorsForBlobIndex:(NSUInteger)index dark:(BOOL)dark {
    if (dark) {
        NSArray *palette = @[
            (__bridge id)YMCalibratedColor(0.22, 0.35, 1.00, 0.95),
            (__bridge id)YMCalibratedColor(0.78, 0.22, 0.95, 0.90),
            (__bridge id)YMCalibratedColor(0.04, 0.78, 0.86, 0.88),
            (__bridge id)YMCalibratedColor(1.00, 0.42, 0.18, 0.82),
            (__bridge id)YMCalibratedColor(0.30, 0.94, 0.56, 0.76),
            (__bridge id)YMCalibratedColor(0.95, 0.20, 0.52, 0.82),
        ];
        id c0 = palette[index % palette.count];
        id c1 = palette[(index + 2) % palette.count];
        id c2 = palette[(index + 4) % palette.count];
        return @[c0, c1, c2, c0];
    } else {
        NSArray *palette = @[
            (__bridge id)YMCalibratedColor(0.68, 0.82, 1.00, 0.80),
            (__bridge id)YMCalibratedColor(1.00, 0.72, 0.92, 0.72),
            (__bridge id)YMCalibratedColor(0.76, 1.00, 0.90, 0.70),
            (__bridge id)YMCalibratedColor(1.00, 0.88, 0.58, 0.68),
            (__bridge id)YMCalibratedColor(0.78, 0.72, 1.00, 0.72),
            (__bridge id)YMCalibratedColor(0.62, 0.96, 1.00, 0.70),
        ];
        id c0 = palette[index % palette.count];
        id c1 = palette[(index + 2) % palette.count];
        id c2 = palette[(index + 4) % palette.count];
        return @[c0, c1, c2, c0];
    }
}

- (void)ym_rebuildColorfulLayersWithDarkStyle:(BOOL)dark {
    [self.ym_colorRootLayer removeFromSuperlayer];

    CGFloat blurRadius = MAX(0.0, kYMColorfulInternalBlurRadius);
    CGFloat inset = MAX(120.0, blurRadius * 2.0);
    CGRect rootFrame = NSInsetRect(self.bounds, -inset, -inset);

    CALayer *root = [CALayer layer];
    root.frame = rootFrame;
    root.masksToBounds = NO;
    root.opaque = NO;
    root.backgroundColor = NSColor.clearColor.CGColor;
    root.opacity = dark ? kYMColorfulOpacityDark : kYMColorfulOpacityLight;

    if (blurRadius > 0.0) {
        CIFilter *blur = [CIFilter filterWithName:@"CIGaussianBlur"];
        [blur setDefaults];
        [blur setValue:@(blurRadius) forKey:kCIInputRadiusKey];
        root.filters = @[blur];
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
        move.duration = kYMColorfulAnimationDuration + (NSTimeInterval)i * 1.75;
        move.repeatCount = HUGE_VALF;
        move.autoreverses = YES;
        move.calculationMode = kCAAnimationCubicPaced;
        move.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        move.removedOnCompletion = NO;
        [blob addAnimation:move forKey:@"ym_colorful_move"];

        CAKeyframeAnimation *color = [CAKeyframeAnimation animationWithKeyPath:@"backgroundColor"];
        color.values = [self ym_colorsForBlobIndex:i dark:dark];
        color.duration = kYMColorfulAnimationDuration * 1.15 + (NSTimeInterval)i;
        color.repeatCount = HUGE_VALF;
        color.autoreverses = YES;
        color.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        color.removedOnCompletion = NO;
        [blob addAnimation:color forKey:@"ym_colorful_color"];

        CABasicAnimation *scale = [CABasicAnimation animationWithKeyPath:@"transform.scale"];
        scale.fromValue = @(0.92 + 0.02 * (CGFloat)i);
        scale.toValue = @(1.18 - 0.015 * (CGFloat)i);
        scale.duration = kYMColorfulAnimationDuration * 0.72 + (NSTimeInterval)i;
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
    BOOL sizeChanged = fabs(size.width - self.ym_lastSize.width) > 2.0 ||
                       fabs(size.height - self.ym_lastSize.height) > 2.0;
    BOOL styleChanged = self.ym_hasBuiltLayers && self.ym_lastDarkStyle != dark;

    if (!self.ym_hasBuiltLayers || sizeChanged || styleChanged) {
        [self ym_rebuildColorfulLayersWithDarkStyle:dark];
    } else {
        CGFloat blurRadius = MAX(0.0, kYMColorfulInternalBlurRadius);
        CGFloat inset = MAX(120.0, blurRadius * 2.0);
        self.ym_colorRootLayer.frame = NSInsetRect(self.bounds, -inset, -inset);
        self.ym_colorRootLayer.opacity = dark ? kYMColorfulOpacityDark : kYMColorfulOpacityLight;
    }
}


@end
