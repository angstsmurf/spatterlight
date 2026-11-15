//
//  Preferences+Appearance.m
//  Spatterlight
//

#import "PreferencesPrivate.h"

#import "Constants.h"

#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "GlkStyle.h"
#import "NSColor+integer.h"
#import "Theme.h"

NSString * const SpatterlightAppearanceOverrideKey = @"SpatterlightAppearanceOverride";
NSString * const SpatterlightFabulichModeKey = @"FabulichMode";
static NSString * const SpatterlightThemeSidesMigratedKey = @"SpatterlightThemeSidesV1";

// The light/dark mode symbol: a circle with one half filled, as a template
// image, so that it follows the text color.
static NSImage *SPAppearanceSymbolImage(void) {
    static NSImage *image = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        image = [NSImage imageWithSize:NSMakeSize(13, 13) flipped:NO drawingHandler:^BOOL(NSRect rect) {
            NSRect circle = NSInsetRect(rect, 1, 1);
            NSBezierPath *outline = [NSBezierPath bezierPathWithOvalInRect:circle];
            outline.lineWidth = 1;
            [[NSColor blackColor] set];
            [outline stroke];
            // The right half of the circle, filled up to the outline.
            NSPoint center = NSMakePoint(NSMidX(circle), NSMidY(circle));
            NSBezierPath *half = [NSBezierPath bezierPath];
            [half moveToPoint:NSMakePoint(center.x, NSMaxY(circle))];
            [half appendBezierPathWithArcWithCenter:center radius:NSWidth(circle) / 2 startAngle:90 endAngle:270 clockwise:YES];
            [half closePath];
            [half fill];
            return YES;
        }];
        image.template = YES;
    });
    return image;
}

@implementation Preferences (Appearance)

#pragma mark - Override API

// The appearance the theme sides follow. It is read once per switch and
// cached, so that all the work a switch sets off agrees on one answer instead
// of asking the system again halfway through. -1 means "not read yet".
static NSInteger SPCachedSystemAppearance = -1;
static NSInteger SPCachedResolvedAppearance = -1;
// The theme side last announced, which can change without the app's
// appearance changing, when Fabulich mode is switched on or off.
static NSInteger SPAnnouncedSidesDark = -1;

+ (BOOL)fabulichMode {
    return [[NSUserDefaults standardUserDefaults] boolForKey:SpatterlightFabulichModeKey];
}

+ (void)setFabulichMode:(BOOL)fabulich {
    [[NSUserDefaults standardUserDefaults] setBool:fabulich forKey:SpatterlightFabulichModeKey];
}

+ (BOOL)themeSidesAreDark {
    return [Preferences fabulichMode] && [Preferences resolvedAppearance] == kDarkAppearance;
}

// Outside Fabulich mode there is no toggle, and the app follows the system.
+ (NSString *)appearanceOverride {
    if (![Preferences fabulichMode])
        return nil;
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

// Re-reads the system appearance and, if the resolved appearance or the
// active theme side has changed, tells the preferences window and every game
// about it, once. Called for AppKit's effectiveAppearance change, for the
// system's distributed notification and the rechecks after it, for the
// light/dark toggle, and for switching Fabulich mode.
+ (void)noteAppearanceMayHaveChanged {
    SPCachedSystemAppearance = (NSInteger)[Preferences readSystemAppearance];
    [Preferences dropOverrideMatchingSystem];
    kAppearanceType previous = (kAppearanceType)SPCachedResolvedAppearance;
    BOOL wasKnown = SPCachedResolvedAppearance >= 0;
    SPCachedResolvedAppearance = -1;
    kAppearanceType now = [Preferences resolvedAppearance];

    NSInteger previousSides = SPAnnouncedSidesDark;
    SPAnnouncedSidesDark = [Preferences themeSidesAreDark];

    Preferences *prefs = [Preferences instance];
    [prefs syncAppearanceToggleButtons];

    BOOL sidesUnchanged = previousSides < 0 || previousSides == SPAnnouncedSidesDark;
    if (wasKnown && previous == now && sidesUnchanged)
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
     postNotification:[NSNotification notificationWithName:kColorModeChangedNotification object:nil]];
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
    if (!context)
        return NO;
    return [Preferences migrateToThemeSidesV1InContext:context];
}

