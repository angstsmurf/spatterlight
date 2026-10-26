#import "GlkController.h"

// Optional retro "CRT" look, set per theme: a phosphor glow (a Core Image
// filter on a view) and scanlines with a vignette (a static overlay
// view on top of a view's subviews). Both are drawn over the ordinary text
// views, so selection, hyperlinks and VoiceOver work as usual. They are left
// out while the system "Increase contrast" or "Reduce transparency"
// accessibility options are on.
@interface NSView (CRTEffects)

// Adds or removes a glow over this layer-backed view and its subviews.
- (void)updateCRTGlow:(BOOL)on;

// Adds, removes or brings to the front a scanline overlay covering this view.
- (void)updateCRTScanlines:(BOOL)on;

@end

// The game window: glow on the game view, scanlines over the border view.
@interface GlkController (CRTEffects)

// Matches the effects to the current theme. Safe to call any number of times.
- (void)updateCRTEffects;

// Observer for NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification.
- (void)noteAccessibilityDisplayOptionsChanged:(NSNotification *)notification;

// NSWindowDelegate: the window moved to a screen with another scale.
- (void)windowDidChangeBackingProperties:(NSNotification *)notification;

@end
