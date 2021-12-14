#import "GlkSoundChannel.h"
#import "SoundHandler.h"

#include <SFBAudioEngine/AudioPlayer.h>
#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/LoopableRegionDecoder.h>
#include <SFBAudioEngine/CoreAudioOutput.h>

class CFDataInputSource final: public SFB::InputSource {
public:
    CFDataInputSource(CFDataRef bytes, bool copyBytes = true);
    virtual ~CFDataInputSource() = default;

private:
    virtual bool _Open(CFErrorRef *error) {
        return true;
    }
    virtual bool _Close(CFErrorRef *error) {
        _pos = 0;
        return true;
    }
    virtual SInt64 _Read(void *buffer, SInt64 byteCount);
    virtual bool _AtEOF() const {
        return _pos == CFDataGetLength(_data);
    }
    virtual SInt64 _GetOffset() const {
        return _pos;
    }
    virtual SInt64 _GetLength() const {
        return CFDataGetLength(_data);
    }

    // Optional seeking support
    inline virtual bool _SupportsSeeking() const {
        return true;
    }

    virtual bool _SeekToOffset(SInt64 offset) {
        if (offset > CFDataGetLength(_data)) {
            return false;
        }

        _pos = offset;
        return true;
    }

    // Data members
    SFB::CFData _data;
    off_t _pos;
};


SFB::InputSource::unique_ptr CreateWithCFData(CFDataRef bytes, bool copyBytes, CFErrorRef *error = nullptr) {
#pragma unused(error)

    if(nullptr == bytes || 0 >= CFDataGetLength(bytes))
        return nullptr;

    return SFB::InputSource::unique_ptr(new CFDataInputSource(bytes, copyBytes));
}

@interface GlkSoundChannel () {
@private
    SFB::Audio::Player    *_player;        // The player instance
}

@end

@implementation GlkSoundChannel

- (instancetype)initWithHandler:(SoundHandler*)handler name:(int)channelname volume:(glui32)vol {
    if (self = [super init]) {
        _handler = handler;
        loop = 0;
        notify = 0;
        _name = channelname;

        _status = GlkSoundChannelStatusIdle;
        volume = (float)vol / GLK_MAXVOLUME;
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

- (BOOL)playSound:(glsi32)snd countOfRepeats:(glsi32)areps notification:(glui32)anot {
    _status = GlkSoundChannelStatusSound;

    GlkSoundBlorbFormatType type;

    NSData *buf = nil;

    /* stop previous noise */
    if (_player) {
        _player->SetRenderingFinishedBlock(nullptr);
        _player->Stop();
    }

    if (areps == 0 || snd == -1)
        return NO;

    /* load sound resource into memory */
    type = [_handler loadSoundResourceFromSound:snd data:&buf];

    notify = anot;
    resid = snd;
    loop = areps;

    mimeString = [SoundHandler MIMETypeFromFormatType:type];

    if (!mimeString) {
        NSLog(@"schannel_play_ext: unknown resource type (%ld).", type);
        return NO;
    }

    auto decoder = SFB::Audio::Decoder::CreateForInputSource(CreateWithCFData((__bridge CFDataRef)buf, false), (__bridge CFStringRef)mimeString);

    if (!_player) {
        _player = new SFB::Audio::Player();
    }

    [self setVolume];

    if (areps != -1) {
        glui32 blocknotify = notify;
        glsi32 blockresid = resid;
        GlkSoundChannel __weak *weakSelf = self;
        _player->SetRenderingFinishedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
            dispatch_async(dispatch_get_main_queue(), ^{
                weakSelf.status = GlkSoundChannelStatusIdle;
                if (blocknotify)
                    [weakSelf.handler handleSoundNotification:blocknotify withSound:blockresid];
            });
        });
    }

    if (areps == -1)
        areps = (glsi32)(SFB_INFINITE_LOOP + 1);
    CFErrorRef error = nullptr;
    if (!decoder->Open(&error))
        NSLog(@"GlkSoundChannel: Could not open decoder (format:%@) %@", mimeString, error);
    SInt64 frames = decoder->GetTotalFrames();
    auto loopableRegionDecoder = SFB::Audio::LoopableRegionDecoder::CreateForDecoderRegion((std::move(decoder)), 0, (UInt32)frames, (UInt32)areps - 1);
    if (paused)
        _player->Enqueue(loopableRegionDecoder);
    else
        _player->Play(loopableRegionDecoder);
    return YES;
}

- (void)stop {
    paused = NO;
    if (_player) {
        _player->SetRenderingFinishedBlock(nullptr);
        _player->Stop();
    }
    [self cleanup];
}

- (void)pause {
    paused = YES;
    if (_player)
        _player->Pause();
}

