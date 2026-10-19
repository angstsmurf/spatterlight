#import "Preferences.h"

/*
 * The font and colour panels: font buttons, changeFont:/changeAttributes:
 * from the shared font panel (via DummyTextView), and the colour wells.
 */

@interface Preferences (Fonts)

- (IBAction)showFontPanel:(id)sender;
- (void)changeAttributes:(id)sender;
- (void)changeDocumentBackgroundColor:(id)sender;

@end

// "Name size" title of a font button
NSString *fontToString(NSFont *font);
