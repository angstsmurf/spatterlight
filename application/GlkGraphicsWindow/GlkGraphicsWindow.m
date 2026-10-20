#import "Constants.h"
#import "glk.h"
#import "protocol.h"
#import "GlkGraphicsWindow.h"
#import "GlkController.h"
#import "GlkController+BorderColor.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+Speech.h"
#import "NSColor+integer.h"
#import "SubImage.h"
#import "Theme.h"
#import "Game.h"
#import "ImageDragSource.h"
#import "NSView+Categories.h"

#import "DebugLog.h"

// Debug builds log to stderr without the NSLog timestamp prefix.
#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#endif

// The window's contents live in a bitmap context we own rather than in an
// NSImage. A FILLRECT batch is a run of opaque fills that must land in order,
// and writing those straight into the pixel buffer is far cheaper than an
// NSColor + NSRectFill per rect -- maps sent as one-pixel-high spans arrive in
// the thousands per redraw. Image draws, clears and resizes draw into the same
// context through AppKit, so everything shares one surface, in request order.
//
// The context is DeviceRGB, the space colorFromInteger: uses, so a Glk colour
// is stored exactly as the 8-bit components the game asked for. Its memory is
// 32-bit little-endian premultiplied-first, i.e. each pixel reads as
// 0xAARRGGBB, with row 0 at the top -- Glk's own orientation. It is sized in
// device pixels (point size x backing scale), with the CTM scaled so AppKit
// drawing works in points, bottom-left origin, as it did with NSImage
// lockFocus (hence florpCoords).
@interface GlkGraphicsWindow () {
    BOOL transparent;
    NSMutableArray <NSValue *> *dirtyRects;
    NSMutableArray <SubImage *> *subImages;
    // Stands in for the window's contents when VoiceOver should speak all
    // images and none of what was drawn came with a description. Never in
    // subImages, so it is not archived or pruned, and its uuid stays stable.
    SubImage *placeholderImage;
    ImageDragSource *dragSource; // made on first drag

    CGContextRef canvas;
    NSSize canvasSize;      // in points
    CGFloat canvasScale;    // device pixels per point
    NSImage *cachedImage;   // snapshot of canvas, until the next change
    NSData *cachedPNG;      // the same as PNG, for archiving
}

@end

// Colors reach setBgColor: from requests as signed ints, so zcolor_Default
// arrives as -1 and zcolor_Current as -2, and from glui32 paths unsigned.
static BOOL SPIsZColor(NSInteger value, glui32 zcolor) {
    return value == (NSInteger)zcolor || value == (NSInteger)(int32_t)zcolor;
}

// Glk 0.7.6 glk_image_draw_scaled_ext(): resolve the scaling rule once,
// against the window width at draw time. The spec explicitly says
// imagerule_WidthRatio does not dynamically resize in graphics windows, and
// maxwidth is ignored here. With no rule (glk_image_draw and
// glk_image_draw_scaled), a zero width or height means the image's own.
static NSSize SPResolveImageSize(NSSize srcsize, NSInteger w, NSInteger h,
                                 NSUInteger imagerule, CGFloat winwidth) {
    if (imagerule) {
        switch (imagerule & imagerule_WidthMask) {
            case imagerule_WidthFixed:
                break;
            case imagerule_WidthRatio:
                w = (NSInteger)(winwidth * (CGFloat)w / 65536.0);
                break;
            default: /* imagerule_WidthOrig */
                w = (NSInteger)srcsize.width;
                break;
        }
        switch (imagerule & imagerule_HeightMask) {
            case imagerule_HeightFixed:
                break;
            case imagerule_AspectRatio:
                if (srcsize.width > 0)
                    h = (NSInteger)((CGFloat)w * (srcsize.height / srcsize.width)
                                    * (CGFloat)h / 65536.0);
                break;
            default: /* imagerule_HeightOrig */
                h = (NSInteger)srcsize.height;
                break;
        }
    } else {
        if (w == 0)
            w = (NSInteger)srcsize.width;
        if (h == 0)
            h = (NSInteger)srcsize.height;
    }
    return NSMakeSize((CGFloat)w, (CGFloat)h);
}

