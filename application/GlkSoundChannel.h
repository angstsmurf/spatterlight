@class GlkController;

@interface GlkSoundChannel : NSObject {
    GlkController *glkctl;
    void *channel;
    void *sound;
    int volume;
    int repeats;
    int notify;
}

- (id)initWithGlkController:(GlkController *)glkctl name:(int)name;
- (void)setVolume:(int)avol;
- (void)play:(NSData *)sample repeats:(int)areps notify:(int)anot;
- (void)stop;

@end
