//
//  MapWindowController.m
//  Spatterlight
//
//  Separate OS window for the Glk mapping extension.
//  Loads SVG as an image so embedded scripts cannot run.
//

#import "MapWindowController.h"
#import "GlkController.h"
#import "GlkController+Autorestore.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkEvent.h"
#import "Game.h"
#import "Metadata.h"
#import "Theme.h"

#include "glk.h"
#include "protocol.h"

@implementation MapFocusRect
@end

@implementation MapHyperlink
@end

@implementation MapOverlay
@end

@class MapViewportView;

@interface MapClipView : NSClipView
@end

@interface MapViewportView : NSView
@property (weak) MapWindowController *controller;
@property (weak) NSScrollView *scrollView;
@property (strong, nullable) NSImage *mapImage;
@property (strong, nullable) NSColor *canvasColor;
@property (strong) NSArray<MapHyperlink *> *hyperlinks;
@property (strong) NSArray<MapOverlay *> *overlays;
@property NSInteger focusIndex; /* -1 = none */
@property CGFloat scale;
/* Offset of map (0,0) within this view when letterboxed to fill the clip. */
@property CGFloat mapOriginX;
@property CGFloat mapOriginY;
@property NSPoint clickStart;
- (void)updateDocumentFrame;
- (void)recenterOnFocus:(MapFocusRect *)focus;
- (void)zoomBy:(CGFloat)factor atPoint:(NSPoint)pointInClip;
- (nullable MapHyperlink *)hyperlinkAtDocumentPoint:(NSPoint)doc;
- (NSPoint)documentPointFromViewPoint:(NSPoint)viewPt;
@end

@implementation MapClipView
- (void)setFrameSize:(NSSize)newSize {
    NSSize old = self.frame.size;
    [super setFrameSize:newSize];
    if (NSEqualSizes(old, newSize))
        return;
    MapViewportView *vp = (MapViewportView *)self.documentView;
    if ([vp isKindOfClass:[MapViewportView class]])
        [vp updateDocumentFrame];
}
@end

@implementation MapViewportView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _scale = 1.0;
        _hyperlinks = @[];
        _overlays = @[];
        _focusIndex = -1;
        self.wantsLayer = YES;
        self.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)setCanvasColor:(NSColor *)canvasColor {
    _canvasColor = canvasColor;
    NSColor *fill = canvasColor ?: NSColor.windowBackgroundColor;
    self.layer.backgroundColor = fill.CGColor;
    /* Clip/scroll view show through when the document is smaller than the
       window; match the map canvas so letterboxing isn't window-white. */
    NSScrollView *sv = self.scrollView;
    if (sv) {
        sv.drawsBackground = YES;
        sv.backgroundColor = fill;
        sv.contentView.drawsBackground = YES;
        sv.contentView.backgroundColor = fill;
    }
    [self setNeedsDisplay:YES];
}

- (void)setHyperlinks:(NSArray<MapHyperlink *> *)hyperlinks {
    _hyperlinks = [hyperlinks copy] ?: @[];
    NSInteger n = (NSInteger)[self focusableCount];
    if (self.focusIndex >= n)
        self.focusIndex = n > 0 ? 0 : -1;
    [self setNeedsDisplay:YES];
}

- (void)setOverlays:(NSArray<MapOverlay *> *)overlays {
    _overlays = [overlays copy] ?: @[];
    NSInteger n = (NSInteger)[self focusableCount];
    if (self.focusIndex >= n)
        self.focusIndex = n > 0 ? 0 : -1;
    /* Letterbox chrome (side pads / top fill) may have changed. */
    [self setNeedsDisplay:YES];
}

static BOOL MapPointInPolygon(NSPoint p, NSArray<NSValue *> *pts) {
    if (pts.count < 3)
        return NO;
    /* Even-odd ray cast in document space. */
    BOOL inside = NO;
    NSUInteger n = pts.count;
    for (NSUInteger i = 0, j = n - 1; i < n; j = i++) {
        NSPoint pi = pts[i].pointValue;
        NSPoint pj = pts[j].pointValue;
        BOOL intersect = ((pi.y > p.y) != (pj.y > p.y))
            && (p.x < (pj.x - pi.x) * (p.y - pi.y) / ((pj.y - pi.y) + 0.0) + pi.x);
        if (intersect)
            inside = !inside;
    }
    return inside;
}

- (NSPoint)documentPointFromViewPoint:(NSPoint)viewPt {
    if (self.scale <= 0)
        return NSZeroPoint;
    return NSMakePoint((viewPt.x - self.mapOriginX) / self.scale,
                       (viewPt.y - self.mapOriginY) / self.scale);
}

- (NSRect)overlayDocRect:(MapOverlay *)ov {
    CGFloat ow = (CGFloat)ov.width;
    CGFloat oh = (CGFloat)ov.height;
    if (ov.image) {
        NSSize nat = ov.image.size;
        if (ow <= 0)
            ow = nat.width;
        if (oh <= 0)
            oh = nat.height;
    }
    return NSMakeRect((CGFloat)ov.left, (CGFloat)ov.top, ow, oh);
}