@implementation GlkGraphicsWindow

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        bgnd = 0xFFFFFF; // White

        subImages = [NSMutableArray new];
        dirtyRects = [NSMutableArray new];
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        NSData *png = [decoder decodeObjectOfClass:[NSData class] forKey:@"canvasPNG"];
        if (png) {
            NSImage *image = [[NSImage alloc] initWithData:png];
            // The PNG is in device pixels; the canvas wants its point size.
            image.size = [decoder decodeSizeForKey:@"canvasSize"];
            self.image = image;
        }
        transparent = [decoder decodeBoolForKey:@"transparent"];
        _showingImage = [decoder decodeBoolForKey:@"showingImage"];
        subImages = [[decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableArray class], [SubImage class], nil]
                                             forKey:@"subImages"] mutableCopy] ?: [NSMutableArray new];
        // Older versions stored the "Image" placeholder among the real images.
        if (subImages.count == 1 && [subImages.firstObject.accessibilityLabel isEqualToString:NSLocalizedString(@"Image", nil)])
            [subImages removeAllObjects];
        for (SubImage *subImage in subImages)
            subImage.accessibilityParent = self;
        dirtyRects = [NSMutableArray new];
        [self markWholeWindowDirty];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    // As PNG, not an NSImage: that is an uncompressed TIFF, 6 MB per
    // autosave for a Journey window, where the PNG is 30 KB.
    NSData *png = self.canvasPNG;
    if (png) {
        [encoder encodeObject:png forKey:@"canvasPNG"];
        [encoder encodeSize:canvasSize forKey:@"canvasSize"];
    }
    [encoder encodeObject:subImages forKey:@"subImages"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    [encoder encodeBool:_showingImage forKey:@"showingImage"];
}

- (void)dealloc {
    CGContextRelease(canvas);
}

#pragma mark Canvas

static inline uint32_t SPPixelFromRGB(NSInteger rgb) {
    return 0xFF000000u | ((uint32_t)rgb & 0xFFFFFFu);
}

- (CGFloat)currentBackingScale {
    CGFloat scale = self.window.backingScaleFactor;
    if (scale < 1)
        scale = NSScreen.mainScreen.backingScaleFactor;
    return scale < 1 ? 1 : scale;
}

// Replace the canvas with an empty (fully transparent) one of `size` points.
// A zero size leaves no canvas at all.
- (void)makeCanvasWithSize:(NSSize)size {
    CGContextRelease(canvas);
    canvas = NULL;
    [self canvasWillChange];
    canvasSize = NSZeroSize;

    if (size.width < 1 || size.height < 1)
        return;

    canvasScale = [self currentBackingScale];
    size_t pixelsWide = (size_t)ceil(size.width * canvasScale);
    size_t pixelsHigh = (size_t)ceil(size.height * canvasScale);

    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    // NULL data: CG allocates (and zeroes) the buffer and frees it with the context.
    canvas = CGBitmapContextCreate(NULL, pixelsWide, pixelsHigh, 8, 0, space,
                                   kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (!canvas)
        return;

    CGContextScaleCTM(canvas, canvasScale, canvasScale);
    canvasSize = size;
}

// Drop the snapshots of the canvas, which no longer match it.
- (void)canvasWillChange {
    cachedImage = nil;
    cachedPNG = nil;
}

// Run AppKit drawing code against the canvas, in points.
- (void)drawInCanvas:(void (^)(void))drawing {
    if (!canvas)
        return;
    [self canvasWillChange];
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithCGContext:canvas flipped:NO];
    drawing();
    [NSGraphicsContext restoreGraphicsState];
}

