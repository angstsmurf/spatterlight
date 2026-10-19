//
//  ImageDragSource.m
//  Spatterlight
//

#import "ImageDragSource.h"
#import "MyFilePromiseProvider.h"
#import "NSImage+Categories.h"
#import "Constants.h"

@interface ImageDragSource () {
    NSString *baseFilename;
}
@end

@implementation ImageDragSource

- (void)dragImage:(NSImage *)image
             from:(NSView *)source
            event:(NSEvent *)event
         filename:(NSString *)filename
            frame:(NSRect)frame {
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

    _image = image;
    baseFilename = filename.length ? filename : @"image";

    [dragItem setDraggingFrame:frame contents:image];
    [source beginDraggingSessionWithItems:@[dragItem] event:event source:self];
}

- (nonnull NSString *)filePromiseProvider:(nonnull NSFilePromiseProvider *)filePromiseProvider fileNameForType:(nonnull NSString *)fileType {
    return [baseFilename stringByAppendingPathExtension:@"png"];
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
}

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    return NSDragOperationCopy;
}

// The pre-10.14 path, when NSFilePromiseProvider is not available.
- (void)pasteboard:(nullable NSPasteboard *)pasteboard item:(nonnull NSPasteboardItem *)item provideDataForType:(nonnull NSPasteboardType)type {
    // sender has accepted the drag and now we need to send the data for the type we promised
    if ([type isEqual:NSPasteboardTypePNG]) {
        // set data for PNG type on the pasteboard as requested
        [pasteboard setData:[self pngData] forType:NSPasteboardTypePNG];
    } else if ([type isEqualTo:PasteboardFilePromiseContent]) {
        // The receiver will send this asking for the content type for the drop, to figure out
        // whether it wants to/is able to accept the file type.

        [pasteboard setString:@"public.png" forType: PasteboardFilePromiseContent];
    } else if ([type isEqualTo: PasteboardFileURLPromise]) {
        // The receiver is interested in our data, and is happy with the format that we told it
        // about during the PasteboardFilePromiseContent request.
        // The receiver has passed us a URL where we are to write our data to.

        NSString *str = [pasteboard stringForType:PasteboardFilePasteLocation];
        if (!str.length) {
            NSLog(@"ERROR:- Receiver didn't tell us where to put the file?");
            return;
        }
        NSURL *destinationFolderURL = [NSURL fileURLWithPath:str isDirectory:YES];

        // Here, we build the file destination using the receivers destination URL
        NSString *fileName = [baseFilename stringByAppendingPathExtension:@"png"];

        NSURL *destinationFileURL = [destinationFolderURL URLByAppendingPathComponent:fileName isDirectory:NO];

        NSUInteger index = 2;

        // Handle duplicate file names
        // by slapping on a number at the end.
        while ([[NSFileManager defaultManager] fileExistsAtPath:destinationFileURL.path]) {
            NSString *newFileName = [NSString stringWithFormat:@"%@ %lu", baseFilename, (unsigned long)index];
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

// Also asked for by MyFilePromiseProvider, for the PNG pasteboard type.
- (nullable NSData *)pngData {
    NSBitmapImageRep *bitmaprep = _image.bitmapImageRepresentation;
    if (!bitmaprep) {
        NSLog(@"ImageDragSource: no image to drag");
        return nil;
    }

    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypePNG properties:props];
}

@end
