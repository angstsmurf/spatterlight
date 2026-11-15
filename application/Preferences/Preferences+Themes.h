#import "Preferences.h"

/*
 * The themes table: selection, renaming, adding, removing and deleting
 * themes, the one-theme-for-all scope, the themes that Fabulich mode shows and hides, and switching that mode.
 */

@interface Preferences (Themes)

- (void)setUpAddRemoveThemeButtons;
- (void)noteColorModeChanged:(NSNotification *)notification;
- (void)startObservingEffectiveAppearance;
- (void)updateStylesHeader;
- (void)applyThemeFilter;
- (BOOL)themeIsVisible:(Theme *)t;
- (IBAction)toggleFabulichMode:(id)sender;
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
