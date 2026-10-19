//
//  SoundHandlerTests.m
//  SpatterlightTests
//
//  The parts of application/Sounds that need no audio hardware: sniffing a
//  sound resource's format, archiving the handler, its resources and its
//  channels for autosave, the channel state machine around a failed or empty
//  play, fade bookkeeping across an autosave or a channel swap, and the ZX
//  Spectrum beeper's square wave.
//
//  Nothing here plays a sound. A channel is only asked to play a sound that
//  does not exist, which fails before any player or audio graph is created.
//

#import <XCTest/XCTest.h>

#import "SoundHandler.h"
#import "GlkSoundChannel.h"
#import "MIDIChannel.h"
#import "SpectrumBeeper.h"

@interface GlkSoundChannel (Testing)
- (glui32)remainingFadeDuration;
- (void)volumeTimerCallback:(NSTimer *)aTimer;
@end

@interface SoundHandlerTests : XCTestCase
@end

@implementation SoundHandlerTests

#pragma mark Helpers

static NSData *DataWithPrefix(const char *prefix, NSUInteger prefixLength, NSUInteger offset, NSUInteger totalLength) {
    NSMutableData *data = [NSMutableData dataWithLength:totalLength];
    memset(data.mutableBytes, 'x', totalLength);
    memcpy((uint8_t *)data.mutableBytes + offset, prefix, prefixLength);
    return data;
}

static GlkSoundBlorbFormatType Detect(NSData *data) {
    return [SoundResource detectSoundFormatFromData:data];
}

- (id)roundTrip:(id<NSCoding>)object ofClass:(Class)cls {
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object requiringSecureCoding:NO error:&error];
    XCTAssertNotNil(data, @"%@", error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    XCTAssertNotNil(unarchiver, @"%@", error);
    unarchiver.requiresSecureCoding = NO;
    id result = [unarchiver decodeObjectOfClass:cls forKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    return result;
}

/// Decode an object of `cls` from an archive that holds no keys at all, as an
/// autosave written before those keys existed would.
- (id)decodeEmptyArchiveAs:(Class)cls {
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [archiver finishEncoding];
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:archiver.encodedData error:nil];
    unarchiver.requiresSecureCoding = NO;
    id result = [[cls alloc] initWithCoder:unarchiver];
    [unarchiver finishDecoding];
    return result;
}

#pragma mark Format detection

- (void)testDetectEmptyDataIsNone {
    XCTAssertEqual(Detect([NSData data]), GlkSoundBlorbFormatNone);
}

- (void)testDetectMagicThatFillsTheBuffer {
    XCTAssertEqual(Detect([NSData dataWithBytes:"FORM" length:4]), GlkSoundBlorbFormatFORM);
    XCTAssertEqual(Detect([NSData dataWithBytes:"RIFF" length:4]), GlkSoundBlorbFormatWave);
    XCTAssertEqual(Detect([NSData dataWithBytes:"MThd" length:4]), GlkSoundBlorbFormatMIDI);
    XCTAssertEqual(Detect([NSData dataWithBytes:"OggS" length:4]), GlkSoundBlorbFormatOggVorbis);
}

- (void)testDetectShortDataFallsBackToMP3 {
    XCTAssertEqual(Detect([NSData dataWithBytes:"MTh" length:3]), GlkSoundBlorbFormatMP3);
    XCTAssertEqual(Detect([NSData dataWithBytes:"I" length:1]), GlkSoundBlorbFormatMP3);
}

- (void)testDetectEachMagic {
    XCTAssertEqual(Detect(DataWithPrefix("FORM", 4, 0, 30)), GlkSoundBlorbFormatFORM);
    XCTAssertEqual(Detect(DataWithPrefix("WAVE", 4, 0, 30)), GlkSoundBlorbFormatWave);
    XCTAssertEqual(Detect(DataWithPrefix("RIFF", 4, 0, 29)), GlkSoundBlorbFormatWave);
    XCTAssertEqual(Detect(DataWithPrefix("MThd", 4, 0, 100)), GlkSoundBlorbFormatMIDI);
    XCTAssertEqual(Detect(DataWithPrefix("OggS", 4, 0, 100)), GlkSoundBlorbFormatOggVorbis);
    XCTAssertEqual(Detect(DataWithPrefix("SCRM", 4, 0x2c, 0x30)), GlkSoundBlorbFormatMod);
    XCTAssertEqual(Detect(DataWithPrefix("Extended Module: ", 17, 0, 17)), GlkSoundBlorbFormatMod);
    XCTAssertEqual(Detect(DataWithPrefix("M.K.", 4, 1080, 1084)), GlkSoundBlorbFormatMod);
    XCTAssertEqual(Detect(DataWithPrefix("8CHN", 4, 1080, 2000)), GlkSoundBlorbFormatMod);
    XCTAssertEqual(Detect(DataWithPrefix("32CN", 4, 1080, 2000)), GlkSoundBlorbFormatMod);
}

