#import "GlkSoundChannel.h"
#import "SoundHandler.h"
#import "MIDIChannel.h"

#include <SFBAudioEngine/AudioPlayer.h>
#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/LoopableRegionDecoder.h>
#include <SFBAudioEngine/CoreAudioOutput.h>

@interface GlkSoundChannel () {
@private
    SFB::Audio::Player    *_player;        // The player instance
}

//@property MIDIChannel *midiChannel;

@end

@implementation GlkSoundChannel

- (instancetype)initWithHandler:(SoundHandler*)handler name:(NSUInteger)channelname volume:(NSUInteger)vol
{
    if (self = [super init]) {
    _handler = handler;
    loop = 0;
    notify = 0;
    _name = channelname;
    
    _status = CHANNEL_IDLE;
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
    }
    
    return self;
}

- (void)play:(NSInteger)snd repeats:(NSInteger)areps notify:(NSInteger)anot
{
    _status = CHANNEL_SOUND;

    size_t len = 0;
    NSInteger type;

    char *buf = nil;

    /* stop previous noise */
    if (_player) {
        _player->SetRenderingFinishedBlock(nullptr);
        _player->Stop();
    }
    
    if (areps == 0 || snd == -1)
        return;
    
    /* load sound resource into memory */
    type = [_handler load_sound_resource:snd length:&len data:&buf];

    notify = anot;
    resid = snd;
    loop = areps;

    mimeString = @"";

    switch (type)
    {
        case giblorb_ID_FORM:
            mimeString = @"aiff";
            break;
        case giblorb_ID_AIFF:
            mimeString = @"aiff";
            break;
        case giblorb_ID_WAVE:
            mimeString = @"wav";
            break;
        case giblorb_ID_OGG:
            mimeString = @"ogg-vorbis";
            break;
        case giblorb_ID_MP3:
            mimeString = @"mp3";
            break;
        case giblorb_ID_MOD:
            mimeString = @"mod";
            break;
        case giblorb_ID_MIDI:
            mimeString = @"midi";
            break;
            
        default:
            NSLog(@"schannel_play_ext: unknown resource type (%ld).", type);
            return;
    }

    mimeString = [NSString stringWithFormat:@"audio/%@", mimeString];

    auto decoder = SFB::Audio::Decoder::CreateForInputSource(SFB::InputSource::CreateWithMemory(buf, (SInt64)len, false), (__bridge CFStringRef)mimeString);

    if (!_player) {
        _player = new SFB::Audio::Player();
    }

    [self setVolume];

    if (areps != -1) {
        NSInteger blocknotify = notify;
        NSInteger blockresid = resid;
        GlkSoundChannel __weak *weakSelf = self;
        _player->SetRenderingFinishedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
            dispatch_async(dispatch_get_main_queue(), ^{
                weakSelf.status = CHANNEL_IDLE;
                if (blocknotify)
                    [weakSelf.handler handleSoundNotification:blocknotify withSound:blockresid];
            });
        });
    }

    if (areps == -1)
        areps = SFB_INFINITE_LOOP + 1;
    CFErrorRef error = nullptr;
    if (!decoder->Open(&error))
        NSLog(@"GlkSoundChannel: Could not open decoder (format:%@) %@", mimeString, error);
    SInt64 frames = decoder->GetTotalFrames();
    auto loopableRegionDecoder = SFB::Audio::LoopableRegionDecoder::CreateForDecoderRegion((std::move(decoder)), 0, (UInt32)frames, (UInt32)areps - 1);
    if (paused)
        _player->Enqueue(loopableRegionDecoder);
    else
        _player->Play(loopableRegionDecoder);
}

- (void)stop
{
    paused = NO;
    if (_player) {
        _player->SetRenderingFinishedBlock(nullptr);
        _player->Stop();
    }
    [self cleanup];
}

- (void) pause
{
    paused = YES;
    if (_player)
        _player->Pause();
}

- (void) unpause
{
    paused = NO;
    if (!_player)
        [self play:resid repeats:loop notify:notify];
    else
        _player->Play();
}

- (void)cleanup
{
   _status = CHANNEL_IDLE;
   if (timer)
        [timer invalidate];
    timer = nil;
    resid = -1;
}

/** Start a fade timer */
- (void)init_fade:(NSUInteger)glk_volume duration:(NSUInteger)duration notify:(NSInteger)notification
{
    volume_notify = notification;
    target_volume = (CGFloat)glk_volume / GLK_MAXVOLUME;

    volume_delta = (target_volume - volume) / (CGFloat)FADE_GRANULARITY;

    volume_timeout = FADE_GRANULARITY;
    
    if (timer)
        [timer invalidate];

    timer = [NSTimer scheduledTimerWithTimeInterval:ceil(duration / (CGFloat)FADE_GRANULARITY) / 1000.0 target:self selector:@selector(volumeTimerCallback:) userInfo:nil repeats:YES];
}