- (NSArray<MapOverlay *> *)linkedOverlays {
    NSMutableArray<MapOverlay *> *out = [NSMutableArray new];
    for (MapOverlay *ov in self.overlays) {
        if (ov.linkId != 0)
            [out addObject:ov];
    }
    [out sortUsingComparator:^NSComparisonResult(MapOverlay *a, MapOverlay *b) {
        if (a.zindex < b.zindex)
            return NSOrderedAscending;
        if (a.zindex > b.zindex)
            return NSOrderedDescending;
        return NSOrderedSame;
    }];
    return out;
}

- (NSUInteger)focusableCount {
    return self.hyperlinks.count + [self linkedOverlays].count;
}

- (MapHyperlink *)hyperlinkAtDocumentPoint:(NSPoint)doc {
    /* Topmost (last) wins. */
    for (NSInteger i = (NSInteger)self.hyperlinks.count - 1; i >= 0; i--) {
        MapHyperlink *h = self.hyperlinks[(NSUInteger)i];
        if (MapPointInPolygon(doc, h.points))
            return h;
    }
    return nil;
}

- (MapOverlay *)linkedOverlayAtDocumentPoint:(NSPoint)doc {
    NSArray<MapOverlay *> *linked = [self linkedOverlays];
    for (NSInteger i = (NSInteger)linked.count - 1; i >= 0; i--) {
        MapOverlay *ov = linked[(NSUInteger)i];
        if (NSPointInRect(doc, [self overlayDocRect:ov]))
            return ov;
    }
    return nil;
}

- (NSUInteger)linkIdAtDocumentPoint:(NSPoint)doc {
    MapOverlay *ov = [self linkedOverlayAtDocumentPoint:doc];
    if (ov)
        return ov.linkId;
    MapHyperlink *h = [self hyperlinkAtDocumentPoint:doc];
    return h ? h.linkId : 0;
}

/* Document → view. Expand to covering backing pixels so abutting tiles
   (e.g. side extends at ±mapWidth) do not leave hairline gaps under
   fractional scale / Retina rasterization. */
- (NSRect)viewRectFromDocLeft:(CGFloat)left
                          top:(CGFloat)top
                        width:(CGFloat)width
                       height:(CGFloat)height {
    NSRect r = NSMakeRect(self.mapOriginX + left * self.scale,
                          self.mapOriginY + top * self.scale,
                          width * self.scale,
                          height * self.scale);
    return [self backingAlignedRect:r options:NSAlignAllEdgesOutward];
}

