//
//  SoundHandler.m
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import "SoundHandler.h"

#import <AppKit/AppKit.h>

#import "GlkSoundChannel.h"
#import "MIDIChannel.h"
#import "NowPlayingCoordinator.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkEvent.h"

@implementation SoundFile : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithPath:(NSString *)path {
    self = [super init];
    if (self) {
        _URL = [NSURL fileURLWithPath:path isDirectory:NO];
        NSError *theError;
        _bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                   includingResourceValuesForKeys:nil
                                    relativeToURL:nil
                                            error:&theError];
        if (theError || !_bookmark)
            NSLog(@"Soundfile error when encoding bookmark %@: %@", path, theError);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    if (self = [super init]) {
        _URL =  [decoder decodeObjectOfClass:[NSURL class] forKey:@"URL"];
        _bookmark = [decoder decodeObjectOfClass:[NSData class] forKey:@"bookmark"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_URL forKey:@"URL"];
    [encoder encodeObject:_bookmark forKey:@"bookmark"];
}

- (void)resolveBookmark {
    if (!_bookmark)
        return;
    BOOL bookmarkIsStale = NO;
    NSError *error = nil;
    NSURL *resolved = [NSURL URLByResolvingBookmarkData:_bookmark options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:&bookmarkIsStale error:&error];
    if (resolved) {
        _URL = resolved;
        if (bookmarkIsStale)
            [self refreshBookmark];
        return;
    }

    NSLog(@"Soundfile resolveBookmark: %@", error);
    if (error.code != NSFileNoSuchFileError)
        return;

    /* The file has gone from where it was. Look for one of the same name next
       to the game file, which is where a moved game takes its Blorb along. */
    NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                                       fromBookmarkData:_bookmark];
    NSString *oldfilename = [values[NSURLPathKey] lastPathComponent];
    NSString *gamefile = _handler.glkctl.gamefile;
    if (!oldfilename.length || !gamefile.length)
        return;
    NSString *newPath =
    [gamefile.stringByDeletingLastPathComponent stringByAppendingPathComponent:oldfilename];
    if ([[NSFileManager defaultManager] fileExistsAtPath:newPath]) {
        _URL = [NSURL fileURLWithPath:newPath isDirectory:NO];
        [self refreshBookmark];
    }
}

- (void)refreshBookmark {
    NSError *error = nil;
    NSData *bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                      includingResourceValuesForKeys:nil
                                       relativeToURL:nil
                                               error:&error];
    if (bookmark)
        _bookmark = bookmark;
    else
        NSLog(@"Soundfile error when refreshing bookmark %@: %@", _URL.path, error);
}

@end


