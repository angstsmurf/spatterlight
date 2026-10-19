#import "GlkSoundChannel.h"
#import "SoundHandler.h"
#import "SFBSoundBackend.h"
#import "MIDISoundBackend.h"

static float FloatVolumeClamped(float vol) {
    return vol < 0 ? 0 : (vol > 1 ? 1 : vol);
}

/* Glk allows volumes above 0x10000; the output cannot amplify, so they play
   at full volume. */
static float FloatVolumeFromGlk(glui32 glk_volume) {
    return FloatVolumeClamped((float)glk_volume / GLK_MAXVOLUME);
}

static glui32 GlkVolumeFromFloat(float vol) {
    return (glui32)lroundf(FloatVolumeClamped(vol) * GLK_MAXVOLUME);
}

@interface GlkSoundChannel () {
    glsi32 loop;
    glui32 notify;
    BOOL paused;

    glsi32 resid; /* for notifies */

    /* for volume fades */
    glui32 volume_notify;
    glui32 volume_timeout; /* ticks left */
    NSTimeInterval volume_tick; /* seconds per tick */
    float target_volume;
    float volume;
    float volume_delta;
    NSTimer *timer;

    id<GlkSoundBackend> _backend;
    /* Where a deserialized channel resumes, in the units of the backend that
       plays the sound: frames, or for MIDI, beats. */
    double _resumePosition;
    BOOL _soundIsMIDI;
}
@end

@implementation GlkSoundChannel

- (instancetype)initWithHandler:(SoundHandler*)handler name:(int)channelname volume:(glui32)vol {
    if (self = [super init]) {
        _handler = handler;
        _name = channelname;

        _status = GlkSoundChannelStatusIdle;
        volume = FloatVolumeFromGlk(vol);
        resid = -1;
    }

    return self;
}

- (BOOL)playSound:(glsi32)snd countOfRepeats:(glsi32)areps notification:(glui32)anot {
    BOOL result = [self playSoundInternal:snd countOfRepeats:areps notification:anot];
    [_handler nowPlayingStateDidChange];
    return result;
}

- (BOOL)playSoundInternal:(glsi32)snd countOfRepeats:(glsi32)areps notification:(glui32)anot {
    /* Idle until the new sound is actually under way: every early return below
       leaves nothing playing. */
    _status = GlkSoundChannelStatusIdle;
    _nowPlayingDuration = 0;
    _nowPlayingLooping = NO;

    /* stop previous noise */
    [_backend stop];

    if (areps == 0 || snd == -1)
        return NO;

    /* load sound resource into memory */
    NSData *buf = nil;
    GlkSoundBlorbFormatType type = [_handler loadSoundResourceFromSound:snd data:&buf];

    /* A resume point belongs to the sound it was taken from, and to nothing else. */
    if (snd != resid)
        _resumePosition = 0;

    notify = anot;
    resid = snd;
    loop = areps;

    if (type == GlkSoundBlorbFormatNone || !buf)
        return NO;

    _soundIsMIDI = (type == GlkSoundBlorbFormatMIDI);
    Class backendClass = _soundIsMIDI ? [MIDISoundBackend class] : [SFBSoundBackend class];
    if (![_backend isKindOfClass:backendClass])
        _backend = [backendClass new];

    glui32 blocknotify = notify;
    glsi32 blockresid = resid;
    GlkSoundChannel __weak *weakSelf = self;
    void (^finished)(void) = ^{
        GlkSoundChannel *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        strongSelf.status = GlkSoundChannelStatusIdle;
        [strongSelf.handler nowPlayingStateDidChange];
        if (blocknotify)
            [strongSelf.handler handleSoundNotification:blocknotify withSound:blockresid];
    };

    /* Picking up where an autosave left off, if it did. Hand the position over
       either way: a later replay of this sound is a fresh start. */
    double position = _resumePosition;
    _resumePosition = 0;

    if (![_backend playData:buf format:type repeats:areps position:position paused:paused volume:volume finished:finished]) {
        NSLog(@"GlkSoundChannel: Could not play sound %d", snd);
        return NO;
    }

    _nowPlayingDuration = _backend.duration;
    _nowPlayingLooping = (areps == -1);
    _status = GlkSoundChannelStatusSound;
    return YES;
}

