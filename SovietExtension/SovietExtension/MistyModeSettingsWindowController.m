//
//  MistyModeSettingsWindowController.m
//  SovietExtension
//
//  Created by MustangYM on 2026/7/2.
//

#import "MistyModeSettingsWindowController.h"
#import <QuartzCore/QuartzCore.h>
#import <math.h>
#import "MenuManager.h"

@interface MistyModeSettingsWindowController ()
@property (nonatomic, strong) NSSlider *alphaSlider;
@property (nonatomic, strong) NSTextField *alphaValueLabel;
@property (nonatomic, strong) NSSlider *blurRadiusSlider;
@property (nonatomic, strong) NSTextField *blurRadiusValueLabel;
@property (nonatomic, strong) NSButton *enableBlurCheckbox;
@property (nonatomic, strong) NSButton *colorfulCheckbox;
@property (nonatomic, strong) NSPopUpButton *carrierStylePopup;
@property (nonatomic, strong) NSButton *keepAliveCheckbox;
@end

@implementation MistyModeSettingsWindowController

#pragma mark - Defaults

+ (void)registerDefaults
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        kThemeMistyMode: @(NO),
        kThemeMistyQNSAlpha: @(0.90),
        kThemeMistyWindowBlurEnabled: @(YES),
        kThemeMistyWindowBlurRadius: @(10),
        kThemeMistyColorful: @(NO),
        kThemeMistyCarrierStyle: kThemeMistyCarrierStyleDark,
        kThemeMistyKeepAlive: @(YES),
    }];
}

#pragma mark - Init

- (instancetype)init
{
    NSPanel *panel = [MistyModeSettingsWindowController ym_createPanel];
    self = [super initWithWindow:panel];
    if (self) {
        [self ym_buildInterfaceInView:panel.contentView];
        [self loadSettingsToControls];
    }
    return self;
}

