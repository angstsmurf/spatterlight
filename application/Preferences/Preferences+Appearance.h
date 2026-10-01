//
//  Preferences+Appearance.h
//  Spatterlight
//

#import "Preferences.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString * const SpatterlightAppearanceOverrideKey;
extern NSString * const SpatterlightFabulichModeKey;

@interface Preferences (Appearance)

/// In Fabulich mode the light/dark toggle is shown, and themes switch to
/// their dark side along with the app. Off by default: then the app follows
/// the system and only the light side of each theme is used.
+ (BOOL)fabulichMode;
+ (void)setFabulichMode:(BOOL)fabulich;
/// YES when the themes should show their dark side: Fabulich mode in dark mode.
+ (BOOL)themeSidesAreDark;

/// nil = follow system, @"light", or @"dark". Always nil outside Fabulich mode.
+ (nullable NSString *)appearanceOverride;
+ (void)setAppearanceOverride:(nullable NSString *)override;
+ (void)applyAppearanceOverrideToApp;
/// The override to keep when the system is in the given appearance: nil once
/// they agree, so that the system's next switch takes effect again.
+ (nullable NSString *)override:(nullable NSString *)override keptForSystemAppearance:(kAppearanceType)system;
/// Clears a stored override that the system appearance has come to match.
+ (void)dropOverrideMatchingSystem;

/// The system's appearance, whatever the override. Cached until the next switch.
+ (kAppearanceType)systemAppearance;
/// Override if set, else system appearance. Cached until the next switch.
+ (kAppearanceType)resolvedAppearance;

/// Re-reads the system appearance, and announces a switch if the resolved one changed.
+ (void)noteAppearanceMayHaveChanged;
/// Runs noteAppearanceMayHaveChanged again shortly, for a notification that may arrive early.
+ (void)scheduleAppearanceRechecks;

- (void)appearanceDidChange;

/// Run once when moving to light and dark theme sides. Returns YES if the
/// built-in themes must be rebuilt.
+ (BOOL)migrateToThemeSidesIfNeededInContext:(NSManagedObjectContext *)context;
/// Swap every theme with separate sides to the side for the current appearance.
+ (void)activateThemeSidesInContext:(NSManagedObjectContext *)context;

- (void)configureAppearanceToggleButtons;
- (void)syncAppearanceToggleButtons;

@end

NS_ASSUME_NONNULL_END
