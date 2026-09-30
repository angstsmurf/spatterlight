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
static NSString * const SpatterlightPerThemeDarkColorsMigratedKey = @"SpatterlightPerThemeDarkColorsMigratedV2";

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

#pragma mark - Migration

static BOOL SPColorsEqual(NSColor *a, NSColor *b) {
    if (a == b)
        return YES;
    if (!a || !b)
        return NO;
    return [a isEqualToColor:b];
}

static Theme *SPThemeNamed(NSString *name, NSManagedObjectContext *context) {
    NSFetchRequest *req = [Theme fetchRequest];
    req.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];
    req.fetchLimit = 1;
    return [context executeFetchRequest:req error:nil].firstObject;
}

static Theme *SPBuiltInAncestor(Theme *theme) {
    Theme *parent = theme.defaultParent;
    NSMutableSet *seen = [NSMutableSet new];
    while (parent) {
        if ([seen containsObject:parent.objectID])
            break;
        [seen addObject:parent.objectID];
        if (!parent.editable)
            return parent;
        parent = parent.defaultParent;
    }
    return nil;
}

static BOOL SPThemeLightColorsMatch(Theme *a, Theme *b) {
    if (!SPColorsEqual(a.bufferBackground, b.bufferBackground) ||
        !SPColorsEqual(a.gridBackground, b.gridBackground) ||
        !SPColorsEqual(a.borderColor, b.borderColor) ||
        !SPColorsEqual(a.spacingColor, b.spacingColor) ||
        !SPColorsEqual(a.bufLinkColor, b.bufLinkColor) ||
        !SPColorsEqual(a.gridLinkColor, b.gridLinkColor))
        return NO;

    NSArray<GlkStyle *> *stylesA = a.allStyles;
    NSArray<GlkStyle *> *stylesB = b.allStyles;
    if (stylesA.count != stylesB.count)
        return NO;
    for (NSUInteger i = 0; i < stylesA.count; i++) {
        if (!SPColorsEqual(stylesA[i].color, stylesB[i].color))
            return NO;
    }
    return YES;
}

static void SPCopyDarkColorsFromTheme(Theme *dst, Theme *src) {
    dst.bufferBackgroundDark = src.bufferBackgroundDark;
    dst.gridBackgroundDark = src.gridBackgroundDark;
    dst.borderColorDark = src.borderColorDark;
    dst.spacingColorDark = src.spacingColorDark;
    dst.bufLinkColorDark = src.bufLinkColorDark;
    dst.gridLinkColorDark = src.gridLinkColorDark;

    // Keep light slots in sync with the current built-in as well.
    dst.bufferBackground = src.bufferBackground;
    dst.gridBackground = src.gridBackground;
    dst.borderColor = src.borderColor;
    dst.spacingColor = src.spacingColor;
    dst.bufLinkColor = src.bufLinkColor;
    dst.gridLinkColor = src.gridLinkColor;

    NSArray<GlkStyle *> *dstStyles = dst.allStyles;
    NSArray<GlkStyle *> *srcStyles = src.allStyles;
    NSUInteger count = MIN(dstStyles.count, srcStyles.count);
    for (NSUInteger i = 0; i < count; i++) {
        dstStyles[i].color = srcStyles[i].color;
        dstStyles[i].darkColor = srcStyles[i].darkColor;
    }
}

static void SPFillDarkColorsFromLight(Theme *theme) {
    if (!theme.bufferBackgroundDark)
        theme.bufferBackgroundDark = theme.bufferBackground;
    if (!theme.gridBackgroundDark)
        theme.gridBackgroundDark = theme.gridBackground;
    if (!theme.borderColorDark)
        theme.borderColorDark = theme.borderColor;
    if (!theme.spacingColorDark)
        theme.spacingColorDark = theme.spacingColor;
    if (!theme.bufLinkColorDark)
        theme.bufLinkColorDark = theme.bufLinkColor;
    if (!theme.gridLinkColorDark)
        theme.gridLinkColorDark = theme.gridLinkColor;

    for (GlkStyle *style in theme.allStyles) {
        if (!style.darkColor)
            style.darkColor = style.color;
    }
}

+ (void)migratePerThemeDarkColorsIfNeededInContext:(NSManagedObjectContext *)context {
    if (!context)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:SpatterlightPerThemeDarkColorsMigratedKey])
        return;

    Theme *lectrote = SPThemeNamed(@"Lectrote", context);
    Theme *lectroteDark = SPThemeNamed(@"Lectrote Dark", context);

    if (lectroteDark) {
        NSString *themeName = [defaults objectForKey:@"themeName"];
        BOOL selected = [themeName caseInsensitiveCompare:@"Lectrote Dark"] == NSOrderedSame;
        Theme *current = [Preferences currentTheme];
        if (current == lectroteDark)
            selected = YES;
        BOOL inUse = selected || lectroteDark.games.count > 0;

        if (inUse) {
            // Keep as a user theme: dark palette in both light and dark slots.
            lectroteDark.editable = YES;
            if (lectrote)
                lectroteDark.defaultParent = lectrote;
            SPFillDarkColorsFromLight(lectroteDark);
        } else {
            // Unused leftover built-in — reparent any children, then remove.
            if (lectrote) {
                for (Theme *child in [lectroteDark.defaultChild copy])
                    child.defaultParent = lectrote;
            }
            [context deleteObject:lectroteDark];
        }
    }

    NSFetchRequest *req = [Theme fetchRequest];
    req.predicate = [NSPredicate predicateWithFormat:@"editable == YES"];
    NSArray<Theme *> *editableThemes = [context executeFetchRequest:req error:nil] ?: @[];

    for (Theme *editable in editableThemes) {
        Theme *ancestor = SPBuiltInAncestor(editable);
        if (ancestor && SPThemeLightColorsMatch(editable, ancestor))
            SPCopyDarkColorsFromTheme(editable, ancestor);
        else
            SPFillDarkColorsFromLight(editable);
    }

    NSError *error = nil;
    if ([context hasChanges] && ![context save:&error])
        NSLog(@"migratePerThemeDarkColors: save failed: %@", error);

    [defaults setBool:YES forKey:SpatterlightPerThemeDarkColorsMigratedKey];
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