// Fill a rect given in Glk window coordinates (points, top-left origin) with
// an opaque or fully transparent pixel, clipped to the canvas.
- (void)fillPixels:(uint32_t)pixel x:(NSInteger)x y:(NSInteger)y width:(NSInteger)w height:(NSInteger)h {
    if (!canvas || w <= 0 || h <= 0)
        return;

    NSInteger pixelsWide = (NSInteger)CGBitmapContextGetWidth(canvas);
    NSInteger pixelsHigh = (NSInteger)CGBitmapContextGetHeight(canvas);
    NSInteger x0 = (NSInteger)lround((CGFloat)x * canvasScale);
    NSInteger y0 = (NSInteger)lround((CGFloat)y * canvasScale);
    NSInteger x1 = (NSInteger)lround((CGFloat)(x + w) * canvasScale);
    NSInteger y1 = (NSInteger)lround((CGFloat)(y + h) * canvasScale);
    x0 = MAX(x0, 0);
    y0 = MAX(y0, 0);
    x1 = MIN(x1, pixelsWide);
    y1 = MIN(y1, pixelsHigh);
    if (x0 >= x1 || y0 >= y1)
        return;

    [self canvasWillChange];
    size_t rowPixels = CGBitmapContextGetBytesPerRow(canvas) / sizeof(uint32_t);
    uint32_t *row = (uint32_t *)CGBitmapContextGetData(canvas) + (size_t)y0 * rowPixels + (size_t)x0;
    size_t runBytes = (size_t)(x1 - x0) * sizeof(uint32_t);
    for (NSInteger py = y0; py < y1; py++, row += rowPixels)
        memset_pattern4(row, &pixel, runBytes);
}

// The canvas contents as an NSImage of the window's point size, for drawing,
// archiving and drag-and-drop. Cached until the canvas next changes; the
// snapshot is copy-on-write, so taking it does not copy the pixels.
- (NSImage *)image {
    if (!canvas)
        return nil;
    if (!cachedImage) {
        CGImageRef cgImage = CGBitmapContextCreateImage(canvas);
        if (!cgImage)
            return nil;
        cachedImage = [[NSImage alloc] initWithCGImage:cgImage size:canvasSize];
        CGImageRelease(cgImage);
    }
    return cachedImage;
}

// The canvas pixels as PNG. Encoding a large window takes some 20 ms, so
// the result is kept, and an autosave of an unchanged window is free.
- (NSData *)canvasPNG {
    if (!canvas)
        return nil;
    if (!cachedPNG) {
        CGImageRef cgImage = CGBitmapContextCreateImage(canvas);
        if (!cgImage)
            return nil;
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        CGImageRelease(cgImage);
        cachedPNG = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    }
    return cachedPNG;
}

- (void)setImage:(NSImage *)image {
    [self makeCanvasWithSize:image.size];
    [self drawInCanvas:^{
        [image drawInRect:NSMakeRect(0, 0, image.size.width, image.size.height)
                 fromRect:NSZeroRect
                operation:NSCompositingOperationCopy
                 fraction:1.0];
    }];
}


- (BOOL)isOpaque {
    return !transparent;
}

// Transparency changes what drawRect: composites over, not the canvas, so
// nothing else queues a rect for it; queue the whole window.
- (void)makeTransparent {
    transparent = YES;
    [self markWholeWindowDirty];
}

// The inherited dirty flag means: dirtyRects holds rects that need
// repainting. Every site that changes what is on screen appends one and
// sets the flag together, so flushDisplay (called for every window on
// every interpreter update) does nothing for a window nobody drew into.
- (void)markDirtyRect:(NSRect)rect {
    [dirtyRects addObject:@(rect)];
    dirty = YES;
}

- (void)markWholeWindowDirty {
    [self markDirtyRect:self.bounds];
}

- (void)setBgColor:(NSInteger)bc {
    if (SPIsZColor(bc, zcolor_Current))
        return;
    NSColor *color;
    if (SPIsZColor(bc, zcolor_Default)) {
        color = self.glkctl.theme.bufferBackground;
        bgnd = color.integerColor;
    } else {
        bgnd = bc;
        color = [NSColor colorFromInteger:bgnd];
    }
    [self.glkctl setBorderColor:color fromWindow:self];
    if (transparent)
        self.layer.backgroundColor = NSColor.clearColor.CGColor;
    else
        self.layer.backgroundColor = color.CGColor;
}

- (void)recalcBackground {
    [self.glkctl setBorderColor:[NSColor colorFromInteger:bgnd] fromWindow:self];
}

- (void)clear {
    if (!canvas) {
        // Image (and view) is zero size, so
        // there is nothing to clear
        return;
    }
    [self fillPixels:(transparent ? 0 : SPPixelFromRGB(bgnd))
                   x:0
                   y:0
               width:(NSInteger)ceil(canvasSize.width)
              height:(NSInteger)ceil(canvasSize.height)];
    _showingImage = NO;
    [subImages removeAllObjects];
    // Everything is repainted, so any rects queued before are moot.
    [dirtyRects removeAllObjects];
    [self markWholeWindowDirty];
}

