#import <Foundation/Foundation.h>

#import "SoundResource.h"

@class SoundHandler;

NS_ASSUME_NONNULL_BEGIN

/// A Glk sound channel. It plays each sound through the backend its format
/// needs (see GlkSoundBackend.h), and keeps what Glk can see across them.
@interface GlkSoundChannel : NSObject <NSSecureCoding>

/// YES from a successful play until the sound ends or is stopped. Paused
/// sounds are still playing.
@property (getter=isPlaying) BOOL playing;
@property int name;
@property (weak, nullable) SoundHandler *handler;

/// YES while this channel holds a music-like sound (looping or >5s).
/// Derived from playing, nowPlayingLooping and nowPlayingDuration.
@property (readonly) BOOL claimsNowPlaying;
/// Decoded length in seconds (0 if unknown). Not multiplied by repeats.
@property NSTimeInterval nowPlayingDuration;
/// YES when the current sound was started with infinite repeats.
@property BOOL nowPlayingLooping;

- (BOOL)isPaused;

- (instancetype)initWithHandler:(nullable SoundHandler *)handler name:(int)name volume:(glui32)vol;
- (oneway void)setVolume:(glui32) vol duration:(glui32)dur notification:(glui32)noti;
- (BOOL)playSound:(glsi32) sound countOfRepeats:(glsi32)repeat notification:(glui32)noti;
- (oneway void)stop;
- (oneway void)pause;
- (oneway void)unpause;

/// Pick up a deserialized channel's sound and fade where they left off.
- (void)restartInternal;

- (nullable instancetype)initWithCoder:(NSCoder *)decoder;
- (void) encodeWithCoder:(NSCoder *)encoder;

@end

/// The class name a channel playing MIDI is archived under. Channels were
/// once a pair of classes, one for MIDI; this keeps autosaves readable both
/// by older builds and from them.
@interface MIDIChannel : GlkSoundChannel
@end

NS_ASSUME_NONNULL_END
