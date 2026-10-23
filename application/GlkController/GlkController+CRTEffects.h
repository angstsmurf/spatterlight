#import "GlkController.h"

// Optional retro "CRT" look, set per theme: a phosphor glow (a Core Image
// bloom filter on the game view) and scanlines with a vignette (a static
// overlay view above it). Both are drawn on top of the ordinary text views,
// so selection, hyperlinks and VoiceOver work as usual. They are left out
// while the system "Increase contrast" or "Reduce transparency" accessibility
// options are on.
@interface GlkController (CRTEffects)

// Adds, removes or reorders the effects to match the current theme. Safe to
// call any number of times.
- (void)updateCRTEffects;

// Observer for NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification.
- (void)noteAccessibilityDisplayOptionsChanged:(NSNotification *)notification;

@end