- (void)drawRect:(NSRect)dirtyRect {
    [(self.canvasColor ?: NSColor.windowBackgroundColor) setFill];
    NSRectFill(self.bounds);
    if (!self.mapImage)
        return;
    NSSize sz = self.mapImage.size;
    NSDictionary *drawHints = @{NSImageHintInterpolation: @(NSImageInterpolationHigh)};
    NSRect dest = [self viewRectFromDocLeft:0 top:0 width:sz.width height:sz.height];
    [self.mapImage drawInRect:dest
                     fromRect:NSZeroRect
                    operation:NSCompositingOperationSourceOver
                     fraction:1.0
               respectFlipped:YES
                        hints:drawHints];

    NSArray<MapOverlay *> *sorted = [self.overlays sortedArrayUsingComparator:^NSComparisonResult(MapOverlay *a, MapOverlay *b) {
        if (a.zindex < b.zindex)
            return NSOrderedAscending;
        if (a.zindex > b.zindex)
            return NSOrderedDescending;
        return NSOrderedSame;
    }];
    for (MapOverlay *ov in sorted) {
        NSRect odest;
        if (ov.fillColor) {
            NSRect doc = [self overlayDocRect:ov];
            if (doc.size.width <= 0 || doc.size.height <= 0)
                continue;
            odest = [self viewRectFromDocLeft:doc.origin.x
                                          top:doc.origin.y
                                        width:doc.size.width
                                       height:doc.size.height];
            [ov.fillColor setFill];
            NSRectFill(odest);
            NSString *lab = ov.label;
            if (lab.length == 0) {
                CGFloat cx = doc.origin.x + doc.size.width / 2.0;
                CGFloat cy = doc.origin.y + doc.size.height / 2.0;
                for (MapHyperlink *h in self.hyperlinks) {
                    if (h.points.count < 3 || h.label.length == 0)
                        continue;
                    NSPoint p0 = h.points[0].pointValue;
                    NSPoint p2 = h.points.count > 2 ? h.points[2].pointValue : p0;
                    CGFloat minx = MIN(p0.x, p2.x), maxx = MAX(p0.x, p2.x);
                    CGFloat miny = MIN(p0.y, p2.y), maxy = MAX(p0.y, p2.y);
                    if (cx >= minx - 1 && cx <= maxx + 1 && cy >= miny - 1 && cy <= maxy + 1) {
                        lab = h.label;
                        break;
                    }
                }
            }
            if (lab.length > 0) {
                NSDictionary *attrs = @{
                    NSFontAttributeName: [NSFont systemFontOfSize:11.0],
                    NSForegroundColorAttributeName: [NSColor whiteColor]
                };
                NSSize ts = [lab sizeWithAttributes:attrs];
                NSPoint tp = NSMakePoint(odest.origin.x + (odest.size.width - ts.width) / 2.0,
                                        odest.origin.y + (odest.size.height - ts.height) / 2.0);
                [lab drawAtPoint:tp withAttributes:attrs];
            }
            continue;
        }
        if (!ov.image)
            continue;
        NSSize nat = ov.image.size;
        CGFloat ow = ov.width > 0 ? (CGFloat)ov.width : nat.width;
        CGFloat oh = ov.height > 0 ? (CGFloat)ov.height : nat.height;
        odest = [self viewRectFromDocLeft:(CGFloat)ov.left
                                      top:(CGFloat)ov.top
                                    width:ow
                                   height:oh];
        [ov.image drawInRect:odest
                    fromRect:NSZeroRect
                   operation:NSCompositingOperationSourceOver
                    fraction:1.0
               respectFlipped:YES
                       hints:drawHints];
    }

    if (self.focusIndex >= 0 && self.focusIndex < (NSInteger)[self focusableCount]) {
        NSColor *fill = [NSColor colorWithCalibratedRed:0.1 green:0.45 blue:0.95 alpha:0.35];
        NSColor *stroke = [NSColor colorWithCalibratedRed:0.1 green:0.35 blue:0.85 alpha:0.9];
        if (self.focusIndex < (NSInteger)self.hyperlinks.count) {
            MapHyperlink *h = self.hyperlinks[(NSUInteger)self.focusIndex];
            if (h.points.count >= 3) {
                NSBezierPath *path = [NSBezierPath bezierPath];
                NSPoint p0 = h.points[0].pointValue;
                [path moveToPoint:NSMakePoint(self.mapOriginX + p0.x * self.scale,
                                              self.mapOriginY + p0.y * self.scale)];
                for (NSUInteger i = 1; i < h.points.count; i++) {
                    NSPoint p = h.points[i].pointValue;
                    [path lineToPoint:NSMakePoint(self.mapOriginX + p.x * self.scale,
                                                  self.mapOriginY + p.y * self.scale)];
                }
                [path closePath];
                [fill setFill];
                [path fill];
                [stroke setStroke];
                path.lineWidth = 2.0;
                [path stroke];
            }
        } else {
            NSArray<MapOverlay *> *linked = [self linkedOverlays];
            NSInteger oi = self.focusIndex - (NSInteger)self.hyperlinks.count;
            if (oi >= 0 && oi < (NSInteger)linked.count) {
                MapOverlay *ov = linked[(NSUInteger)oi];
                NSRect r = [self overlayDocRect:ov];
                NSRect vr = [self viewRectFromDocLeft:r.origin.x
                                                 top:r.origin.y
                                               width:r.size.width
                                              height:r.size.height];
                NSBezierPath *path = [NSBezierPath bezierPathWithRect:vr];
                [fill setFill];
                [path fill];
                [stroke setStroke];
                path.lineWidth = 2.0;
                [path stroke];
            }
        }
    }
}

- (void)setMapImage:(NSImage *)mapImage {
    _mapImage = mapImage;
    [self updateDocumentFrame];
}

- (void)updateDocumentFrame {
    if (!self.mapImage) {
        self.mapOriginX = 0;
        self.mapOriginY = 0;
        self.frame = NSZeroRect;
        [self setNeedsDisplay:YES];
        return;
    }
    NSSize sz = self.mapImage.size;
    CGFloat scaledW = sz.width * self.scale;
    CGFloat scaledH = sz.height * self.scale;
    NSSize clipSize = NSZeroSize;
    if (self.scrollView)
        clipSize = self.scrollView.contentView.bounds.size;
    /* Letterbox: grow the document to at least the clip so overhanging
       overlays (side pads, top fill) paint into the empty margins, and
       center the map within that document. Scroll range stays map-sized
       when zoomed in past the window. */
    CGFloat docW = MAX(scaledW, clipSize.width);
    CGFloat docH = MAX(scaledH, clipSize.height);
    if (docW < 1)
        docW = scaledW;
    if (docH < 1)
        docH = scaledH;
    self.mapOriginX = MAX(0, (docW - scaledW) / 2.0);
    self.mapOriginY = MAX(0, (docH - scaledH) / 2.0);
    NSRect newFrame = NSMakeRect(0, 0, docW, docH);
    if (!NSEqualRects(self.frame, newFrame))
        self.frame = newFrame;
    [self setNeedsDisplay:YES];
}

