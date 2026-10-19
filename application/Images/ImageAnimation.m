//
//  ImageAnimation.m
//  Spatterlight
//

#import "ImageAnimation.h"
#import <objc/runtime.h>

// Like web browsers, treat frame delays too short to display (GIF delays
// are in hundredths of a second, so anything under 2) as 100 ms.
static const NSTimeInterval kShortestFrameDelay = 0.011;
static const NSTimeInterval kDefaultFrameDelay = 0.1;

static const void *kImageAnimationKey = &kImageAnimationKey;

@implementation ImageAnimation {
    NSBitmapImageRep *_rep;
    // End time of each frame, in seconds from the start of a pass.
    NSTimeInterval *_frameEnds;
}

// The first bitmap rep of image that has more than one frame, or nil.
static NSBitmapImageRep *animatedRep(NSImage *image) {
    for (NSImageRep *rep in image.representations) {
        if (![rep isKindOfClass:[NSBitmapImageRep class]])
            continue;
        NSNumber *frames = [(NSBitmapImageRep *)rep valueForProperty:NSImageFrameCount];
        if (frames.unsignedIntegerValue > 1)
            return (NSBitmapImageRep *)rep;
    }
    return nil;
}

+ (nullable ImageAnimation *)animationForImage:(nullable NSImage *)image {
    if (!image)
        return nil;
    // Kept as an associated object so that it goes when the image goes: the
    // image cache's cost limit then bounds animations along with stills, and
    // every cell that draws the image gets the same instance.
    ImageAnimation *animation = objc_getAssociatedObject(image, kImageAnimationKey);
    if (animation)
        return animation == (id)[NSNull null] ? nil : animation;
    NSBitmapImageRep *rep = animatedRep(image);
    animation = rep ? [[ImageAnimation alloc] initWithImage:image rep:rep] : nil;
    objc_setAssociatedObject(image, kImageAnimationKey, animation ?: [NSNull null],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return animation;
}

- (instancetype)initWithImage:(NSImage *)image rep:(NSBitmapImageRep *)rep {
    self = [super init];
    if (self) {
        _image = image;
        _rep = rep;
        _frameCount = ((NSNumber *)[rep valueForProperty:NSImageFrameCount]).unsignedIntegerValue;

        // A GIF without a looping extension plays once.
        _loopCount = 1;
        NSNumber *loops = [rep valueForProperty:NSImageLoopCount];
        if (loops)
            _loopCount = loops.unsignedIntegerValue;

        // Each frame's delay is only readable while it is the current frame.
        NSNumber *current = [rep valueForProperty:NSImageCurrentFrame];
        _frameEnds = calloc(_frameCount, sizeof(NSTimeInterval));
        if (!_frameEnds)
            return nil;
        for (NSUInteger i = 0; i < _frameCount; i++) {
            [rep setProperty:NSImageCurrentFrame withValue:@(i)];
            NSTimeInterval seconds = ((NSNumber *)[rep valueForProperty:NSImageCurrentFrameDuration]).doubleValue;
            if (seconds < kShortestFrameDelay)
                seconds = kDefaultFrameDelay;
            _duration += seconds;
            _frameEnds[i] = _duration;
        }
        [rep setProperty:NSImageCurrentFrame withValue:current ?: @0];
    }
    return self;
}

- (void)dealloc {
    free(_frameEnds);
}

- (NSUInteger)frameAtTime:(NSTimeInterval)elapsed
                untilNext:(nullable NSTimeInterval *)untilNext {
    if (elapsed < 0)
        elapsed = 0;
    if (_loopCount && elapsed >= _duration * _loopCount) {
        if (untilNext)
            *untilNext = INFINITY;
        return _frameCount - 1;
    }
    NSTimeInterval position = fmod(elapsed, _duration);
    NSUInteger frame = 0;
    while (frame < _frameCount - 1 && _frameEnds[frame] <= position)
        frame++;
    if (untilNext)
        *untilNext = _frameEnds[frame] - position;
    return frame;
}

- (nullable NSImage *)imageForFrame:(NSUInteger)frame {
    if (frame >= _frameCount)
        return nil;
    [_rep setProperty:NSImageCurrentFrame withValue:@(frame)];
    return _image;
}

@end
