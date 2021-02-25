#import "main.h"
#import "GlkSoundChannel.h"
#import "AudioResourceHandler.h"

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#define FADE_GRANULARITY 100
#define GLK_MAXVOLUME 0x10000

#define GLK_VOLUME_TO_SDL_VOLUME(x) ((x) < GLK_MAXVOLUME ? (round(pow(((double)x) / GLK_MAXVOLUME, log(4)) * MIX_MAX_VOLUME)) : (MIX_MAX_VOLUME))

enum { CHANNEL_IDLE, CHANNEL_SOUND, CHANNEL_MUSIC };

@interface GlkSoundChannel () {
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
    NSMutableDictionary <NSNumber *, SoundResource *> *resources;
}
@end

@implementation GlkSoundChannel

static void *glk_controller;

- (instancetype)initWithGlkController:(GlkController*)glkctl name:(NSUInteger)channelname volume:(NSUInteger)vol
{
    _glkctl = glkctl;
    
    sound = nil;
    volume = vol;
    loop = 0;
    notify = 0;
    _name = channelname;
    
    status = CHANNEL_IDLE;
    volume = (NSUInteger)GLK_VOLUME_TO_SDL_VOLUME(vol);
    resid = -1;
    loop = 0;
    notify = 0;
    sdl_memory = 0;
    sdl_rwops = 0;
    sample = 0;
    paused = FALSE;
    sdl_channel = -1;
    music = 0;

    volume_notify = 0;
    volume_timeout = 0;
    target_volume = 0;
    float_volume = 0;
    volume_delta = 0;
    timer = nil;
    
    [self postInit];
    return self;
}

- (void)postInit {
    GlkController *glkctl = _glkctl;
    _handler = glkctl.audioResourceHandler;
    AudioResourceHandler *handler = _handler;
    if (!handler.sound_channels)
        handler.sound_channels = [[NSMutableDictionary alloc] init];
    sound_channels = handler.sound_channels;
    resources = handler.resources;
    glk_controller = (__bridge void *)glkctl;
}


- (void)play:(NSInteger)snd repeats:(NSInteger)areps notify:(NSInteger)anot
{
    size_t len = 0;
    NSInteger type;
    NSUInteger result = 0;
    NSUInteger temppaused;
    char *buf = nil;
    
    /* store paused state of channel */
    temppaused = paused;
    
    /* stop previous noise */
    [self stop];
    
    if (areps == 0)
        return;
    
    /* load sound resource into memory */
    type = [_glkctl.audioResourceHandler load_sound_resource:snd length:&len data:&buf];
    
    sdl_memory = (unsigned char*)buf;
    sdl_rwops = SDL_RWFromConstMem(buf, (int)len);
    notify = anot;
    resid = snd;
    loop = areps;
    
    switch (type)
    {
        case giblorb_ID_FORM:
        case giblorb_ID_AIFF:
        case giblorb_ID_WAVE:
        case giblorb_ID_OGG:
        case giblorb_ID_MP3:
            result = [self play_sound];
            break;
            
        case giblorb_ID_MOD:
        case giblorb_ID_MIDI:
            result = [self play_mod:len];
            break;
            
        default:
            NSLog(@"schannel_play_ext: unknown resource type (%ld).", type);
    }
    
    /* if channel was paused it should be paused again */
    if (result && temppaused)
    {
        [self pause];
    }
}

- (void) stop
{
    SDL_LockAudio();
    [self unpause];
    SDL_UnlockAudio();
    switch (status)
    {
        case CHANNEL_SOUND:
            notify = 0;
            Mix_HaltChannel(sdl_channel);
            break;
        case CHANNEL_MUSIC:
            if (_handler.music_channel == self)
            {
                Mix_HookMusicFinished(NULL);
            }
            Mix_HaltMusic();
            break;
    }
    SDL_LockAudio();
    [self cleanup];
    SDL_UnlockAudio();
}

- (void) pause
{
    switch (status)
    {
        case CHANNEL_SOUND:
            Mix_Pause(sdl_channel);
            break;
        case CHANNEL_MUSIC:
            Mix_PauseMusic();
            break;
    }
    
    paused = YES;
}

