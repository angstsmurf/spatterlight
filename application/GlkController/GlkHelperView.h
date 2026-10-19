//
//  GlkHelperView.h
//  Spatterlight
//

#import <AppKit/AppKit.h>

@class GlkController;

// GlkHelperView is the content area that hosts all Glk windows (text buffers,
// text grids, and graphics windows). It uses a flipped coordinate system
// (origin at top-left) to match the Glk layout model, and forwards resize
// events to GlkController so the interpreter can be notified of arrange events.
@interface GlkHelperView : NSView

@property (weak) IBOutlet GlkController *glkctrl;

@end
