//
//  Preferences+ColorsChoice.m
//  Spatterlight
//
//  Game windows show the dark colors of their theme in dark mode.
//

#import "Preferences+StylesEditor.h"
#import "Preferences+Appearance.h"

@implementation Preferences (ColorsChoice)

+ (BOOL)themeUsesDarkColors:(Theme *)theme {
    return [Preferences resolvedAppearance] == kDarkAppearance;
}

@end

@implementation Preferences (ColorsChoiceControls)

- (NSArray<NSArray<NSView *> *> *)colorsChoiceAppRows {
    return @[];
}

- (NSArray<NSArray<NSView *> *> *)colorsChoiceThemeHeaderRows {
    return @[];
}

- (NSArray<NSArray<NSView *> *> *)colorsChoiceThemeRows {
    return @[];
}

- (void)configureColorsChoiceOnOtherTabs {
}

- (void)syncColorsChoice {
}

- (BOOL)stylesEditorShowsLightColors {
    return YES;
}

- (BOOL)stylesEditorShowsDarkColors {
    return YES;
}

@end
