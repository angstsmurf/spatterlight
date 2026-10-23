#import <CoreImage/CoreImage.h>
#import <QuartzCore/QuartzCore.h>

#import "CRTEffects.h"

#import "Theme.h"

static NSString * const kCRTGlowFilterName = @"crtGlow";

// Bloom radius in points and strength. Small enough that text stays sharp,
// big enough to see on a dark background.
static const CGFloat kGlowRadius = 2.5;
static const CGFloat kGlowIntensity = 0.7;

// One dark scanline every kScanlinePeriod points.
static const CGFloat kScanlinePeriod = 3;
static const CGFloat kScanlineHeight = 1;
static const CGFloat kScanlineAlpha = 0.22;
static const CGFloat kVignetteAlpha = 0.4;

// Scanlines and a vignette over the whole border view. Static, so it is drawn
// only when its size changes. It takes no clicks and is not an accessibility
// element: everything underneath works as without it.
@interface CRTOverlayView : NSView
@end

@implementation CRTOverlayView

- (BOOL)isOpaque {
    return NO;
}

- (NSView *)hitTest:(NSPoint)point {
    return nil;
}

- (BOOL)isAccessibilityElement {
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    static NSColor *scanlines = nil;
    if (!scanlines) {
        NSImage *tile = [NSImage imageWithSize:NSMakeSize(1, kScanlinePeriod)
                                       flipped:NO
                                drawingHandler:^BOOL(NSRect rect) {
            [[NSColor colorWithWhite:0 alpha:kScanlineAlpha] set];
            NSRectFill(NSMakeRect(0, 0, 1, kScanlineHeight));
            return YES;
        }];
        scanlines = [NSColor colorWithPatternImage:tile];
    }
    // Views no longer clip to their bounds by default (macOS 14), and both
    // the dirty rect and the vignette can reach past them.
    NSRect bounds = self.bounds;
    NSRectClip(bounds);
    [scanlines set];
    NSRectFillUsingOperation(NSIntersectionRect(dirtyRect, bounds), NSCompositingOperationSourceOver);

    // An elliptical vignette: a radial gradient in a unit circle, stretched
    // to the view, which reaches full strength in the corners.
    NSGradient *vignette = [[NSGradient alloc] initWithColorsAndLocations:
                            [NSColor clearColor], 0.0,
                            [NSColor clearColor], 0.5,
                            [NSColor colorWithWhite:0 alpha:kVignetteAlpha], 1.0,
                            nil];
    [NSGraphicsContext saveGraphicsState];
    NSAffineTransform *transform = [NSAffineTransform transform];
    [transform translateXBy:NSMidX(bounds) yBy:NSMidY(bounds)];
    [transform scaleXBy:NSWidth(bounds) / 2 yBy:NSHeight(bounds) / 2];
    [transform concat];
    [vignette drawFromCenter:NSZeroPoint radius:0
                    toCenter:NSZeroPoint radius:M_SQRT2
                     options:NSGradientDrawsAfterEndingLocation];
    [NSGraphicsContext restoreGraphicsState];
}

@end

// The effects are decoration, so they give way to the accessibility display
// options that ask for a plainer screen.
static BOOL accessibilityWantsPlainDisplay(void) {
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    return workspace.accessibilityDisplayShouldIncreaseContrast
    || workspace.accessibilityDisplayShouldReduceTransparency;
}

@implementation NSView (CRTEffects)

- (void)updateCRTGlow:(BOOL)on {
    on = on && !accessibilityWantsPlainDisplay();
    // Layer filters work in pixels, so the radius follows the screen.
    CGFloat scale = self.window.backingScaleFactor ?: 1;
    NSNumber *radius = @(kGlowRadius * scale);

    CIFilter *glow = nil;
    for (CIFilter *filter in self.contentFilters)
        if ([filter.name isEqualToString:kCRTGlowFilterName])
            glow = filter;
    if (on && [[glow valueForKey:kCIInputRadiusKey] isEqual:radius])
        return;
    if (!on) {
        if (glow)
            self.contentFilters = @[];
        return;
    }

    CIFilter *bloom = [CIFilter filterWithName:@"CIBloom"];
    [bloom setDefaults];
    bloom.name = kCRTGlowFilterName;
    [bloom setValue:radius forKey:kCIInputRadiusKey];
    [bloom setValue:@(kGlowIntensity) forKey:kCIInputIntensityKey];
    self.layerUsesCoreImageFilters = YES;
    self.contentFilters = @[bloom];
}

- (void)updateCRTScanlines:(BOOL)on {
    on = on && !accessibilityWantsPlainDisplay();
    CRTOverlayView *overlay = nil;
    for (NSView *view in self.subviews)
        if ([view isKindOfClass:[CRTOverlayView class]])
            overlay = (CRTOverlayView *)view;

    if (!on) {
        [overlay removeFromSuperview];
        return;
    }

    if (!overlay) {
        overlay = [[CRTOverlayView alloc] initWithFrame:self.bounds];
        overlay.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }
    // Keep it frontmost, also when subviews are added or taken out and added
    // back later (see CoverImageHandler).
    if (self.subviews.lastObject != overlay) {
        [overlay removeFromSuperview];
        overlay.frame = self.bounds;
        [self addSubview:overlay positioned:NSWindowAbove relativeTo:nil];
    }
}

@end

@implementation GlkController (CRTEffects)

- (void)updateCRTEffects {
    [self.gameView updateCRTGlow:self.theme.crtGlow];
    [self.borderView updateCRTScanlines:self.theme.crtScanlines];
}

- (void)noteAccessibilityDisplayOptionsChanged:(NSNotification *)notification {
    [self updateCRTEffects];
}

@end
