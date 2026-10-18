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


// The frames of an animated image (currently only GIF), decoded on demand.
// Shared by every attachment cell that shows the same resource; the cells
// keep their own playback position.
@interface ImageAnimation : NSObject

// Returns nil unless data is a GIF with more than one frame.
- (nullable instancetype)initWithData:(NSData *)data;

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
- (nullable NSImage *)imageForFrame:(NSUInteger)frame;

@end


@interface ImageResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL load NS_SWIFT_UNAVAILABLE("Use the throwing method instead");
- (BOOL)loadWithError:(NSError**)outError;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSImage * _Nullable createImage;
@property (NS_NONATOMIC_IOSONLY, readonly) ImageAnimation * _Nullable createAnimation;

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
