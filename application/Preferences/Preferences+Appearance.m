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

@interface Preferences (AppearancePrivate)
@property (nonatomic, strong) NSArray<NSButton *> *appearanceToggleButtons;
@end

@implementation Preferences (Appearance)

#pragma mark - Associated objects

- (NSArray<NSButton *> *)appearanceToggleButtons {
    return objc_getAssociatedObject(self, @selector(appearanceToggleButtons));
}
- (void)setAppearanceToggleButtons:(NSArray<NSButton *> *)value {
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

#pragma mark - Light/dark toggle buttons

- (void)configureAppearanceToggleButtons {
    if (self.appearanceToggleButtons.count)
        return;

    NSImage *image = nil;
    if (@available(macOS 11.0, *)) {
        NSString *description = NSLocalizedString(@"Switch between light and dark mode", nil);
        image = [NSImage imageWithSystemSymbolName:@"circle.lefthalf.filled.inverse" accessibilityDescription:description];
        if (!image)
            image = [NSImage imageWithSystemSymbolName:@"circle.lefthalf.filled" accessibilityDescription:description];
    }

    NSMutableArray<NSButton *> *buttons = [NSMutableArray new];

    // One button at the left edge of every tab, on the "Settings for theme …" line.
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

        NSButton *button;
        if (image) {
            button = [NSButton buttonWithImage:image target:self action:@selector(toggleAppearance:)];
        } else {
            button = [NSButton buttonWithTitle:@"◐" target:self action:@selector(toggleAppearance:)];
        }
        button.bordered = NO;
        button.imagePosition = NSImageOnly;
        button.translatesAutoresizingMaskIntoConstraints = NO;
        [panel addSubview:button positioned:NSWindowAbove relativeTo:label];

        [NSLayoutConstraint activateConstraints:@[
            [button.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:8],
            [button.centerYAnchor constraintEqualToAnchor:label.centerYAnchor],
        ]];
        [buttons addObject:button];
    }

    self.appearanceToggleButtons = buttons;

    [Preferences applyAppearanceOverrideToApp];
    [self syncAppearanceToggleButtons];
}

- (void)syncAppearanceToggleButtons {
    NSString *toolTip = ([Preferences resolvedAppearance] == kDarkAppearance) ?
        NSLocalizedString(@"Switch to Light Mode", nil) :
        NSLocalizedString(@"Switch to Dark Mode", nil);
    for (NSButton *button in self.appearanceToggleButtons)
        button.toolTip = toolTip;
}

- (IBAction)toggleAppearance:(id)sender {
    // https://lea.verou.me/blog/2026/dark-mode-toggles/
    // appearanceOverride can be dark, light, or nil, but the toggle only
    // switches between nil (follow the system) and the opposite of the system.
    kAppearanceType system = [Preferences systemAppearance];
    kAppearanceType desired = ([Preferences resolvedAppearance] == kDarkAppearance) ? kLightAppearance : kDarkAppearance;

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
