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

- (instancetype)initWithHandler:(SoundHandler *)handler name:(int)name volume:(glui32)vol;
- (oneway void)setVolume:(glui32) vol duration:(glui32)dur notification:(glui32)noti;
- (BOOL)playSound:(glsi32) sound countOfRepeats:(glsi32)repeat notification:(glui32)noti;
- (oneway void)stop;
- (oneway void)pause;
- (oneway void)unpause;
- (void)cleanup;

- (void)copyValues:(GlkSoundChannel *)otherChannel;

- (void)restartInternal;

- (instancetype)initWithCoder:(NSCoder *)decoder;
- (void) encodeWithCoder:(NSCoder *)encoder;

@end
