@class SoundResource, SoundHandler;

@interface GlkSoundChannel : NSObject

@property NSUInteger name;
@property (weak) SoundHandler *handler;

- (instancetype)initWithHandler:(SoundHandler *)handler name:(NSUInteger)name volume:(NSUInteger)vol;
- (void)setVolume:(NSUInteger)avol duration:(NSUInteger)duration notify:(NSInteger)notify;
- (void)play:(NSInteger)snd repeats:(NSInteger)areps notify:(NSInteger)anot;
- (void)stop;
- (void)pause;
- (void)unpause;
- (void)cleanup;

- (void)restartInternal;
- (void)postInit;

@end