- (void)testDetectGarbageIsMP3 {
    XCTAssertEqual(Detect(DataWithPrefix("ID3", 3, 0, 2000)), GlkSoundBlorbFormatMP3);
    XCTAssertEqual(Detect(DataWithPrefix("zzzz", 4, 0, 2000)), GlkSoundBlorbFormatMP3);
}

#pragma mark Archiving

- (void)testSoundResourceKeepsOffsetsAbove4GB {
    NSUInteger offset = 5000000000ULL, length = 4300000000ULL;
    SoundResource *res = [[SoundResource alloc] initWithFilename:@"/tmp/big.blb" offset:offset length:length];
    res.type = GlkSoundBlorbFormatOggVorbis;
    SoundResource *copy = [self roundTrip:res ofClass:[SoundResource class]];
    XCTAssertEqual(copy.offset, offset);
    XCTAssertEqual(copy.length, length);
    XCTAssertEqual(copy.type, GlkSoundBlorbFormatOggVorbis);
    XCTAssertEqualObjects(copy.filename, @"/tmp/big.blb");
}

- (void)testSoundHandlerRoundTrip {
    SoundHandler *handler = [SoundHandler new];
    NSString *path = @"/tmp/Spatterlight sound test.gblorb";
    handler.files[path] = [[SoundFile alloc] initWithPath:path];
    SoundResource *res = [[SoundResource alloc] initWithFilename:path offset:12 length:34];
    res.type = GlkSoundBlorbFormatWave;
    handler.resources[@3] = res;
    int channel = [handler handleNewSoundChannel:0x8000];
    handler.lastsoundresno = 3;

    SoundHandler *copy = [self roundTrip:handler ofClass:[SoundHandler class]];
    XCTAssertEqual(copy.lastsoundresno, 3);
    XCTAssertEqualObjects(copy.files.allKeys, @[path]);
    XCTAssertEqual(copy.resources[@3].offset, 12U);
    XCTAssertEqual(copy.resources[@3].length, 34U);
    XCTAssertEqual(copy.resources[@3].soundFile, copy.files[path], @"resources are re-linked to their files");
    XCTAssertEqual(copy.files[path].handler, copy);
    XCTAssertNotNil(copy.glkchannels[@(channel)]);
    XCTAssertEqual(copy.glkchannels[@(channel)].name, channel);
}

- (void)testSoundHandlerDecodedWithoutKeysStillWorks {
    SoundHandler *handler = [self decodeEmptyArchiveAs:[SoundHandler class]];
    XCTAssertNotNil(handler.files);
    XCTAssertNotNil(handler.resources);
    XCTAssertNotNil(handler.glkchannels);
    XCTAssertEqual(handler.lastsoundresno, -1);

    int channel = [handler handleNewSoundChannel:0x10000];
    XCTAssertEqual(channel, 0);
    XCTAssertNotNil(handler.glkchannels[@0], @"a new channel is actually stored");
}

#pragma mark Channel state

- (void)testFailedPlayLeavesChannelIdle {
    SoundHandler *handler = [SoundHandler new];
    int name = [handler handleNewSoundChannel:0x10000];
    GlkSoundChannel *chan = handler.glkchannels[@(name)];

    XCTAssertFalse([chan playSound:42 countOfRepeats:1 notification:0], @"there is no sound 42");
    XCTAssertEqual(chan.status, GlkSoundChannelStatusIdle);
    XCTAssertFalse([chan playSound:-1 countOfRepeats:1 notification:0]);
    XCTAssertEqual(chan.status, GlkSoundChannelStatusIdle);
    XCTAssertFalse(chan.claimsNowPlaying);
}

- (void)testUnpauseOfIdleChannelOnlyClearsPause {
    SoundHandler *handler = [SoundHandler new];
    int name = [handler handleNewSoundChannel:0x10000];
    GlkSoundChannel *chan = handler.glkchannels[@(name)];

    [chan pause];
    XCTAssertTrue(chan.isPaused);
    [chan unpause];
    XCTAssertFalse(chan.isPaused);
    XCTAssertEqual(chan.status, GlkSoundChannelStatusIdle);
}

#pragma mark Fades

/// A one second fade, half run.
- (GlkSoundChannel *)channelHalfwayThroughFade {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x10000];
    [chan setVolume:0 duration:1000 notification:0];
    for (int i = 0; i < 50; i++)
        [chan volumeTimerCallback:nil];
    return chan;
}

