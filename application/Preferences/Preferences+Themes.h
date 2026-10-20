#import "Preferences.h"

/*
 * The themes table: selection, renaming, adding, removing and deleting
 * themes, the one-theme-for-all scope, and the light/dark mode overrides.
 */

@interface Preferences (Themes)

+ (kModeType)currentSystemMode;
- (void)noteColorModeChanged:(NSNotification *)notification;
- (void)lightOrDarkOverrideWasRemoved;
- (void)findDarkAndLightThemes;
- (void)noteManagedObjectContextDidChange:(NSNotification *)notify;
- (void)restoreThemeSelection:(Theme *)sender;
- (void)stopWaitingForThemesTable;
- (void)changeThemeName:(NSString *)name;
- (BOOL)notDuplicate:(NSString *)string;
- (BOOL)notDuplicate:(NSString *)string exceptTheme:(Theme *)exception;
- (void)showDuplicateThemeNameAlert:(NSText *)fieldEditor theme:(Theme *)renamed;
- (Theme *)themeForTableCellSubview:(NSView *)view;
- (Theme *)themeForMenuCommand:(id)sender;
- (void)showUseForAllAlert:(NSArray *)games;
- (NSString *)themeScopeTitle;

@end
