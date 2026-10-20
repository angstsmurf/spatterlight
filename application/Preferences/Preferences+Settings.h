#import "Preferences.h"

/*
 * The per-setting actions of the Details, Presentation, VoiceOver, Misc and
 * Global panes, and the margin and paragraph popovers.
 */

@interface Preferences (Settings)

// Outlet names of the controls that changeSimpleSetting: handles, with the
// theme attribute or user defaults key each one stores
@property (class, readonly) NSDictionary<NSString *, NSString *> *simpleSettingKeys;

- (void)setUpSimpleSettings;
- (void)updateSimpleSettings;
- (IBAction)changeSimpleSetting:(NSControl *)sender;

- (void)changeBooleanAttribute:(NSString *)attribute fromButton:(NSButton *)button;
- (void)changeBorderColor:(NSColor *)color;
- (void)changeMarginAttribute:(NSString *)attribute fromControl:(id)sender
                    textField:(NSTextField *)textField stepper:(NSStepper *)stepper;
- (NSString *)playBeepFromMenu:(NSPopUpButton *)sender;
- (void)showOverwriteStylesAlert:(NSArray *)styles;
- (void)overWriteStyles;

@end