// Graphics windows have no text, so only the background colour applies.
- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg {
    [self setBgColor:bg];
}

- (void)drawRect:(NSRect)rect {
    rect = NSIntersectionRect(rect, self.bounds);

    [self.image drawInRect:rect
                  fromRect:rect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0];
}

- (void)setFrame:(NSRect)frame {
    frame.origin.y = round(frame.origin.y);

    if (NSEqualRects(frame, self.frame) && canvas)
        return;

    super.frame = frame;

    if (frame.size.width == 0 || frame.size.height == 0)
        return;

    [self rebuildCanvasWithSize:frame.size];
}

// A window dragged to a screen of another scale gets a canvas at the new one.
- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    if (canvas && [self currentBackingScale] != canvasScale)
        [self rebuildCanvasWithSize:canvasSize];
}

// Make a new canvas of `size` points, in the background colour (left clear
// if the window is transparent), with the old contents kept at the top left,
// where Glk coordinates are anchored. The accessibility images move with
// them; those now wholly outside the window are dropped.
- (void)rebuildCanvasWithSize:(NSSize)size {
    NSImage *oldimage = self.image;
    CGFloat heightChange = size.height - canvasSize.height;

    [self makeCanvasWithSize:size];
    if (!transparent)
        [self fillPixels:SPPixelFromRGB(bgnd)
                       x:0
                       y:0
                   width:(NSInteger)ceil(size.width)
                  height:(NSInteger)ceil(size.height)];

    if (oldimage) {
        NSSize oldsize = oldimage.size;
        [self drawInCanvas:^{
            [oldimage drawInRect:NSMakeRect(0, size.height - oldsize.height, oldsize.width, oldsize.height)
                        fromRect:NSZeroRect
                       operation:NSCompositingOperationSourceOver
                        fraction:1.0];
        }];
    }

    NSRect bounds = NSMakeRect(0, 0, size.width, size.height);
    for (SubImage *subImage in subImages.copy) {
        subImage.frameRect = NSOffsetRect(subImage.frameRect, 0, heightChange);
        if (!NSIntersectsRect(subImage.frameRect, bounds))
            [subImages removeObject:subImage];
    }

    [self markWholeWindowDirty];
}

// A FILLRECT batch: opaque fills, applied in order straight into the canvas
// pixels, so later rects overwrite earlier ones exactly as the game drew them.
- (void)fillRects:(struct fillrect *)rawrects count:(NSInteger)count {
    if (!canvas || count <= 0)
        return;

    // The rects arrive in the message buffer, which need not be aligned for
    // struct fillrect; copy them out if it is not.
    struct fillrect *rects = rawrects;
    struct fillrect *alignedCopy = NULL;
    if ((uintptr_t)rawrects % _Alignof(struct fillrect) != 0) {
        size_t rectssize = (size_t)count * sizeof(struct fillrect);
        alignedCopy = malloc(rectssize);
        if (!alignedCopy)
            return;
        memcpy(alignedCopy, rawrects, rectssize);
        rects = alignedCopy;
    }

    // The window's default background is resolved against the theme now,
    // not when the game asked (see win_fillrect).
    uint32_t defaultPixel = SPPixelFromRGB(self.glkctl.theme.bufferBackground.integerColor);

    // Bounding box of the batch, in Glk (top-left origin) coordinates.
    NSInteger minX = NSIntegerMax, minY = NSIntegerMax;
    NSInteger maxX = NSIntegerMin, maxY = NSIntegerMin;

    for (NSInteger i = 0; i < count; i++) {
        const struct fillrect *r = &rects[i];
        if (r->w <= 0 || r->h <= 0)
            continue;
        uint32_t pixel = (r->color == zcolor_Default) ? defaultPixel : SPPixelFromRGB(r->color);
        [self fillPixels:pixel x:r->x y:r->y width:r->w height:r->h];
        minX = MIN(minX, (NSInteger)r->x);
        minY = MIN(minY, (NSInteger)r->y);
        maxX = MAX(maxX, (NSInteger)r->x + r->w);
        maxY = MAX(maxY, (NSInteger)r->y + r->h);
    }

    if (minX <= maxX) {
        NSRect unionRect = [self florpCoords:NSMakeRect(minX, minY, maxX - minX, maxY - minY)];
        [self markDirtyRect:unionRect];
        [self pruneSubimagesInRect:unionRect];
    }
    _showingImage = YES;
    free(alignedCopy);
}

