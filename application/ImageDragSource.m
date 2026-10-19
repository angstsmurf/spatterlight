//
//  ImageDragSource.m
//  Spatterlight
//

#import "ImageDragSource.h"
#import "MyFilePromiseProvider.h"
#import "NSImage+Categories.h"
#import "Constants.h"

#import "DebugLog.h"

@interface ImageDragSource () {
    NSString *baseFilename;
    NSURL *temporaryFileURL;
}
@end

@implementation ImageDragSource

- (NSDraggingSession *)dragImage:(NSImage *)image
                            from:(NSView *)source
                           event:(NSEvent *)event
                        filename:(nullable NSString *)filename
                           frame:(NSRect)frame {
    NSDraggingItem *dragItem;

    if (@available(macOS 10.14, *)) {
        MyFilePromiseProvider *provider = [[MyFilePromiseProvider alloc] initWithFileType: NSPasteboardTypePNG delegate:self];

        dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:provider];
    } else {
        NSPasteboardItem *pasteboardItem = [NSPasteboardItem new];

        [pasteboardItem setDataProvider:self forTypes:@[NSPasteboardTypePNG, NSPasteboardTypeFileURL, PasteboardFileURLPromise, PasteboardFilePromiseContent]];

        // Create the dragging item for the drag operation
        dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:pasteboardItem];
    }

    _image = image;
    _sourceView = source;
    baseFilename = filename.length ? filename : @"image";
    temporaryFileURL = nil;

    [dragItem setDraggingFrame:frame contents:image];
    return [source beginDraggingSessionWithItems:@[dragItem] event:event source:self];
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

- (NSOperationQueue *)operationQueueForFilePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider {
    return _operationQueue ?: [NSOperationQueue mainQueue];
}

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    return NSDragOperationCopy;
}

// The pre-10.14 path, when NSFilePromiseProvider is not available.
- (void)pasteboard:(nullable NSPasteboard *)pasteboard item:(nonnull NSPasteboardItem *)item provideDataForType:(nonnull NSPasteboardType)type {
    // sender has accepted the drag and now we need to send the data for the type we promised
    if ([type isEqualToString:NSPasteboardTypePNG]) {
        // set data for PNG type on the pasteboard as requested
        [pasteboard setData:[self pngData] forType:NSPasteboardTypePNG];
    } else if ([type isEqualToString:NSPasteboardTypeFileURL]) {
        NSURL *url = self.temporaryFileURL;
        if (url)
            [item setString:url.absoluteString forType:NSPasteboardTypeFileURL];
    } else if ([type isEqualToString:PasteboardFilePromiseContent]) {
        // The receiver will send this asking for the content type for the drop, to figure out
        // whether it wants to/is able to accept the file type.

        [pasteboard setString:@"public.png" forType: PasteboardFilePromiseContent];
    } else if ([type isEqualToString:PasteboardFileURLPromise]) {
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

// A PNG file holding the image, for receivers that take file URLs but not
// file promises, such as Terminal (which pastes the path). Written when first
// asked for. In the sandboxed release this is inside the app's container,
// the only place it may write, so Terminal can read it but macOS 14+ may ask
// once whether Terminal may access data from other apps.
//
// The file must outlive the drag, since the receiver may read it much later
// (Claude Code in Terminal reads it when the prompt is sent), so each drag
// only removes the folders of drags more than a day old.
- (nullable NSURL *)temporaryFileURL {
    if (temporaryFileURL)
        return temporaryFileURL;
    NSData *data = self.pngData;
    if (!data)
        return nil;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *parent = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES]
                     URLByAppendingPathComponent:@"Dragged images" isDirectory:YES];
    [ImageDragSource removeFoldersOlderThanADayIn:parent];
    NSURL *folder = [parent URLByAppendingPathComponent:[NSUUID UUID].UUIDString isDirectory:YES];
    NSError *error = nil;
    if (![fileManager createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:nil error:&error]) {
        NSLog(@"ImageDragSource: could not create %@: %@", folder.path, error);
        return nil;
    }
    NSURL *url = [folder URLByAppendingPathComponent:[baseFilename stringByAppendingPathExtension:@"png"] isDirectory:NO];
    if (![data writeToURL:url options:NSDataWritingAtomic error:&error]) {
        NSLog(@"ImageDragSource: could not write %@: %@", url.path, error);
        return nil;
    }
    temporaryFileURL = url;
    return url;
}

+ (void)removeFoldersOlderThanADayIn:(NSURL *)parent {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSDate *cutoff = [NSDate dateWithTimeIntervalSinceNow:-24 * 60 * 60];
    NSArray<NSURL *> *folders = [fileManager contentsOfDirectoryAtURL:parent
                                           includingPropertiesForKeys:@[NSURLCreationDateKey]
                                                              options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                error:nil];
    for (NSURL *folder in folders) {
        NSDate *created = nil;
        [folder getResourceValue:&created forKey:NSURLCreationDateKey error:nil];
        if (created && [created compare:cutoff] == NSOrderedAscending)
            [fileManager removeItemAtURL:folder error:nil];
    }
}

// Also asked for by MyFilePromiseProvider, for the PNG pasteboard type.
- (nullable NSData *)pngData {
    NSData *data = [ImageDragSource pngDataForImage:_image];
    if (!data)
        NSLog(@"ImageDragSource: no image to drag");
    return data;
}

+ (nullable NSData *)pngDataForImage:(nullable NSImage *)image {
    NSBitmapImageRep *bitmaprep = image.bitmapImageRepresentation;
    if (!bitmaprep)
        return nil;

    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypePNG properties:props];
}

@end
