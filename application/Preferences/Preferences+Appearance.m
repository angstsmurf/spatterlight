//
//  Preferences+Appearance.m
//  Spatterlight
//

#import "Preferences+Appearance.h"

#import "GlkStyle.h"
#import "NSColor+integer.h"
#import "Theme.h"

#import <objc/runtime.h>

NSString * const SpatterlightAppearanceOverrideKey = @"SpatterlightAppearanceOverride";
static NSString * const SpatterlightThemeSidesMigratedKey = @"SpatterlightThemeSidesV1";

@interface Preferences ()
@property (strong) IBOutlet NSView *globalView;
- (NSString *)themeScopeTitle;
@end

// The light/dark toggle: a grey capsule with a filled circle at the
// left end (light mode) or the right end (dark mode). Any click switches it.
@interface AppearanceToggle : NSControl
@property (nonatomic) BOOL dark;
@end

@implementation AppearanceToggle

// Room around the pill for the knob's shadow.
static const CGFloat kAppearanceToggleMargin = 3;

- (NSSize)intrinsicContentSize {
    return NSMakeSize(24 + 2 * kAppearanceToggleMargin, 13 + 2 * kAppearanceToggleMargin);
}

- (BOOL)isFlipped {
    return NO;
}

- (void)setDark:(BOOL)dark {
    _dark = dark;
    self.toolTip = dark ? NSLocalizedString(@"Dark Mode", nil) : NSLocalizedString(@"Light Mode", nil);
    self.needsDisplay = YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    NSRect pillRect = NSInsetRect(self.bounds, kAppearanceToggleMargin, kAppearanceToggleMargin);
    CGFloat height = NSHeight(pillRect);

    // The same translucent grey as an unchecked checkbox.
    CGFloat radius = height / 2;
    [[NSColor quaternaryLabelColor] set];
    [[NSBezierPath bezierPathWithRoundedRect:pillRect xRadius:radius yRadius:radius] fill];

    // The knob, like a slider knob: white (light grey in dark mode) with a
    // soft shadow.
    BOOL darkAppearance = NO;
    if (@available(macOS 10.14, *))
        darkAppearance = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]] isEqualToString:NSAppearanceNameDarkAqua];

    NSRect knobRect = NSMakeRect(self.dark ? NSMaxX(pillRect) - height : NSMinX(pillRect), NSMinY(pillRect), height, height);
    [NSGraphicsContext saveGraphicsState];
    NSShadow *shadow = [NSShadow new];
    shadow.shadowColor = [NSColor colorWithWhite:0 alpha:darkAppearance ? 0.5 : 0.3];
    shadow.shadowOffset = NSMakeSize(0, -0.5);
    shadow.shadowBlurRadius = 2;
    [shadow set];
    [(darkAppearance ? [NSColor colorWithWhite:0.88 alpha:1] : [NSColor whiteColor]) set];
    [[NSBezierPath bezierPathWithOvalInRect:knobRect] fill];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)selectDark:(BOOL)dark {
    if (dark == self.dark)
        return;
    self.dark = dark;
    [self sendAction:self.action to:self.target];
}

- (void)mouseDown:(NSEvent *)event {
    if (!self.enabled)
        return;
    // Clicking the knob or the empty side both switch it.
    [self selectDark:!self.dark];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return YES;
}

- (NSAccessibilityRole)accessibilityRole {
    return NSAccessibilityCheckBoxRole;
}

- (NSString *)accessibilityLabel {
    return NSLocalizedString(@"Dark Mode", nil);
}

- (id)accessibilityValue {
    return @(self.dark);
}

- (BOOL)accessibilityPerformPress {
    [self selectDark:!self.dark];
    return YES;
}

@end

@interface Preferences (AppearancePrivate)
@property (nonatomic, strong) NSArray<AppearanceToggle *> *appearanceToggleButtons;
@end

@implementation Preferences (Appearance)

#pragma mark - Associated objects

