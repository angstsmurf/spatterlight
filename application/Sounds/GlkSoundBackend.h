//
//  GlkSoundBackend.h
//  Spatterlight
//
//  What a GlkSoundChannel plays its sounds through. The channel holds
//  everything Glk can see (volume and fades, pause, repeats, notifications);
//  a backend only turns one sound's bytes into audio. MIDI goes to
//  MIDISoundBackend and everything else to SFBSoundBackend.
//

#import <Foundation/Foundation.h>

#import "SoundResource.h"

NS_ASSUME_NONNULL_BEGIN

@protocol GlkSoundBackend <NSObject>

/// Stop whatever is playing and play `data` instead, `repeats` times (-1 for
/// ever), starting at `position`. With `paused`, get the sound ready but leave
/// it to -resume to start it. `finished` is called on the main queue when the
/// last repeat ends; it is never called for a sound that repeats for ever.
/// Returns NO if the sound cannot be played.
- (BOOL)playData:(NSData *)data
          format:(GlkSoundBlorbFormatType)format
         repeats:(glsi32)repeats
        position:(double)position
          paused:(BOOL)paused
          volume:(float)volume
        finished:(nullable void (^)(void))finished;
- (void)stop;
- (void)pause;
/// Carry on after -pause, or start a sound readied with `paused`. Returns NO
/// if there is nothing to carry on with, so the sound has to be played again.
- (BOOL)resume;
- (void)setVolume:(float)volume;

/// The length of one pass of the sound in seconds, or 0 if unknown.
@property (readonly) NSTimeInterval duration;

/// Where the sound has got to, and the repeat count to go with it: passing
/// the two back to -playData:… carries on from there. The position is in the
/// backend's own units.
@property (readonly) double resumePosition;
@property (readonly) glsi32 resumeRepeats;

@end

NS_ASSUME_NONNULL_END
