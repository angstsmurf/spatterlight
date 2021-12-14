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

    NSData *dat = nil;
    GlkSoundBlorbFormatType type;

    /* stop previous noise */
    if (_player) {
        [_player stop];
    }

    if (areps == 0 || snd == -1)
        return NO;

    /* load sound resource into memory */
    type = [self.handler loadSoundResourceFromSound:snd data:&dat];

    if (type != GlkSoundBlorbFormatMIDI)
        return NO;

    notify = anot;
    resid = snd;
    loop = areps;

    _player = [[MIDIPlayer alloc] initWithData:dat];

    [_player setVolume:volume];

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
                    if (blocknotify)
                        [blockHandler handleSoundNotification:blocknotify withSound:blockresid];
                }
            });
        })];
    }

    [_player loop:areps];

    if (!paused)
        [_player play];
    return YES;
}

- (void)stop {
    paused = NO;
    if (_player) {
        [_player stop];
    }
    [self cleanup];
}

- (void)pause {
    paused = YES;
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
