#import "GlkWindow.h"

@interface GlkGraphicsWindow : GlkWindow <NSSecureCoding, NSFilePromiseProviderDelegate, NSDraggingSource, NSPasteboardItemDataProvider>

@property NSImage *image;
@property BOOL showingImage;

@end
