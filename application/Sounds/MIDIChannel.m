#import "MIDIChannel.h"
#import "SoundHandler.h"
#import "MIDIPlayer.h"

@interface MIDIChannel () {

@private
    MIDIPlayer    *_player;        // The player instance
    double _pendingPosition;       // Where a deserialized channel resumes, in beats
}

@end

@implementation MIDIChannel

- (BOOL)playSoundInternal:(glsi32)snd countOfRepeats:(glsi32)areps notification:(glui32)anot {
    self.status = GlkSoundChannelStatusSound;
    self.nowPlayingDuration = 0;
    self.nowPlayingLooping = NO;

    NSData *dat = nil;
    GlkSoundBlorbFormatType type;

    /* stop previous noise. -stop leaves the player unusable, so let go of it. */
    if (_player) {
        [_player stop];
        _player = nil;
    }

    if (areps == 0 || snd == -1)
        return NO;

    /* load sound resource into memory */
    type = [self.handler loadSoundResourceFromSound:snd data:&dat];

    if (type != GlkSoundBlorbFormatMIDI)
        return NO;

    /* A resume point belongs to the sound it was taken from, and to nothing else. */
    if (snd != resid)
        _pendingPosition = 0;

    notify = anot;
    resid = snd;
    loop = areps;

    _player = [[MIDIPlayer alloc] initWithData:dat];

    [_player setVolume:volume];

    self.nowPlayingDuration = _player.duration;
    self.nowPlayingLooping = (areps == -1);

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
                    [blockHandler nowPlayingStateDidChange];
                    if (blocknotify)
                        [blockHandler handleSoundNotification:blocknotify withSound:blockresid];
                }
            });
        })];
    }

    [_player loop:areps];

    /* Picking up where an autosave left off. Consume the position either way: a
       later replay of this sound is a fresh start, not a resume. */
    if (_pendingPosition > 0) {
        [_player seekToPosition:_pendingPosition];
        _pendingPosition = 0;
    }

    if (!paused)
        [_player play];

    return YES;
}

- (void)stop {
    paused = NO;
    if (_player) {
        [_player stop];
        _player = nil;
    }
    _pendingPosition = 0;
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

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _pendingPosition = [decoder decodeDoubleForKey:@"midiPosition"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    /* Where to resume, for a channel that has somewhere to resume to. With no
       player there is a restored position that has not been handed to one yet. */
    double position = 0;
    if (self.status != GlkSoundChannelStatusIdle)
        position = _player ? _player.position : _pendingPosition;
    [encoder encodeDouble:position forKey:@"midiPosition"];
}

@end