- (void)testFadeRemainingDuration {
    GlkSoundChannel *chan = [self channelHalfwayThroughFade];
    XCTAssertEqual(chan.remainingFadeDuration, 500U);
    XCTAssertEqualWithAccuracy([[chan valueForKey:@"volume"] floatValue], 0.5f, 0.001f);
    [chan stop];
}

- (void)testFadeRemainingDurationSurvivesArchiving {
    GlkSoundChannel *chan = [self channelHalfwayThroughFade];
    GlkSoundChannel *copy = [self roundTrip:chan ofClass:[GlkSoundChannel class]];
    [chan stop];
    XCTAssertEqual(copy.remainingFadeDuration, 500U);
    XCTAssertEqualWithAccuracy([[copy valueForKey:@"volume"] floatValue], 0.5f, 0.001f);
}

- (void)testFadeFromOldAutosaveAssumesOneSecond {
    GlkSoundChannel *chan = [self decodeEmptyArchiveAs:[GlkSoundChannel class]];
    [chan setValue:@25 forKey:@"volume_timeout"];
    XCTAssertEqual(chan.remainingFadeDuration, 250U);
}

- (void)testChannelSwapCarriesVolumeAndFade {
    GlkSoundChannel *chan = [self channelHalfwayThroughFade];
    MIDIChannel *midi = [[MIDIChannel alloc] initWithHandler:nil name:0 volume:0x10000];
    [chan copyValues:midi];

    XCTAssertEqualWithAccuracy([[midi valueForKey:@"volume"] floatValue], 0.5f, 0.001f,
                               @"the fade continues from where it was, not from full volume");
    XCTAssertEqual(midi.remainingFadeDuration, 500U);
    XCTAssertEqualWithAccuracy([[midi valueForKey:@"target_volume"] floatValue], 0.0f, 0.001f);
    [midi stop];
}

- (void)testVolumeAboveGlkMaximumIsFull {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x30000];
    XCTAssertEqualWithAccuracy([[chan valueForKey:@"volume"] floatValue], 1.0f, 0.0001f);
}

#pragma mark Beeper

- (void)testWAVHeader {
    NSData *wav = [SpectrumBeeper WAVDataWithSampleCount:100 sampleRate:44100];
    XCTAssertEqual(wav.length, 44U + 200U);
    const uint8_t *p = wav.bytes;
    XCTAssertEqual(memcmp(p, "RIFF", 4), 0);
    XCTAssertEqual(memcmp(p + 8, "WAVEfmt ", 8), 0);
    XCTAssertEqual(memcmp(p + 36, "data", 4), 0);
    uint32_t riffSize, sampleRate, byteRate, dataBytes;
    uint16_t format, channels, bits;
    memcpy(&riffSize, p + 4, 4);
    memcpy(&format, p + 20, 2);
    memcpy(&channels, p + 22, 2);
    memcpy(&sampleRate, p + 24, 4);
    memcpy(&byteRate, p + 28, 4);
    memcpy(&bits, p + 34, 2);
    memcpy(&dataBytes, p + 40, 4);
    XCTAssertEqual(riffSize, 36U + 200U);
    XCTAssertEqual(format, 1);
    XCTAssertEqual(channels, 1);
    XCTAssertEqual(sampleRate, 44100U);
    XCTAssertEqual(byteRate, 88200U);
    XCTAssertEqual(bits, 16);
    XCTAssertEqual(dataBytes, 200U);
}

/// Count sign changes over one second of beep: a square wave of f Hz has 2f.
- (void)testBeepPitchIsAccurate {
    const int frequencies[] = { 100, 261, 440, 1000, 2000, 4000, 8000 };
    for (size_t f = 0; f < sizeof frequencies / sizeof *frequencies; f++) {
        const int frequency = frequencies[f];
        NSData *wav = [SpectrumBeeper WAVWithFrequency:frequency duration:1000];
        XCTAssertEqual(wav.length, 44U + 44100U * 2U);
        const int16_t *samples = (const int16_t *)((const uint8_t *)wav.bytes + 44);
        int changes = 0;
        for (int i = 1; i < 44100; i++)
            if ((samples[i] < 0) != (samples[i - 1] < 0))
                changes++;
        XCTAssertEqualWithAccuracy(changes, 2 * frequency, 1, @"%d Hz", frequency);
    }
}

- (void)testEmptyBeepIsNil {
    XCTAssertNil([SpectrumBeeper WAVWithFrequency:0 duration:100]);
    XCTAssertNil([SpectrumBeeper WAVWithFrequency:440 duration:0]);
}

@end