- (void)dealloc {
    [timer invalidate];
    [_backend stop];
}

- (BOOL)claimsNowPlaying {
    return _status != GlkSoundChannelStatusIdle &&
           (_nowPlayingLooping || _nowPlayingDuration > 5.0);
}

- (BOOL)isPaused {
    return paused;
}

- (void)stop {
    paused = NO;
    [_backend stop];
    _resumePosition = 0;
    [self cleanup];
    [_handler nowPlayingStateDidChange];
}

- (void)pause {
    paused = YES;
    // Publish paused state before stopping output so macOS does not treat
    // the ensuing silence as a full stop / session teardown.
    [_handler nowPlayingStateDidChange];
    [_backend pause];
}

- (void)unpause {
    /* Unpausing a channel that is not paused does nothing. Without this, the
       restart below would play a sound that had run to its end all over again. */
    if (!paused)
        return;
    paused = NO;
    /* Nor is there anything to resume on a channel that was paused with no
       sound in it, or whose sound has since run to its end. */
    if (_status == GlkSoundChannelStatusIdle)
        return;
    if ([_backend resume]) {
        [_handler nowPlayingStateDidChange];
        return;
    }
    /* Nothing live to carry on with: play the sound again, from wherever the
       backend had got to with it. */
    if (_backend) {
        _resumePosition = _backend.resumePosition;
        loop = _backend.resumeRepeats;
    }
    if (![self playSound:resid countOfRepeats:loop notification:notify])
        NSLog(@"GlkSoundChannel: Failed to unpause sound %d", resid);
}

- (void)cleanup {
    _status = GlkSoundChannelStatusIdle;
    if (timer)
        [timer invalidate];
    timer = nil;
    resid = -1;
}

/** Start a fade timer */
- (void)init_fade:(glui32)glk_volume duration:(glui32)duration notify:(glui32)notification {
    volume_notify = notification;
    target_volume = FloatVolumeFromGlk(glk_volume);

    volume_delta = (target_volume - volume) / (float)FADE_GRANULARITY;

    volume_timeout = FADE_GRANULARITY;
    volume_tick = ceil(duration / (NSTimeInterval)FADE_GRANULARITY) / 1000.0;

    if (timer)
        [timer invalidate];

    timer = [NSTimer scheduledTimerWithTimeInterval:volume_tick target:self selector:@selector(volumeTimerCallback:) userInfo:nil repeats:YES];
}

/* What is left of a fade in progress, in milliseconds. Autosaves made before the
   tick length was archived do not know it; give those the 10 ms tick of a one
   second fade. */
- (glui32)remainingFadeDuration {
    NSTimeInterval tick = volume_tick > 0 ? volume_tick : 0.01;
    return (glui32)lround(volume_timeout * tick * 1000.0);
}

- (void)volumeTimerCallback:(NSTimer *)aTimer {
    volume = FloatVolumeClamped(volume + volume_delta);

    volume_timeout--;

    /* If the timer has fired FADE_GRANULARITY times, kill it */
    if (volume_timeout <= 0) {
        if (volume_notify) {
            glui32 notification = volume_notify;
            SoundHandler *handler = _handler;
            dispatch_async(dispatch_get_main_queue(), ^{
                [handler handleVolumeNotification:notification];
            });
        }

        if (!timer) {
            NSLog(@"volumeTimerCallback: invalid timer.");
            timer = aTimer;
        }
        [timer invalidate];
        timer = nil;

        if (volume != target_volume) {
            volume = target_volume;
        }
    }

    [self setVolume];
}

- (void) setVolume:(glui32)glk_volume duration:(glui32)duration notification:(glui32)notification {
    if (!duration) {
        volume = FloatVolumeFromGlk(glk_volume);
        [self setVolume];
    } else {
        [self init_fade:glk_volume duration:duration notify:notification];
    }
}

- (void)setVolume {
    [_backend setVolume:volume];
}


/* Restart the sound channel after a deserialize, and also any fade timer.
 The archived playback position is picked up by -playSoundInternal:, so a sound
 resumes where it was when the game was autosaved or killed rather than starting
 over. A fade in progress resumes too, except that its remaining time counts from
 the last glk_select and not from when the process was actually terminated.
 */