@implementation SoundResource : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length {
    self = [super init];
    if (self) {
        _filename = filename;
        _offset = offset;
        _length = length;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    if (self = [super init]) {
        _filename = [decoder decodeObjectOfClass:[NSString class] forKey:@"filename"];
        _length = (NSUInteger)[decoder decodeIntegerForKey:@"length"];
        _offset = (NSUInteger)[decoder decodeIntegerForKey:@"offset"];
        _type = (GlkSoundBlorbFormatType)[decoder decodeIntegerForKey:@"type"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_filename forKey:@"filename"];
    [encoder encodeInteger:(NSInteger)_length forKey:@"length"];
    [encoder encodeInteger:(NSInteger)_offset forKey:@"offset"];
    [encoder encodeInteger:(NSInteger)_type forKey:@"type"];
}

-(BOOL)load {
    if (!_soundFile.URL)
        [_soundFile resolveBookmark];
    NSError *error = nil;
    NSFileHandle *fileHandle = [NSFileHandle fileHandleForReadingFromURL:_soundFile.URL error:&error];
    if (!fileHandle) {
        [_soundFile resolveBookmark];
        fileHandle = [NSFileHandle fileHandleForReadingFromURL:_soundFile.URL error:&error];
        if (!fileHandle) {
            NSLog(@"Could not open sound file %@: %@", _soundFile.URL.path, error);
            return NO;
        }
    }

    NSData *data = nil;
    if (@available(macOS 10.15, *)) {
        if ([fileHandle seekToOffset:_offset error:&error])
            data = [fileHandle readDataUpToLength:_length error:&error];
        [fileHandle closeAndReturnError:nil];
    } else {
        @try {
            [fileHandle seekToFileOffset:_offset];
            data = [fileHandle readDataOfLength:_length];
        } @catch (NSException *exception) {
            NSLog(@"SoundResource load: %@", exception);
        }
        [fileHandle closeFile];
    }

    if (data.length != _length) {
        NSLog(@"Could not read sound resource from file %@, length %lu, offset %lu (got %lu bytes) %@", _soundFile.URL.path, (unsigned long)_length, (unsigned long)_offset, (unsigned long)data.length, error ?: @"");
        return NO;
    }

    if (!_type) {
        _type = [SoundResource detectSoundFormatFromData:data];
        if (!_type) {
            NSLog(@"Unknown format");
            return NO;
        }
    }

    _data = data;
    return YES;
}

/* Sniff the format from the data. MP3 has no reliable magic of its own, so
   anything not recognised is taken to be MP3; only empty data is None. */
+ (GlkSoundBlorbFormatType)detectSoundFormatFromData:(NSData*)data {
    const char *buf = (const char *)data.bytes;
    const NSUInteger length = data.length;

    if (length == 0)
        return GlkSoundBlorbFormatNone;

    /* AIFF */
    if (length >= 4 && !memcmp(buf, "FORM", 4))
        return GlkSoundBlorbFormatFORM;

    /* WAVE */
    if (length >= 4 && !memcmp(buf, "WAVE", 4))
        return GlkSoundBlorbFormatWave;

    if (length >= 4 && !memcmp(buf, "RIFF", 4))
        return GlkSoundBlorbFormatWave;

    /* midi */
    if (length >= 4 && !memcmp(buf, "MThd", 4))
        return GlkSoundBlorbFormatMIDI;

    /* s3m */
    if (length >= 0x30 && !memcmp(buf + 0x2c, "SCRM", 4))
        return GlkSoundBlorbFormatMod;

    /* XM */
    if (length >= 17 && !memcmp(buf, "Extended Module: ", 17))
        return GlkSoundBlorbFormatMod;

    /* MOD */
    if (length >= 1084) {
        const char *resname = buf + 1080;
        if (!memcmp(resname+1, "CHN", 3) ||        /* 4CHN, 6CHN, 8CHN */
            !memcmp(resname+2, "CN", 2) ||         /* 16CN, 32CN */
            !memcmp(resname, "M.K.", 4) || !memcmp(resname, "M!K!", 4) ||
            !memcmp(resname, "FLT4", 4) || !memcmp(resname, "CD81", 4) ||
            !memcmp(resname, "OKTA", 4) || !memcmp(resname, "    ", 4))
            return GlkSoundBlorbFormatMod;
    }

    /* ogg */
    if (length >= 4 && !memcmp(buf, "OggS", 4))
        return GlkSoundBlorbFormatOggVorbis;

    return GlkSoundBlorbFormatMP3;
}

@end

@interface SoundHandler () <NSSoundDelegate>
/* Synthesised ZX Spectrum BEEP tones, held strongly while they play or wait
 * their turn. */
@property (strong, nullable) NSMutableArray<NSSound *> *beepSounds;
/* Bumped by -stopBeeps, so that notes still waiting to start can tell that
 * the tune they belonged to has been cancelled. */
@property (nonatomic) NSUInteger beepGeneration;
/* Absolute time (NSDate referenceDate epoch, in seconds) at which the next
 * queued BEEP should start. Used to chain a tune's notes back-to-back without
 * blocking the interpreter. */
@property (nonatomic) NSTimeInterval nextBeepTime;
/* A short silent sound played at game launch to warm up the CoreAudio output
 * device ahead of the first real tone, held strongly while it plays. See
 * primeBeepAudio. */
@property (strong, nullable) NSSound *beepPrimeSound;
@property (nonatomic) BOOL beepAudioPrimed;
@end

@implementation SoundHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _resources = [NSMutableDictionary new];
        _glkchannels = [NSMutableDictionary new];
        _files = [NSMutableDictionary new];
        _lastsoundresno = -1;
    }
    return self;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        /* A missing or undecodable key leaves an empty store rather than nil:
           messages to a nil dictionary would silently drop every new sound
           and channel for the rest of the session. */
        _files = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSString class], [SoundFile class], nil] forKey:@"files"];
        if (!_files)
            _files = [NSMutableDictionary new];
        for (SoundFile *file in _files.allValues)
            file.handler = self;
        _resources = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [SoundResource class], nil] forKey:@"resources"];
        if (!_resources)
            _resources = [NSMutableDictionary new];
        for (SoundResource *res in _resources.allValues) {
            res.soundFile = _files[res.filename];
        }
        _glkchannels = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [GlkSoundChannel class], [MIDIChannel class], nil] forKey:@"gchannels"];
        if (!_glkchannels)
            _glkchannels = [NSMutableDictionary new];
        _lastsoundresno = [decoder containsValueForKey:@"lastsoundresno"] ? [decoder decodeIntForKey:@"lastsoundresno"] : -1;
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_files forKey:@"files"];
    [encoder encodeObject:_resources forKey:@"resources"];
    [encoder encodeObject:_glkchannels forKey:@"gchannels"];
    [encoder encodeInt:_lastsoundresno forKey:@"lastsoundresno"];
}

