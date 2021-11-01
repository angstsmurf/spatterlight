#import <Foundation/Foundation.h>
@class SoundHandler, MIDIChannel;

#define FADE_GRANULARITY 100
#define GLK_MAXVOLUME 0x10000
#define MIX_MAX_VOLUME 1.0f

enum { CHANNEL_IDLE, CHANNEL_SOUND };

@interface GlkSoundChannel : NSObject <NSSecureCoding> {
    NSInteger loop;
    NSInteger notify;
    NSUInteger paused;

    NSInteger resid; /* for notifies */

    NSString *mimeString;

    /* for volume fades */
    NSInteger volume_notify;
    NSUInteger volume_timeout;
    CGFloat target_volume;
    CGFloat volume;
    CGFloat volume_delta;
    NSTimer *timer;
}

@property NSInteger status;
@property NSUInteger name;
@property (weak) SoundHandler *handler;

- (instancetype)initWithHandler:(SoundHandler *)handler name:(NSUInteger)name volume:(NSUInteger)vol;
- (void)setVolume:(NSUInteger)avol duration:(NSUInteger)duration notify:(NSInteger)notify;
- (void)play:(NSInteger)snd repeats:(NSInteger)areps notify:(NSInteger)anot;
- (void)stop;
- (void)pause;
- (void)unpause;
- (void)cleanup;

- (void)copyValues:(GlkSoundChannel *)otherChannel;

- (void)restartInternal;

- (instancetype)initWithCoder:(NSCoder *)decoder;
- (void) encodeWithCoder:(NSCoder *)encoder;

@end
