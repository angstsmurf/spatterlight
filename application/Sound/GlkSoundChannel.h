@class GlkController, SoundResource, AudioResourceHandler;

@interface GlkSoundChannel : NSObject {

    void *channel;
    void *sound;
    NSInteger loop;
    NSInteger notify;
    NSUInteger paused;
    
    NSInteger resid; /* for notifies */
    int status;
    
    void *sample; /* Mix_Chunk */
    void *music; /* Mix_Music */
    
    void *sdl_rwops; /* SDL_RWops */
    unsigned char *sdl_memory;
    int sdl_channel;
    
    /* for volume fades */
    NSUInteger volume;
    NSInteger volume_notify;
    NSUInteger volume_timeout;
    NSUInteger target_volume;
    CGFloat float_volume;
    CGFloat volume_delta;
    NSTimer *timer;
    NSMutableDictionary <NSNumber *, GlkSoundChannel *> *sound_channels;
    NSMutableDictionary <NSNumber *, SoundResource *> *my_resources;
}

@property NSUInteger name;
@property (weak) GlkController *glkctl;
@property (weak) AudioResourceHandler *handler;

- (id)initWithGlkController:(GlkController*)glkctl_ name:(NSUInteger)name volume:(NSUInteger)vol;
- (void)setVolume:(NSUInteger)avol duration:(NSUInteger)duration notify:(NSInteger)notify;
- (void)play:(NSInteger)snd repeats:(NSInteger)areps notify:(NSInteger)anot;
- (void)stop;
- (void)pause;
- (void)unpause;
- (void)cleanup;

- (void)restartInternal;
- (void)postInit;

@end