- (NSArray<AppearanceToggle *> *)appearanceToggleButtons {
    return objc_getAssociatedObject(self, @selector(appearanceToggleButtons));
}
- (void)setAppearanceToggleButtons:(NSArray<AppearanceToggle *> *)value {
    objc_setAssociatedObject(self, @selector(appearanceToggleButtons), value, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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

#pragma mark - Light and dark theme sides

+ (BOOL)migrateToThemeSidesIfNeededInContext:(NSManagedObjectContext *)context {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (!context || [defaults boolForKey:SpatterlightThemeSidesMigratedKey])
        return NO;

    // An earlier build turned Lectrote Dark into a user theme. Make it
    // built-in again.
    NSFetchRequest *req = [Theme fetchRequest];
    req.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", @"Lectrote Dark"];
    for (Theme *lectroteDark in [context executeFetchRequest:req error:nil]) {
        lectroteDark.editable = NO;
        lectroteDark.defaultParent = nil;
    }

    [defaults setBool:YES forKey:SpatterlightThemeSidesMigratedKey];
    return YES;
}

+ (void)activateThemeSidesInContext:(NSManagedObjectContext *)context {
    if (!context)
        return;
    [Theme activateSidesForDark:([Preferences resolvedAppearance] == kDarkAppearance) inContext:context];
}

#pragma mark - Light/dark toggle

- (void)configureAppearanceToggleButtons {
    if (self.appearanceToggleButtons.count)
        return;

    NSMutableArray<AppearanceToggle *> *toggles = [NSMutableArray new];

    // One toggle centered at the top of every tab, above the "Settings for theme …"
    // line. The xib leaves room for it above each header.
    NSMutableArray<NSView *> *labels = [NSMutableArray new];
    for (NSTextFieldCell *header in @[ self.themesHeader, self.stylesHeader, self.detailsHeader,
                                       self.zcodeHeader, self.vOHeader, self.miscHeader ]) {
        if (header.controlView)
            [labels addObject:header.controlView];
    }
    // The Global tab has no header outlet; use its topmost label
    // ("These settings apply to all themes").
    NSView *globalLabel = nil;
    for (NSView *sub in self.globalView.subviews) {
        if ([sub isKindOfClass:[NSTextField class]] && (!globalLabel || NSMaxY(sub.frame) > NSMaxY(globalLabel.frame)))
            globalLabel = sub;
    }
    if (globalLabel)
        [labels addObject:globalLabel];

    for (NSView *label in labels) {
        NSView *panel = label.superview;
        if (!panel)
            continue;

        // Keep the header centered across the full width of the tab.
        NSRect labelFrame = label.frame;
        labelFrame.origin.x = -2;
        labelFrame.size.width = NSWidth(panel.bounds) + 4;
        label.frame = labelFrame;
        label.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;

        AppearanceToggle *toggle = [AppearanceToggle new];
        toggle.target = self;
        toggle.action = @selector(toggleAppearance:);
        toggle.translatesAutoresizingMaskIntoConstraints = NO;
        [panel addSubview:toggle positioned:NSWindowAbove relativeTo:label];

        [NSLayoutConstraint activateConstraints:@[
            [toggle.centerXAnchor constraintEqualToAnchor:panel.centerXAnchor],
            [toggle.topAnchor constraintEqualToAnchor:panel.topAnchor constant:7 - kAppearanceToggleMargin],
        ]];
        [toggles addObject:toggle];
    }

    self.appearanceToggleButtons = toggles;

    [Preferences applyAppearanceOverrideToApp];
    [self syncAppearanceToggleButtons];
}

- (void)syncAppearanceToggleButtons {
    BOOL dark = ([Preferences resolvedAppearance] == kDarkAppearance);
    for (AppearanceToggle *toggle in self.appearanceToggleButtons)
        toggle.dark = dark;
}

- (IBAction)toggleAppearance:(id)sender {
    // https://lea.verou.me/blog/2026/dark-mode-toggles/
    // appearanceOverride can be dark, light, or nil, but the toggle only
    // switches between nil (follow the system) and the opposite of the system.
    kAppearanceType system = [Preferences systemAppearance];
    kAppearanceType desired = ([Preferences resolvedAppearance] == kDarkAppearance) ? kLightAppearance : kDarkAppearance;
    if ([sender isKindOfClass:[AppearanceToggle class]])
        desired = ((AppearanceToggle *)sender).dark ? kDarkAppearance : kLightAppearance;

    if (desired == [Preferences resolvedAppearance]) {
        [self syncAppearanceToggleButtons];
        return;
    }

    if (desired == system)
        [Preferences setAppearanceOverride:nil];
    else
        [Preferences setAppearanceOverride:(desired == kDarkAppearance ? @"dark" : @"light")];

    [Preferences activateThemeSidesInContext:self.managedObjectContext];

    [self syncAppearanceToggleButtons];
    self.themesHeader.stringValue = [self themeScopeTitle];

    [self updatePrefsPanel];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:[Preferences currentTheme]]];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"ColorModeChanged" object:nil]];
}

@end
