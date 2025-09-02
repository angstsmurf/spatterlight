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

    if (_glkImgAlign == imagealign_InlineCenter) {
        return NSMakePoint(0, -(self.image.size.height / 2) + xHeight / 2);
    } else if (_glkImgAlign == imagealign_InlineDown) {
        return NSMakePoint(0, -self.image.size.height + ascender);
    }

    return [super cellBaselineOffset];
}

- (NSSize)cellSize {
    if (_glkImgAlign == imagealign_MarginLeft || _glkImgAlign == imagealign_MarginRight) {
        return NSZeroSize;
    }
    return [super cellSize];
}

- (NSRect)cellFrameForTextContainer:(NSTextContainer *)textContainer
               proposedLineFragment:(NSRect)lineFrag
                      glyphPosition:(NSPoint)position
                     characterIndex:(NSUInteger)charIndex {
    if (_glkImgAlign == imagealign_MarginLeft || _glkImgAlign == imagealign_MarginRight) {
        return NSZeroRect;
    }
    return [super cellFrameForTextContainer:textContainer
                       proposedLineFragment:lineFrag
                              glyphPosition:position
                             characterIndex:charIndex];
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView {
    switch (_glkImgAlign) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
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
        NSURL *destinationFolderURL = [NSURL fileURLWithPath:str  isDirectory:YES];
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
