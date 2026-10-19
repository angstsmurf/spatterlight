//
//  SpectrumBeeper.m
//  Spatterlight
//

#import "SpectrumBeeper.h"

#import <AppKit/AppKit.h>

@interface SpectrumBeeper () <NSSoundDelegate>
/* Synthesised ZX Spectrum BEEP tones, held strongly while they play or wait
 * their turn. */
@property (strong, nullable) NSMutableArray<NSSound *> *sounds;
/* Bumped by -stop, so that notes still waiting to start can tell that the tune
 * they belonged to has been cancelled. */
@property (nonatomic) NSUInteger generation;
/* Absolute time (NSDate referenceDate epoch, in seconds) at which the next
 * queued BEEP should start. Used to chain a tune's notes back-to-back without
 * blocking the interpreter. */
@property (nonatomic) NSTimeInterval nextBeepTime;
/* A short silent sound played at game launch to warm up the CoreAudio output
 * device ahead of the first real tone, held strongly while it plays. See
 * -prime. */
@property (strong, nullable) NSSound *primeSound;
@property (nonatomic) BOOL primed;
@end

@implementation SpectrumBeeper

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
- (void)playFrequency:(int)frequency duration:(int)millisecs {
    NSData *wav = [SpectrumBeeper WAVWithFrequency:frequency duration:millisecs];
    if (!wav)
        return;

    NSSound *sound = [[NSSound alloc] initWithData:wav];
    if (!sound)
        return;
    sound.delegate = self;
    if (!self.sounds)
        self.sounds = [NSMutableArray new];
    [self.sounds addObject:sound];

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
        __weak SpectrumBeeper *weakSelf = self;
        NSUInteger tune = self.generation;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            SpectrumBeeper *strongSelf = weakSelf;
            if (strongSelf && strongSelf.generation == tune)
                [sound play];
        });
    }
}

/* The square wave for one BEEP as a WAV file, or nil if it would be empty. */
+ (nullable NSData *)WAVWithFrequency:(int)frequency duration:(int)millisecs {
    if (frequency <= 0 || millisecs <= 0)
        return nil;

    const uint32_t sampleRate = 44100;
    const int16_t amplitude = 7000;
    uint32_t nSamples = (uint32_t)((uint64_t)sampleRate * (uint32_t)millisecs / 1000);
    if (nSamples == 0)
        return nil;

    NSMutableData *wav = [SpectrumBeeper WAVDataWithSampleCount:nSamples sampleRate:sampleRate];
    int16_t *samples = (int16_t *)((uint8_t *)wav.mutableBytes + kWAVHeaderSize);
    /* Which half-period each sample falls in, counted exactly rather than with
     * a whole number of samples per half-period, which would put a 4000 Hz
     * tone at 4410 Hz. */
    for (uint32_t i = 0; i < nSamples; i++)
        samples[i] = (((uint64_t)i * (uint64_t)frequency * 2 / sampleRate) & 1) ? -amplitude : amplitude;
    return wav;
}

/* Silence the beeper: the note sounding now and every note queued after it.
 * Called when the game stops, so a tune does not outlive its window. */
- (void)stop {
    self.generation++;
    self.nextBeepTime = 0;
    NSArray<NSSound *> *sounds = self.sounds;
    self.sounds = nil;
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
 * Called from -[GlkController runTerp...] via SoundHandler; harmless for games that never beep. */
- (void)prime {
    if (self.primed)
        return;
    self.primed = YES;

    const uint32_t sampleRate = 44100;
    const uint32_t millisecs = 300;
    /* sample data left as zeroes: silence */
    NSMutableData *wav = [SpectrumBeeper WAVDataWithSampleCount:sampleRate * millisecs / 1000 sampleRate:sampleRate];

    NSSound *sound = [[NSSound alloc] initWithData:wav];
    if (!sound)
        return;
    sound.delegate = self;
    self.primeSound = sound;
    [sound play];
}

- (void)sound:(NSSound *)sound didFinishPlaying:(BOOL)finished {
    if (sound == self.primeSound)
        self.primeSound = nil;
    else
        [self.sounds removeObject:sound];
}

@end