- (BOOL)soundIsLoaded:(glsi32)soundId {
    return _resources[@(soundId)].data != nil;
}

/* The resource for a sound, read in from its file if it has not been yet, or
   nil if there is no such sound or it cannot be read. */
- (nullable SoundResource *)loadedResourceForSound:(glsi32)soundId {
    SoundResource *resource = _resources[@(soundId)];
    if (resource && !resource.data)
        [resource load];
    return resource.data ? resource : nil;
}

+ (NSString *)MIMETypeFromFormatType:(GlkSoundBlorbFormatType)format {
    NSString *mimeString;
    switch (format) {
        case GlkSoundBlorbFormatFORM:
        case GlkSoundBlorbFormatAIFF:
            mimeString = @"aiff";
            break;
        case GlkSoundBlorbFormatWave:
            mimeString = @"wav";
            break;
        case GlkSoundBlorbFormatOggVorbis:
            mimeString = @"ogg-vorbis";
            break;
        case GlkSoundBlorbFormatMP3:
            mimeString = @"mp3";
            break;
        case GlkSoundBlorbFormatMod:
            mimeString = @"mod";
            break;
        case GlkSoundBlorbFormatMIDI:
            mimeString = @"midi";
            break;

        default:
            NSLog(@"schannel_play_ext: unknown resource type (%ld).", format);
            return nil;
    }

    return [NSString stringWithFormat:@"audio/%@", mimeString];
}

#pragma mark Glk request calls from GlkController

- (BOOL)handleFindSoundNumber:(glsi32)resno {
    BOOL result = ([self loadedResourceForSound:resno] != nil);
    _lastsoundresno = result ? resno : -1;
    return result;
}

- (void)handleLoadSoundNumber:(glsi32)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       length:(NSUInteger)length {

    if (_lastsoundresno == resno && [self soundIsLoaded:resno])
        return;

    _lastsoundresno = resno;

    [self setSoundID:resno filename:path length:length offset:offset];
}

