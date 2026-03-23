#import "GlkController.h"

@interface GlkController (FullScreen)

- (void)storeScrollOffsets;
- (void)restoreScrollOffsets;
- (void)adjustContentView;
- (NSRect)contentFrameForWindowed;
- (NSRect)contentFrameForFullscreen;
- (void)startInFullscreen;
- (void)deferredEnterFullscreen:(id)sender;

@end
