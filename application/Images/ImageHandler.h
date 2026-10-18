//
//  ImageHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-03-30.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageFile : NSObject

- (instancetype)initWithPath:(NSString *)path;
- (instancetype)initWithURL:(NSURL *)path;
- (void)resolveBookmark;

@property (nullable) NSData *bookmark;
@property (nullable) NSURL *URL;

@end


// The playback timing of an animated image (currently only GIF). An NSImage
// decoded from a multi-frame GIF already carries every frame in its bitmap
// rep, so there is nothing to decode here: this reads the rep's frame delays
// and loop count once, and switches the rep's current frame on request. The
// one instance for an image lives as long as the image does, so it shares the
// image cache's budget and is shared by every attachment cell that shows the
// same image; the cells keep their own playback position.
@interface ImageAnimation : NSObject

// The animation of image, made the first time it is asked for. Returns nil
// unless image has a bitmap rep with more than one frame.
+ (nullable ImageAnimation *)animationForImage:(NSImage *)image;

@property (readonly) NSImage *image;
@property (readonly) NSUInteger frameCount;
// Length of one pass through all frames, in seconds.
@property (readonly) NSTimeInterval duration;
// Number of passes to play; 0 means loop forever.
@property (readonly) NSUInteger loopCount;

// The frame showing elapsed seconds into playback. If untilNext is non-NULL
// it receives the time left before a later frame is due, or INFINITY when
// playback has stopped on the final frame.
- (NSUInteger)frameAtTime:(NSTimeInterval)elapsed
                untilNext:(nullable NSTimeInterval *)untilNext;
// The image, switched to show frame. Call it right before drawing: the frame
// is a property of the rep, which every cell showing the image shares.
- (nullable NSImage *)imageForFrame:(NSUInteger)frame;

@end


@interface ImageResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL load NS_SWIFT_UNAVAILABLE("Use the throwing method instead");
- (BOOL)loadWithError:(NSError**)outError;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSImage * _Nullable createImage;

@property (nullable) NSData *data;
@property (nullable) ImageFile *imageFile;
@property NSString *filename;
@property NSUInteger offset;
@property NSUInteger length;
@property NSString *a11yDescription;

@end


@interface ImageHandler : NSObject

@property NSCache<NSNumber *, NSImage *> *imageCache;

@property NSInteger lastimageresno;
@property (nullable) NSImage *lastimage;
@property NSMutableDictionary <NSNumber *, ImageResource *> *resources;
@property NSMutableDictionary <NSString *, ImageFile *> *files;
@property NSMutableDictionary <NSNumber *, NSString *> *imageDescriptions;

- (void)cacheImagesFromBlorbURL:(NSURL *)file withData:(NSData *)data;
@property (readonly, nonatomic, copy) NSString *lastImageLabel;

- (BOOL)handleFindImageNumber:(NSInteger)resno;
- (void)handleLoadImageNumber:(NSInteger)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       size:(NSUInteger)size;

// The animation for image resno, or nil if it is a still image.
- (nullable ImageAnimation *)animationForImageNumber:(NSInteger)resno;

- (void)purgeImage:(NSInteger)resno withReplacement:(nullable NSString *)path
            size:(NSUInteger)size;

@end

NS_ASSUME_NONNULL_END
