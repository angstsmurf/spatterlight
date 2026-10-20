#import "GlkSoundChannel.h"
#import "SoundHandler.h"
#import "SFBSoundBackend.h"
#import "MIDISoundBackend.h"

/* A fade is this many volume steps, however long it lasts. */
static const glui32 kFadeSteps = 100;
static const glui32 kGlkMaxVolume = 0x10000;

static float FloatVolumeClamped(float vol) {
    return vol < 0 ? 0 : (vol > 1 ? 1 : vol);
}

/* Glk allows volumes above 0x10000; the output cannot amplify, so they play
   at full volume. */
static float FloatVolumeFromGlk(glui32 glk_volume) {
    return FloatVolumeClamped((float)glk_volume / kGlkMaxVolume);
}

static glui32 GlkVolumeFromFloat(float vol) {
    return (glui32)lroundf(FloatVolumeClamped(vol) * kGlkMaxVolume);
}

@interface GlkSoundChannel () {
    glsi32 loop;
    glui32 notify;
    BOOL paused;

    glsi32 resid; /* for notifies */

    float volume;

    /* A fade in progress: fadeStepsLeft is 0 when there is none. */
    glui32 fadeNotify;
    glui32 fadeStepsLeft;
    NSTimeInterval fadeStepInterval; /* seconds */
    float fadeTarget;
    float fadeStep;
    /* Fires on the main queue, and holds the channel weakly: a channel that
       goes away cancels it in -dealloc. */
    dispatch_source_t fadeTimer;

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
    _playing = NO;
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
        strongSelf.playing = NO;
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
    _playing = YES;
    return YES;
}

- (void)dealloc {
    [self cancelFadeTimer];
    [_backend stop];
}

- (BOOL)claimsNowPlaying {
    return _playing && (_nowPlayingLooping || _nowPlayingDuration > 5.0);
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
    if (!_playing)
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
    _playing = NO;
    [self cancelFadeTimer];
    resid = -1;
}

- (void)cancelFadeTimer {
    if (fadeTimer)
        dispatch_source_cancel(fadeTimer);
    fadeTimer = nil;
}

- (void)startFadeTo:(glui32)glk_volume duration:(glui32)duration notify:(glui32)notification {
    fadeNotify = notification;
    fadeTarget = FloatVolumeFromGlk(glk_volume);
    fadeStep = (fadeTarget - volume) / (float)kFadeSteps;
    fadeStepsLeft = kFadeSteps;
    fadeStepInterval = ceil(duration / (NSTimeInterval)kFadeSteps) / 1000.0;

    [self cancelFadeTimer];
    fadeTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    uint64_t interval = (uint64_t)(fadeStepInterval * NSEC_PER_SEC);
    dispatch_source_set_timer(fadeTimer, dispatch_time(DISPATCH_TIME_NOW, (int64_t)interval), interval, NSEC_PER_MSEC);
    GlkSoundChannel __weak *weakSelf = self;
    dispatch_source_set_event_handler(fadeTimer, ^{
        [weakSelf fadeOneStep];
    });
    dispatch_resume(fadeTimer);
}

/* What is left of a fade in progress, in milliseconds. Autosaves made before the
   step length was archived do not know it; give those the 10 ms step of a one
   second fade. */
- (glui32)remainingFadeDuration {
    NSTimeInterval interval = fadeStepInterval > 0 ? fadeStepInterval : 0.01;
    return (glui32)lround(fadeStepsLeft * interval * 1000.0);
}

- (void)fadeOneStep {
    if (fadeStepsLeft == 0)
        return;
    volume = FloatVolumeClamped(volume + fadeStep);
    fadeStepsLeft--;

    if (fadeStepsLeft == 0) {
        [self cancelFadeTimer];
        volume = fadeTarget;
        if (fadeNotify) {
            glui32 notification = fadeNotify;
            SoundHandler *handler = _handler;
            dispatch_async(dispatch_get_main_queue(), ^{
                [handler handleVolumeNotification:notification];
            });
        }
    }

    [self setVolume];
}

- (void) setVolume:(glui32)glk_volume duration:(glui32)duration notification:(glui32)notification {
    if (!duration) {
        volume = FloatVolumeFromGlk(glk_volume);
        [self setVolume];
    } else {
        [self startFadeTo:glk_volume duration:duration notify:notification];
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

    if (!_playing)
        return;

    if (paused) {
        [self pause];
    }

    BOOL result = [self playSound:resid countOfRepeats:loop notification:notify];
    if (!result)
        NSLog(@"restartInternal: failed to restart sound %d.", resid);

    if (fadeStepsLeft > 0)
        [self setVolume:GlkVolumeFromFloat(fadeTarget) duration:[self remainingFadeDuration] notification:fadeNotify];
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
        /* The Glk values are 32-bit. Older autosaves wrote some of them with
           -encodeInt: or -encodeInteger:, which a keyed archive stores the
           same way, so they read back alike. */
        _name = [decoder decodeInt32ForKey:@"name"];
        resid = [decoder decodeInt32ForKey:@"resid"]; /* for notifies */
        /* Archived as the old GlkSoundChannelStatus: 0 idle, 1 sound. */
        _playing = ([decoder decodeIntegerForKey:@"status"] != 0);
        volume = (float)[decoder decodeDoubleForKey:@"volume"];
        loop = [decoder decodeInt32ForKey:@"loop"];
        notify = (glui32)[decoder decodeInt32ForKey:@"notify"];
        paused = ([decoder decodeInt32ForKey:@"paused"] != 0);

        /* for volume fades */
        fadeNotify = (glui32)[decoder decodeInt32ForKey:@"volume_notify"];
        fadeStepsLeft = (glui32)[decoder decodeInt32ForKey:@"volume_timeout"];
        fadeStepInterval = [decoder decodeDoubleForKey:@"volume_tick"];
        fadeTarget = (float)[decoder decodeDoubleForKey:@"target_volume"];

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
    if (_playing) {
        position = _backend ? _backend.resumePosition : _resumePosition;
        if (_backend)
            repeats = _backend.resumeRepeats;
    }

    [encoder encodeInt32:_name forKey:@"name"];
    [encoder encodeInt32:resid forKey:@"resid"];
    [encoder encodeInteger:_playing ? 1 : 0 forKey:@"status"];
    [encoder encodeDouble:volume forKey:@"volume"];
    [encoder encodeInt32:repeats forKey:@"loop"];
    [encoder encodeInt32:(int32_t)notify forKey:@"notify"];
    [encoder encodeInt32:paused ? 1 : 0 forKey:@"paused"];

    /* for volume fades, under the names of the old ivars */
    [encoder encodeInt32:(int32_t)fadeNotify forKey:@"volume_notify"];
    [encoder encodeInt32:(int32_t)fadeStepsLeft forKey:@"volume_timeout"];
    [encoder encodeDouble:fadeStepInterval forKey:@"volume_tick"];
    [encoder encodeDouble:fadeTarget forKey:@"target_volume"];

    if (_soundIsMIDI)
        [encoder encodeDouble:position forKey:@"midiPosition"];
    else
        [encoder encodeInt64:(int64_t)position forKey:@"frame"];
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
