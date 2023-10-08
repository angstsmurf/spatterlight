#import "Constants.h"
#import "GlkGraphicsWindow.h"
#import "GlkController.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface GlkGraphicsWindow () <NSSecureCoding> {
    BOOL mouse_request;
    BOOL transparent;
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
        _image = [[NSImage alloc] initWithSize:NSZeroSize];
        bgnd = 0xFFFFFF; // White

        mouse_request = NO;
        transparent = NO;
        _showingImage = NO;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _image = [decoder decodeObjectOfClass:[NSImage class] forKey:@"image"];
        dirty = YES;
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];
        transparent = [decoder decodeBoolForKey:@"transparent"];
        _showingImage = [decoder decodeBoolForKey:@"showingImage"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_image forKey:@"image"];
    [encoder encodeBool:mouse_request forKey:@"mouse_request"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    [encoder encodeBool:_showingImage forKey:@"showingImage"];
}

- (void)postRestoreAdjustments:(GlkWindow *)win {
    GlkGraphicsWindow *gwin = (GlkGraphicsWindow *)win;
    if (gwin.showingImage) {
        GlkGraphicsWindow * __weak weakSelf = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.6 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
            GlkGraphicsWindow *strongSelf = weakSelf;
            if (strongSelf && NSEqualSizes(strongSelf.frame.size, gwin.image.size)) {
                strongSelf.image = gwin.image.copy;
                strongSelf.needsDisplay = YES;
            }
        });
    }
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
}

- (void)drawRect:(NSRect)rect {

    [_image drawInRect:rect
              fromRect:rect
            operation:NSCompositingOperationSourceOver
              fraction:1.0];
}

- (void)setFrame:(NSRect)frame {

    frame.origin.y = round(frame.origin.y);
    
    if (NSEqualRects(frame, self.frame) && !NSEqualSizes(_image.size, NSZeroSize))
        return;

    super.frame = frame;

    if (frame.size.width == 0 || frame.size.height == 0)
        return;

    // First we store the current contents
    NSImage *oldimage = _image;

    // Then we create a new image, filling it with background color
    if (!transparent) {

        _image = [[NSImage alloc] initWithSize:frame.size];

        [_image lockFocus];
        NSRectFill(self.bounds);
        [_image unlockFocus];
    }

    dirty = YES;
}

- (NSRect)florpCoords:(NSRect)r {
    NSRect res = r;
    NSSize size = _image.size;
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

    if (_image.size.width == 0 || _image.size.height == 0) {
        if (NSWidth(self.frame) != 0 && NSHeight(self.frame) != 0) {
            _image = [[NSImage alloc] initWithSize:self.frame.size];
        } else {
            return;
        }
    }

    if (w == 0)
        w = (NSInteger)srcsize.width;
    if (h == 0)
        h = (NSInteger)srcsize.height;

    NSRect florpedRect;

    @autoreleasepool {
        [_image lockFocus];

        [NSGraphicsContext currentContext].imageInterpolation =
        NSImageInterpolationHigh;

        florpedRect = [self florpCoords:NSMakeRect(x, y, w, h)];

        [src drawInRect:florpedRect
               fromRect:NSMakeRect(0, 0, srcsize.width, srcsize.height)
              operation:NSCompositingOperationSourceOver
               fraction:1.0];

        [_image unlockFocus];
    }

    _showingImage = YES;
    dirty = YES;
}

- (void)initMouse {
    mouse_request = YES;
}

- (void)cancelMouse {
    mouse_request = NO;
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

- (IBAction)saveAsRTF:(id)sender {
    NSLog(@"saveAsRTF in graphics windows not supported");
}

@end
