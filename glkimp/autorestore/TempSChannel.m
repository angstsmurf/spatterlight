/*  TempSChannel.m: Temporary sound channel objc class
 for serializing, inspired by IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempSChannel.h"

#define SDL_MIX_MAXVOLUME 128
#define GLK_MAXVOLUME 65536
#define FADE_GRANULARITY 100

@implementation TempSChannel

- (instancetype) initWithCStruct:(channel_t *)chan {

    self = [super init];

    if (self) {
        _rock = chan->rock;
        _tag = chan->tag;

        sdl_channel = chan->sdl_channel;

        resid = chan->resid; /* for notifies */
        status = chan->status;
        channel = chan->channel;
        volume = chan->volume;
        loop = chan->loop;
        notify = chan->notify;
        buffered = chan->buffered;
        paused = chan->paused;

        /* for volume fades */
        volume_notify = chan->volume_notify;
        volume_timeout = chan->volume_timeout;
        target_volume = chan->target_volume;
        float_volume = chan->float_volume;
        volume_delta = chan->volume_delta;

        _next = _prev = 0;

        if (chan->next)
            _next = (chan->next)->tag;
        if (chan->prev)
            _prev = (chan->prev)->tag;
    }
    return self;
}

- (id) initWithCoder:(NSCoder *)decoder {

    _rock = [decoder decodeInt32ForKey:@"rock"];
    _tag = [decoder decodeInt32ForKey:@"tag"];

    _prev = [decoder decodeInt32ForKey:@"prev"];
    _next = [decoder decodeInt32ForKey:@"next"];

    sdl_channel = [decoder decodeInt32ForKey:@"sdl_channel"];

    resid =  [decoder decodeInt32ForKey:@"resid"]; /* for notifies */
    status =  [decoder decodeInt32ForKey:@"status"];
    channel =  [decoder decodeInt32ForKey:@"channel"];
    volume =  [decoder decodeInt32ForKey:@"volume"];
    loop =  [decoder decodeInt32ForKey:@"loop"];
    notify = [decoder decodeInt32ForKey:@"notify"];
    buffered = [decoder decodeInt32ForKey:@"buffered"];
    paused = [decoder decodeInt32ForKey:@"paused"];

    /* for volume fades */
    volume_notify = [decoder decodeInt32ForKey:@"volume_notify"];
    volume_timeout = [decoder decodeInt32ForKey:@"volume_timeout"];
    target_volume = [decoder decodeInt32ForKey:@"target_volume"];
    float_volume = [decoder decodeDoubleForKey:@"float_volume"];
    volume_delta = [decoder decodeDoubleForKey:@"volume_delta"];

    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInt32:_rock forKey:@"rock"];
    [encoder encodeInt32:_tag forKey:@"tag"];

    [encoder encodeInt32:_prev forKey:@"prev"];
    [encoder encodeInt32:_next forKey:@"next"];

    [encoder encodeInt32:sdl_channel forKey:@"sdl_channel"];

    [encoder encodeInt32:resid forKey:@"resid"]; /* for notifies */
    [encoder encodeInt32:status forKey:@"status"];
    [encoder encodeInt32:channel forKey:@"channel"];
    [encoder encodeInt32:volume forKey:@"volume"];
    [encoder encodeInt32:loop forKey:@"loop"];
    [encoder encodeInt32:notify forKey:@"notify"];
    [encoder encodeInt32:buffered forKey:@"buffered"];
    [encoder encodeInt32:paused forKey:@"paused"];

    /* for volume fades */
    [encoder encodeInt32:volume_notify forKey:@"volume_notify"];
    [encoder encodeInt32:volume_timeout forKey:@"volume_timeout"];
    [encoder encodeInt32:target_volume forKey:@"target_volume"];
    [encoder encodeDouble:float_volume forKey:@"float_volume"];
    [encoder encodeDouble:volume_delta forKey:@"volume_delta"];
}

- (void) copyToCStruct:(channel_t *)chan {

    chan->sample = NULL; /* Mix_Chunk (or FMOD Sound) */
    chan->music = NULL; /* Mix_Music (or FMOD Music) */
    chan->decode = NULL; /* Sound_Sample */

    chan->sdl_rwops = NULL; /* SDL_RWops */
    chan->sdl_memory = NULL;
    chan->timer = 0;

    chan->tag = _tag;
    chan->rock = _rock;
    chan->sdl_channel = sdl_channel;

    chan->resid = resid; /* for notifies */
    chan->status = status;
    chan->channel = channel;
    chan->volume = volume;
    chan->loop = loop;
    chan->notify = notify;
    chan->buffered = buffered;
    chan->paused = paused;

    /* for volume fades */
    chan->volume_notify = volume_notify;
    chan->volume_timeout = volume_timeout;
    chan->target_volume = target_volume;
    chan->float_volume = float_volume;
    chan->volume_delta = volume_delta;
}

/* Restart the sound channel after a deserialize, and also any fade timer. Called from TempLibrary.updateFromLibraryLate. This is currently a primitive implementation
    which disregards how much of the sound had played when the game was autosaved
    or killed. Fade timers remember this, however, so a clip halfway through a 10
    second fade out will restart from the beginning but fade out in 10 seconds.
    Well, except that it counts from last glk_select and not from when the process
    was actually killed.
 */
- (void)restartInternal {

    channel_t *chan = gli_schan_for_tag(_tag);

    if (chan->status == CHANNEL_IDLE) {
        return;
    }

    if (chan->paused == TRUE) {
        glk_schannel_pause(chan);
    }

    NSLog(@"TempSChannel restartInternal: chan %d: loop: %d notify: %d", _tag, chan->loop, chan->notify);

    glui32 result = glk_schannel_play_ext(chan, chan->resid, chan->loop, chan->notify);
    if (!result) {
        NSLog(@"TempSChannel restartInternal: failed to restart sound channel %d. resid:%d loop:%d notify:%d status: %d", _tag, chan->resid, chan->loop, chan->notify, status);
        return;
    }

    if (chan->volume_timeout > 0) {

        int duration = (chan->volume_timeout * FADE_GRANULARITY);

        int sdl_volume = chan->target_volume;
        int glk_target_volume = GLK_MAXVOLUME;

        // This ridiculous calculation seems to be necessary to convert the SDL volume back to the correct GLK volume.
        if (sdl_volume < SDL_MIX_MAXVOLUME)
           glk_target_volume = expf(logf((float)sdl_volume/SDL_MIX_MAXVOLUME)/logf(4)) * GLK_MAXVOLUME;

        glk_schannel_set_volume_ext(chan, glk_target_volume, duration, chan->volume_notify);

        if (!chan->timer)
        {
            gli_strict_warning("TempSChannel restartInternal: failed to create volume change timer.");
        }
    }
    return;
}

@end
