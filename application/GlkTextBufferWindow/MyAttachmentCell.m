//
//  MyAttachmentCell.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MyAttachmentCell.h"
#import "MarginImage.h"
#import "MyFilePromiseProvider.h"
#import "NSImage+Categories.h"
#import "Constants.h"
#include "glk.h"

@interface MyAttachmentCell () <NSSecureCoding> {
    NSString *baseFilename;
    CGFloat lastXHeight;
    CGFloat lastAscender;
    // Display size resolved by the most recent layout pass
    // (cellFrameForTextContainer:). Starts out as the image's size.
    NSSize lastDisplaySize;
}
@end

@implementation MyAttachmentCell

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)alignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos
                        index:(NSInteger)index {
    if (alignment != imagealign_MarginLeft && alignment != imagealign_MarginRight)
        self = [super initImageCell:image];
    else
        self = [super initImageCell:nil];
    if (self) {
        _glkImgAlign = alignment;
        _attrstr = anattrstr;
        _pos = apos;
        _index = index;
        _naturalSize = image.size;
        lastDisplaySize = image.size;
        if (image.accessibilityDescription.length) {
            self.accessibilityLabel = image.accessibilityDescription;
            _hasDescription = YES;
        }
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _glkImgAlign = [decoder decodeIntegerForKey:@"align"];
        _attrstr = [decoder decodeObjectOfClass:[NSAttributedString class] forKey:@"attstr"];
        _marginImgUUID = [decoder decodeObjectOfClass:[NSString class] forKey:@"marginImgUUID"];
        _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
        _index = [decoder decodeIntegerForKey:@"index"];
        lastXHeight = [decoder decodeDoubleForKey:@"lastXHeight"];
        lastAscender = [decoder decodeDoubleForKey:@"lastAscender"];
        _hasDescription = [decoder decodeBoolForKey:@"hasDescription"];
        self.accessibilityLabel = (NSString *)[decoder decodeObjectOfClass:[NSString class] forKey:@"label"];
        // Glk 0.7.6 scaling rule; all absent (0) in pre-rule archives.
        _imagerule = (NSUInteger)[decoder decodeIntegerForKey:@"imagerule"];
        _ruleWidth = (NSUInteger)[decoder decodeIntegerForKey:@"ruleWidth"];
        _ruleHeight = (NSUInteger)[decoder decodeIntegerForKey:@"ruleHeight"];
        _ruleMaxWidth = (NSUInteger)[decoder decodeIntegerForKey:@"ruleMaxWidth"];
        _naturalSize = [decoder decodeSizeForKey:@"naturalSize"];
        if (NSEqualSizes(_naturalSize, NSZeroSize))
            _naturalSize = self.image.size;
        lastDisplaySize = self.image.size;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:_glkImgAlign forKey:@"align"];
    [encoder encodeObject:_attrstr forKey:@"attrstr"];
    [encoder encodeObject:_marginImgUUID forKey:@"marginImgUUID"];
    [encoder encodeObject:self.accessibilityLabel forKey:@"label"];
    [encoder encodeDouble:lastXHeight forKey:@"lastXHeight"];
    [encoder encodeDouble:lastAscender forKey:@"lastAscender"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeInteger:_index forKey:@"index"];
    [encoder encodeBool:_hasDescription forKey:@"hasDescription"];
    [encoder encodeInteger:(NSInteger)_imagerule forKey:@"imagerule"];
    [encoder encodeInteger:(NSInteger)_ruleWidth forKey:@"ruleWidth"];
    [encoder encodeInteger:(NSInteger)_ruleHeight forKey:@"ruleHeight"];
    [encoder encodeInteger:(NSInteger)_ruleMaxWidth forKey:@"ruleMaxWidth"];
    [encoder encodeSize:_naturalSize forKey:@"naturalSize"];
}

- (BOOL)wantsToTrackMouse {
    return YES;
}

