//
//  ImageAnimation.h
//  Spatterlight
//
//  The playback timing of an animated image (currently only GIF). An NSImage
//  decoded from a multi-frame GIF already carries every frame in its bitmap
//  rep, so there is nothing to decode here: this reads the rep's frame delays
//  and loop count once, and switches the rep's current frame on request. The
//  one instance for an image lives as long as the image does, so it shares the
//  image cache's budget and is shared by every attachment cell that shows the
//  same image; the cells keep their own playback position.
//
//  Main thread only, like the drawing it serves.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageAnimation : NSObject

// The animation of image, made the first time it is asked for. Returns nil
// unless image has a bitmap rep with more than one frame.
+ (nullable ImageAnimation *)animationForImage:(nullable NSImage *)image;

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

NS_ASSUME_NONNULL_END
