#import "Constants.h"
#import "glk.h"
#import "protocol.h"
#import "GlkEvent.h"
#import "GlkGraphicsWindow.h"
#import "GlkController.h"
#import "NSColor+integer.h"
#import "SubImage.h"
#import "Theme.h"
#import "Game.h"
#import "MyFilePromiseProvider.h"
#import "NSImage+Categories.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface GlkGraphicsWindow () <NSSecureCoding> {
    NSImage *image;

    BOOL mouse_request;
    BOOL transparent;
    BOOL showingImage;
    NSMutableArray <NSValue *> *dirtyRects;
    NSMutableArray <SubImage *> *subImages;
}

@property NSOperationQueue *workQueue;

@end

@implementation GlkGraphicsWindow

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        image = [[NSImage alloc] initWithSize:NSZeroSize];
        bgnd = 0xFFFFFF; // White

        mouse_request = NO;
        transparent = NO;
        showingImage = NO;
        subImages = [NSMutableArray new];
        dirtyRects = [NSMutableArray new];
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        image = [decoder decodeObjectOfClass:[NSImage class] forKey:@"image"];
        dirty = YES;
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];
        transparent = [decoder decodeBoolForKey:@"transparent"];
        showingImage = [decoder decodeBoolForKey:@"showingImage"];
        subImages =  [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"subImages"];
        dirtyRects = [NSMutableArray new];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:image forKey:@"image"];
    [encoder encodeObject:subImages forKey:@"subImages"];
    [encoder encodeBool:mouse_request forKey:@"mouse_request"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    [encoder encodeBool:showingImage forKey:@"showingImage"];
}

- (BOOL)isOpaque {
    return !transparent;
}

- (void)makeTransparent {
    transparent = YES;
    dirty = YES;
}

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;

    [self.glkctl setBorderColor:[NSColor colorFromInteger:bgnd] fromWindow:self];
}

- (void)drawRect:(NSRect)rect {

    if (!transparent) {
        [[NSColor colorFromInteger:bgnd] set];
        NSRectFill(rect);
    }

    [image drawInRect:rect
              fromRect:rect
            operation:NSCompositingOperationSourceOver
              fraction:1.0];
}

- (void)setFrame:(NSRect)frame {

    frame.origin.y = round(frame.origin.y);
    
    if (NSEqualRects(frame, self.frame) && !NSEqualSizes(image.size, NSZeroSize))
        return;

    super.frame = frame;

    if (frame.size.width == 0 || frame.size.height == 0)
        return;

    // First we store the current contents
    NSImage *oldimage = image;

    // Then we create a new image, filling it with background color
    if (!transparent) {

        image = [[NSImage alloc] initWithSize:frame.size];

        [image lockFocus];
        [[NSColor colorFromInteger:bgnd] set];
        NSRectFill(self.bounds);
        [image unlockFocus];
    }

    // The we draw the old contents over it
    [self drawImage:oldimage
               val1:0
               val2:0
              width:(NSInteger)oldimage.size.width
             height:(NSInteger)oldimage.size.height
              style:style_Normal];

    dirty = YES;
}

- (void)fillRects:(struct fillrect *)rects count:(NSInteger)count {
    NSSize size;
    NSInteger i;

    size = image.size;

    if (size.width == 0 || size.height == 0 || size.height > INT_MAX)
        return;

    NSRect unionRect = NSZeroRect;
    if (count) {
        unionRect.origin.x = rects[0].x;
        unionRect.origin.y = rects[0].y;
    }
        [image lockFocus];
        uint32_t current_color = zcolor_Default;
        for (i = 0; i < count; i++) {
            if (current_color != rects[i].color)
            [[NSColor colorFromInteger:rects[i].color] set];
            current_color = rects[i].color;
            NSRect rect = [self florpCoords:NSMakeRect(rects[i].x, rects[i].y, rects[i].w, rects[i].h)];
            NSRectFill(rect);
            unionRect = NSUnionRect(unionRect, rect);
        }
        [image unlockFocus];
    [dirtyRects addObject:@(unionRect)];
    [self pruneSubimagesInRect:unionRect];
    dirty = YES;
    showingImage = YES;
}