- (BOOL)wantsToTrackMouseForEvent:(NSEvent *)theEvent
                           inRect:(NSRect)cellFrame
                           ofView:(NSView *)controlView
                 atCharacterIndex:(NSUInteger)charIndex {
    if (_glkImgAlign == imagealign_MarginLeft || _glkImgAlign == imagealign_MarginRight) {
        return NO;
            }
    if (theEvent.type == NSEventTypeLeftMouseDragged || theEvent.type == NSEventTypeLeftMouseDown) {
        return YES;
        }
    return NO;
}

#pragma mark Glk 0.7.6 rule scaling

+ (NSSize)resolveImageRule:(NSUInteger)imagerule
                     width:(NSUInteger)ruleWidth
                    height:(NSUInteger)ruleHeight
                  maxwidth:(NSUInteger)maxwidth
                   natural:(NSSize)natural
                 available:(CGFloat)availwidth {
    CGFloat w, h;
    switch (imagerule & imagerule_WidthMask) {
        case imagerule_WidthFixed:
            w = (CGFloat)ruleWidth;
            break;
        case imagerule_WidthRatio:
            w = availwidth * (CGFloat)ruleWidth / 65536.0;
            break;
        default: /* imagerule_WidthOrig */
            w = natural.width;
            break;
    }
    switch (imagerule & imagerule_HeightMask) {
        case imagerule_HeightFixed:
            h = (CGFloat)ruleHeight;
            break;
        case imagerule_AspectRatio:
            h = natural.width > 0
                ? w * (natural.height / natural.width) * (CGFloat)ruleHeight / 65536.0
                : 0;
            break;
        default: /* imagerule_HeightOrig */
            h = natural.height;
            break;
    }
    // maxwidth: an additional window-width bound, applied after both rules,
    // reducing proportionally (regardless of how the height was determined).
    if (maxwidth && availwidth > 1) {
        CGFloat maxw = availwidth * (CGFloat)maxwidth / 65536.0;
        if (w > maxw) {
            if (w > 0)
                h = h * maxw / w;
            w = maxw;
        }
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return NSMakeSize(w, h);
}

// The display size for a given wrap width: the full 0.7.6 rule resolution
// for rule cells; for legacy cells the image size, proportionally reduced
// when wider than the window (the 0.7.6 behaviour change for plain
// glk_image_draw/glk_image_draw_scaled in buffer windows).
- (NSSize)displaySizeForWidth:(CGFloat)availwidth {
    if (_imagerule)
        return [MyAttachmentCell resolveImageRule:_imagerule
                                            width:_ruleWidth
                                           height:_ruleHeight
                                         maxwidth:_ruleMaxWidth
                                          natural:_naturalSize
                                        available:availwidth];
    NSSize size = self.image.size;
    if (availwidth > 1 && size.width > availwidth) {
        size.height = size.height * availwidth / size.width;
        size.width = availwidth;
        if (size.height < 1)
            size.height = 1;
    }
    return size;
}

- (NSPoint)cellBaselineOffset {

    NSUInteger lastCharPos = _pos - 1;

    if (lastCharPos > _attrstr.length)
        lastCharPos = 0;

    NSFont *font = [_attrstr attribute:NSFontAttributeName
                               atIndex:lastCharPos
                        effectiveRange:nil];

    CGFloat ascender = font.ascender;
    if (ascender == 0)
        ascender = lastAscender;
    else
        lastAscender = ascender;

    CGFloat xHeight = font.xHeight;
    if (xHeight == 0)
        xHeight = lastXHeight;
    else
        lastXHeight = xHeight;

    CGFloat displayHeight = lastDisplaySize.height > 0
        ? lastDisplaySize.height : self.image.size.height;

    if (_glkImgAlign == imagealign_InlineCenter) {
        return NSMakePoint(0, -(displayHeight / 2) + xHeight / 2);
    } else if (_glkImgAlign == imagealign_InlineDown) {
        return NSMakePoint(0, -displayHeight + ascender);
    }

    return [super cellBaselineOffset];
}

- (NSSize)cellSize {
    if (_glkImgAlign == imagealign_MarginLeft || _glkImgAlign == imagealign_MarginRight) {
        return NSZeroSize;
    }
    if (lastDisplaySize.width > 0 && !NSEqualSizes(lastDisplaySize, self.image.size))
        return lastDisplaySize;
    return [super cellSize];
}

- (NSRect)cellFrameForTextContainer:(NSTextContainer *)textContainer
               proposedLineFragment:(NSRect)lineFrag
                      glyphPosition:(NSPoint)position
                     characterIndex:(NSUInteger)charIndex {
    if (_glkImgAlign == imagealign_MarginLeft || _glkImgAlign == imagealign_MarginRight) {
        return NSZeroRect;
    }
    // Resolve the display size against the current wrap width, every layout
    // pass, so rule-scaled (and oversized legacy) images track resizes.
    CGFloat availwidth = lineFrag.size.width - 2 * textContainer.lineFragmentPadding;
    NSSize size = [self displaySizeForWidth:availwidth];
    if (!NSEqualSizes(size, lastDisplaySize)) {
        lastDisplaySize = size;
    }
    if (_imagerule || !NSEqualSizes(size, self.image.size)) {
        NSPoint offset = [self cellBaselineOffset];
        return NSMakeRect(offset.x, offset.y, size.width, size.height);
    }
    return [super cellFrameForTextContainer:textContainer
                       proposedLineFragment:lineFrag
                              glyphPosition:position
                             characterIndex:charIndex];
}

// Draw the image scaled into cellFrame when the resolved display size
// differs from the stored image size (rule-scaled cells, and legacy images
// reduced to the window width). Returns NO when the default unscaled
// drawing should run instead.
- (BOOL)drawScaledInFrame:(NSRect)cellFrame {
    if (!self.image || lastDisplaySize.width <= 0 ||
        NSEqualSizes(lastDisplaySize, self.image.size))
        return NO;
    NSGraphicsContext *ctx = NSGraphicsContext.currentContext;
    NSImageInterpolation oldInterpolation = ctx.imageInterpolation;
    ctx.imageInterpolation = NSImageInterpolationHigh;
    [self.image drawInRect:cellFrame
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0
            respectFlipped:YES
                     hints:nil];
    ctx.imageInterpolation = oldInterpolation;
    return YES;
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView {
    switch (_glkImgAlign) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            if ([self drawScaledInFrame:cellFrame])
                break;
            [super drawWithFrame:cellFrame
                          inView:controlView];
            break;
    }
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
       characterIndex:(NSUInteger)charIndex {
    switch (_glkImgAlign) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            if ([self drawScaledInFrame:cellFrame])
                break;
            [super drawWithFrame:cellFrame
                          inView:controlView
                  characterIndex:charIndex];
            break;
    }
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
       characterIndex:(NSUInteger)charIndex
        layoutManager:(NSLayoutManager *)layoutManager  {
    switch (_glkImgAlign) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            if ([self drawScaledInFrame:cellFrame])
                break;
            [super drawWithFrame:cellFrame
                          inView:controlView
                  characterIndex:charIndex
                   layoutManager:layoutManager];
            break;
    }

}

- (void)highlight:(BOOL)flag
        withFrame:(NSRect)cellFrame
           inView:(NSView *)controlView {
    switch (_glkImgAlign) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            [super highlight:flag
                   withFrame:cellFrame
                      inView:controlView];
            break;
    }
}