+ (NSPanel *)ym_createPanel
{
    NSRect frame = NSMakeRect(0, 0, 560, 580);
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    NSPanel *panel = [[NSPanel alloc] initWithContentRect:frame
                                                 styleMask:styleMask
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
    panel.title = @"迷离模式";
    panel.releasedWhenClosed = NO;
    panel.movableByWindowBackground = YES;
    panel.level = NSFloatingWindowLevel;
    panel.backgroundColor = [NSColor colorWithCalibratedWhite:0.08 alpha:0.96];
    panel.opaque = NO;

    NSView *contentView = [[NSView alloc] initWithFrame:frame];
    contentView.wantsLayer = YES;
    contentView.layer.backgroundColor = [NSColor colorWithCalibratedWhite:0.06 alpha:0.96].CGColor;
    panel.contentView = contentView;

    return panel;
}

#pragma mark - Public

- (void)showWindowCentered
{
    [MistyModeSettingsWindowController registerDefaults];
    [self loadSettingsToControls];

    if (!self.window.isVisible) {
        [self.window center];
    }

    [NSApp activateIgnoringOtherApps:YES];
    [self.window makeKeyAndOrderFront:nil];
}

#pragma mark - UI

- (void)ym_buildInterfaceInView:(NSView *)contentView
{
    NSVisualEffectView *effect = [[NSVisualEffectView alloc] initWithFrame:contentView.bounds];
    effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    effect.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    effect.material = NSVisualEffectMaterialUnderWindowBackground;
    effect.state = NSVisualEffectStateActive;
    [contentView addSubview:effect];

    NSTextField *titleLabel = [self ym_labelWithFrame:NSMakeRect(32, 524, 300, 28)
                                                text:@"迷离模式"
                                                font:[NSFont systemFontOfSize:22 weight:NSFontWeightSemibold]
                                               color:[NSColor colorWithCalibratedWhite:0.98 alpha:1.0]];
    [contentView addSubview:titleLabel];

    NSTextField *subtitleLabel = [self ym_labelWithFrame:NSMakeRect(32, 496, 460, 20)
                                                   text:@"调节透明、模糊与流光氛围"
                                                   font:[NSFont systemFontOfSize:12 weight:NSFontWeightRegular]
                                                  color:[NSColor colorWithCalibratedWhite:0.72 alpha:1.0]];
    [contentView addSubview:subtitleLabel];

    NSView *card = [self ym_cardViewWithFrame:NSMakeRect(24, 86, 512, 390)];
    [contentView addSubview:card];

    self.enableBlurCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(26, 340, 220, 24)];
    self.enableBlurCheckbox.buttonType = NSSwitchButton;
    self.enableBlurCheckbox.title = @"启用背景模糊";
    self.enableBlurCheckbox.font = [NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
    self.enableBlurCheckbox.target = self;
    self.enableBlurCheckbox.action = @selector(themeCheckboxChanged:);
    [card addSubview:self.enableBlurCheckbox];
    [card addSubview:[self ym_labelWithFrame:NSMakeRect(48, 318, 420, 18)
                                        text:@"开启后让窗口背后的桌面产生柔和虚化；关闭后仅保留界面透明。"
                                        font:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]
                                       color:[NSColor colorWithCalibratedWhite:0.62 alpha:1.0]]];

    self.colorfulCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(26, 286, 220, 24)];
    self.colorfulCheckbox.buttonType = NSSwitchButton;
    self.colorfulCheckbox.title = @"启用流光氛围";
    self.colorfulCheckbox.font = [NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
    self.colorfulCheckbox.target = self;
    self.colorfulCheckbox.action = @selector(themeCheckboxChanged:);
    [card addSubview:self.colorfulCheckbox];
    [card addSubview:[self ym_labelWithFrame:NSMakeRect(48, 264, 420, 18)
                                        text:@"一种很牛批的效果，几乎没有多余性能损耗。"
                                        font:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]
                                       color:[NSColor colorWithCalibratedWhite:0.62 alpha:1.0]]];

    [card addSubview:[self ym_labelWithFrame:NSMakeRect(26, 226, 120, 20)
                                        text:@"界面透明度"
                                        font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                       color:[NSColor colorWithCalibratedWhite:0.92 alpha:1.0]]];
    self.alphaValueLabel = [self ym_labelWithFrame:NSMakeRect(430, 226, 52, 20)
                                             text:@"90%"
                                             font:[NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightMedium]
                                            color:[NSColor colorWithCalibratedWhite:0.86 alpha:1.0]];
    self.alphaValueLabel.alignment = NSTextAlignmentRight;
    [card addSubview:self.alphaValueLabel];

    self.alphaSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(26, 200, 456, 24)];
    self.alphaSlider.minValue = 0.60;
    self.alphaSlider.maxValue = 1.00;
    self.alphaSlider.target = self;
    self.alphaSlider.action = @selector(alphaSliderChanged:);
    [card addSubview:self.alphaSlider];
    [card addSubview:[self ym_labelWithFrame:NSMakeRect(26, 180, 456, 18)
                                        text:@"数值越低，底部模糊与流光越明显；推荐 85% ~ 95%。"
                                        font:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]
                                       color:[NSColor colorWithCalibratedWhite:0.62 alpha:1.0]]];

    [card addSubview:[self ym_labelWithFrame:NSMakeRect(26, 142, 120, 20)
                                        text:@"模糊程度"
                                        font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                       color:[NSColor colorWithCalibratedWhite:0.92 alpha:1.0]]];
    self.blurRadiusValueLabel = [self ym_labelWithFrame:NSMakeRect(430, 142, 52, 20)
                                                  text:@"10"
                                                  font:[NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightMedium]
                                                 color:[NSColor colorWithCalibratedWhite:0.86 alpha:1.0]];
    self.blurRadiusValueLabel.alignment = NSTextAlignmentRight;
    [card addSubview:self.blurRadiusValueLabel];

    self.blurRadiusSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(26, 116, 456, 24)];
    self.blurRadiusSlider.minValue = 0;
    self.blurRadiusSlider.maxValue = 80;
    self.blurRadiusSlider.target = self;
    self.blurRadiusSlider.action = @selector(blurRadiusSliderChanged:);
    [card addSubview:self.blurRadiusSlider];
    [card addSubview:[self ym_labelWithFrame:NSMakeRect(26, 96, 456, 18)
                                        text:@"数值越大越朦胧；推荐 10，过高会让背景细节变少。"
                                        font:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]
                                       color:[NSColor colorWithCalibratedWhite:0.62 alpha:1.0]]];

    [card addSubview:[self ym_labelWithFrame:NSMakeRect(26, 58, 120, 22)
                                        text:@"风格"
                                        font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                       color:[NSColor colorWithCalibratedWhite:0.92 alpha:1.0]]];
    self.carrierStylePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(150, 53, 180, 30) pullsDown:NO];
    [self.carrierStylePopup addItemsWithTitles:@[@"深色", @"浅色"]];
    [card addSubview:self.carrierStylePopup];
    [card addSubview:[self ym_labelWithFrame:NSMakeRect(26, 34, 456, 18)
                                        text:@"深色更沉稳，浅色更通透；会自动匹配内部承载层强度。"
                                        font:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]
                                       color:[NSColor colorWithCalibratedWhite:0.62 alpha:1.0]]];

    self.keepAliveCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(26, 8, 260, 24)];
    self.keepAliveCheckbox.buttonType = NSSwitchButton;
    self.keepAliveCheckbox.title = @"窗口重建后自动保持效果";
    self.keepAliveCheckbox.font = [NSFont systemFontOfSize:12 weight:NSFontWeightRegular];
    [card addSubview:self.keepAliveCheckbox];

    NSButton *cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(326, 28, 88, 34)];
    cancelButton.title = @"关闭主题";
    cancelButton.bezelStyle = NSBezelStyleRounded;
    cancelButton.target = self;
    cancelButton.action = @selector(cancelMistySettings:);
    [contentView addSubview:cancelButton];

    NSButton *confirmButton = [[NSButton alloc] initWithFrame:NSMakeRect(424, 28, 88, 34)];
    confirmButton.title = @"确定配置";
    confirmButton.bezelStyle = NSBezelStyleRounded;
    confirmButton.keyEquivalent = @"\r";
    confirmButton.target = self;
    confirmButton.action = @selector(confirmMistySettings:);
    [contentView addSubview:confirmButton];
}

