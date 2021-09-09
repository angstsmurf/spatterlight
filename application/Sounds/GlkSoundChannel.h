@class SoundHandler, MIDIChannel;

@interface GlkSoundChannel : NSObject {
    NSInteger loop;
    NSInteger notify;
    NSUInteger paused;

    NSInteger resid; /* for notifies */
    int status;

    NSString *mimeString;

    /* for volume fades */
    NSInteger volume_notify;
    NSUInteger volume_timeout;
    CGFloat target_volume;
    CGFloat volume;
    CGFloat volume_delta;
    NSTimer *timer;
}

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

//- (void)postInit;

@end