- (void)recenterOnFocus:(MapFocusRect *)focus {
    if (!focus || !self.mapImage || !self.scrollView)
        return;
    NSClipView *clip = self.scrollView.contentView;
    NSSize vp = clip.bounds.size;
    if (vp.width < 8 || vp.height < 8)
        return;
    CGFloat pad = 28;
    CGFloat fw = (CGFloat)MAX(focus.width, 1);
    CGFloat fh = (CGFloat)MAX(focus.height, 1);
    CGFloat fit = MIN((vp.width - pad * 2) / fw, (vp.height - pad * 2) / fh);
    if (self.scale > fit)
        self.scale = MAX(0.2, fit);
    [self updateDocumentFrame];

    NSPoint origin = NSMakePoint(self.mapOriginX + (CGFloat)focus.left * self.scale + fw * self.scale / 2.0 - vp.width / 2.0,
                                 self.mapOriginY + (CGFloat)focus.top * self.scale + fh * self.scale / 2.0 - vp.height / 2.0);

    CGFloat fl = self.mapOriginX + (CGFloat)focus.left * self.scale;
    CGFloat ft = self.mapOriginY + (CGFloat)focus.top * self.scale;
    CGFloat fr = self.mapOriginX + ((CGFloat)focus.left + fw) * self.scale;
    CGFloat fb = self.mapOriginY + ((CGFloat)focus.top + fh) * self.scale;
    if (fl - origin.x < pad)
        origin.x = fl - pad;
    else if (fr - origin.x > vp.width - pad)
        origin.x = fr - (vp.width - pad);
    if (ft - origin.y < pad)
        origin.y = ft - pad;
    else if (fb - origin.y > vp.height - pad)
        origin.y = fb - (vp.height - pad);

    origin = [clip constrainScrollPoint:origin];
    [clip scrollToPoint:origin];
    [self.scrollView reflectScrolledClipView:clip];
}

- (void)zoomBy:(CGFloat)factor atPoint:(NSPoint)pointInClip {
    if (!self.scrollView)
        return;
    NSClipView *clip = self.scrollView.contentView;
    NSPoint scrollOrigin = clip.bounds.origin;
    CGFloat beforeX = (scrollOrigin.x + pointInClip.x - self.mapOriginX) / self.scale;
    CGFloat beforeY = (scrollOrigin.y + pointInClip.y - self.mapOriginY) / self.scale;
    self.scale = MIN(4.0, MAX(0.2, self.scale * factor));
    [self updateDocumentFrame];
    NSPoint newOrigin = NSMakePoint(beforeX * self.scale + self.mapOriginX - pointInClip.x,
                                    beforeY * self.scale + self.mapOriginY - pointInClip.y);
    newOrigin = [clip constrainScrollPoint:newOrigin];
    [clip scrollToPoint:newOrigin];
    [self.scrollView reflectScrolledClipView:clip];
}

- (void)mouseDown:(NSEvent *)event {
    self.clickStart = [self convertPoint:event.locationInWindow fromView:nil];
}

- (void)mouseUp:(NSEvent *)event {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    CGFloat dx = p.x - self.clickStart.x;
    CGFloat dy = p.y - self.clickStart.y;
    if (dx * dx + dy * dy >= 16.0)
        return;
    GlkController *glkctl = self.controller.glkctl;
    if (!glkctl.mapEventRequest)
        return;
    NSPoint doc = [self documentPointFromViewPoint:p];
    NSUInteger linkId = [self linkIdAtDocumentPoint:doc];
    if (!linkId)
        return;
    [self.controller noteHyperlinkActivated:linkId];
}

- (void)magnifyWithEvent:(NSEvent *)event {
    if (!self.scrollView)
        return;
    NSClipView *clip = self.scrollView.contentView;
    NSPoint p = [clip convertPoint:event.locationInWindow fromView:nil];
    [self zoomBy:event.magnification + 1.0 atPoint:p];
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    NSPoint doc = [self documentPointFromViewPoint:p];
    if ([self linkIdAtDocumentPoint:doc])
        [[NSCursor pointingHandCursor] set];
    else
        [[NSCursor arrowCursor] set];
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea *area in self.trackingAreas)
        [self removeTrackingArea:area];
    NSTrackingArea *area = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseMoved | NSTrackingActiveInKeyWindow |
                      NSTrackingInVisibleRect)
               owner:self
            userInfo:nil];
    [self addTrackingArea:area];
}

- (void)keyDown:(NSEvent *)event {
    NSString *chars = event.charactersIgnoringModifiers;
    if (chars.length == 0) {
        [super keyDown:event];
        return;
    }
    unichar c = [chars characterAtIndex:0];
    NSInteger n = (NSInteger)[self focusableCount];
    if (c == NSTabCharacter && n > 0) {
        BOOL shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
        if (self.focusIndex < 0)
            self.focusIndex = shift ? n - 1 : 0;
        else if (shift)
            self.focusIndex = (self.focusIndex + n - 1) % n;
        else
            self.focusIndex = (self.focusIndex + 1) % n;
        [self setNeedsDisplay:YES];
        return;
    }
    if ((c == NSCarriageReturnCharacter || c == NSEnterCharacter || c == ' ')
        && self.focusIndex >= 0 && self.focusIndex < n) {
        GlkController *glkctl = self.controller.glkctl;
        if (glkctl.mapEventRequest) {
            NSUInteger linkId = 0;
            if (self.focusIndex < (NSInteger)self.hyperlinks.count) {
                linkId = self.hyperlinks[(NSUInteger)self.focusIndex].linkId;
            } else {
                NSArray<MapOverlay *> *linked = [self linkedOverlays];
                NSInteger oi = self.focusIndex - (NSInteger)self.hyperlinks.count;
                if (oi >= 0 && oi < (NSInteger)linked.count)
                    linkId = linked[(NSUInteger)oi].linkId;
            }
            if (linkId)
                [self.controller noteHyperlinkActivated:linkId];
        }
        return;
    }
    [super keyDown:event];
}

