#import "Preferences.h"

/*
 * The per-setting actions of the Details, Presentation, VoiceOver, Misc and
 * Global panes, and the margin and paragraph popovers.
 */

@interface Preferences (Settings)

- (void)changeBooleanAttribute:(NSString *)attribute fromButton:(NSButton *)button;
- (void)changeMenuAttribute:(NSString *)attribute fromPopUp:(NSPopUpButton *)button;
- (void)changeBorderColor:(NSColor *)color;
- (void)changeMarginAttribute:(NSString *)attribute fromControl:(id)sender
                    textField:(NSTextField *)textField stepper:(NSStepper *)stepper;
- (NSString *)playBeepFromMenu:(NSPopUpButton *)sender;
- (void)showOverwriteStylesAlert:(NSArray *)styles;
- (void)overWriteStyles;

@end
