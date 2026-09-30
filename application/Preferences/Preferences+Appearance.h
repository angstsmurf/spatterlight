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

/// Override if set, else system appearance. Cached until the next switch.
+ (kAppearanceType)resolvedAppearance;


/// Re-reads the system appearance, and announces a switch if the resolved one changed.
+ (void)noteAppearanceMayHaveChanged;
/// Runs noteAppearanceMayHaveChanged again shortly, for a notification that may arrive early.
+ (void)scheduleAppearanceRechecks;

- (void)appearanceDidChange;

+ (void)migratePerThemeDarkColorsIfNeededInContext:(NSManagedObjectContext *)context;

- (void)configureStylesTabAppearanceControls;
- (void)syncDarkModeSwitchFromResolvedMode;

@end

NS_ASSUME_NONNULL_END
