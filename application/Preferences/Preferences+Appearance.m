//
//  Preferences+Appearance.m
//  Spatterlight
//

#import "Preferences+Appearance.h"
#import "Theme.h"

#import <objc/runtime.h>

NSString * const SpatterlightAppearanceOverrideKey = @"SpatterlightAppearanceOverride";

@interface Preferences ()
@property (strong) IBOutlet NSView *stylesView;
- (NSString *)themeScopeTitle;
@end

@interface Preferences (AppearancePrivate)
@property (nonatomic, strong) NSButton *darkModeSwitch;
@end

@implementation Preferences (Appearance)

#pragma mark - Associated objects

- (NSButton *)darkModeSwitch {
    return objc_getAssociatedObject(self, @selector(darkModeSwitch));
}
- (void)setDarkModeSwitch:(NSButton *)value {
    objc_setAssociatedObject(self, @selector(darkModeSwitch), value, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

#pragma mark - Override API

+ (NSString *)appearanceOverride {
    NSString *value = [[NSUserDefaults standardUserDefaults] stringForKey:SpatterlightAppearanceOverrideKey];
    if ([value isEqualToString:@"light"] || [value isEqualToString:@"dark"])
        return value;
    return nil;
}

+ (void)setAppearanceOverride:(NSString *)override {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (override.length)
        [defaults setObject:override forKey:SpatterlightAppearanceOverrideKey];
    else
        [defaults removeObjectForKey:SpatterlightAppearanceOverrideKey];
    [Preferences applyAppearanceOverrideToApp];
}

+ (void)applyAppearanceOverrideToApp {
    NSString *override = [Preferences appearanceOverride];
    if ([override isEqualToString:@"dark"]) {
        NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    } else if ([override isEqualToString:@"light"]) {
        NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    } else {
        NSApp.appearance = nil;
    }
}

+ (kAppearanceType)resolvedAppearance {
    NSString *override = [Preferences appearanceOverride];
    if ([override isEqualToString:@"dark"])
        return kDarkAppearance;
    if ([override isEqualToString:@"light"])
        return kLightAppearance;
    return [Preferences systemAppearance];
}

#pragma mark - Styles tab UI

- (void)configureStylesTabAppearanceControls {
    if (self.darkModeSwitch)
        return;

    NSView *panel = self.stylesView;
    if (!panel)
        return;

    NSButton *modeSwitch = [NSButton checkboxWithTitle:NSLocalizedString(@"Dark Mode", nil)
                                                target:self
                                                action:@selector(darkModeSwitchChanged:)];
    modeSwitch.translatesAutoresizingMaskIntoConstraints = NO;
    self.darkModeSwitch = modeSwitch;
    [panel addSubview:modeSwitch];

    // Make room for the toggle above the "Settings for theme …" caption.
    CGFloat topPad = 8;
    CGFloat gapBelowToggle = 10;
    CGFloat toggleHeight = 18; // checkbox row
    CGFloat darkModeExtra = topPad + toggleHeight + gapBelowToggle;
    for (NSView *sub in panel.subviews) {
        if (sub == modeSwitch)
            continue;
        NSRect f = sub.frame;
        if (NSMaxY(f) > 100) {
            f.origin.y -= darkModeExtra;
            sub.frame = f;
        }
    }

    [NSLayoutConstraint activateConstraints:@[
        [modeSwitch.centerXAnchor constraintEqualToAnchor:panel.centerXAnchor],
        [modeSwitch.topAnchor constraintEqualToAnchor:panel.topAnchor constant:topPad],
    ]];

    [Preferences applyAppearanceOverrideToApp];
    [self syncDarkModeSwitchFromResolvedMode];
}

- (void)syncDarkModeSwitchFromResolvedMode {
    if (!self.darkModeSwitch)
        return;
    NSControlStateValue state =
        ([Preferences resolvedAppearance] == kDarkAppearance) ? NSOnState : NSOffState;
    if (self.darkModeSwitch.state != state)
        self.darkModeSwitch.state = state;
}

- (IBAction)darkModeSwitchChanged:(NSButton *)sender {
    // https://lea.verou.me/blog/2026/dark-mode-toggles/
    // appearanceOverride can be dark, light, or nil but we're controlling it with a checkbox
    // The checkbox switches between nil appearanceOverride and the opposite of the system appearance
    
    // Suppose the system is in light mode. The default appearanceOverride is nil, so the "Dark Mode" checkbox is off.
    // When the user clicks the checkbox, the appearanceOverride is set to dark, and the checkbox is on.
    // When the user clicks the checkbox again, the appearanceOverride is set back to nil, and the checkbox is off.
    
    // Suppose the system is in dark mode. The default appearanceOverride is nil, so the "Dark Mode" checkbox is on.
    // When the user clicks the checkbox, the appearanceOverride is set to light, and the checkbox is off.
    // When the user clicks the checkbox again, the appearanceOverride is set back to nil, and the checkbox is on.
    BOOL wantDark = (sender.state == NSOnState);
    kAppearanceType system = [Preferences systemAppearance];
    kAppearanceType desired = wantDark ? kDarkAppearance : kLightAppearance;

    if (desired == system)
        [Preferences setAppearanceOverride:nil];
    else
        [Preferences setAppearanceOverride:(wantDark ? @"dark" : @"light")];

    self.themesHeader.stringValue = [self themeScopeTitle];

    [self updatePrefsPanel];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:[Preferences currentTheme]]];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"ColorModeChanged" object:nil]];
}

@end