- (void)setSoundID:(NSInteger)snd filename:(NSString *)filename length:(NSUInteger)length offset:(NSUInteger)offset {

    SoundResource *res = _resources[@(snd)];

    if (res == nil) {
        res = [[SoundResource alloc] initWithFilename:filename offset:offset length:length];
        _resources[@(snd)] = res;
    } else if (res.data) {
        return;
    }

    res.length = length;
    res.offset = offset;

    res.soundFile = _files[filename];
    if (!res.soundFile) {
        res.soundFile = [[SoundFile alloc] initWithPath:filename];
        res.soundFile.handler = self;
        _files[filename] = res.soundFile;
    }
    [res load];
}

- (int)handleNewSoundChannel:(glui32)volume {
    int i;
    for (i = 0; i < MAXSND; i++)
        if (_glkchannels[@(i)] == nil)
            break;

    if (i == MAXSND)
        return -1;

    _glkchannels[@(i)] = [[GlkSoundChannel alloc] initWithHandler:self
                                                             name:i
                                                           volume:volume];
    return i;
}

- (void)handleDeleteChannel:(int)channel {
    [_glkchannels[@(channel)] stop];
    _glkchannels[@(channel)] = nil;
}

- (void)handleSetVolume:(glui32)volume channel:(int)channel duration:(glui32)duration notify:(glui32)notify {
    [_glkchannels[@(channel)] setVolume:volume duration:duration notification:notify];
}

/* MIDI and sampled sound are played by different channel classes. Swap the
   channel for one of the class the sound needs, carrying its state across. */
- (GlkSoundChannel *)channel:(GlkSoundChannel *)glkchan ofClass:(Class)channelClass {
    if ([glkchan isMemberOfClass:channelClass])
        return glkchan;
    int channel = glkchan.name;
    GlkSoundChannel *replacement = [[channelClass alloc] initWithHandler:self
                                                                    name:channel
                                                                  volume:GLK_MAXVOLUME];
    _glkchannels[@(channel)] = replacement;
    [glkchan copyValues:replacement];
    return replacement;
}

- (void)handlePlaySoundOnChannel:(int)channel repeats:(glsi32)repeats notify:(glui32)notify {
    GlkSoundChannel *glkchan = _glkchannels[@(channel)];
    if (_lastsoundresno == -1 || !glkchan)
        return;

    BOOL isMIDI = (_resources[@(_lastsoundresno)].type == GlkSoundBlorbFormatMIDI);
    glkchan = [self channel:glkchan ofClass:isMIDI ? [MIDIChannel class] : [GlkSoundChannel class]];

    BOOL result = [glkchan playSound:_lastsoundresno countOfRepeats:repeats notification:notify];
    if (!result) {
        NSLog(@"handlePlaySoundOnChannel: failed to play sound %d on channel %d.", _lastsoundresno, channel);
    }
}

- (void)handleStopSoundOnChannel:(int)channel {
    [_glkchannels[@(channel)] stop];
}

- (void)handlePauseOnChannel:(int)channel {
    [_glkchannels[@(channel)] pause];
}

- (void)handleUnpauseOnChannel:(int)channel {
    [_glkchannels[@(channel)] unpause];
}

- (void)restartAll {
    for (GlkSoundChannel *chan in _glkchannels.allValues) {
        chan.handler = self;
        [chan restartInternal];
    }
}

- (void)stopAllAndCleanUp {
    for (GlkSoundChannel* chan in _glkchannels.allValues) {
        [chan stop];
    }
    [self stopBeeps];
    [[NowPlayingCoordinator shared] clearForHandler:self];
}

#pragma mark Now Playing

- (void)nowPlayingStateDidChange {
    [[NowPlayingCoordinator shared] refreshForHandler:self];
}

- (NSArray<GlkSoundChannel *> *)eligibleNowPlayingChannels {
    NSMutableArray<GlkSoundChannel *> *eligible = [NSMutableArray new];
    for (GlkSoundChannel *chan in _glkchannels.allValues) {
        if (chan.claimsNowPlaying)
            [eligible addObject:chan];
    }
    return eligible;
}

- (void)pauseEligibleNowPlayingChannels {
    for (GlkSoundChannel *chan in self.eligibleNowPlayingChannels) {
        if (!chan.isPaused)
            [chan pause];
    }
}

