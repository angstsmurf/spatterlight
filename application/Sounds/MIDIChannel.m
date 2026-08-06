#import "MIDIChannel.h"
#import "SoundHandler.h"
#import "MIDIPlayer.h"

@interface MIDIChannel () {

@private
    MIDIPlayer    *_player;        // The player instance
}

@end

@implementation MIDIChannel

- (BOOL)playSound:(glsi32)snd countOfRepeats:(glsi32)areps notification:(glui32)anot {
    self.status = GlkSoundChannelStatusSound;
    self.claimsNowPlaying = NO;
    self.nowPlayingDuration = 0;
    self.nowPlayingLooping = NO;

    NSData *dat = nil;
    GlkSoundBlorbFormatType type;

    /* stop previous noise */
    if (_player) {
        [_player stop];
    }

    if (areps == 0 || snd == -1) {
        [self.handler nowPlayingStateDidChange];
        return NO;
    }

    /* load sound resource into memory */
    type = [self.handler loadSoundResourceFromSound:snd data:&dat];

    if (type != GlkSoundBlorbFormatMIDI) {
        [self.handler nowPlayingStateDidChange];
        return NO;
    }

    notify = anot;
    resid = snd;
    loop = areps;

    _player = [[MIDIPlayer alloc] initWithData:dat];

    [_player setVolume:volume];

    BOOL looping = (areps == -1);
    self.nowPlayingDuration = _player.duration;
    self.nowPlayingLooping = looping;
    self.claimsNowPlaying = looping || self.nowPlayingDuration > 5.0;

    if (areps != -1) {
        MIDIChannel __weak *weakSelf = self;
        SoundHandler *blockHandler = self.handler;
        glui32 blocknotify = notify;
        glsi32 blockresid = resid;
        [_player addCallback:(^(void){
            dispatch_async(dispatch_get_main_queue(), ^{
                MIDIChannel *strongSelf = weakSelf;
                if (strongSelf && --strongSelf->loop < 1) {
                    strongSelf.status = GlkSoundChannelStatusIdle;
                    strongSelf.claimsNowPlaying = NO;
                    strongSelf.nowPlayingDuration = 0;
                    strongSelf.nowPlayingLooping = NO;
                    [blockHandler nowPlayingStateDidChange];
                    if (blocknotify)
                        [blockHandler handleSoundNotification:blocknotify withSound:blockresid];
                }
            });
        })];
    }

    [_player loop:areps];

    if (!paused)
        [_player play];

    [self.handler nowPlayingStateDidChange];
    return YES;
}

- (void)stop {
    paused = NO;
    if (_player) {
        [_player stop];
    }
    [self cleanup];
    [self.handler nowPlayingStateDidChange];
}

- (void)pause {
    paused = YES;
    [self.handler nowPlayingStateDidChange];
    if (_player)
        [_player pause];
}

- (void)unpause {
    paused = NO;
    if (!_player) {
        BOOL result = [self playSound:resid countOfRepeats:loop notification:notify];
        if (!result)
            NSLog(@"MIDIChannel: Failed to unpause sound %d", resid);
    } else {
        [_player play];
        [self.handler nowPlayingStateDidChange];
    }
}

- (void)setVolume {
    if (!_player)
        return;
    [_player setVolume:volume];
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

@end