- (void)flushDisplay {
    if (!dirty)
        return;
    for (NSValue *val in dirtyRects) {
        [self setNeedsDisplayInRect:val.rectValue];
    }
    [dirtyRects removeAllObjects];
    dirty = NO;
}

// Glk rect (top-left origin) -> canvas/AppKit rect (bottom-left origin).
- (NSRect)florpCoords:(NSRect)r {
    NSRect res = r;
    res.origin.y = canvasSize.height - res.origin.y - res.size.height;
    return res;
}

- (void)drawImage:(NSImage *)src
             val1:(NSInteger)x
             val2:(NSInteger)y
            width:(NSInteger)w
           height:(NSInteger)h
        imagerule:(NSUInteger)imagerule
         maxwidth:(NSUInteger)maxwidth
            style:(NSUInteger)style {
    NSSize srcsize = src.size;

    if (!canvas) {
        if (NSWidth(self.frame) != 0 && NSHeight(self.frame) != 0)
            [self makeCanvasWithSize:self.frame.size];
        if (!canvas)
            return;
    }

    NSSize drawsize = SPResolveImageSize(srcsize, w, h, imagerule, canvasSize.width);
    NSRect florpedRect = [self florpCoords:NSMakeRect((CGFloat)x, (CGFloat)y, drawsize.width, drawsize.height)];

    @autoreleasepool {
        [self drawInCanvas:^{
            [NSGraphicsContext currentContext].imageInterpolation =
            NSImageInterpolationHigh;

            [src drawInRect:florpedRect
                   fromRect:NSMakeRect(0, 0, srcsize.width, srcsize.height)
                  operation:NSCompositingOperationSourceOver
                   fraction:1.0];
        }];
    }

    _showingImage = YES;
    [self markDirtyRect:florpedRect];

    [self pruneSubimagesInRect:florpedRect];

    if (src.accessibilityDescription.length)
        [subImages addObject:[SubImage imageWithLabel:src.accessibilityDescription parent:self frame:florpedRect]];
}

- (void)mouseDown:(NSEvent *)theEvent {
    if (!self.window)
        return;

    NSPoint location = [self convertPoint:theEvent.locationInWindow fromView: nil];

    [self trackClickOrDragFromEvent:theEvent click:^(NSEvent *upEvent) {
        // Forward every click, not just clickCount == 1. macOS
        // coalesces two quick clicks at the same spot into a
        // double-click and reports clickCount == 2 on the second
        // press; filtering on == 1 silently dropped it, so rapid
        // same-target clicks (e.g. navigating the Zork Zero compass
        // rose) were randomly lost. The interpreter classifies
        // single vs. double clicks itself from event timing, so we
        // just deliver each click.
        if (self->mouse_request)
            [self sendMouseEventAtPoint:NSMakePoint(location.x, self.frame.size.height - location.y)];
    } drag:^(NSEvent *dragEvent) {
        NSImage *image = self.image;
        if (!image)
            return;
        if (!self->dragSource) {
            self->dragSource = [ImageDragSource new];
            self->dragSource.operationQueue = [NSOperationQueue new];
            self->dragSource.operationQueue.qualityOfService = NSQualityOfServiceUserInitiated;
        }
        [self->dragSource dragImage:image
                               from:self
                              event:dragEvent
                           filename:self.glkctl.game.fileBaseName
                              frame:self.bounds];
    }];
}

- (BOOL)wantsFocus {
    return char_request;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)initChar {
    char_request = YES;
}

- (void)cancelChar {
    char_request = NO;
}

- (void)keyDown:(NSEvent *)evt {
    unsigned ch = [GlkWindow keycodeForEvent:evt mapNumericPad:NO];

    if ([self forwardKeyDownToFocusedWindow:evt])
        return;

    if (char_request && ch != keycode_Unknown)
        [self sendCharEvent:ch];
}

// A key the player pressed (as opposed to one from a command script).
- (void)sendCharEvent:(unsigned)ch {
    [self.glkctl markLastSeen];
    [self sendKeypress:ch];
}

#pragma mark Accessibility