- (void)volumeTimerCallback:(NSTimer *)aTimer {
    volume += volume_delta;

    if (volume < 0)
        volume = 0;

    volume_timeout--;

    /* If the timer has fired FADE_GRANULARITY times, kill it */
    if (volume_timeout <= 0)
    {
        if (volume_notify)
        {
            NSInteger notification = volume_notify;
            SoundHandler *handler = _handler;
            dispatch_async(dispatch_get_main_queue(), ^{
                [handler handleVolumeNotification:notification];
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
        }
    }

    [self setVolume];
}

- (void) setVolume:(NSUInteger)glk_volume duration:(NSUInteger)duration notify:(NSInteger)notification
{
    if (!duration)
    {
        volume = (CGFloat)glk_volume / GLK_MAXVOLUME;
        [self setVolume];
    }
    else
    {
        [self init_fade:glk_volume duration:duration notify:notification];
    }
}

- (void)setVolume {
    if (!_player)
        return;
    auto& output = dynamic_cast<SFB::Audio::CoreAudioOutput&>(_player->GetOutput());
    output.SetVolume((float)volume);
}


/* Restart the sound channel after a deserialize, and also any fade timer.
 This primitive implementation disregards how much of the sound that had
 played when the game was autosaved or killed.
 Fade timers remember this, however, so a clip halfway through a 10
 second fade out will restart from the beginning but fade out in 10 seconds.
 Well, except that it counts from last glk_select and not from when the process
 was actually terminated.
 */
- (void)restartInternal {

//    [self postInit];
    
    if (_status == CHANNEL_IDLE) {
        return;
    }

    if (paused == YES) {
        [self pause];
    }

    [self play:resid repeats:loop notify:notify];

    if (volume_timeout > 0) {

        NSUInteger duration = (volume_timeout * FADE_GRANULARITY);

        CGFloat float_volume = target_volume;
        NSUInteger glk_target_volume = GLK_MAXVOLUME;

        if (float_volume < MIX_MAX_VOLUME)
           glk_target_volume = (NSUInteger)(float_volume * GLK_MAXVOLUME);

        [self setVolume:glk_target_volume duration:duration notify:volume_notify];

        if (!timer)
        {
           NSLog(@"restartInternal: failed to create volume change timer.");
        }
    }
    return;
}

- (void)copyValues:(GlkSoundChannel *)otherChannel {
    if (paused)
        [otherChannel pause];
    [self stop];
    otherChannel.name = self.name;

    if (timer)
        [timer invalidate];

    NSUInteger duration = 0;
    NSUInteger glk_target_volume = (NSUInteger)(volume * GLK_MAXVOLUME);

    if (volume_timeout > 0) {
        CGFloat float_volume = target_volume;
        duration = (volume_timeout * FADE_GRANULARITY);
        if (float_volume < MIX_MAX_VOLUME)
            glk_target_volume = (NSUInteger)(float_volume * GLK_MAXVOLUME);
        else
            glk_target_volume = GLK_MAXVOLUME;
    }

    [otherChannel setVolume:glk_target_volume duration:duration notify:volume_notify];
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];

    if (self) {
        _name = (NSUInteger)[decoder decodeIntForKey:@"name"];

        resid =  [decoder decodeIntForKey:@"resid"]; /* for notifies */
        _status =  [decoder decodeIntForKey:@"status"];
        volume = [decoder decodeDoubleForKey:@"volume"];
        loop = [decoder decodeIntForKey:@"loop"];
        notify = [decoder decodeIntForKey:@"notify"];
        paused = (NSUInteger)[decoder decodeIntForKey:@"paused"];

        /* for volume fades */
        volume_notify = [decoder decodeInt32ForKey:@"volume_notify"];
        volume_timeout = (NSUInteger)[decoder decodeIntForKey:@"volume_timeout"];
        target_volume = [decoder decodeDoubleForKey:@"target_volume"];
        volume_delta = [decoder decodeDoubleForKey:@"volume_delta"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {

    [encoder encodeInteger:(NSInteger)_name forKey:@"name"];
    [encoder encodeInteger:resid forKey:@"resid"];
    [encoder encodeInteger:_status forKey:@"status"];
    [encoder encodeDouble:volume forKey:@"volume"];
    [encoder encodeInteger:loop forKey:@"loop"];
    [encoder encodeInteger:notify forKey:@"notify"];
    [encoder encodeInteger:(NSInteger)paused forKey:@"paused"];

    /* for volume fades */
    [encoder encodeInteger:volume_notify forKey:@"volume_notify"];
    [encoder encodeInteger:(NSInteger)volume_timeout forKey:@"volume_timeout"];
    [encoder encodeDouble:target_volume forKey:@"target_volume"];
    [encoder encodeDouble:volume_delta forKey:@"volume_delta"];
}

@end