- (NSView *)ym_cardViewWithFrame:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    view.wantsLayer = YES;
    view.layer.cornerRadius = 18.0;
    view.layer.masksToBounds = YES;
    view.layer.borderWidth = 1.0;
    view.layer.borderColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.10].CGColor;
    view.layer.backgroundColor = [NSColor colorWithCalibratedWhite:0.12 alpha:0.78].CGColor;
    return view;
}

- (NSTextField *)ym_labelWithFrame:(NSRect)frame
                              text:(NSString *)text
                              font:(NSFont *)font
                             color:(NSColor *)color
{
    NSTextField *label = [[NSTextField alloc] initWithFrame:frame];
    label.stringValue = text ?: @"";
    label.font = font;
    label.textColor = color;
    label.bezeled = NO;
    label.drawsBackground = NO;
    label.editable = NO;
    label.selectable = NO;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    return label;
}

#pragma mark - Settings

- (void)loadSettingsToControls
{
    [MistyModeSettingsWindowController registerDefaults];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    self.enableBlurCheckbox.state = [defaults boolForKey:kThemeMistyWindowBlurEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    self.colorfulCheckbox.state = [defaults boolForKey:kThemeMistyColorful] ? NSControlStateValueOn : NSControlStateValueOff;
    self.alphaSlider.doubleValue = [defaults doubleForKey:kThemeMistyQNSAlpha];
    self.blurRadiusSlider.integerValue = [defaults integerForKey:kThemeMistyWindowBlurRadius];
    self.keepAliveCheckbox.state = [defaults boolForKey:kThemeMistyKeepAlive] ? NSControlStateValueOn : NSControlStateValueOff;

    NSString *carrierStyle = [defaults stringForKey:kThemeMistyCarrierStyle];
    if ([carrierStyle isEqualToString:kThemeMistyCarrierStyleLight]) {
        [self.carrierStylePopup selectItemAtIndex:1];
    } else {
        [self.carrierStylePopup selectItemAtIndex:0];
    }

    [self updateValueLabels];
}

- (void)saveSettings:(BOOL)isOpen
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    // 点击确定后即认为用户启用迷离模式，菜单打勾。
    [defaults setBool:isOpen forKey:kThemeMistyMode];
    [defaults setDouble:self.alphaSlider.doubleValue forKey:kThemeMistyQNSAlpha];
    [defaults setBool:(self.enableBlurCheckbox.state == NSControlStateValueOn) forKey:kThemeMistyWindowBlurEnabled];
    [defaults setInteger:self.blurRadiusSlider.integerValue forKey:kThemeMistyWindowBlurRadius];
    [defaults setBool:(self.colorfulCheckbox.state == NSControlStateValueOn) forKey:kThemeMistyColorful];
    [defaults setBool:(self.keepAliveCheckbox.state == NSControlStateValueOn) forKey:kThemeMistyKeepAlive];

    NSString *carrierStyle = self.carrierStylePopup.indexOfSelectedItem == 1 ? kThemeMistyCarrierStyleLight : kThemeMistyCarrierStyleDark;
    [defaults setObject:carrierStyle forKey:kThemeMistyCarrierStyle];
    [defaults synchronize];
}

- (void)applyThemeSettingsImmediately
{
    Class themeHookClass = NSClassFromString(@"ThemeHook");
    SEL refreshSelector = @selector(refreshAllQNSViews);

    if (themeHookClass && [themeHookClass respondsToSelector:refreshSelector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        [themeHookClass performSelector:refreshSelector];
#pragma clang diagnostic pop
    }
}

#pragma mark - Actions

- (void)updateValueLabels
{
    NSInteger alphaPercent = (NSInteger)lround(self.alphaSlider.doubleValue * 100.0);
    self.alphaValueLabel.stringValue = [NSString stringWithFormat:@"%ld%%", (long)alphaPercent];
    self.blurRadiusValueLabel.stringValue = [NSString stringWithFormat:@"%ld", (long)self.blurRadiusSlider.integerValue];
}

- (void)alphaSliderChanged:(NSSlider *)sender
{
    (void)sender;
    [self updateValueLabels];
}

- (void)blurRadiusSliderChanged:(NSSlider *)sender
{
    (void)sender;
    [self updateValueLabels];
}

- (void)themeCheckboxChanged:(NSButton *)sender
{
    (void)sender;
}

- (void)cancelMistySettings:(id)sender
{
    (void)sender;
    [self saveSettings:NO];
    if (self.confirmHandler) {
        self.confirmHandler(NO);
    }
    [self.window close];
}

- (void)confirmMistySettings:(id)sender
{
    (void)sender;

    [self saveSettings:YES];
    [self applyThemeSettingsImmediately];

    if (self.confirmHandler) {
        self.confirmHandler(YES);
    }

    [self.window close];
}

@end