#pragma mark Dragging

- (void)dragTextAttachmentFrom:(NSTextView *)source event:(NSEvent *)event filename:(NSString *)filename inRect:(NSRect)frame {
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

    baseFilename = filename;

    [dragItem setDraggingFrame:frame contents:self.image];
    [source beginDraggingSessionWithItems:@[dragItem] event:event source:self];
}


- (nonnull NSString *)filePromiseProvider:(nonnull NSFilePromiseProvider *)filePromiseProvider fileNameForType:(nonnull NSString *)fileType {
    NSString *fileName = [baseFilename stringByAppendingPathExtension:@"png"];
    if (!fileName.length)
        fileName = @"image.png";

    return fileName;
}

- (void)filePromiseProvider:(nonnull NSFilePromiseProvider *)filePromiseProvider writePromiseToURL:(nonnull NSURL *)url completionHandler:(nonnull void (^)(NSError * _Nullable))completionHandler {

    NSData *bitmapData = [self pngData];

    NSError *error = nil;

    if (![bitmapData writeToURL:url options:NSDataWritingAtomic error:&error]) {
        NSLog(@"Error: Could not write PNG data to url %@: %@", url.path, error);
        completionHandler(error);
        return;
    }

    completionHandler(nil);
    NSLog(@"Your image has been saved to %@", url.path);
}

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    return NSDragOperationCopy;
}

