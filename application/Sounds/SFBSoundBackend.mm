//
//  SFBSoundBackend.mm
//  Spatterlight
//

#import "SFBSoundBackend.h"
#import "SoundHandler.h"

#include <SFBAudioEngine/AudioPlayer.h>
#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/LoopableRegionDecoder.h>
#include <SFBAudioEngine/CoreAudioOutput.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

static const AudioObjectPropertyAddress kDefaultOutputDeviceAddress = {
    .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
    .mScope    = kAudioObjectPropertyScopeGlobal,
    .mElement  = kAudioObjectPropertyElementMain
};

class CFDataInputSource final: public SFB::InputSource {
public:
    explicit CFDataInputSource(CFDataRef bytes);
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
        if (offset < 0 || offset > CFDataGetLength(_data)) {
            return false;
        }

        _pos = offset;
        return true;
    }

    // Data members
    SFB::CFData _data;
    off_t _pos;
};


/* SFB's own CreateWithMemory either copies the bytes or borrows them without
   keeping them alive; this one retains the CFData for as long as a decoder
   (which SFB may free late, on its collector queue) can read from it. */
static SFB::InputSource::unique_ptr CreateWithCFData(CFDataRef bytes) {
    if(nullptr == bytes || 0 >= CFDataGetLength(bytes))
        return nullptr;

    return SFB::InputSource::unique_ptr(new CFDataInputSource(bytes));
}

@interface SFBSoundBackend () {
    std::unique_ptr<SFB::Audio::Player> _player;
    SInt64 _pendingFrame;                  // Where a paused sound will resume
    BOOL _resumeEnqueued;                  // A decoder is queued at that position
    glsi32 _repeats;
    float _volume;
    NSUInteger _generation;                // Bumped by -stop, to drop stale finishes
    AudioObjectPropertyListenerBlock _defaultDeviceListener;
}
@end

@implementation SFBSoundBackend

@synthesize duration = _duration;

- (BOOL)playData:(NSData *)data
          format:(GlkSoundBlorbFormatType)type
         repeats:(glsi32)areps
        position:(double)position
          paused:(BOOL)paused
          volume:(float)volume
        finished:(void (^)(void))finished {
    [self stop];
    _duration = 0;
    _volume = volume;
    _repeats = areps;
    _pendingFrame = (SInt64)position;

    NSString *mimeString = [SoundHandler MIMETypeFromFormatType:type];
    if (!mimeString)
        return NO;

    auto decoder = SFB::Audio::Decoder::CreateForInputSource(CreateWithCFData((__bridge CFDataRef)data), (__bridge CFStringRef)mimeString);
    if (!decoder) {
        NSLog(@"SFBSoundBackend: No decoder for format %@", mimeString);
        return NO;
    }

    // CoreAudio's AudioFileOpenWithCallbacks autosniff can fail on streams
    // supplied via callbacks (e.g. ID3-prefixed MP3) because there is no
    // file extension to consult. Hand the decoder an explicit AudioFileTypeID
    // hint derived from the format we already detected.
    AudioFileTypeID hint = 0;
    switch (type) {
        case GlkSoundBlorbFormatMP3:  hint = kAudioFileMP3Type;  break;
        case GlkSoundBlorbFormatWave: hint = kAudioFileWAVEType; break;
        case GlkSoundBlorbFormatFORM: hint = kAudioFileAIFCType; break;
        // kAudioFileMIDIType doesn't exist; MIDI handled by separate decoder
        default: break;
    }
    if (hint)
        decoder->SetFileTypeHint(hint);

    CFErrorRef error = nullptr;
    if (!decoder->Open(&error)) {
        NSLog(@"SFBSoundBackend: Could not open decoder (format:%@) %@", mimeString, error);
        if (error)
            CFRelease(error);
        return NO;
    }

    if (!_player) {
        _player.reset(new SFB::Audio::Player());
        [self startFollowingDefaultOutputDevice];
    }

    [self setVolume:_volume];

    BOOL looping = (areps == -1);

    if (!looping && finished) {
        void (^block)(void) = [finished copy];
        SFBSoundBackend __weak *weakSelf = self;
        NSUInteger generation = _generation;
        _player->SetRenderingFinishedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
            /* The sound may have been stopped or replaced by the time this
               gets to the main queue. */
            dispatch_async(dispatch_get_main_queue(), ^{
                SFBSoundBackend *strongSelf = weakSelf;
                if (strongSelf && strongSelf->_generation == generation)
                    block();
            });
        });
    }

    if (looping)
        areps = (glsi32)(SFB_INFINITE_LOOP + 1);
    SInt64 frames = decoder->GetTotalFrames();
    Float64 sampleRate = decoder->GetFormat().mSampleRate;
    if (frames > 0 && sampleRate > 0)
        _duration = (NSTimeInterval)frames / sampleRate;

    auto loopableRegionDecoder = SFB::Audio::LoopableRegionDecoder::CreateForDecoderRegion((std::move(decoder)), 0, (UInt32)frames, (UInt32)areps - 1);

    /* Picking up where an autosave left off. The seek happens here rather than
       through the player because the player takes a decoder's current frame as the
       starting position, and only opens a decoder that is not open already: doing
       it first means no race with the decoding thread, and a position that reads
       back correctly straight away. The frame counts every pass of a looping
       region, so it restores the remaining repeats along with the position.
       Consume it either way: a later replay of this sound is a fresh start. */
    BOOL resumed = NO;
    if (_pendingFrame > 0 && loopableRegionDecoder && frames > 0) {
        /* Keep the frame inside the region: a sound looping forever counts passes
           forever, past what the decoder can seek to, while one with a set number
           of repeats still has to stop after the last of them. */
        if (looping) {
            _pendingFrame %= frames;
        } else {
            SInt64 lastFrame = frames * areps - 1;
            if (_pendingFrame > lastFrame)
                _pendingFrame = lastFrame;
        }
        CFErrorRef seekError = nullptr;
        if (!loopableRegionDecoder->Open(&seekError)) {
            NSLog(@"SFBSoundBackend: Could not open decoder to resume at frame %lld %@", _pendingFrame, seekError);
            if (seekError)
                CFRelease(seekError);
        } else if (loopableRegionDecoder->SupportsSeeking())
            resumed = (loopableRegionDecoder->SeekToFrame(_pendingFrame) == _pendingFrame);
    }

    if (paused) {
        _player->Enqueue(loopableRegionDecoder);
        /* Nothing has been started, and an unpause treats a player that has not
           started as finished. Remember that this one holds a resume point, so it
           gets played rather than replaced. The frame stays put as well: with
           nothing rendering it is all this backend knows about where it is, and a
           channel can be autosaved many times over before anyone unpauses it. */
        _resumeEnqueued = resumed;
    } else {
        _pendingFrame = 0;
        _player->Play(loopableRegionDecoder);
    }

    return YES;
}