- (void) unpause
{
    switch (status)
    {
        case CHANNEL_SOUND:
            Mix_Resume(sdl_channel);
            break;
        case CHANNEL_MUSIC:
            Mix_ResumeMusic();
            break;
    }
    
    paused = 0;
}

- (void)cleanup
{
    if (sdl_rwops)
    {
        SDL_FreeRW(sdl_rwops);
        sdl_rwops = nil;
    }
    if (sdl_memory)
    {
        sdl_memory = nil;
    }
    switch (status)
    {
        case CHANNEL_SOUND:
            if (sample)
                Mix_FreeChunk(sample);
            if (sdl_channel >= 0)
            {
                Mix_GroupChannel(sdl_channel, FREE);
                [sound_channels removeObjectForKey:@(sdl_channel)];
            }
            break;
        case CHANNEL_MUSIC:
            if (music)
            {
                Mix_FreeMusic(music);
                _handler.music_channel = nil;
            }
            break;
    }
    status = CHANNEL_IDLE;
    sdl_channel = -1;
    music = 0;
    
    if (timer)
        [timer invalidate];
    timer = nil;
    resid = -1;
}

/** Start a fade timer */
- (void)init_fade:(NSUInteger)glk_volume duration:(NSUInteger)duration notify:(NSInteger)notification
{

    volume_notify = notification;
    target_volume = (NSUInteger)GLK_VOLUME_TO_SDL_VOLUME(glk_volume);
    
    float_volume = (CGFloat)volume;

    volume_delta = (NSInteger)(target_volume - volume) / (CGFloat)FADE_GRANULARITY;

    volume_timeout = FADE_GRANULARITY;
    
    if (timer)
        [timer invalidate];

    timer = [NSTimer scheduledTimerWithTimeInterval:(duration / FADE_GRANULARITY) / 1000.0 target:self selector:@selector(volumeTimerCallback:) userInfo:nil repeats:YES];
}

- (void)volumeTimerCallback:(NSTimer *)aTimer {
    float_volume += volume_delta;

    if (float_volume < 0)
        float_volume = 0;

    if ((NSUInteger)float_volume != volume)
    {
        volume = (NSUInteger)float_volume;

        if (status == CHANNEL_SOUND)
            Mix_Volume(sdl_channel, volume);
        else if (status == CHANNEL_MUSIC)
            Mix_VolumeMusic(volume);
    }

    volume_timeout--;

    /* If the timer has fired FADE_GRANULARITY times, kill it */
    if (volume_timeout <= 0)
    {
        if (volume_notify)
        {
            NSInteger notification = volume_notify;
            GlkController *glkController = _glkctl;
            dispatch_async(dispatch_get_main_queue(), ^{
                [glkController handleVolumeNotification:notification];
            });
        }

        if (!timer)
        {
            NSLog(@"volumeTimerCallback: invalid timer.");
            timer = aTimer;
        }
        [timer invalidate];
        timer = nil;

        if (volume != target_volume)
        {
            volume = target_volume;
            if (status == CHANNEL_SOUND)
                Mix_Volume(sdl_channel, volume);
            else if (status == CHANNEL_MUSIC)
                Mix_VolumeMusic(volume);
        }
    }
}

- (void) setVolume:(NSUInteger)glk_volume duration:(NSUInteger)duration notify:(NSInteger)notification
{
    if (!duration)
    {
        volume = (NSUInteger)GLK_VOLUME_TO_SDL_VOLUME(glk_volume);

        switch (status)
        {
            case CHANNEL_IDLE:
                break;
            case CHANNEL_SOUND:
                Mix_Volume(sdl_channel, volume);
                break;
            case CHANNEL_MUSIC:
                Mix_VolumeMusic(volume);
                break;
        }
    }
    else
    {
        [self init_fade:glk_volume duration:duration notify:notification];
    }
}