+ (BOOL)migrateToThemeSidesV1InContext:(NSManagedObjectContext *)context {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:SpatterlightThemeSidesMigratedKey])
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
    SPAnnouncedSidesDark = [Preferences themeSidesAreDark];
    [Theme activateSidesForDark:SPAnnouncedSidesDark inContext:context];
}

#pragma mark - Light/dark toggle

- (void)configureAppearanceToggleButtons {
    if (self.appearanceToggleButtons.count)
        return;

    NSMutableArray<NSButton *> *toggles = [NSMutableArray new];

    // One small mode symbol on every tab, to the right of the "Settings for
    // theme …" text.
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

    BOOL fabulich = [Preferences fabulichMode];
    for (NSView *label in labels) {
        NSView *panel = label.superview;
        if (!panel)
            continue;

        NSButton *toggle = [NSButton buttonWithImage:SPAppearanceSymbolImage() target:self action:@selector(toggleAppearance:)];
        toggle.bordered = NO;
        toggle.imagePosition = NSImageOnly;
        toggle.translatesAutoresizingMaskIntoConstraints = NO;
        toggle.hidden = !fabulich;
        [toggle setAccessibilityLabel:NSLocalizedString(@"Light or dark mode", nil)];
        [panel addSubview:toggle positioned:NSWindowAbove relativeTo:label];

        // The header is as wide as its panel in the xib, with centered text.
        // Here it shrinks to its text and stays centered, so that the symbol
        // can follow the end of the text whatever the theme is called.
        CGFloat top = NSHeight(panel.bounds) - NSMaxY(label.frame);
        label.translatesAutoresizingMaskIntoConstraints = NO;
        [label setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
        [label setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];

        [NSLayoutConstraint activateConstraints:@[
            [label.centerXAnchor constraintEqualToAnchor:panel.centerXAnchor],
            [label.topAnchor constraintEqualToAnchor:panel.topAnchor constant:top],
            [label.widthAnchor constraintLessThanOrEqualToAnchor:panel.widthAnchor constant:-60],
            [toggle.leadingAnchor constraintEqualToAnchor:label.trailingAnchor constant:3],
            [toggle.centerYAnchor constraintEqualToAnchor:label.centerYAnchor],
            [toggle.widthAnchor constraintEqualToConstant:15],
            [toggle.heightAnchor constraintEqualToConstant:15],
        ]];
        [toggles addObject:toggle];
    }

    self.appearanceToggleButtons = toggles;

    [Preferences applyAppearanceOverrideToApp];
    [self syncAppearanceToggleButtons];
}

- (void)syncAppearanceToggleButtons {
    BOOL dark = ([Preferences resolvedAppearance] == kDarkAppearance);
    BOOL fabulich = [Preferences fabulichMode];
    // The symbol sets the mode of the whole app, not of the theme whose
    // settings sit below it.
    NSString *toolTip = dark ? NSLocalizedString(@"Switch all of Spatterlight to Light Mode", nil) : NSLocalizedString(@"Switch all of Spatterlight to Dark Mode", nil);
    for (NSControl *control in self.appearanceToggleButtons) {
        control.toolTip = toolTip;
        control.hidden = !fabulich;
    }
}

- (IBAction)toggleAppearance:(id)sender {
    // https://lea.verou.me/blog/2026/dark-mode-toggles/
    // appearanceOverride can be dark, light, or nil, but the toggle only
    // switches between nil (follow the system) and the opposite of the system.
    kAppearanceType system = [Preferences systemAppearance];
    kAppearanceType desired = ([Preferences resolvedAppearance] == kDarkAppearance) ? kLightAppearance : kDarkAppearance;

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
