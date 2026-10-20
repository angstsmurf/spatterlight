//
//  MIDISoundBackend.m
//  Spatterlight
//

#import "MIDISoundBackend.h"
#import "MIDIPlayer.h"

@interface MIDISoundBackend () {
    MIDIPlayer *_midiPlayer;
    /* Passes still to play. MIDIPlayer calls back at the end of every pass. */
    glsi32 _repeats;
}
@end

@implementation MIDISoundBackend

- (BOOL)playData:(NSData *)data
          format:(GlkSoundBlorbFormatType)format
         repeats:(glsi32)repeats
        position:(double)position
          paused:(BOOL)paused
          volume:(float)volume
        finished:(void (^)(void))finished {
    [self stop];
    _repeats = repeats;
    if (format != GlkSoundBlorbFormatMIDI)
        return NO;

    _midiPlayer = [[MIDIPlayer alloc] initWithData:data];
    if (!_midiPlayer)
        return NO;
    [_midiPlayer setVolume:volume];

    if (repeats != -1) {
        MIDISoundBackend __weak *weakSelf = self;
        MIDIPlayer __weak *weakPlayer = _midiPlayer;
        void (^block)(void) = [finished copy];
        [_midiPlayer addCallback:^{
            dispatch_async(dispatch_get_main_queue(), ^{
                /* A pass that ended just before the sound was stopped or
                   replaced has nothing more to count. */
                MIDISoundBackend *strongSelf = weakSelf;
                if (!strongSelf || !weakPlayer || strongSelf->_midiPlayer != weakPlayer)
                    return;
                if (--strongSelf->_repeats < 1 && block)
                    block();
            });
        }];
    }

    [_midiPlayer loop:repeats];

    if (position > 0)
        [_midiPlayer seekToPosition:position];

    if (!paused && ![_midiPlayer play]) {
        [self stop];
        return NO;
    }
    return YES;
}

/* -[MIDIPlayer stop] leaves the player unusable, so let go of it. */
- (void)stop {
    [_midiPlayer stop];
    _midiPlayer = nil;
}

- (void)pause {
    [_midiPlayer pause];
}

- (BOOL)resume {
    return [_midiPlayer play];
}

- (void)setVolume:(float)volume {
    [_midiPlayer setVolume:volume];
}

- (NSTimeInterval)duration {
    return _midiPlayer.duration;
}

- (double)resumePosition {
    return _midiPlayer.position;
}

- (glsi32)resumeRepeats {
    return _repeats;
}

@end