- (void)stop {
    _generation++;
    if (_player) {
        _player->SetRenderingFinishedBlock(nullptr);
        _player->Stop();
    }
    _pendingFrame = 0;
    _resumeEnqueued = NO;
}

- (void)pause {
    if (_player)
        _player->Pause();
}

- (BOOL)resume {
    if (!_player)
        return NO;
    if (_resumeEnqueued) {
        // A decoder seeked to the archived position is waiting in the queue.
        _resumeEnqueued = NO;
        _pendingFrame = 0;
        return _player->Play();
    }
    // A player that is fully stopped has no live decoder to carry on with.
    if (_player->IsStopped())
        return NO;
    return _player->Play();
}

- (void)setVolume:(float)volume {
    _volume = volume;
    SFB::Audio::CoreAudioOutput *output = [self coreAudioOutput];
    if (output)
        output->SetVolume(volume);
}

- (double)resumePosition {
    /* With nothing rendering there is a position waiting to be handed to a
       decoder, which is what a sound paused across an autosave holds. */
    SInt64 current = 0;
    if (_player && _player->GetCurrentFrame(current) && current > 0)
        return (double)current;
    return (double)_pendingFrame;
}

- (glsi32)resumeRepeats {
    return _repeats;
}

/* The player's output unit binds to whichever device was the system default
   when it was opened, and stays on it. Move it along when the default changes
   (headphones plugged in or pulled out, a new choice in the Sound settings),
   or the channel keeps playing through the old device for as long as it lives. */
- (void)startFollowingDefaultOutputDevice {
    if (_defaultDeviceListener)
        return;
    SFBSoundBackend __weak *weakSelf = self;
    _defaultDeviceListener = ^(UInt32 /*count*/, const AudioObjectPropertyAddress * /*addresses*/) {
        [weakSelf defaultOutputDeviceDidChange];
    };
    OSStatus result = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &kDefaultOutputDeviceAddress, dispatch_get_main_queue(), _defaultDeviceListener);
    if (result != kAudioHardwareNoError) {
        NSLog(@"SFBSoundBackend: Could not listen for default output device changes: %d", (int)result);
        _defaultDeviceListener = nil;
    }
}

- (void)defaultOutputDeviceDidChange {
    if (!_player)
        return;

    AudioDeviceID defaultDevice = kAudioDeviceUnknown;
    UInt32 size = sizeof(defaultDevice);
    OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &kDefaultOutputDeviceAddress, 0, nullptr, &size, &defaultDevice);
    if (result != kAudioHardwareNoError || defaultDevice == kAudioDeviceUnknown)
        return;

    SFB::Audio::CoreAudioOutput *output = [self coreAudioOutput];
    if (!output)
        return;
    AudioDeviceID currentDevice = kAudioDeviceUnknown;
    if (output->GetDeviceID(currentDevice) && currentDevice == defaultDevice)
        return;

    /* The output unit will not change device while it is running. A player
       pause only stops the output and leaves the decoders and position alone,
       so the sound carries on from where it was. */
    bool wasPlaying = _player->IsPlaying();
    if (wasPlaying)
        _player->Pause();
    if (!output->SetDeviceID(defaultDevice))
        NSLog(@"SFBSoundBackend: Could not move to output device %u", (unsigned)defaultDevice);
    if (wasPlaying)
        _player->Play();
}

/* The player's output, or null if it is not a CoreAudio one (the only kind
   this backend creates, but the player API does not promise it). */
- (SFB::Audio::CoreAudioOutput *)coreAudioOutput {
    if (!_player)
        return nullptr;
    return dynamic_cast<SFB::Audio::CoreAudioOutput *>(&_player->GetOutput());
}

- (void)dealloc {
    if (_defaultDeviceListener)
        AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &kDefaultOutputDeviceAddress, dispatch_get_main_queue(), _defaultDeviceListener);
    if (_player) {
        _player->SetRenderingFinishedBlock(nullptr);
        _player.reset();
    }
}

@end

CFDataInputSource::CFDataInputSource(CFDataRef bytes): SFB::InputSource(), _pos(0) {
    _data = SFB::CFWrapper<CFDataRef>((CFDataRef)CFRetain(bytes), true);
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
