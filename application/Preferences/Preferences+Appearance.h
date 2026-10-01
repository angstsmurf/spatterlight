//
//  Preferences+Appearance.h
//  Spatterlight
//

#import "Preferences.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString * const SpatterlightAppearanceOverrideKey;

@interface Preferences (Appearance)

/// nil = follow system, @"light", or @"dark".
+ (nullable NSString *)appearanceOverride;
+ (void)setAppearanceOverride:(nullable NSString *)override;
+ (void)applyAppearanceOverrideToApp;

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
