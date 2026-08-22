//
//  MapWindowController.m
//  Spatterlight
//
//  Separate OS window for the Glk map-document extension (SVG).
//  Loads SVG as an image so embedded scripts cannot run.
//

#import "MapWindowController.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkEvent.h"
#import "Game.h"
#import "Metadata.h"

#include "glk.h"
#include "protocol.h"

@implementation MapFocusRect
@end

@implementation MapHyperlink
@end

@implementation MapOverlay
@end

@interface MapViewportView : NSView
@property (weak) MapWindowController *controller;
@property (strong, nullable) NSImage *mapImage;
@property (strong, nullable) NSColor *canvasColor;
@property (strong) NSArray<MapHyperlink *> *hyperlinks;
@property (strong) NSArray<MapOverlay *> *overlays;
@property NSInteger focusIndex; /* -1 = none */
@property CGFloat scale;
@property CGFloat panX;
@property CGFloat panY;
@property BOOL dragging;
@property BOOL didDrag;
@property NSPoint dragLast;
@property NSPoint dragStart;
- (void)recenterOnFocus:(MapFocusRect *)focus;
- (void)zoomBy:(CGFloat)factor atPoint:(NSPoint)pointInView;
- (nullable MapHyperlink *)hyperlinkAtDocumentPoint:(NSPoint)doc;
- (NSPoint)documentPointFromViewPoint:(NSPoint)viewPt;
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
    self.layer.backgroundColor = (canvasColor ?: NSColor.windowBackgroundColor).CGColor;
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
    return NSMakePoint((viewPt.x - self.panX) / self.scale,
                       (viewPt.y - self.panY) / self.scale);
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
    NSRect r = NSMakeRect(self.panX + left * self.scale,
                          self.panY + top * self.scale,
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
                [path moveToPoint:NSMakePoint(p0.x * self.scale + self.panX,
                                              p0.y * self.scale + self.panY)];
                for (NSUInteger i = 1; i < h.points.count; i++) {
                    NSPoint p = h.points[i].pointValue;
                    [path lineToPoint:NSMakePoint(p.x * self.scale + self.panX,
                                                  p.y * self.scale + self.panY)];
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
    [self setNeedsDisplay:YES];
}

- (void)applyTransform {
    [self setNeedsDisplay:YES];
}

- (void)recenterOnFocus:(MapFocusRect *)focus {
    if (!focus || !self.mapImage)
        return;
    NSSize vp = self.bounds.size;
    if (vp.width < 8 || vp.height < 8)
        return;
    CGFloat pad = 28;
    CGFloat fw = (CGFloat)MAX(focus.width, 1);
    CGFloat fh = (CGFloat)MAX(focus.height, 1);
    CGFloat fit = MIN((vp.width - pad * 2) / fw, (vp.height - pad * 2) / fh);
    if (self.scale > fit)
        self.scale = MAX(0.2, fit);

    CGFloat fx = (CGFloat)focus.left + fw / 2.0;
    CGFloat fy = (CGFloat)focus.top + fh / 2.0;
    self.panX = vp.width / 2.0 - fx * self.scale;
    self.panY = vp.height / 2.0 - fy * self.scale;

    CGFloat fl = (CGFloat)focus.left * self.scale + self.panX;
    CGFloat ft = (CGFloat)focus.top * self.scale + self.panY;
    CGFloat fr = ((CGFloat)focus.left + fw) * self.scale + self.panX;
    CGFloat fb = ((CGFloat)focus.top + fh) * self.scale + self.panY;
    if (fl < pad)
        self.panX += pad - fl;
    else if (fr > vp.width - pad)
        self.panX -= fr - (vp.width - pad);
    if (ft < pad)
        self.panY += pad - ft;
    else if (fb > vp.height - pad)
        self.panY -= fb - (vp.height - pad);
    [self applyTransform];
}

- (void)zoomBy:(CGFloat)factor atPoint:(NSPoint)pointInView {
    CGFloat beforeX = (pointInView.x - self.panX) / self.scale;
    CGFloat beforeY = (pointInView.y - self.panY) / self.scale;
    self.scale = MIN(4.0, MAX(0.2, self.scale * factor));
    self.panX = pointInView.x - beforeX * self.scale;
    self.panY = pointInView.y - beforeY * self.scale;
    [self applyTransform];
}

- (void)mouseDown:(NSEvent *)event {
    self.dragging = YES;
    self.didDrag = NO;
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.dragStart = p;
    self.dragLast = p;
}

- (void)mouseDragged:(NSEvent *)event {
    if (!self.dragging)
        return;
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    if (!self.didDrag) {
        CGFloat dx = p.x - self.dragStart.x;
        CGFloat dy = p.y - self.dragStart.y;
        if (dx * dx + dy * dy < 16.0)
            return;
        self.didDrag = YES;
    }
    self.panX += p.x - self.dragLast.x;
    self.panY += p.y - self.dragLast.y;
    self.dragLast = p;
    [self applyTransform];
}

- (void)mouseUp:(NSEvent *)event {
    if (!self.dragging) {
        self.dragging = NO;
        return;
    }
    self.dragging = NO;
    if (self.didDrag)
        return;
    GlkController *glkctl = self.controller.glkctl;
    if (!glkctl.mapEventRequest)
        return;
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    NSPoint doc = [self documentPointFromViewPoint:p];
    NSUInteger linkId = [self linkIdAtDocumentPoint:doc];
    if (!linkId)
        return;
    [self.controller noteHyperlinkActivated:linkId];
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

- (void)magnifyWithEvent:(NSEvent *)event {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    [self zoomBy:event.magnification + 1.0 atPoint:p];
}

@end

@interface MapWindowController () <NSToolbarDelegate>
@property (strong) MapViewportView *viewport;
@property (strong, nullable) NSString *latentSVG;
@property (strong, nullable) NSImage *latentBitmap;
@property (strong, nullable) MapFocusRect *focus;
@property (strong, nullable) NSImage *cachedImage;
@property (strong) NSArray<MapHyperlink *> *latentHyperlinks;
@property (strong) NSMutableArray<MapOverlay *> *overlayList;
@property NSUInteger latentBgcolor; /* mapcolor_Default = auto */
@property BOOL suggestedOnce;
@property BOOL visibleFlag;
@end

static NSString * const MapToolbarZoomOutID = @"MapZoomOut";
static NSString * const MapToolbarZoomInID = @"MapZoomIn";

static NSColor *MapCanvasColorFromSVG(NSString *svg) {
    if (svg.length < 20)
        return nil;
    NSRange origin = [svg rangeOfString:@"<rect x=\"0\" y=\"0\""];
    if (origin.location == NSNotFound)
        return nil;
    NSUInteger from = origin.location;
    NSUInteger len = MIN((NSUInteger)160, svg.length - from);
    NSRange fill = [svg rangeOfString:@"fill=\"#"
                              options:0
                                range:NSMakeRange(from, len)];
    if (fill.location == NSNotFound)
        return nil;
    NSUInteger hexAt = fill.location + fill.length;
    if (hexAt + 6 > svg.length)
        return nil;
    NSString *hex = [svg substringWithRange:NSMakeRange(hexAt, 6)];
    unsigned int rgb = 0;
    NSScanner *scanner = [NSScanner scannerWithString:hex];
    if (![scanner scanHexInt:&rgb])
        return nil;
    return [NSColor colorWithSRGBRed:((rgb >> 16) & 0xff) / 255.0
                               green:((rgb >> 8) & 0xff) / 255.0
                                blue:(rgb & 0xff) / 255.0
                               alpha:1.0];
}

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
        win.delegate = self;

        NSString *gameTitle = glkctl.game.metadata.title;
        if (gameTitle.length == 0)
            gameTitle = glkctl.gamefile.lastPathComponent;
        if (gameTitle.length)
            win.title = [NSString stringWithFormat:NSLocalizedString(@"Map - %@", nil), gameTitle];
        else
            win.title = NSLocalizedString(@"Map", nil);

        _viewport = [[MapViewportView alloc] initWithFrame:NSMakeRect(0, 0, 720, 640)];
        _viewport.controller = self;
        _viewport.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        win.contentView = _viewport;

        NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"SpatterlightMapToolbar"];
        toolbar.delegate = self;
        toolbar.displayMode = NSToolbarDisplayModeIconAndLabel;
        toolbar.allowsUserCustomization = NO;
        win.toolbar = toolbar;
    }
    return self;
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

