#import <Foundation/Foundation.h>

@class SoundHandler, MIDIChannel;

typedef uint32_t glui32;
typedef int32_t glsi32;

#define FADE_GRANULARITY 100
#define GLK_MAXVOLUME 0x10000
#define FLOAT_MAX_VOLUME 1.0

typedef NS_ENUM(NSInteger, GlkSoundChannelStatus) {
    GlkSoundChannelStatusIdle,
    GlkSoundChannelStatusSound
};

@interface GlkSoundChannel : NSObject <NSSecureCoding> {
    glsi32 loop;
    glui32 notify;
    glui32 paused;

    glsi32 resid; /* for notifies */

    NSString *mimeString;

    /* for volume fades */
    glui32 volume_notify;
    glui32 volume_timeout;
    float target_volume;
    float volume;
    float volume_delta;
    NSTimer *timer;
}

@property GlkSoundChannelStatus status;
@property int name;
@property (weak) SoundHandler *handler;

/// YES while this channel holds a music-like sound (looping or >5s).
/// Derived from status, nowPlayingLooping and nowPlayingDuration.
@property (readonly) BOOL claimsNowPlaying;
/// Decoded length in seconds (0 if unknown). Not multiplied by repeats.
@property NSTimeInterval nowPlayingDuration;
/// YES when the current sound was started with infinite repeats.
@property BOOL nowPlayingLooping;

- (BOOL)isPaused;

- (instancetype)initWithHandler:(SoundHandler *)handler name:(int)name volume:(glui32)vol;
- (oneway void)setVolume:(glui32) vol duration:(glui32)dur notification:(glui32)noti;
- (BOOL)playSound:(glsi32) sound countOfRepeats:(glsi32)repeat notification:(glui32)noti;
/// The actual playback work; subclasses override this rather than playSound:,
/// which wraps it in a single Now Playing update.
- (BOOL)playSoundInternal:(glsi32) sound countOfRepeats:(glsi32)repeat notification:(glui32)noti;
- (oneway void)stop;
- (oneway void)pause;
- (oneway void)unpause;
- (void)cleanup;

- (void)copyValues:(GlkSoundChannel *)otherChannel;

- (void)restartInternal;

- (instancetype)initWithCoder:(NSCoder *)decoder;
- (void) encodeWithCoder:(NSCoder *)encoder;

@end