- (void)unpauseEligibleNowPlayingChannels {
    for (GlkSoundChannel *chan in self.eligibleNowPlayingChannels) {
        if (chan.isPaused)
            [chan unpause];
    }
}

- (BOOL)hasPlayingEligibleNowPlayingChannel {
    for (GlkSoundChannel *chan in self.eligibleNowPlayingChannels) {
        if (!chan.isPaused)
            return YES;
    }
    return NO;
}

#pragma mark Called from GlkSoundChannel

- (void)handleVolumeNotification:(unsigned int)notify {
    GlkEvent *gev = [[GlkEvent alloc] initVolumeNotify:notify];
    [_glkctl queueEvent:gev];
}

- (void)handleSoundNotification:(glui32)notify withSound:(glsi32)sound {
    GlkEvent *gev = [[GlkEvent alloc] initSoundNotify:notify withSound:sound];
    [_glkctl queueEvent:gev];
}

-(GlkSoundBlorbFormatType)loadSoundResourceFromSound:(glsi32)snd data:(NSData *__autoreleasing*)buf {
    SoundResource *resource = [self loadedResourceForSound:snd];
    if (!resource)
        return GlkSoundBlorbFormatNone;
    *buf = resource.data;
    return resource.type;
}

#pragma mark ZX Spectrum BEEP synthesis

static const NSUInteger kWAVHeaderSize = 44;

/* Synthesise a square wave at the given frequency for the given number of
 * milliseconds and queue it for playback. ZX Spectrum BEEPs are raw square
 * waves, so this reproduces their characteristic timbre. The little PCM buffer
 * is wrapped in a WAV header and handed to an NSSound, which is retained until
 * it finishes so it is not deallocated mid-note. (Both x86_64 and arm64 macOS
 * are little-endian, so the 16-bit samples are written in native byte order.)
 *
 * Playback is non-blocking: successive notes are scheduled to start when the
 * previous one ends, so a tune plays in rhythm while the interpreter continues
 * to print text and accept input. */
- (void)playSpectrumBeepFrequency:(int)frequency duration:(int)millisecs {
    if (frequency <= 0 || millisecs <= 0)
        return;

    const uint32_t sampleRate = 44100;
    const int16_t amplitude = 7000;
    uint32_t nSamples = (uint32_t)((uint64_t)sampleRate * (uint32_t)millisecs / 1000);
    if (nSamples == 0)
        return;

    NSMutableData *wav = [SoundHandler WAVDataWithSampleCount:nSamples sampleRate:sampleRate];
    int16_t *samples = (int16_t *)((uint8_t *)wav.mutableBytes + kWAVHeaderSize);
    /* Which half-period each sample falls in, counted exactly rather than with
     * a whole number of samples per half-period, which would put a 4000 Hz
     * tone at 4410 Hz. */
    for (uint32_t i = 0; i < nSamples; i++)
        samples[i] = (((uint64_t)i * (uint64_t)frequency * 2 / sampleRate) & 1) ? -amplitude : amplitude;

    NSSound *sound = [[NSSound alloc] initWithData:wav];
    if (!sound)
        return;
    sound.delegate = self;
    if (!self.beepSounds)
        self.beepSounds = [NSMutableArray new];
    [self.beepSounds addObject:sound];

    /* On real hardware the Quill engine spends a near-constant ~27 ms between
     * consecutive notes (bytecode dispatch + the ROM floating-point calculator
     * setting up the next BEEP before the beeper loop starts), so a tune is
     * audibly more spaced out than gapless playback. Reproduce that gap so the
     * rhythm matches the Spectrum. Measured in MAME from the Bored of the Rings
     * intro tune: 14 inter-note gaps, 26.2-29.8 ms, mean 27.4 ms. The gap is
     * only added to nextBeepTime, so it spaces out queued notes without delaying
     * the first note or notes separated by a real pause (MAX(now, ...) below). */
    const NSTimeInterval interNoteGap = 0.027;

    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    NSTimeInterval startTime = MAX(now, self.nextBeepTime);
    NSTimeInterval delay = startTime - now;
    self.nextBeepTime = startTime + millisecs / 1000.0 + interNoteGap;

    if (delay < 0.001) {
        [sound play];
    } else {
        __weak SoundHandler *weakSelf = self;
        NSUInteger generation = self.beepGeneration;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            SoundHandler *strongSelf = weakSelf;
            if (strongSelf && strongSelf.beepGeneration == generation)
                [sound play];
        });
    }
}