- (void)clearAllOverlays {
    [self.overlayList removeAllObjects];
    [self applyOverlaysToViewport];
}

- (void)setHyperlinks:(NSArray<MapHyperlink *> *)hyperlinks {
    self.latentHyperlinks = [hyperlinks copy] ?: @[];
    self.viewport.hyperlinks = self.latentHyperlinks;
}

- (void)setOverlay:(MapOverlay *)overlay {
    if (!overlay)
        return;
    for (NSUInteger i = 0; i < self.overlayList.count; i++) {
        if (self.overlayList[i].overlayId == overlay.overlayId) {
            self.overlayList[i] = overlay;
            [self applyOverlaysToViewport];
            return;
        }
    }
    [self.overlayList addObject:overlay];
    [self applyOverlaysToViewport];
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
            [self applyOverlaysToViewport];
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
    [self applyOverlaysToViewport];
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
    if (self.latentSVG.length)
        return MapCanvasColorFromSVG(self.latentSVG);
    return nil;
}

- (void)renderDocument {
    if (self.latentBitmap) {
        self.cachedImage = self.latentBitmap;
        self.viewport.canvasColor = [self resolvedCanvasColor];
        self.viewport.mapImage = self.latentBitmap;
        self.viewport.hyperlinks = self.latentHyperlinks;
        [self applyOverlaysToViewport];
        return;
    }
    if (!self.latentSVG.length) {
        self.cachedImage = nil;
        self.viewport.mapImage = nil;
        self.viewport.canvasColor = nil;
        self.viewport.hyperlinks = @[];
        self.viewport.overlays = @[];
        return;
    }
    NSData *data = [self.latentSVG dataUsingEncoding:NSUTF8StringEncoding];
    NSImage *img = [[NSImage alloc] initWithData:data];
    self.cachedImage = img;
    self.viewport.canvasColor = [self resolvedCanvasColor];
    self.viewport.mapImage = img;
    self.viewport.hyperlinks = self.latentHyperlinks;
    [self applyOverlaysToViewport];
}

