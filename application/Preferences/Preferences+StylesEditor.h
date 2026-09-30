//
//  Preferences+StylesEditor.h
//  Spatterlight
//
//  The Glk Styles tab, with the light colors and the dark colors of the
//  theme side by side.
//

#import "Preferences.h"

NS_ASSUME_NONNULL_BEGIN

/// A color well for one of the light or dark colors of the theme. key is
/// gridNormal, gridBackground, bufferNormal, bufferBackground, or
/// selectedStyle for the style chosen in the style popups.
@interface ThemeColorWell : NSColorWell
@property (copy) NSString *key;
@property BOOL dark;
@end

/// Two small samples of the theme's proportional text: with Dark mode off,
/// then with it on.
@interface ThemePreviewView : NSView
@property (weak, nullable) Theme *theme;
+ (NSSize)previewSize;
@end

@interface Preferences ()
- (NSString *)selectedStyleName;
- (NSTableView *)themesTable;
@end

@interface Preferences (StylesEditor)

- (nullable NSColor *)themeColorForKey:(NSString *)key dark:(BOOL)dark;
- (void)setThemeColor:(NSColor *)color forKey:(NSString *)key dark:(BOOL)dark;

/// Lays out the Glk Styles tab. Called once, from windowDidLoad.
- (void)configureStylesEditor;
/// Updates the color wells from the current theme.
- (void)syncStylesEditor;
/// Redraws the previews in the list of themes, e.g. after a font change.
- (void)refreshThemePreviews;

/// A label for a row of settings, right-aligned like the others on the tab.
- (NSTextField *)formLabel:(NSString *)text;

@end

/// How the light and dark colors are chosen for game windows. Each design
/// implements this in Preferences+ColorsChoice.m.
@interface Preferences (ColorsChoiceControls)

/// Rows of label and control, shown under the Dark mode checkbox. These apply
/// to the whole app.
- (NSArray<NSArray<NSView *> *> *)colorsChoiceAppRows;
/// Rows of label and control, shown between the name of the theme and its
/// colors.
- (NSArray<NSArray<NSView *> *> *)colorsChoiceThemeHeaderRows;
/// Rows of label and control, shown under the colors of the theme.
- (NSArray<NSArray<NSView *> *> *)colorsChoiceThemeRows;
/// Controls on other tabs.
- (void)configureColorsChoiceOnOtherTabs;
- (void)syncColorsChoice;

- (BOOL)stylesEditorShowsLightColors;
- (BOOL)stylesEditorShowsDarkColors;

@end

NS_ASSUME_NONNULL_END