- (void)unpause {
    paused = NO;
    BOOL result;
    if (!_player) {
        result = [self playSound:resid countOfRepeats:loop notification:notify];
    } else {
        result = _player->Play();
    }
    if (!result)
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
    target_volume = (float)glk_volume / GLK_MAXVOLUME;

    volume_delta = (target_volume - volume) / (float)FADE_GRANULARITY;

    volume_timeout = FADE_GRANULARITY;

    if (timer)
        [timer invalidate];

    timer = [NSTimer scheduledTimerWithTimeInterval:ceil(duration / (NSTimeInterval)FADE_GRANULARITY) / 1000.0 target:self selector:@selector(volumeTimerCallback:) userInfo:nil repeats:YES];
}

- (void)volumeTimerCallback:(NSTimer *)aTimer {
    volume += volume_delta;

    if (volume < 0)
        volume = 0;

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
        volume = (float)glk_volume / GLK_MAXVOLUME;
        [self setVolume];
    } else {
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

    if (_status == GlkSoundChannelStatusIdle) {
        return;
    }

    if (paused == YES) {
        [self pause];
    }

    BOOL result = [self playSound:resid countOfRepeats:loop notification:notify];
    if (!result)
        NSLog(@"restartInternal: failed to restart sound %d.", resid);

    if (volume_timeout > 0) {

        glui32 duration = (volume_timeout * FADE_GRANULARITY);

        float float_volume = target_volume;
        glui32 glk_target_volume = GLK_MAXVOLUME;

        if (float_volume < FLOAT_MAX_VOLUME)
            glk_target_volume = (glui32)(float_volume * GLK_MAXVOLUME);

        [self setVolume:glk_target_volume duration:duration notification:volume_notify];

        if (!timer) {
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

    glui32 duration = 0;
    glui32 glk_target_volume = (glui32)(volume * GLK_MAXVOLUME);

    if (volume_timeout > 0) {
        float float_volume = target_volume;
        duration = (volume_timeout * FADE_GRANULARITY);
        if (float_volume < FLOAT_MAX_VOLUME)
            glk_target_volume = (glui32)(float_volume * GLK_MAXVOLUME);
        else
            glk_target_volume = GLK_MAXVOLUME;
    }

    [otherChannel setVolume:glk_target_volume duration:duration notification:volume_notify];
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];

    if (self) {
        _name = [decoder decodeIntForKey:@"name"];

        resid = (glsi32)[decoder decodeIntForKey:@"resid"]; /* for notifies */
        _status = GlkSoundChannelStatus([decoder decodeIntegerForKey:@"status"]);
        volume = (float)[decoder decodeDoubleForKey:@"volume"];
        loop = [decoder decodeIntForKey:@"loop"];
        notify = (glui32)[decoder decodeIntForKey:@"notify"];
        paused = (glui32)[decoder decodeIntForKey:@"paused"];

        /* for volume fades */
        volume_notify = (glui32)[decoder decodeInt32ForKey:@"volume_notify"];
        volume_timeout = (glui32)[decoder decodeIntForKey:@"volume_timeout"];
        target_volume = (float)[decoder decodeDoubleForKey:@"target_volume"];
        volume_delta = (float)[decoder decodeDoubleForKey:@"volume_delta"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInt:_name forKey:@"name"];
    [encoder encodeInt:resid forKey:@"resid"];
    [encoder encodeInteger:_status forKey:@"status"];
    [encoder encodeDouble:volume forKey:@"volume"];
    [encoder encodeInt:loop forKey:@"loop"];
    [encoder encodeInt:(int)notify forKey:@"notify"];
    [encoder encodeInt:(int)paused forKey:@"paused"];

    /* for volume fades */
    [encoder encodeInt:(int)volume_notify forKey:@"volume_notify"];
    [encoder encodeInt:(int)volume_timeout forKey:@"volume_timeout"];
    [encoder encodeDouble:target_volume forKey:@"target_volume"];
    [encoder encodeDouble:volume_delta forKey:@"volume_delta"];
}

@end

CFDataInputSource::CFDataInputSource(CFDataRef bytes, bool copyBytes): SFB::InputSource(), _pos(0) {
    if (copyBytes) {
        _data = SFB::CFWrapper<CFDataRef>(CFDataCreateCopy(kCFAllocatorDefault, bytes), true);
    } else {
        _data = SFB::CFWrapper<CFDataRef>((CFDataRef)CFRetain(bytes), true);
    }
}

SInt64 CFDataInputSource::_Read(void *buffer, SInt64 byteCount) {
    if (_pos + byteCount > CFDataGetLength(_data)) {
        byteCount = CFDataGetLength(_data) - _pos;
    }
    CFRange range = CFRangeMake(_pos, byteCount);
    ::CFDataGetBytes(_data, range, (UInt8 *)buffer);
    _pos += byteCount;
    return byteCount;
}
