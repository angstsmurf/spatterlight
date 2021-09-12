#import "MIDIChannel.h"
#import "SoundHandler.h"
#import "MIDIPlayer.h"

@interface MIDIChannel () {

@private
    MIDIPlayer    *_player;        // The player instance
}

@end

@implementation MIDIChannel

- (instancetype)initWithHandler:(SoundHandler*)handler name:(NSUInteger)channelname volume:(NSUInteger)vol
{
    self.handler = handler;
    loop = 0;
    notify = 0;
    self.name = channelname;
    
    status = CHANNEL_IDLE;
    volume = (CGFloat)vol / GLK_MAXVOLUME;
    resid = -1;
    loop = 0;
    notify = 0;
    paused = NO;

    volume_notify = 0;
    volume_timeout = 0;
    target_volume = 0;
    volume_delta = 0;
    timer = nil;
    
    return self;
}

- (void)play:(NSInteger)snd repeats:(NSInteger)areps notify:(NSInteger)anot
{
    status = CHANNEL_SOUND;

    char *buf = nil;
    size_t len = 0;
    NSInteger type;

    /* stop previous noise */
    if (_player) {
        [_player stop];
    }
    
    if (areps == 0 || snd == -1)
        return;
    
    /* load sound resource into memory */
    type = [self.handler load_sound_resource:snd length:&len data:&buf];

    if (type != giblorb_ID_MIDI)
        return;

    notify = anot;
    resid = snd;
    loop = areps;

//    if (_player) {
        _player = [[MIDIPlayer alloc] initWithData:[NSData dataWithBytes:buf length:len]];
//    }

    [_player setVolume:volume];

    if (notify && areps != -1) {
        __unsafe_unretained MIDIChannel *weakSelf = self;
        SoundHandler *blockHandler = self.handler;
        NSInteger blocknotify = notify;
        NSInteger blockresid = resid;
        [_player addCallback:(^(void){
            dispatch_async(dispatch_get_main_queue(), ^{
                MIDIChannel *strongSelf = weakSelf;
                if (!strongSelf || --strongSelf->loop < 1) {
                    strongSelf->status = CHANNEL_IDLE;
                    [blockHandler handleSoundNotification:blocknotify withSound:blockresid];
                }
            });
        })];
    }

   [_player loop:areps];

       if (!paused)
           [_player play];
}

- (void)stop
{
    paused = NO;
    if (_player) {
        [_player stop];
    }
    [self cleanup];
}

- (void)pause
{
    paused = YES;
    if (_player)
        [_player pause];
}

- (void)unpause
{
    paused = NO;
    if (!_player)
        [self play:resid repeats:loop notify:notify];
    else
        [_player play];
}

- (void)setVolume {
    if (!_player)
        return;
    [_player setVolume:volume];
}


/* Restart the sound channel after a deserialize, and also any fade timer.
 This primitive implementation disregards how much of the sound that had
 played when the game was autosaved or killed.
 Fade timers remember this, however, so a clip halfway through a 10
 second fade out will restart from the beginning but fade out in 10 seconds.
 Well, except that it counts from last glk_select and not from when the process
 was actually terminated.
 */
//- (void)restartInternal {
//    if (status == CHANNEL_IDLE) {
//        return;
//    }
//
//    if (paused == TRUE) {
//        [self pause];
//    }
//
//    [self play:resid repeats:loop notify:notify];
//
//    if (volume_timeout > 0) {
//
//        NSUInteger duration = (volume_timeout * FADE_GRANULARITY);
//
//        CGFloat float_volume = target_volume;
//        NSUInteger glk_target_volume = GLK_MAXVOLUME;
//
//        if (float_volume < MIX_MAX_VOLUME)
//           glk_target_volume = (NSUInteger)(float_volume * GLK_MAXVOLUME);
//
//        [self setVolume:glk_target_volume duration:duration notify:volume_notify];
//
//        if (!timer)
//        {
//           NSLog(@"restartInternal: failed to create volume change timer.");
//        }
//    }
//    return;
//}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {

    self = [super initWithCoder:decoder];

    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
}

@end