- (void)pasteboard:(nullable NSPasteboard *)pasteboard item:(nonnull NSPasteboardItem *)item provideDataForType:(nonnull NSPasteboardType)type {
    // sender has accepted the drag and now we need to send the data for the type we promised
    if ( [type isEqual:NSPasteboardTypePNG]) {
        // set data for PNG type on the pasteboard as requested
        [pasteboard setData:[self pngData] forType:NSPasteboardTypePNG];
    } else if ([type isEqualTo:PasteboardFilePromiseContent]) {
        // The receiver will send this asking for the content type for the drop, to figure out
        // whether it wants to/is able to accept the file type.

        [pasteboard setString:@"public.png" forType: PasteboardFilePromiseContent];
    }
    else if ([type isEqualTo: PasteboardFileURLPromise]) {
        // The receiver is interested in our data, and is happy with the format that we told it
        // about during the PasteboardFilePromiseContent request.
        // The receiver has passed us a URL where we are to write our data to.

        NSString *str = [pasteboard stringForType:PasteboardFilePasteLocation];
        NSURL *destinationFolderURL = [NSURL fileURLWithPath:str isDirectory:YES];
        if (!destinationFolderURL) {
            NSLog(@"ERROR:- Receiver didn't tell us where to put the file?");
            return;
        }

        // Here, we build the file destination using the receivers destination URL
        if (!baseFilename.length)
            baseFilename = @"image";

        NSString *fileName = [baseFilename stringByAppendingPathExtension:@"png"];

        NSURL *destinationFileURL = [destinationFolderURL URLByAppendingPathComponent:fileName isDirectory:NO];

        NSUInteger index = 2;

        // Handle duplicate file names
        // by slapping on a number at the end.
        while ([[NSFileManager defaultManager] fileExistsAtPath:destinationFileURL.path]) {
            NSString *newFileName = [NSString stringWithFormat:@"%@ %ld", baseFilename, index];
            newFileName = [newFileName stringByAppendingPathExtension:@"png"];
            destinationFileURL = [destinationFolderURL URLByAppendingPathComponent:newFileName isDirectory:NO];
            index++;
        }

        NSData *bitmapData = [self pngData];

        NSError *error = nil;

        if (![bitmapData writeToURL:destinationFileURL options:NSDataWritingAtomic error:&error]) {
            NSLog(@"Error: Could not write PNG data to url %@: %@", destinationFileURL.path, error);
        }

        // And finally, tell the receiver where we wrote our file
        if (destinationFileURL.path.length > 0)
            [pasteboard setString:destinationFileURL.path forType:PasteboardFileURLPromise];
    }
}

- (NSData *)pngData {
    if (!self.image)
        NSLog(@"No image?");
    NSBitmapImageRep *bitmaprep = self.image.bitmapImageRepresentation;

    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypePNG properties:props];
}

#pragma mark Accessibility

- (NSString *)customA11yLabel {
    if (_marginImage) {
        return _marginImage.customA11yLabel;
    }
    NSString *label = self.image.accessibilityDescription;
    NSUInteger lastCharPos = _pos - 1;

    if (lastCharPos > _attrstr.length)
        lastCharPos = 0;

    NSNumber *linkVal = (NSNumber *)[_attrstr
                                     attribute:NSLinkAttributeName
                                     atIndex:lastCharPos
                                     effectiveRange:nil];

    NSUInteger linkid = linkVal.unsignedIntegerValue;

    if (!label.length) {
        if (linkid)
            label = @"Clickable attached image";
        else
            label = @"Attached image";
    }
    return label;
}


@end
