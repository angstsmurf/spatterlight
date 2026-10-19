#import "MIDIChannel.h"
#import "SoundHandler.h"
#import "MIDIPlayer.h"

@interface MIDIChannel () {

@private
    MIDIPlayer *_midiPlayer;	   // The player instance
    double _pendingPosition;       // Where a deserialized channel resumes, in beats
}

@end

@implementation MIDIChannel

- (BOOL)playSoundInternal:(glsi32)snd countOfRepeats:(glsi32)areps notification:(glui32)anot {
    /* Idle until the new sound is actually under way, as in GlkSoundChannel. */
    self.status = GlkSoundChannelStatusIdle;
    self.nowPlayingDuration = 0;
    self.nowPlayingLooping = NO;

    NSData *dat = nil;
    GlkSoundBlorbFormatType type;

    /* stop previous noise. -stop leaves the player unusable, so let go of it. */
    if (_midiPlayer) {
        [_midiPlayer stop];
        _midiPlayer = nil;
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

    _midiPlayer = [[MIDIPlayer alloc] initWithData:dat];

    [_midiPlayer setVolume:volume];

    self.nowPlayingDuration = _midiPlayer.duration;
    self.nowPlayingLooping = (areps == -1);

    if (areps != -1) {
        MIDIChannel __weak *weakSelf = self;
        glui32 blocknotify = notify;
        glsi32 blockresid = resid;
        [_midiPlayer addCallback:(^(void){
            dispatch_async(dispatch_get_main_queue(), ^{
                MIDIChannel *strongSelf = weakSelf;
                if (strongSelf && --strongSelf->loop < 1) {
                    strongSelf.status = GlkSoundChannelStatusIdle;
                    [strongSelf.handler nowPlayingStateDidChange];
                    if (blocknotify)
                        [strongSelf.handler handleSoundNotification:blocknotify withSound:blockresid];
                }
            });
        })];
    }

    [_midiPlayer loop:areps];

    /* Picking up where an autosave left off. Consume the position either way: a
       later replay of this sound is a fresh start, not a resume. */
    if (_pendingPosition > 0) {
        [_midiPlayer seekToPosition:_pendingPosition];
        _pendingPosition = 0;
    }

    if (!paused)
        [_midiPlayer play];

    self.status = GlkSoundChannelStatusSound;
    return YES;
}

- (void)stop {
    paused = NO;
    if (_midiPlayer) {
        [_midiPlayer stop];
        _midiPlayer = nil;
    }
    _pendingPosition = 0;
    [self cleanup];
    [self.handler nowPlayingStateDidChange];
}

- (void)pause {
    paused = YES;
    [self.handler nowPlayingStateDidChange];
    if (_midiPlayer)
        [_midiPlayer pause];
}

- (void)unpause {
    // As in GlkSoundChannel: a channel that is not paused is left alone.
    if (!paused)
        return;
    paused = NO;
    if (self.status == GlkSoundChannelStatusIdle)
        return;
    if (!_midiPlayer) {
        BOOL result = [self playSound:resid countOfRepeats:loop notification:notify];
        if (!result)
            NSLog(@"MIDIChannel: Failed to unpause sound %d", resid);
    } else {
        [_midiPlayer play];
        [self.handler nowPlayingStateDidChange];
    }
}

- (void)setVolume {
    if (!_midiPlayer)
        return;
    [_midiPlayer setVolume:volume];
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
        position = _midiPlayer ? _midiPlayer.position : _pendingPosition;
    [encoder encodeDouble:position forKey:@"midiPosition"];
}

@end