- (void)flushDisplay {
    if (dirty) {
        for (NSValue *val in dirtyRects) {
            NSRect rect = val.rectValue;
            [self setNeedsDisplayInRect:rect];
        }
    }
    dirtyRects = [NSMutableArray new];
    dirty = NO;
}

- (NSRect)florpCoords:(NSRect)r {
    NSRect res = r;
    NSSize size = image.size;
    res.origin.y = size.height - res.origin.y - res.size.height;
    return res;
}

- (void)drawImage:(NSImage *)src
             val1:(NSInteger)x
             val2:(NSInteger)y
            width:(NSInteger)w
           height:(NSInteger)h
            style:(NSUInteger)style {
    NSSize srcsize = src.size;

    if (NSEqualSizes(image.size, NSZeroSize)) {
        return;
    }

    if (w == 0)
        w = (NSInteger)srcsize.width;
    if (h == 0)
        h = (NSInteger)srcsize.height;

    NSRect florpedRect;

    @autoreleasepool {
        [image lockFocus];

        [NSGraphicsContext currentContext].imageInterpolation =
        NSImageInterpolationHigh;

        florpedRect = [self florpCoords:NSMakeRect(x, y, w, h)];

        [src drawInRect:florpedRect
               fromRect:NSMakeRect(0, 0, srcsize.width, srcsize.height)
              operation:NSCompositingOperationSourceOver
               fraction:1.0];

        [image unlockFocus];
    }

    showingImage = YES;
    dirty = YES;
    [dirtyRects addObject:@(florpedRect)];

    [self pruneSubimagesInRect:florpedRect];

    if (src.accessibilityDescription.length) {
        SubImage *subImage = [SubImage new];
        subImage.accessibilityLabel = src.accessibilityDescription;
        subImage.accessibilityParent = self;
        subImage.accessibilityRole = NSAccessibilityImageRole;
        subImage.frameRect = florpedRect;

        [subImages addObject:subImage];
        self.accessibilityLabel = subImages.firstObject.accessibilityLabel;
    }
}

- (void)initMouse {
    mouse_request = YES;
}

- (void)cancelMouse {
    mouse_request = NO;
}

- (void)mouseDown:(NSEvent *)theEvent {

    NSDate *mouseTime = [NSDate date];
    
    NSPoint __block location = [self convertPoint:theEvent.locationInWindow fromView: nil];
    
    NSEventMask eventMask = NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp;
    NSTimeInterval timeout = NSEventDurationForever;
    
    CGFloat dragThreshold = 0.3;

    [self.window trackEventsMatchingMask:eventMask timeout:timeout mode:NSEventTrackingRunLoopMode handler:^(NSEvent * _Nullable event, BOOL * _Nonnull stop) {

        if (!event) { return; }
        
        if (event.type == NSEventTypeLeftMouseUp) {
            *stop = YES;
            if (mouseTime.timeIntervalSinceNow > -0.5) {
                if (mouse_request && theEvent.clickCount == 1) {
                    [self.glkctl markLastSeen];
                    location.y = self.frame.size.height - location.y;
                    GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:location forWindow:self.name];
                    [self.glkctl queueEvent:gev];
                    mouse_request = NO;
                }
            }
        } else if (event.type == NSEventTypeLeftMouseDragged) {
            NSPoint movedLocation = [self convertPoint:event.locationInWindow fromView: nil];
            if (ABS(movedLocation.x - location.x) >dragThreshold || ABS(movedLocation.y - location.y) > dragThreshold) {
                *stop = YES;
                
                NSDraggingItem *dragItem;
                
                if (@available(macOS 10.14, *)) {
                    
                    MyFilePromiseProvider *provider = [[MyFilePromiseProvider alloc] initWithFileType: NSPasteboardTypePNG delegate:self];
                    
                    dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:provider];
                    
                } else {
                    NSPasteboardItem *pasteboardItem = [NSPasteboardItem new];
                    
                    [pasteboardItem setDataProvider:self forTypes:@[NSPasteboardTypePNG, PasteboardFileURLPromise, PasteboardFilePromiseContent]];
                    
                    // Create the dragging item for the drag operation
                    dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:pasteboardItem];
                }
                
                [dragItem setDraggingFrame:self.bounds contents:image];
                [self beginDraggingSessionWithItems:@[dragItem] event:event source:self];
            }
        }
    }];
}