@end

@interface MapWindowController () <NSToolbarDelegate>
@property (strong) NSScrollView *scrollView;
@property (strong) MapViewportView *viewport;
@property (strong, nullable) NSString *latentSVG;
@property (strong, nullable) NSImage *latentBitmap;
@property (strong, nullable) MapFocusRect *focus;
@property (strong, nullable) NSImage *cachedImage;
@property (strong) NSArray<MapHyperlink *> *latentHyperlinks;
@property (strong) NSMutableArray<MapOverlay *> *overlayList;
@property NSUInteger latentBgcolor; /* mapcolor_Default = theme buffer background */
@property BOOL visibleFlag;
/* Overlay/hyperlink mutations update overlayList only; viewport paints on
   flush (MAPFOCUS, present, Glk flushDisplay) so mid-turn Inform gaps do
   not flash "here" before focus. */
@property BOOL mapDisplayDirty;
@property BOOL mapFocusDirty;
@property NSUInteger mapDeferredMutations;
@property (strong, nullable) NSTimer *mapDisplaySafetyTimer;
@property BOOL ignoringMapFramePersistence;
@property BOOL mapLiveResizing;
@end

static BOOL AgentMapFrameOverlapsWindow(NSRect mapFrame, NSRect otherFrame) {
    if (NSIsEmptyRect(mapFrame) || NSIsEmptyRect(otherFrame))
        return NO;
    NSRect isect = NSIntersectionRect(mapFrame, otherFrame);
    if (NSIsEmptyRect(isect))
        return NO;
    double overlapArea = isect.size.width * isect.size.height;
    double mapArea = mapFrame.size.width * mapFrame.size.height;
    return mapArea > 0 && overlapArea > mapArea * 0.1;
}

static BOOL AgentMapFrameChangeFromUser(void) {
    NSEvent *event = NSApp.currentEvent;
    if (!event)
        return NO;
    switch (event.type) {
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeLeftMouseUp:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeRightMouseUp:
            return YES;
        default:
            return NO;
    }
}

static NSString * const MapToolbarZoomOutID = @"MapZoomOut";
static NSString * const MapToolbarZoomInID = @"MapZoomIn";

@implementation MapWindowController

- (instancetype)initWithGlkController:(GlkController *)glkctl {
    NSWindow *win = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 720, 640)
                  styleMask:(NSWindowStyleMaskTitled |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable |
                             NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:YES];
    win.releasedWhenClosed = NO;
    win.minSize = NSMakeSize(200, 160);

    self = [super initWithWindow:win];
    if (self) {
        _glkctl = glkctl;
        _latentHyperlinks = @[];
        _overlayList = [NSMutableArray new];
        _latentBgcolor = mapcolor_Default;

        NSString *gameTitle = glkctl.game.metadata.title;
        if (gameTitle.length == 0)
            gameTitle = glkctl.gamefile.lastPathComponent;
        if (gameTitle.length)
            win.title = [NSString stringWithFormat:NSLocalizedString(@"Map - %@", nil), gameTitle];
        else
            win.title = NSLocalizedString(@"Map", nil);

        _viewport = [[MapViewportView alloc] initWithFrame:NSZeroRect];
        _viewport.controller = self;

        _scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 720, 640)];
        _scrollView.hasVerticalScroller = YES;
        _scrollView.hasHorizontalScroller = YES;
        _scrollView.autohidesScrollers = NO;
        _scrollView.borderType = NSNoBorder;
        _scrollView.drawsBackground = YES;
        _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _scrollView.contentView = [[MapClipView alloc] initWithFrame:NSZeroRect];
        _scrollView.documentView = _viewport;
        _viewport.scrollView = _scrollView;
        win.contentView = _scrollView;

        NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"SpatterlightMapToolbar"];
        toolbar.delegate = self;
        toolbar.displayMode = NSToolbarDisplayModeIconAndLabel;
        toolbar.allowsUserCustomization = NO;
        win.toolbar = toolbar;

        NSRect stored = glkctl.storedMapWindowFrame;
        NSRect gameFrame = glkctl.window.frame;
        BOOL overlapsGame = AgentMapFrameOverlapsWindow(stored, gameFrame);
        if (!NSIsEmptyRect(stored) && !overlapsGame) {
            if (stored.size.width < win.minSize.width)
                stored.size.width = win.minSize.width;
            if (stored.size.height < win.minSize.height)
                stored.size.height = win.minSize.height;
            [self setMapWindowFrame:stored display:NO];
        } else if (overlapsGame) {
            glkctl.storedMapWindowFrame = NSZeroRect;
        }
        win.delegate = self;
    }
    return self;
}

- (BOOL)userHidMap {
    return self.glkctl.userHidMap;
}

- (void)setUserHidMap:(BOOL)userHidMap {
    if (self.glkctl.userHidMap == userHidMap)
        return;
    self.glkctl.userHidMap = userHidMap;
    if (self.glkctl.supportsAutorestore && self.glkctl.theme.autosave)
        [self.glkctl handleAutosave:self.glkctl.autosaveTag];
}