- (NSString *)accessibilityRoleDescription {
        return [NSString
            stringWithFormat:@"Graphics window%@%@",
                             mouse_request ? @", waiting for mouse clicks" : @"",
                             char_request ? @", waiting for a key press" : @""];
}

// The images VoiceOver should offer, given the speak-images preference.
- (NSArray <SubImage *> *)images {
    kVOImagePrefsType mode = self.theme.vOSpeakImages;
    if (mode == kVOImageNone)
        return @[];
    if (subImages.count) {
        if (mode == kVOImageWithDescriptionOnly) {
            return [subImages filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                return ((SubImage *)object).accessibilityLabel.length > 0;
            }]];
        }
        return subImages.copy;
    }
    if (mode == kVOImageAll && _showingImage)
        return @[ self.placeholderSubImage ];
    return @[];
}

- (SubImage *)placeholderSubImage {
    if (!placeholderImage)
        placeholderImage = [SubImage imageWithLabel:NSLocalizedString(@"Image", nil) parent:self frame:self.bounds];
    placeholderImage.frameRect = self.bounds;
    return placeholderImage;
}

// Worked out on each request, so it follows clears and prunes: the oldest
// described image still on show, else "Image" when the placeholder is.
- (NSString *)accessibilityLabel {
    if (subImages.count)
        return subImages.firstObject.accessibilityLabel;
    if (self.theme.vOSpeakImages == kVOImageAll && _showingImage)
        return NSLocalizedString(@"Image", nil);
    return nil;
}

- (NSArray *)accessibilityCustomActions {
    NSMutableArray *mutable = [NSMutableArray new];

    if (mouse_request) {
        NSAccessibilityCustomAction *mouseClick = [[NSAccessibilityCustomAction alloc]
                                                    initWithName:NSLocalizedString(@"click on image", nil) target:self selector:@selector(accessibilityClick:)];
        [mutable addObject:mouseClick];
    }

    if (char_request) {
        NSAccessibilityCustomAction *keyPress = [[NSAccessibilityCustomAction alloc]
                                             initWithName:NSLocalizedString(@"press key", nil) target:self selector:@selector(accessibilityClick:)];
        [mutable addObject:keyPress];
    }

    NSArray *actions = mutable;
    actions = [actions arrayByAddingObjectsFromArray:self.glkctl.accessibilityCustomActions];

    return actions;
}

- (void)accessibilityClick:(id)sender {
    if (mouse_request)
        [self sendMouseEventAtPoint:NSMakePoint(self.frame.size.width / 2, self.frame.size.height / 2)];
    else if (char_request)
        [self sendCharEvent:' '];
}

- (NSArray *)accessibilityChildren {
    NSArray<SubImage *> *images = subImages;
    if (images.count == 0 && self.theme.vOSpeakImages == kVOImageAll && _showingImage)
        images = @[ self.placeholderSubImage ];

    NSMutableArray *children = [NSMutableArray arrayWithArray:super.accessibilityChildren];
    for (SubImage *si in images)
        si.accessibilityFrame = NSAccessibilityFrameInView(self, si.frameRect);
    [children addObjectsFromArray:images];

    return children;
}

- (void)pruneSubimagesInRect:(NSRect)rect {
    NSEnumerator *enumerator = [subImages.copy reverseObjectEnumerator];
    SubImage *img;
    while (img = [enumerator nextObject]) {
        if ([self rect:rect coversRect:img.frameRect]) {
            [subImages removeObject:img];
        }
    }
}

- (BOOL)rect:(NSRect)newrect coversRect:(NSRect)oldrect {
    NSRect intersection = NSIntersectionRect(newrect, oldrect);

    if (NSEqualRects(intersection, NSZeroRect))
        return NO;

    CGFloat intersectionarea = intersection.size.height * intersection.size.width;

    CGFloat oldrectarea = oldrect.size.height * oldrect.size.width;
    // A degenerate image has nothing left to cover; let any overlapping draw prune it.
    if (oldrectarea <= 0)
        return YES;

    if ((oldrectarea - intersectionarea) / oldrectarea > 0.5)
        return NO;

    return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (menuItem.action == @selector(saveAsRTF:)) return NO;
    return YES;
}

// GlkController's saveAsRTF: can pick any window, and the GlkWindow version
// assumes a text view, so there is nothing to save here.
- (IBAction)saveAsRTF:(id)sender {
}

@end
