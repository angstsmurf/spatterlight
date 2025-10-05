//
//  MarginImage.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MarginImage.h"
#import "MarginContainer.h"
#import "MyFilePromiseProvider.h"
#import "NSImage+Categories.h"
#import "Constants.h"

#include "glk.h"

@interface MarginImage () <NSSecureCoding> {
    BOOL recalc;
    NSString *baseFilename;
}
@end

@implementation MarginImage

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    return [self
        initWithImage:[[NSImage alloc] initWithContentsOfFile:@"../Resources/Question.png"]
            alignment:kAlignLeft
               linkId:0
                   at:0
               sender:self];
}

- (instancetype)initWithImage:(NSImage *)animage
                    alignment:(NSInteger)imageAlignment
                       linkId:(NSUInteger)linkId
                           at:(NSUInteger)apos
                       sender:(id)sender {
    self = [super init];
    if (self) {
        _image = animage;
        _glkImgAlign = imageAlignment;
        _bounds = NSZeroRect;
        _linkid = linkId;
        _pos = apos;
        recalc = YES;
        _container = sender;
        _uuid = [[NSUUID UUID] UUIDString];

        self.accessibilityParent = _container.textView;
        self.accessibilityRoleDescription = self.customA11yLabel;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        _image = [decoder decodeObjectOfClass:[NSImage class] forKey:@"image"];
        _glkImgAlign = [decoder decodeIntegerForKey:@"alignment"];
        _bounds = [decoder decodeRectForKey:@"bounds"];
        _linkid = (NSUInteger)[decoder decodeIntegerForKey:@"linkid"];
        _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
        _uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"uuid"];
        self.accessibilityRoleDescription = [decoder decodeObjectOfClass:[NSString class] forKey:@"accessibilityRoleDescription"];
        recalc = YES;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_image forKey:@"image"];
    [encoder encodeInteger:_glkImgAlign forKey:@"alignment"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
    [encoder encodeInteger:(NSInteger)_linkid forKey:@"linkid"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeObject:_uuid forKey:@"uuid"];
    [encoder encodeObject:self.accessibilityRoleDescription forKey:@"accessibilityRoleDescription"];
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout {
    NSRect theline;
    NSSize size = _image.size;

    if (recalc) {
        recalc = NO; /* don't infiniloop in here, settle for the first result */

        _bounds = NSZeroRect;
        NSTextView *textview = _container.textView;

        if (_pos >= textview.textStorage.length) {
            NSLog(@"MarginImage boundsWithLayout: _pos: %ld textStorage.length: %ld", _pos, textview.textStorage.length);
            return NSZeroRect;
        }

        /* get position of anchor glyph */
        theline = [layout lineFragmentRectForGlyphAtIndex:_pos
                                           effectiveRange:nil];

        NSParagraphStyle *para = [textview.textStorage attribute:NSParagraphStyleAttributeName atIndex:_pos effectiveRange:nil];
        theline.origin.y += para.paragraphSpacingBefore;

        /* set bounds to be at the same line as anchor but in left/right margin
         */
        if (_glkImgAlign == imagealign_MarginRight) {
            CGFloat rightMargin = textview.frame.size.width -
                                  textview.textContainerInset.width * 2 -
                                  _container.lineFragmentPadding;
            _bounds = NSMakeRect(rightMargin - size.width, theline.origin.y,
                                 size.width, size.height);

            // If the above places the image outside margin, move it within
            if (NSMaxX(_bounds) > rightMargin) {
                _bounds.origin.x = rightMargin - size.width;
            }
        } else {
            _bounds =
                NSMakeRect(theline.origin.x + _container.lineFragmentPadding,
                           theline.origin.y, size.width, size.height);
        }
    }

    [_container unoverlap:self];
    return _bounds;
}

- (void)uncacheBounds {
    recalc = YES;
//    _bounds = NSZeroRect;
}

-(void)cursorUpdate {
    if (_linkid) {
        [NSCursor.pointingHandCursor set];
    } else {
        [NSCursor.arrowCursor set];
    }
}

#pragma mark Dragging

- (void)dragMarginImageFrom:(NSTextView *)source event:(NSEvent *)event filename:(NSString *)filename rect:(NSRect)rect {
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

    [dragItem setDraggingFrame:rect contents:_image];
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
    if (!_image)
        NSLog(@"No image?");
    NSBitmapImageRep *bitmaprep = _image.bitmapImageRepresentation;

    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypePNG properties:props];
}

#pragma mark Accessibility

- (void)setAccessibilityFocused:(BOOL)accessibilityFocused {
    if (accessibilityFocused) {
        NSRect bounds = [self boundsWithLayout:_container.layoutManager];
        bounds = NSAccessibilityFrameInView(_container.textView, bounds);
        bounds.origin.x += _container.textView.textContainerInset.width;
        bounds.origin.y -= _container.textView.textContainerInset.height;
        self.accessibilityFrame = bounds;
        NSTextView *parent = (NSTextView *)self.accessibilityParent;
        if (parent) {
            [parent becomeFirstResponder];
        }
    }
}

- (NSString *)customA11yLabel {
    NSString *label = _image.accessibilityDescription;
    if (!label.length) {
        if (_linkid) {
            label = [NSString stringWithFormat: @"Clickable %@ margin image", _glkImgAlign == imagealign_MarginLeft ? @"left" : @"right"];
        } else {
            label = [NSString stringWithFormat: @"%@ margin image", _glkImgAlign == imagealign_MarginLeft ? @"Left" : @"Right"];
        }
    }
    return label;
}

- (BOOL)isAccessibilityElement {
    return YES;
}

@end
