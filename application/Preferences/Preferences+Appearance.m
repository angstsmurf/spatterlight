//
//  Preferences+Appearance.m
//  Spatterlight
//

#import "Preferences+Appearance.h"

#import "CoreDataManager.h"
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

// The appearance the theme sides follow. It is read once per switch and
// cached, so that all the work a switch sets off agrees on one answer instead
// of asking the system again halfway through. -1 means "not read yet".
static NSInteger SPCachedSystemAppearance = -1;
static NSInteger SPCachedResolvedAppearance = -1;

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
    // Setting NSApp.appearance fires the effectiveAppearance observer, which
    // re-reads the cache; callers announce through noteAppearanceMayHaveChanged,
    // which does nothing if that already happened.
    [Preferences applyAppearanceOverrideToApp];
}

// The override that should stay in place given the system appearance. The
// toggle only ever stores the opposite of the system, so once the system
// comes round to the same mode the override has no work left to do. Keeping
// it would pin the app to that mode and make it ignore the system's next
// switch.
+ (nullable NSString *)override:(nullable NSString *)override keptForSystemAppearance:(kAppearanceType)system {
    NSString *systemName = (system == kDarkAppearance) ? @"dark" : @"light";
    return [override isEqualToString:systemName] ? nil : override;
}

+ (void)dropOverrideMatchingSystem {
    NSString *override = [Preferences appearanceOverride];
    if (override && ![Preferences override:override keptForSystemAppearance:[Preferences systemAppearance]])
        [Preferences setAppearanceOverride:nil];
}

// Asks the system for its appearance now, bypassing the cache.
//
// AppleInterfaceThemeChangedNotification can arrive before this process's
// NSUserDefaults has picked up the new AppleInterfaceStyle, so reading the
// default from its handler can return the old mode. With no override applied,
// NSApp's effective appearance is the system's, and AppKit has updated it by
// the time its KVO notification fires. With an override applied, NSApp only
// reports the override, so the global default is read directly, after
// synchronizing so that CFPreferences does not answer from a stale cache.
+ (kAppearanceType)readSystemAppearance {
    if (@available(macOS 10.14, *)) {
        if (NSApp && NSApp.appearance == nil) {
            NSAppearanceName match =
            [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
            if (match)
                return [match isEqualToString:NSAppearanceNameDarkAqua] ? kDarkAppearance : kLightAppearance;
        }
    }

    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);
    BOOL dark = NO;
    if (value) {
        dark = CFGetTypeID(value) == CFStringGetTypeID() &&
        CFStringCompare((CFStringRef)value, CFSTR("Dark"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
        CFRelease(value);
    }
    return dark ? kDarkAppearance : kLightAppearance;
}

+ (kAppearanceType)systemAppearance {
    if (SPCachedSystemAppearance < 0)
        SPCachedSystemAppearance = (NSInteger)[Preferences readSystemAppearance];
    return (kAppearanceType)SPCachedSystemAppearance;
}

// Re-reads the system appearance and, if the resolved appearance has changed,
// tells the preferences window and every game about it, once. Called for
// AppKit's effectiveAppearance change, for the system's distributed
// notification and the rechecks after it, and for the light/dark toggle.
+ (void)noteAppearanceMayHaveChanged {
    SPCachedSystemAppearance = (NSInteger)[Preferences readSystemAppearance];
    [Preferences dropOverrideMatchingSystem];
    kAppearanceType previous = (kAppearanceType)SPCachedResolvedAppearance;
    BOOL wasKnown = SPCachedResolvedAppearance >= 0;
    SPCachedResolvedAppearance = -1;
    kAppearanceType now = [Preferences resolvedAppearance];

    Preferences *prefs = [Preferences instance];
    [prefs syncAppearanceToggleButtons];

    if (wasKnown && previous == now)
        return;
    [prefs appearanceDidChange];
}

// Checks again a little later: the distributed notification can arrive
// before the new mode can be read. Each check announces only a real change,
// so the extra ones cost nothing once the switch has been seen.
+ (void)scheduleAppearanceRechecks {
    for (NSNumber *delay in @[@0.25, @1.0]) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay.doubleValue * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            [Preferences noteAppearanceMayHaveChanged];
        });
    }
}

// The one place a light/dark switch is announced. The themes swap to the
// side for the new appearance, the preferences window refreshes itself, and
// ColorModeChanged goes to every game, each of which updates only itself
// (GlkController noteColorModeChanged:). Posting a PreferencesChanged as well
// would make the games showing the current theme rearrange twice.
- (void)appearanceDidChange {
    [Preferences activateThemeSidesInContext:self.managedObjectContext];
    self.themesHeader.stringValue = [self themeScopeTitle];
    [self updatePrefsPanel];
    [self.coreDataManager saveChanges];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"ColorModeChanged" object:nil]];
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
    if (SPCachedResolvedAppearance >= 0)
        return (kAppearanceType)SPCachedResolvedAppearance;
    NSString *override = [Preferences appearanceOverride];
    kAppearanceType resolved;
    if ([override isEqualToString:@"dark"])
        resolved = kDarkAppearance;
    else if ([override isEqualToString:@"light"])
        resolved = kLightAppearance;
    else
        resolved = [Preferences systemAppearance];
    SPCachedResolvedAppearance = (NSInteger)resolved;
    return resolved;
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

    [Preferences noteAppearanceMayHaveChanged];
}

@end