/** Start a mod music channel */
-(NSUInteger)play_mod:(size_t)len
{
    int  music_busy;
    NSInteger repeats;
    
    music_busy = Mix_PlayingMusic();
    
    if (music_busy)
    {
        /* We already checked for music playing on *this* channel
         in gls_schannel_play_ext */
        
        NSLog(@"MOD player already in use on another channel!");
        return 0;
    }
    
    status = CHANNEL_MUSIC;
    /* The fscking mikmod lib want to read the mod only from disk! */
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *temporaryDirectoryURL = [NSURL fileURLWithPath: NSTemporaryDirectory()
                                              isDirectory: YES];

    NSString *temporaryFilename =
    [[NSProcessInfo processInfo] globallyUniqueString];

    NSURL *temporaryFileURL =
        [temporaryDirectoryURL
            URLByAppendingPathComponent:temporaryFilename];
    [fileManager createFileAtPath:temporaryFileURL.path contents:[NSData dataWithBytes:sdl_memory length:len] attributes:nil];

    music = Mix_LoadMUS(temporaryFileURL.path.UTF8String);

    if (music)
    {
        SDL_LockAudio();
        _handler.music_channel = self;
        SDL_UnlockAudio();
        Mix_VolumeMusic(volume);
        Mix_HookMusicFinished(&music_completion_callback);
        repeats = loop;
        if (repeats < -1)
            repeats = -1;
        if (Mix_PlayMusic(music, repeats) >= 0)
            return 1;
    }
    NSLog(@"play mod failed");
    NSLog(@"%s", Mix_GetError());
    SDL_LockAudio();
    [self cleanup];
    SDL_UnlockAudio();
    return 0;
}


/** Start a sound channel */
-(NSUInteger) play_sound
{
    int repeats;
    SDL_LockAudio();
    status = CHANNEL_SOUND;
    sdl_channel = Mix_GroupAvailable(FREE);
    Mix_GroupChannel(sdl_channel, BUSY);
    SDL_UnlockAudio();
    sample = Mix_LoadWAV_RW(sdl_rwops, FALSE);
    if (sample == 0)
        NSLog(@"play_sound: drats\n");
    if (sdl_channel < 0)
    {
        NSLog(@"No available sound channels");
    }
    if (sdl_channel >= 0 && sample)
    {
        SDL_LockAudio();
        sound_channels[@(sdl_channel)] = self;
        SDL_UnlockAudio();
        Mix_Volume(sdl_channel, volume);
        Mix_ChannelFinished(&sound_completion_callback);
        repeats = loop - 1;
        if (repeats < -1)
            repeats = -1;
        if (Mix_PlayChannel(sdl_channel, sample, repeats) >= 0)
            return 1;
    }
    NSLog(@"play sound failed");
    NSLog(@"%s", Mix_GetError());
    sdl_memory = 0;
    sdl_rwops = 0;
    SDL_LockAudio();
    [self cleanup];
    SDL_UnlockAudio();
    return 0;
}

/* Notify the sound channel completion */
static void sound_completion_callback(int chan)
{
    GlkController *controller;
    NSMutableDictionary *sound_channels;
    @try {
        controller = (__bridge GlkController *)glk_controller;
        sound_channels = controller.audioResourceHandler.sound_channels;
    } @catch (NSException *ex) {
        NSLog(@"sound_completion_callback failed: %@", ex);
        return;
    }

    GlkSoundChannel *sound_channel = sound_channels[@(chan)];

    if (!sound_channel)
    {
        NSLog(@"sound completion callback called with invalid channel");
        return;
    }
    
    if (sound_channel->notify)
    {
        NSInteger snd = sound_channel->resid;
        dispatch_async(dispatch_get_main_queue(), ^{
            [controller handleSoundNotification:sound_channel->notify withSound:snd];
        });
    }
    [sound_channel cleanup];
    [sound_channels removeObjectForKey:@(chan)];
    return;
}

/* Notify the music channel completion */
static void music_completion_callback()
{
    GlkController *controller;
    @try {
        controller = (__bridge GlkController *)glk_controller;
    } @catch (NSException *ex) {
        NSLog(@"music_completion_callback failed: %@", ex);
        return;
    }
    GlkSoundChannel *music_channel = controller.audioResourceHandler.music_channel;
    if (!music_channel || music_channel->resid < 0)
    {
        NSLog(@"music callback failed");
        return;
    }
    else
    {
        NSInteger snd = music_channel->resid;
        dispatch_async(dispatch_get_main_queue(), ^{
            [controller handleSoundNotification:music_channel->notify withSound:snd];
        });
    }
    [music_channel cleanup];
}

