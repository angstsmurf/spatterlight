#import "GlkController.h"

@interface GlkController (BorderColor)

- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow;
- (void)setBorderColor:(NSColor *)color;
- (GlkWindow *)largestWindow;

@end
