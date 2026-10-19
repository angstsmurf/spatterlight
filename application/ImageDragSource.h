//
//  ImageDragSource.h
//  Spatterlight
//
//  Dragging an image out of a view as a PNG file. Shared by buffer window
//  images (MyAttachmentCell, MarginImage), graphics windows and the library
//  cover image view.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageDragSource : NSObject <NSFilePromiseProviderDelegate, NSDraggingSource, NSPasteboardItemDataProvider>

@property (nullable) NSImage *image;
@property (readonly, nullable) NSData *pngData;
// The image written to a temporary PNG file, for receivers that want a file
// URL rather than a file promise.
@property (readonly, nullable) NSURL *temporaryFileURL;
// The view the current drag started from.
@property (weak, readonly, nullable) NSView *sourceView;
// Where the PNG file is written on 10.14+. Main queue if nil.
@property (nullable) NSOperationQueue *operationQueue;

+ (nullable NSData *)pngDataForImage:(nullable NSImage *)image;

// Start dragging image from source. filename is the name of the PNG file
// dropped, without extension. The drag needs the caller to keep this
// object alive, as NSFilePromiseProvider does not retain its delegate.
- (NSDraggingSession *)dragImage:(NSImage *)image
                            from:(NSView *)source
                           event:(NSEvent *)event
                        filename:(nullable NSString *)filename
                           frame:(NSRect)frame;

@end

NS_ASSUME_NONNULL_END