- (void)setMapWindowFrame:(NSRect)frame display:(BOOL)display {
    self.ignoringMapFramePersistence = YES;
    [self.window setFrame:frame display:display];
    self.ignoringMapFramePersistence = NO;
}

- (void)noteMapWindowFrameChanged {
    if (self.ignoringMapFramePersistence)
        return;
    NSRect frame = self.window.frame;
    if (NSEqualRects(self.glkctl.storedMapWindowFrame, frame))
        return;
    self.glkctl.storedMapWindowFrame = frame;
    if (self.glkctl.supportsAutorestore && self.glkctl.theme.autosave)
        [self.glkctl handleAutosave:self.glkctl.autosaveTag];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return @[ MapToolbarZoomOutID, MapToolbarZoomInID, NSToolbarFlexibleSpaceItemIdentifier ];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    return @[ MapToolbarZoomOutID, MapToolbarZoomInID, NSToolbarFlexibleSpaceItemIdentifier ];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag {
    if ([itemIdentifier isEqualToString:MapToolbarZoomOutID]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom Out", nil);
        item.paletteLabel = item.label;
        item.toolTip = NSLocalizedString(@"Zoom out", nil);
        if (@available(macOS 11.0, *)) {
            item.image = [NSImage imageWithSystemSymbolName:@"minus.magnifyingglass"
                                   accessibilityDescription:item.label];
        } else {
            NSButton *button = [NSButton buttonWithTitle:@"−" target:self action:@selector(zoomOut:)];
            button.bezelStyle = NSBezelStyleTexturedRounded;
            item.view = button;
        }
        item.target = self;
        item.action = @selector(zoomOut:);
        return item;
    }
    if ([itemIdentifier isEqualToString:MapToolbarZoomInID]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom In", nil);
        item.paletteLabel = item.label;
        item.toolTip = NSLocalizedString(@"Zoom in", nil);
        if (@available(macOS 11.0, *)) {
            item.image = [NSImage imageWithSystemSymbolName:@"plus.magnifyingglass"
                                   accessibilityDescription:item.label];
        } else {
            NSButton *button = [NSButton buttonWithTitle:@"+" target:self action:@selector(zoomIn:)];
            button.bezelStyle = NSBezelStyleTexturedRounded;
            item.view = button;
        }
        item.target = self;
        item.action = @selector(zoomIn:);
        return item;
    }
    return nil;
}

- (BOOL)mapVisible {
    return self.visibleFlag && self.window.isVisible;
}

- (BOOL)hasDocument {
    return self.latentSVG.length > 0 || self.latentBitmap != nil;
}

- (void)applyOverlaysToViewport {
    self.viewport.overlays = [self.overlayList copy];
}

- (void)cancelMapDisplaySafetyTimer {
    [self.mapDisplaySafetyTimer invalidate];
    self.mapDisplaySafetyTimer = nil;
}

- (void)markMapDisplayDirty {
    self.mapDisplayDirty = YES;
    self.mapDeferredMutations += 1;
    [self cancelMapDisplaySafetyTimer];
    /* Safety only: overlays with no MAPFOCUS / flushDisplay still paint. */
    __weak typeof(self) weakSelf = self;
    self.mapDisplaySafetyTimer =
        [NSTimer scheduledTimerWithTimeInterval:0.25
                                        repeats:NO
                                          block:^(__unused NSTimer *timer) {
                                              [weakSelf flushPendingMapDisplayWithReason:"timer"];
                                          }];
}

- (void)flushPendingMapDisplay {
    [self flushPendingMapDisplayWithReason:"flushDisplay"];
}

- (void)flushPendingMapDisplayWithReason:(const char *)reason {
    BOOL dirty = self.mapDisplayDirty;
    BOOL focusDirty = self.mapFocusDirty;
    if (!dirty && !focusDirty) {
        [self cancelMapDisplaySafetyTimer];
        return;
    }
    (void)reason;
    if (dirty) {
        self.viewport.hyperlinks = self.latentHyperlinks;
        [self applyOverlaysToViewport];
    }
    if (focusDirty && self.mapVisible && self.focus)
        [self.viewport recenterOnFocus:self.focus];
    self.mapDisplayDirty = NO;
    self.mapFocusDirty = NO;
    self.mapDeferredMutations = 0;
    [self cancelMapDisplaySafetyTimer];
}

- (void)clearAllOverlays {
    [self.overlayList removeAllObjects];
    [self markMapDisplayDirty];
}

- (void)setHyperlinks:(NSArray<MapHyperlink *> *)hyperlinks {
    self.latentHyperlinks = [hyperlinks copy] ?: @[];
    [self markMapDisplayDirty];
}

- (void)setOverlay:(MapOverlay *)overlay {
    if (!overlay)
        return;
    for (NSUInteger i = 0; i < self.overlayList.count; i++) {
        if (self.overlayList[i].overlayId == overlay.overlayId) {
            self.overlayList[i] = overlay;
            [self markMapDisplayDirty];
            return;
        }
    }
    [self.overlayList addObject:overlay];
    [self markMapDisplayDirty];
}

- (void)moveOverlay:(NSUInteger)overlayId
               left:(NSInteger)left
                top:(NSInteger)top
              width:(NSUInteger)width
             height:(NSUInteger)height
             zindex:(NSUInteger)zindex {
    for (MapOverlay *ov in self.overlayList) {
        if (ov.overlayId == overlayId) {
            ov.left = left;
            ov.top = top;
            ov.width = width;
            ov.height = height;
            ov.zindex = zindex;
            [self markMapDisplayDirty];
            return;
        }
    }
}

- (void)clearOverlay:(NSUInteger)overlayId {
    NSMutableArray<MapOverlay *> *keep = [NSMutableArray new];
    for (MapOverlay *ov in self.overlayList) {
        if (ov.overlayId != overlayId)
            [keep addObject:ov];
    }
    self.overlayList = keep;
    [self markMapDisplayDirty];
}

static NSColor *MapColorFromRGB(NSUInteger rgb) {
    return [NSColor colorWithSRGBRed:((rgb >> 16) & 0xff) / 255.0
                               green:((rgb >> 8) & 0xff) / 255.0
                                blue:(rgb & 0xff) / 255.0
                               alpha:1.0];
}

- (NSColor *)resolvedCanvasColor {
    if (self.latentBgcolor != mapcolor_Default)
        return MapColorFromRGB(self.latentBgcolor);
    NSColor *pref = self.glkctl.theme.bufferBackground;
    return pref ?: NSColor.textBackgroundColor;
}

- (void)renderDocument {
    if (self.latentBitmap) {
        self.cachedImage = self.latentBitmap;
        self.viewport.canvasColor = [self resolvedCanvasColor];
        self.viewport.mapImage = self.latentBitmap;
        self.viewport.hyperlinks = self.latentHyperlinks;
        [self applyOverlaysToViewport];
        self.mapDisplayDirty = NO;
        self.mapDeferredMutations = 0;
        [self cancelMapDisplaySafetyTimer];
        return;
    }
    if (!self.latentSVG.length) {
        self.cachedImage = nil;
        self.viewport.mapImage = nil;
        self.viewport.canvasColor = nil;
        self.viewport.hyperlinks = @[];
        self.viewport.overlays = @[];
        self.mapDisplayDirty = NO;
        self.mapDeferredMutations = 0;
        [self cancelMapDisplaySafetyTimer];
        return;
    }
    NSData *data = [self.latentSVG dataUsingEncoding:NSUTF8StringEncoding];
    NSImage *img = data ? [[NSImage alloc] initWithData:data] : nil;
    self.cachedImage = img;
    self.viewport.canvasColor = [self resolvedCanvasColor];
    self.viewport.mapImage = img;
    self.viewport.hyperlinks = self.latentHyperlinks;
    [self applyOverlaysToViewport];
    self.mapDisplayDirty = NO;
    self.mapDeferredMutations = 0;
    [self cancelMapDisplaySafetyTimer];
}

- (void)renderSVG {
    [self renderDocument];
}

- (void)prepareMapWindowFrameBesideGame {
    NSRect stored = self.glkctl.storedMapWindowFrame;
    NSRect gameFrame = self.glkctl.window.frame;
    if (!NSIsEmptyRect(stored)) {
        if (AgentMapFrameOverlapsWindow(stored, gameFrame)) {
            self.glkctl.storedMapWindowFrame = NSZeroRect;
            stored = NSZeroRect;
        } else {
            return;
        }
    }
    NSWindow *gameWin = self.glkctl.window;
    if (gameWin && !self.window.isVisible) {
        NSRect gf = gameWin.frame;
        NSRect mf = self.window.frame;
        mf.origin.x = NSMaxX(gf) + 12;
        mf.origin.y = NSMaxY(gf) - mf.size.height;
        [self setMapWindowFrame:mf display:NO];
        self.glkctl.storedMapWindowFrame = mf;
    }
}

- (void)showInternal {
    if (!self.hasDocument)
        return;
    [self renderDocument];
    [self prepareMapWindowFrameBesideGame];
    /* Glk-driven show: visible beside the game without stealing key focus. */
    [self.window orderFront:nil];
    self.visibleFlag = YES;
    if (self.focus) {
        self.mapFocusDirty = YES;
        [self flushPendingMapDisplayWithReason:"show"];
    }
    NSWindow *gameWin = self.glkctl.window;
    if (gameWin && gameWin.isVisible)
        [gameWin makeKeyWindow];
}

- (void)showInternalTakingKey {
    if (!self.hasDocument)
        return;
    [self renderDocument];
    [self prepareMapWindowFrameBesideGame];
    [self showWindow:nil];
    self.visibleFlag = YES;
    if (self.focus) {
        self.mapFocusDirty = YES;
        [self flushPendingMapDisplayWithReason:"show-key"];
    }
    [self.window makeFirstResponder:self.viewport];
}

- (void)hideInternal {
    self.visibleFlag = NO;
    [self.window orderOut:nil];
}

- (void)presentAfterUpdateWithFocus:(nullable MapFocusRect *)focus {
    BOOL hasFocus = focus && focus.width > 0 && focus.height > 0;
    if (focus) {
        _focus = focus;
        self.mapFocusDirty = YES;
    }

    if (self.mapVisible)
        [self renderDocument];
    else {
        self.mapDisplayDirty = YES;
        [self flushPendingMapDisplayWithReason:"present-hidden"];
    }

    if (!self.userHidMap) {
        [self showInternal];
        if (self.focus && hasFocus) {
            self.mapFocusDirty = YES;
            [self flushPendingMapDisplayWithReason:"present"];
        } else if (self.mapVisible) {
            [self flushPendingMapDisplayWithReason:"present"];
        }
    } else if (self.mapVisible) {
        if (hasFocus && self.focus)
            self.mapFocusDirty = YES;
        [self flushPendingMapDisplayWithReason:"present"];
    }
}

- (void)presentSVG:(NSString *)svg
          bgcolor:(NSUInteger)bgcolor
             focus:(MapFocusRect *)focus
       hyperlinks:(NSArray<MapHyperlink *> *)hyperlinks {
    [self clearAllOverlays];
    self.latentBitmap = nil;
    self.latentSVG = [svg copy];
    self.latentBgcolor = bgcolor;
    self.latentHyperlinks = [hyperlinks copy] ?: @[];
    [self presentAfterUpdateWithFocus:focus];
}

- (void)presentImage:(NSImage *)image
            bgcolor:(NSUInteger)bgcolor
               focus:(MapFocusRect *)focus
          hyperlinks:(NSArray<MapHyperlink *> *)hyperlinks {
    if (!image)
        return;
    [self clearAllOverlays];
    self.latentSVG = nil;
    self.latentBitmap = image;
    self.latentBgcolor = bgcolor;
    self.latentHyperlinks = [hyperlinks copy] ?: @[];
    [self presentAfterUpdateWithFocus:focus];
}

- (void)closeMap {
    [self cancelMapDisplaySafetyTimer];
    self.latentSVG = nil;
    self.latentBitmap = nil;
    self.latentBgcolor = mapcolor_Default;
    _focus = nil;
    self.cachedImage = nil;
    self.latentHyperlinks = @[];
    [self.overlayList removeAllObjects];
    self.mapDisplayDirty = NO;
    self.mapFocusDirty = NO;
    self.mapDeferredMutations = 0;
    self.viewport.mapImage = nil;
    self.viewport.canvasColor = nil;
    self.viewport.hyperlinks = @[];
    self.viewport.overlays = @[];
    self.viewport.focusIndex = -1;
    self.viewport.scale = 1.0;
    [self.viewport updateDocumentFrame];
    NSClipView *clip = self.scrollView.contentView;
    [clip scrollToPoint:NSZeroPoint];
    [self.scrollView reflectScrolledClipView:clip];
    self.glkctl.mapEventRequest = NO;
    [self hideInternal];
}

- (void)setFocus:(MapFocusRect *)focus {
    _focus = focus;
    self.mapFocusDirty = YES;
    /* Flush overlays + pan together so mid-turn Inform work cannot paint
       an updated "here" with the previous focus rect. */
    [self flushPendingMapDisplayWithReason:"focus"];
}

- (void)showMap {
    [self showMapAtUserRequest];
}

- (void)showMapAtUserRequest {
    self.userHidMap = NO;
    [self showInternalTakingKey];
}

- (void)hideMap {
    [self hideFromUser];
}

- (void)noteMapEventSubtype:(NSUInteger)subtype payload:(NSUInteger)payload {
    GlkController *glkctl = self.glkctl;
    if (!glkctl.mapEventRequest)
        return;
    glkctl.mapEventRequest = NO;
    [glkctl markLastSeen];
    GlkEvent *gev = [[GlkEvent alloc] initMapEventSubtype:subtype payload:payload];
    [glkctl queueEvent:gev];
}

- (void)noteHyperlinkActivated:(NSUInteger)linkId {
    [self noteMapEventSubtype:mapevent_Hyperlink payload:linkId];
}

- (void)hideFromUser {
    BOOL wasVisible = self.mapVisible;
    self.userHidMap = YES;
    [self hideInternal];
    if (wasVisible)
        [self noteMapEventSubtype:mapevent_UserHide payload:0];

}

- (BOOL)windowShouldClose:(NSWindow *)sender {
    [self hideFromUser];
    return NO;
}

- (void)windowDidMove:(NSNotification *)notification {
    if (AgentMapFrameChangeFromUser())
        [self noteMapWindowFrameChanged];
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
    self.mapLiveResizing = YES;
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
    self.mapLiveResizing = NO;
    [self noteMapWindowFrameChanged];
}

- (void)windowDidResize:(NSNotification *)notification {
    if (self.mapLiveResizing)
        [self noteMapWindowFrameChanged];
}

- (void)zoomIn:(id)sender {
    NSClipView *clip = self.scrollView.contentView;
    NSSize vp = clip.bounds.size;
    [self.viewport zoomBy:1.25 atPoint:NSMakePoint(vp.width / 2, vp.height / 2)];
}

- (void)zoomOut:(id)sender {
    NSClipView *clip = self.scrollView.contentView;
    NSSize vp = clip.bounds.size;
    [self.viewport zoomBy:1.0 / 1.25 atPoint:NSMakePoint(vp.width / 2, vp.height / 2)];
}

@end