- (void)renderSVG {
    [self renderDocument];
}

- (void)prepareMapWindowFrameBesideGame {
    NSWindow *gameWin = self.glkctl.window;
    if (gameWin && !self.window.isVisible) {
        NSRect gf = gameWin.frame;
        NSRect mf = self.window.frame;
        mf.origin.x = NSMaxX(gf) + 12;
        mf.origin.y = NSMaxY(gf) - mf.size.height;
        [self.window setFrame:mf display:NO];
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
    if (self.focus)
        [self.viewport recenterOnFocus:self.focus];
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
    if (self.focus)
        [self.viewport recenterOnFocus:self.focus];
    [self.window makeFirstResponder:self.viewport];
}

- (void)hideInternal {
    self.visibleFlag = NO;
    [self.window orderOut:nil];
}

- (void)presentWithFlags:(NSUInteger)flags focus:(MapFocusRect *)focus {
    if ((flags & mapflag_HasFocus) != 0 && focus) {
        self.focus = focus;
    } else if (focus) {
        self.focus = focus;
    }

    if (self.mapVisible)
        [self renderDocument];
    else {
        self.viewport.hyperlinks = self.latentHyperlinks;
        [self applyOverlaysToViewport];
    }

    BOOL force = (flags & mapflag_UserRequestedShow) != 0;
    BOOL suggest = (flags & mapflag_SuggestShow) != 0;

    if (force) {
        [self showInternal];
        if (self.focus && (flags & mapflag_HasFocus))
            [self.viewport recenterOnFocus:self.focus];
    } else if (suggest && !self.suggestedOnce) {
        self.suggestedOnce = YES;
        [self showInternal];
        if (self.focus)
            [self.viewport recenterOnFocus:self.focus];
    } else if (self.mapVisible) {
        if ((flags & mapflag_HasFocus) != 0 && self.focus)
            [self.viewport recenterOnFocus:self.focus];
    }
}

- (void)presentSVG:(NSString *)svg
             flags:(NSUInteger)flags
          bgcolor:(NSUInteger)bgcolor
             focus:(MapFocusRect *)focus
          hyperlinks:(NSArray<MapHyperlink *> *)hyperlinks {
    [self clearAllOverlays];
    self.latentBitmap = nil;
    self.latentSVG = [svg copy];
    self.latentBgcolor = bgcolor;
    self.latentHyperlinks = [hyperlinks copy] ?: @[];
    [self presentWithFlags:flags focus:focus];
}

- (void)presentImage:(NSImage *)image
               flags:(NSUInteger)flags
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
    [self presentWithFlags:flags focus:focus];
}

- (void)closeMap {
    self.latentSVG = nil;
    self.latentBitmap = nil;
    self.latentBgcolor = mapcolor_Default;
    self.focus = nil;
    self.cachedImage = nil;
    self.latentHyperlinks = @[];
    [self clearAllOverlays];
    self.viewport.mapImage = nil;
    self.viewport.canvasColor = nil;
    self.viewport.hyperlinks = @[];
    self.viewport.focusIndex = -1;
    self.viewport.scale = 1.0;
    self.viewport.panX = 0;
    self.viewport.panY = 0;
    self.suggestedOnce = NO;
    self.glkctl.mapEventRequest = NO;
    [self hideInternal];
}

- (void)setFocus:(MapFocusRect *)focus {
    _focus = focus;
    if (self.mapVisible && focus)
        [self.viewport recenterOnFocus:focus];
}

- (void)showMap {
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
    [self hideInternal];
    if (wasVisible)
        [self noteMapEventSubtype:mapevent_UserHide payload:0];

}

- (BOOL)windowShouldClose:(NSWindow *)sender {
    [self hideFromUser];
    return NO;
}

- (void)zoomIn:(id)sender {
    NSSize vp = self.viewport.bounds.size;
    [self.viewport zoomBy:1.25 atPoint:NSMakePoint(vp.width / 2, vp.height / 2)];
}

- (void)zoomOut:(id)sender {
    NSSize vp = self.viewport.bounds.size;
    [self.viewport zoomBy:1.0 / 1.25 atPoint:NSMakePoint(vp.width / 2, vp.height / 2)];
}

@end
