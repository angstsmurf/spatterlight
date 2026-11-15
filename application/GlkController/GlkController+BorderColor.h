#import "GlkController.h"

@interface GlkController (BorderColor)

- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow;
- (void)setBorderColor:(NSColor *)color;
/// lastAutoBGColor, re-resolved against the current light/dark mode when it
/// was one of the theme's default backgrounds.
- (NSColor *)resolvedLastAutoBGColor;
- (GlkWindow *)largestWindow;

@end