- (BOOL)wantsFocus {
    return char_request;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)initChar {
    // NSLog(@"init char in %d", name);
    char_request = YES;
}

- (void)cancelChar {
    // NSLog(@"cancel char in %d", name);
    char_request = NO;
}

- (void)keyDown:(NSEvent *)evt {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length - 1]);

    GlkWindow *win;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (win in (self.glkctl.gwindows).allValues) {
            if (win != self && win.wantsFocus) {
                NSLog(@"GlkGraphicsWindow: Passing on keypress");
                [win keyDown:evt];
                [win grabFocus];
                return;
            }
        }

    if (char_request && ch != keycode_Unknown) {
        [self.glkctl markLastSeen];

        // NSLog(@"char event from %d", name);
        GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [self.glkctl queueEvent:gev];
        char_request = NO;
        return;
    }
}

- (void)sendKeypress:(unsigned)ch {
    GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
    [self.glkctl queueEvent:gev];
    char_request = NO;
}

#pragma mark Accessibility

- (NSString *)accessibilityRoleDescription {
        return [NSString
            stringWithFormat:@"Graphics window%@%@",
                             mouse_request ? @", waiting for mouse clicks" : @"",
                             char_request ? @", waiting for a key press" : @""];
}

- (NSArray <SubImage *> *)images {
    if (self.theme.vOSpeakImages == kVOImageNone)
        return @[];
    if (subImages.count) {
        if (subImages.count == 1 && self.theme.vOSpeakImages != kVOImageAll && [subImages.firstObject.accessibilityLabel isEqualToString:@"Image"]) {
            [subImages removeAllObjects];
            return @[];
        }
        if (self.theme.vOSpeakImages == kVOImageWithDescriptionOnly) {
            return [subImages filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                return ((SubImage *)object).accessibilityLabel.length > 0;
            }]];
        } else {
            NSLog(@"GlkGraphicsWindow images: returning %ld subimages", subImages.count);
            return subImages;
        }
    } else if (self.theme.vOSpeakImages == kVOImageAll && showingImage) {
        return @[ [self createDummySubImage] ];
    }

    return @[];
}

- (SubImage *)createDummySubImage {
    SubImage *dummy = [SubImage new];
    dummy.accessibilityLabel = NSLocalizedString(@"Image", nil);
    dummy.accessibilityParent = self;
    dummy.accessibilityRole = NSAccessibilityImageRole;
    dummy.frameRect = self.bounds;
    self.accessibilityLabel = dummy.accessibilityLabel;
    [subImages addObject:dummy];
    return dummy;
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
    actions = [actions arrayByAddingObjectsFromArray:[self.glkctl accessibilityCustomActions]];

    return actions;
}

- (void)accessibilityClick:(id)sender {
    if (mouse_request) {
        NSPoint p;
        p.x = self.frame.size.width / 2;
        p.y = self.frame.size.height / 2;
        GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:p forWindow:self.name];
        [self.glkctl queueEvent:gev];
        mouse_request = NO;
    } else if (char_request) {
        [self.glkctl markLastSeen];
        GlkEvent *gev = [[GlkEvent alloc] initCharEvent:' ' forWindow:self.name];
        [self.glkctl queueEvent:gev];
        char_request = NO;
        return;
    }
}

- (BOOL)isAccessibilityElement {
    return YES;
}

- (NSArray *)accessibilityChildren {
    NSArray *children = super.accessibilityChildren;

    if (subImages.count == 0 && self.theme.vOSpeakImages == kVOImageAll && showingImage) {
        [self createDummySubImage];
    }

    for (SubImage *si in subImages) {
        children = [children arrayByAddingObject:si];
        NSRect bounds = NSAccessibilityFrameInView(self, si.frameRect);
        si.accessibilityFrame = bounds;
    }

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

    if ((oldrectarea - intersectionarea) / oldrectarea > 0.5)
        return NO;

    return YES;
}

#pragma mark Dragging source stuff

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    return NSDragOperationCopy;
}

