#import "GlkWindow.h"

@interface GlkGraphicsWindow : GlkWindow <NSSecureCoding>

@property NSImage *image;
@property BOOL showingImage;

@end