- (void)restartInternal {

    if (_status == GlkSoundChannelStatusIdle) {
        return;
    }

    if (paused) {
        [self pause];
    }

    BOOL result = [self playSound:resid countOfRepeats:loop notification:notify];
    if (!result)
        NSLog(@"restartInternal: failed to restart sound %d.", resid);

    if (volume_timeout > 0) {
        [self setVolume:GlkVolumeFromFloat(target_volume) duration:[self remainingFadeDuration] notification:volume_notify];

        if (!timer) {
            NSLog(@"restartInternal: failed to create volume change timer.");
        }
    }
    return;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

/* Archived under the name of the class that played MIDI before there were
   backends, so that an older build reading the autosave picks the player the
   sound needs. */
- (Class)classForKeyedArchiver {
    return _soundIsMIDI ? [MIDIChannel class] : [GlkSoundChannel class];
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];

    if (self) {
        _name = [decoder decodeIntForKey:@"name"];

        resid = (glsi32)[decoder decodeIntForKey:@"resid"]; /* for notifies */
        _status = (GlkSoundChannelStatus)[decoder decodeIntegerForKey:@"status"];
        volume = (float)[decoder decodeDoubleForKey:@"volume"];
        loop = [decoder decodeIntForKey:@"loop"];
        notify = (glui32)[decoder decodeIntForKey:@"notify"];
        paused = ([decoder decodeIntForKey:@"paused"] != 0);

        /* for volume fades */
        volume_notify = (glui32)[decoder decodeInt32ForKey:@"volume_notify"];
        volume_timeout = (glui32)[decoder decodeIntForKey:@"volume_timeout"];
        volume_tick = [decoder decodeDoubleForKey:@"volume_tick"];
        target_volume = (float)[decoder decodeDoubleForKey:@"target_volume"];
        volume_delta = (float)[decoder decodeDoubleForKey:@"volume_delta"];

        /* Only a channel playing MIDI has a MIDI position, in beats; any other
           sound's position is a frame. */
        _soundIsMIDI = [decoder containsValueForKey:@"midiPosition"];
        if (_soundIsMIDI)
            _resumePosition = [decoder decodeDoubleForKey:@"midiPosition"];
        else
            _resumePosition = (double)[decoder decodeInt64ForKey:@"frame"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    /* How far the sound has played, so a restart can resume rather than begin
       again, and the repeat count that goes with that position. A channel that
       has not been restarted since it was deserialized still holds what it was
       deserialized with. */
    double position = 0;
    glsi32 repeats = loop;
    if (_status != GlkSoundChannelStatusIdle) {
        position = _backend ? _backend.resumePosition : _resumePosition;
        if (_backend)
            repeats = _backend.resumeRepeats;
    }

    [encoder encodeInt:_name forKey:@"name"];
    [encoder encodeInt:resid forKey:@"resid"];
    [encoder encodeInteger:_status forKey:@"status"];
    [encoder encodeDouble:volume forKey:@"volume"];
    [encoder encodeInt:repeats forKey:@"loop"];
    [encoder encodeInt:(int)notify forKey:@"notify"];
    [encoder encodeInt:(int)paused forKey:@"paused"];

    /* for volume fades */
    [encoder encodeInt:(int)volume_notify forKey:@"volume_notify"];
    [encoder encodeInt:(int)volume_timeout forKey:@"volume_timeout"];
    [encoder encodeDouble:volume_tick forKey:@"volume_tick"];
    [encoder encodeDouble:target_volume forKey:@"target_volume"];
    [encoder encodeDouble:volume_delta forKey:@"volume_delta"];

    if (_soundIsMIDI) {
        [encoder encodeInt64:0 forKey:@"frame"];
        [encoder encodeDouble:position forKey:@"midiPosition"];
    } else {
        [encoder encodeInt64:(int64_t)position forKey:@"frame"];
    }
}

@end

@implementation MIDIChannel

/* Secure decoding refuses a subclass that inherits its archiver class. A
   MIDIChannel only ever comes out of an archive, and goes back in as whatever
   it has played since. */
- (Class)classForKeyedArchiver {
    return [super classForKeyedArchiver];
}

@end