/* Silence the beeper: the note sounding now and every note queued after it.
 * Called when the game stops, so a tune does not outlive its window. */
- (void)stopBeeps {
    self.beepGeneration++;
    self.nextBeepTime = 0;
    NSArray<NSSound *> *sounds = self.beepSounds;
    self.beepSounds = nil;
    for (NSSound *sound in sounds) {
        sound.delegate = nil;
        [sound stop];
    }
}

/* A mono 16-bit PCM WAV file of nSamples zero (silent) samples, to be filled
 * in after the header. (Both x86_64 and arm64 macOS are little-endian, so the
 * header fields are written in native byte order.) */
+ (NSMutableData *)WAVDataWithSampleCount:(uint32_t)nSamples sampleRate:(uint32_t)sampleRate {
    uint32_t dataBytes = nSamples * sizeof(int16_t);

    NSMutableData *wav = [NSMutableData dataWithLength:kWAVHeaderSize + dataBytes];
    uint8_t *p = wav.mutableBytes;

    uint32_t riffSize = 36 + dataBytes, fmtSize = 16, byteRate = sampleRate * 2;
    uint16_t pcm = 1, channels = 1, blockAlign = 2, bits = 16;
    memcpy(p,      "RIFF", 4);  memcpy(p + 4,  &riffSize, 4);
    memcpy(p + 8,  "WAVE", 4);  memcpy(p + 12, "fmt ", 4);
    memcpy(p + 16, &fmtSize, 4);
    memcpy(p + 20, &pcm, 2);    memcpy(p + 22, &channels, 2);
    memcpy(p + 24, &sampleRate, 4);
    memcpy(p + 28, &byteRate, 4);
    memcpy(p + 32, &blockAlign, 2);
    memcpy(p + 34, &bits, 2);
    memcpy(p + 36, "data", 4);  memcpy(p + 40, &dataBytes, 4);
    return wav;
}

/* Warm up the CoreAudio output device ahead of the first real BEEP.
 *
 * The first NSSound played in an app session has to spin up the output device,
 * and that warm-up latency (longer than a couple of hundred ms) clips the onset
 * of whatever plays first — for a Quill game that is the very first note of the
 * intro tune, so it comes out garbled. Once the device has been used it stays
 * warm for the rest of the process lifetime, which is why closing the game and
 * relaunching it sounds fine. We reproduce that here by playing a short silent
 * sound when the game launches: the device warms up during the game's boot and
 * intro text, well before the tune, so the first note plays on a warm device.
 *
 * Called from -[GlkController runTerp...]; harmless for games that never beep. */
- (void)primeBeepAudio {
    if (self.beepAudioPrimed)
        return;
    self.beepAudioPrimed = YES;

    const uint32_t sampleRate = 44100;
    const uint32_t millisecs = 300;
    /* sample data left as zeroes: silence */
    NSMutableData *wav = [SoundHandler WAVDataWithSampleCount:sampleRate * millisecs / 1000 sampleRate:sampleRate];

    NSSound *sound = [[NSSound alloc] initWithData:wav];
    if (!sound)
        return;
    sound.delegate = self;
    self.beepPrimeSound = sound;
    [sound play];
}

- (void)sound:(NSSound *)sound didFinishPlaying:(BOOL)finished {
    if (sound == self.beepPrimeSound)
        self.beepPrimeSound = nil;
    else
        [self.beepSounds removeObject:sound];
}

@end