// For pre-10.14
- (void)pasteboard:(NSPasteboard *)sender item:(NSPasteboardItem *)item provideDataForType:(NSString *)type
{
    // sender has accepted the drag and now we need to send the data for the type we promised
    if ( [type isEqual:NSPasteboardTypePNG]) {
        // set data for PNG type on the pasteboard as requested
        [sender setData:[self pngData] forType:NSPasteboardTypePNG];
    } else if ([type isEqualTo:PasteboardFilePromiseContent]) {
        // The receiver will send this asking for the content type for the drop, to figure out
        // whether it wants to/is able to accept the file type.

        [sender setString:@"public.png" forType: PasteboardFilePromiseContent];
    }
    else if ([type isEqualTo: PasteboardFileURLPromise]) {
        // The receiver is interested in our data, and is happy with the format that we told it
        // about during the PasteboardFilePromiseContent request.
        // The receiver has passed us a URL where we are to write our data to.

        NSString *str = [sender stringForType:PasteboardFilePasteLocation];
        NSURL *destinationFolderURL = [NSURL fileURLWithPath:str];
        if (!destinationFolderURL) {
            NSLog(@"ERROR:- Receiver didn't tell us where to put the file?");
            return;
        }

        // Here, we build the file destination using the receivers destination URL
        NSString *baseFileName = self.glkctl.game.path.lastPathComponent.stringByDeletingPathExtension;

        if (!baseFileName.length)
            baseFileName = @"image";

        NSString *fileName = [baseFileName stringByAppendingPathExtension:@"png"];

        NSURL *destinationFileURL = [destinationFolderURL URLByAppendingPathComponent:fileName];

        NSUInteger index = 2;

        // Handle duplicate file names
        // by slapping on a number at the end.
        while ([[NSFileManager defaultManager] fileExistsAtPath:destinationFileURL.path]) {
            NSString *newFileName = [NSString stringWithFormat:@"%@ %ld", baseFileName, index];
            newFileName = [newFileName stringByAppendingPathExtension:@"png"];
            destinationFileURL = [destinationFolderURL URLByAppendingPathComponent:newFileName];
            index++;
        }

        NSData *bitmapData = [self pngData];

        NSError *error = nil;

        if (![bitmapData writeToURL:destinationFileURL options:NSDataWritingAtomic error:&error]) {
            NSLog(@"Error: Could not write PNG data to url %@: %@", destinationFileURL.path, error);
        }

        // And finally, tell the receiver where we wrote our file
        [sender setString:destinationFileURL.path forType:PasteboardFileURLPromise];
    }
}

- (NSString *)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider
                  fileNameForType:(NSString *)fileType {
    NSString *fileName = [self.glkctl.game.path.lastPathComponent.stringByDeletingPathExtension stringByAppendingPathExtension:@"png"];
    if (!fileName.length)
        fileName = @"image.png";

    return fileName;
}

- (void)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider
          writePromiseToURL:(NSURL *)url
          completionHandler:(void (^)(NSError *errorOrNil))completionHandler {

    NSData *bitmapData = [self pngData];

    NSError *error = nil;

    if (![bitmapData writeToURL:url options:NSDataWritingAtomic error:&error]) {
        NSLog(@"Error: Could not write PNG data to url %@: %@", url.path, error);
        completionHandler(error);
        return;
    }

    completionHandler(nil);
}

- (NSOperationQueue *)operationQueueForFilePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider {
    return self.workQueue;
}

- (nullable NSData *)pngData {
    if (!image) {
        return nil;
    }
    NSBitmapImageRep *bitmaprep = [image bitmapImageRepresentation];
    if (!bitmaprep)
        return nil;

    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypePNG properties:props];
}

@synthesize workQueue = _workQueue;

- (NSOperationQueue *)workQueue {
    if (_workQueue == nil) {
        _workQueue = [NSOperationQueue new];
        _workQueue.qualityOfService = NSQualityOfServiceUserInitiated;
    }
    return _workQueue;
}

- (void)setWorkQueue:(NSOperationQueue *)workQueue {
    _workQueue = workQueue;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (menuItem.action == @selector(saveAsRTF:)) return NO;
    return YES;
}

- (IBAction)saveAsRTF:(id)sender {
    NSLog(@"saveAsRTF in graphics windows not supported");
}

@end
