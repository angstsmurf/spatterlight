//
//  ImageDragSource.h
//  Spatterlight
//
//  Dragging an image out of a buffer window as a PNG file, shared by inline
//  images (MyAttachmentCell) and margin images (MarginImage).
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageDragSource : NSObject <NSFilePromiseProviderDelegate, NSDraggingSource, NSPasteboardItemDataProvider>

@property (nullable) NSImage *image;
@property (readonly, nullable) NSData *pngData;

// Start dragging image from source. filename is the name of the PNG file
// dropped, without extension. The drag needs the caller to keep this
// object alive, as NSFilePromiseProvider does not retain its delegate.
- (void)dragImage:(NSImage *)image
             from:(NSView *)source
            event:(NSEvent *)event
         filename:(NSString *)filename
            frame:(NSRect)frame;

@end

NS_ASSUME_NONNULL_END
