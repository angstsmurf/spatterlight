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

/// Override if set, else system appearance.
+ (kAppearanceType)resolvedAppearance;

/// Run once when moving to light and dark theme sides. Returns YES if the
/// built-in themes must be rebuilt.
+ (BOOL)migrateToThemeSidesIfNeededInContext:(NSManagedObjectContext *)context;
/// Swap every theme with separate sides to the side for the current appearance.
+ (void)activateThemeSidesInContext:(NSManagedObjectContext *)context;

- (void)configureAppearanceToggleButtons;
- (void)syncAppearanceToggleButtons;

@end

NS_ASSUME_NONNULL_END