/* Restart the sound channel after a deserialize, and also any fade timer This
    primitive implementation disregards how much of the sound that had played
    when the game was autosaved or killed.
    Fade timers remember this, however, so a clip halfway through a 10
    second fade out will restart from the beginning but fade out in 10 seconds.
    Well, except that it counts from last glk_select and not from when the process
    was actually terminated.
 */
- (void)restartInternal {

    [self postInit];

    if (_handler.restored_music_channel_id == _name) {
        _handler.music_channel = self;
    }

    if (sdl_channel != -1)
        sound_channels[@(sdl_channel)] = self;
    
    if (status == CHANNEL_IDLE) {
        return;
    }

    if (paused == TRUE) {
        [self pause];
    }

    NSLog(@"RestartInternal: chan %lu (sdl channel %d): loop: %ld notify: %ld resid:%ld", (unsigned long)_name, sdl_channel, (long)loop, (long)notify, (long)resid);

    [self play:resid repeats:loop notify:notify];

    if (volume_timeout > 0) {

        NSUInteger duration = (volume_timeout * FADE_GRANULARITY);

        NSUInteger sdl_volume = target_volume;
        NSUInteger glk_target_volume = GLK_MAXVOLUME;

        // This ridiculous calculation seems to be necessary to convert the SDL volume back to the correct GLK volume.
        if (sdl_volume < SDL_MIX_MAXVOLUME)
           glk_target_volume = (NSUInteger)expf(logf((float)sdl_volume / SDL_MIX_MAXVOLUME) / logf(4)) * GLK_MAXVOLUME;

        [self setVolume:glk_target_volume duration:duration notify:volume_notify];

        if (!timer)
        {
           NSLog(@"restartInternal: failed to create volume change timer.");
        }
    }
    return;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {

    _name = (NSUInteger)[decoder decodeIntForKey:@"name"];

    resid =  [decoder decodeIntForKey:@"resid"]; /* for notifies */
    status =  [decoder decodeIntForKey:@"status"];
    volume = (NSUInteger)[decoder decodeIntForKey:@"volume"];
    loop =  [decoder decodeIntForKey:@"loop"];
    notify = [decoder decodeIntForKey:@"notify"];
    paused = (NSUInteger)[decoder decodeIntForKey:@"paused"];

    sdl_channel = [decoder decodeIntForKey:@"sdl_channel"];

    /* for volume fades */
    volume_notify = [decoder decodeInt32ForKey:@"volume_notify"];
    volume_timeout = (NSUInteger)[decoder decodeIntForKey:@"volume_timeout"];
    target_volume = (NSUInteger)[decoder decodeIntForKey:@"target_volume"];
    float_volume = [decoder decodeDoubleForKey:@"float_volume"];
    volume_delta = [decoder decodeDoubleForKey:@"volume_delta"];

    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {

    [encoder encodeInt:_name forKey:@"name"];
    [encoder encodeInt:resid forKey:@"resid"];
    [encoder encodeInt:status forKey:@"status"];
    [encoder encodeInt:volume forKey:@"volume"];
    [encoder encodeInt:loop forKey:@"loop"];
    [encoder encodeInt:notify forKey:@"notify"];
    [encoder encodeInt:paused forKey:@"paused"];

    [encoder encodeInt:sdl_channel forKey:@"sdl_channel"];

    /* for volume fades */
    [encoder encodeInt:volume_notify forKey:@"volume_notify"];
    [encoder encodeInt:volume_timeout forKey:@"volume_timeout"];
    [encoder encodeInt:target_volume forKey:@"target_volume"];
    [encoder encodeDouble:float_volume forKey:@"float_volume"];
    [encoder encodeDouble:volume_delta forKey:@"volume_delta"];
}

@end
