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
#import "SpectrumBeeper.h"
#import "SFBSoundBackend.h"
#import "MIDISoundBackend.h"
#import "NowPlayingCoordinator.h"

@interface GlkSoundChannel (Testing)
- (glui32)remainingFadeDuration;
- (void)fadeOneStep;
@end

/// Records sound notifications instead of queueing Glk events, and keeps out
/// of the system Now Playing session.
@interface RecordingSoundHandler : SoundHandler
@property NSMutableArray<NSArray<NSNumber *> *> *soundNotifications;
@property (nullable) void (^onSoundNotification)(void);
@end

@implementation RecordingSoundHandler
- (void)handleSoundNotification:(glui32)notify withSound:(glsi32)sound {
    if (!_soundNotifications)
        _soundNotifications = [NSMutableArray new];
    [_soundNotifications addObject:@[@(notify), @(sound)]];
    if (_onSoundNotification)
        _onSoundNotification();
}
- (void)nowPlayingStateDidChange {
}
@end

/// Writes a channel the way builds before 2026-10-20 did, with -encodeInt:
/// and -encodeInteger: for the 32-bit values.
@interface OldStyleChannelArchive : NSObject <NSSecureCoding>
@end

@implementation OldStyleChannelArchive
+ (BOOL)supportsSecureCoding {
    return YES;
}
- (instancetype)initWithCoder:(NSCoder *)decoder {
    return [super init];
}
- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInt:5 forKey:@"name"];
    [encoder encodeInt:9 forKey:@"resid"];
    [encoder encodeInteger:1 forKey:@"status"];
    [encoder encodeDouble:0.75 forKey:@"volume"];
    [encoder encodeInt:-1 forKey:@"loop"];
    [encoder encodeInt:(int)0x80000001U forKey:@"notify"];
    [encoder encodeInt:1 forKey:@"paused"];
    [encoder encodeInt:(int)0xFFFFFFFEU forKey:@"volume_notify"];
    [encoder encodeInt:40 forKey:@"volume_timeout"];
    [encoder encodeDouble:0.02 forKey:@"volume_tick"];
    [encoder encodeDouble:0.25 forKey:@"target_volume"];
    [encoder encodeInt64:1234 forKey:@"frame"];
}
@end

/// Stands in for MPNowPlayingInfoCenter.
@interface FakeNowPlayingInfoCenter : NSObject <NowPlayingInfoCenter>
@end

@implementation FakeNowPlayingInfoCenter
@synthesize nowPlayingInfo, playbackState;
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
    XCTAssertEqual(Detect(DataWithPrefix("IMPM", 4, 0, 200)), GlkSoundBlorbFormatMod);
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
    XCTAssertFalse(chan.isPlaying);
    XCTAssertFalse([chan playSound:-1 countOfRepeats:1 notification:0]);
    XCTAssertFalse(chan.isPlaying);
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
    XCTAssertFalse(chan.isPlaying);
}

#pragma mark Fades

/// A one second fade, half run.
- (GlkSoundChannel *)channelHalfwayThroughFade {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x10000];
    [chan setVolume:0 duration:1000 notification:0];
    for (int i = 0; i < 50; i++)
        [chan fadeOneStep];
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

- (void)testFadeTimerRunsToTarget {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x10000];
    [chan setVolume:0x8000 duration:100 notification:0];
    NSPredicate *done = [NSPredicate predicateWithBlock:^BOOL(GlkSoundChannel *c, NSDictionary *bindings) {
        return [[c valueForKey:@"fadeStepsLeft"] unsignedIntValue] == 0;
    }];
    [self waitForExpectations:@[[self expectationForPredicate:done evaluatedWithObject:chan handler:nil]] timeout:5];
    XCTAssertEqualWithAccuracy([[chan valueForKey:@"volume"] floatValue], 0.5f, 0.0001f);
    XCTAssertNil([chan valueForKey:@"fadeTimer"], @"a finished fade cancels its timer");
}

- (void)testStopCancelsFade {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x10000];
    [chan setVolume:0 duration:100000 notification:0];
    XCTAssertNotNil([chan valueForKey:@"fadeTimer"]);
    [chan stop];
    XCTAssertNil([chan valueForKey:@"fadeTimer"]);
}

/// The fade timer holds the channel weakly, so a channel let go mid-fade is
/// freed rather than kept alive until the fade ends.
- (void)testFadeDoesNotKeepChannelAlive {
    GlkSoundChannel __weak *weakChan = nil;
    @autoreleasepool {
        GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x10000];
        [chan setVolume:0 duration:1000 notification:0];
        weakChan = chan;
    }
    XCTAssertNil(weakChan);
    // Let any step already queued run against the freed channel.
    [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
}

- (void)testFadeFromOldAutosaveAssumesOneSecond {
    GlkSoundChannel *chan = [self decodeEmptyArchiveAs:[GlkSoundChannel class]];
    [chan setValue:@25 forKey:@"fadeStepsLeft"];
    XCTAssertEqual(chan.remainingFadeDuration, 250U);
}

/// A channel that was playing, with nothing restarted since: what an autosave
/// holds for a channel just deserialized.
- (GlkSoundChannel *)playingChannelAt:(double)position MIDI:(BOOL)midi {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:3 volume:0x10000];
    chan.playing = YES;
    [chan setValue:@(midi) forKey:@"soundIsMIDI"];
    [chan setValue:@(position) forKey:@"resumePosition"];
    [chan setValue:@4 forKey:@"loop"];
    [chan setValue:@7 forKey:@"resid"];
    return chan;
}

- (void)testMIDIChannelArchivesUnderOldClassName {
    GlkSoundChannel *chan = [self playingChannelAt:12.5 MIDI:YES];
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:chan requiringSecureCoding:YES error:&error];
    XCTAssertNotNil(data, @"%@", error);
    GlkSoundChannel *copy = [NSKeyedUnarchiver unarchivedObjectOfClasses:[NSSet setWithObjects:[GlkSoundChannel class], [MIDIChannel class], nil] fromData:data error:&error];
    XCTAssertNotNil(copy, @"%@", error);
    XCTAssertTrue([copy isMemberOfClass:[MIDIChannel class]],
                  @"an older build reading the autosave needs the MIDI class name");
    XCTAssertEqualWithAccuracy([[copy valueForKey:@"resumePosition"] doubleValue], 12.5, 0.0001);
    XCTAssertTrue([[copy valueForKey:@"soundIsMIDI"] boolValue]);
    XCTAssertEqual([[copy valueForKey:@"loop"] intValue], 4);
    XCTAssertTrue(copy.isPlaying);
}

- (void)testOldStyleChannelArchiveDecodes {
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [archiver setClassName:@"GlkSoundChannel" forClass:[OldStyleChannelArchive class]];
    [archiver encodeObject:[OldStyleChannelArchive new] forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];
    NSError *error = nil;
    GlkSoundChannel *chan = [NSKeyedUnarchiver unarchivedObjectOfClass:[GlkSoundChannel class] fromData:archiver.encodedData error:&error];
    XCTAssertNotNil(chan, @"%@", error);
    XCTAssertEqual(chan.name, 5);
    XCTAssertTrue(chan.isPlaying);
    XCTAssertTrue(chan.isPaused);
    XCTAssertEqual([[chan valueForKey:@"resid"] intValue], 9);
    XCTAssertEqual([[chan valueForKey:@"loop"] intValue], -1);
    XCTAssertEqual([[chan valueForKey:@"notify"] unsignedIntValue], 0x80000001U);
    XCTAssertEqual([[chan valueForKey:@"fadeNotify"] unsignedIntValue], 0xFFFFFFFEU);
    XCTAssertEqual(chan.remainingFadeDuration, 800U);
    XCTAssertEqualWithAccuracy([[chan valueForKey:@"resumePosition"] doubleValue], 1234, 0.0001);
}

- (void)testSampledChannelArchivesFrame {
    GlkSoundChannel *chan = [self playingChannelAt:44100 MIDI:NO];
    GlkSoundChannel *copy = [self roundTrip:chan ofClass:[GlkSoundChannel class]];
    XCTAssertTrue([copy isMemberOfClass:[GlkSoundChannel class]]);
    XCTAssertFalse([[copy valueForKey:@"soundIsMIDI"] boolValue]);
    XCTAssertEqualWithAccuracy([[copy valueForKey:@"resumePosition"] doubleValue], 44100, 0.0001);
    XCTAssertEqual([[copy valueForKey:@"loop"] intValue], 4);
}

- (void)testIdleChannelArchivesNoPosition {
    GlkSoundChannel *chan = [self playingChannelAt:500 MIDI:NO];
    chan.playing = NO;
    GlkSoundChannel *copy = [self roundTrip:chan ofClass:[GlkSoundChannel class]];
    XCTAssertEqualWithAccuracy([[copy valueForKey:@"resumePosition"] doubleValue], 0, 0.0001);
}

- (void)testVolumeAboveGlkMaximumIsFull {
    GlkSoundChannel *chan = [[GlkSoundChannel alloc] initWithHandler:nil name:0 volume:0x30000];
    XCTAssertEqualWithAccuracy([[chan valueForKey:@"volume"] floatValue], 1.0f, 0.0001f);
}

#pragma mark Now Playing arbitration

/// A channel on `handler` that looks like it is playing a sound of
/// `seconds`, or looping.
- (GlkSoundChannel *)channelOn:(SoundHandler *)handler seconds:(NSTimeInterval)seconds looping:(BOOL)looping {
    int name = [handler handleNewSoundChannel:0x10000];
    GlkSoundChannel *chan = handler.glkchannels[@(name)];
    chan.playing = YES;
    chan.nowPlayingDuration = seconds;
    chan.nowPlayingLooping = looping;
    return chan;
}

- (void)testShortSoundDoesNotClaimNowPlaying {
    FakeNowPlayingInfoCenter *center = [FakeNowPlayingInfoCenter new];
    NowPlayingCoordinator *coordinator = [[NowPlayingCoordinator alloc] initWithInfoCenter:center];
    RecordingSoundHandler *handler = [RecordingSoundHandler new];
    [self channelOn:handler seconds:1 looping:NO];
    [coordinator refreshForHandler:handler];
    XCTAssertNil(center.nowPlayingInfo);
    XCTAssertNil(coordinator.activeHandler);
}

- (void)testLongSoundClaimsWithDuration {
    FakeNowPlayingInfoCenter *center = [FakeNowPlayingInfoCenter new];
    NowPlayingCoordinator *coordinator = [[NowPlayingCoordinator alloc] initWithInfoCenter:center];
    RecordingSoundHandler *handler = [RecordingSoundHandler new];
    GlkSoundChannel *chan = [self channelOn:handler seconds:34 looping:NO];
    [coordinator refreshForHandler:handler];
    XCTAssertEqual(coordinator.activeHandler, handler);
    XCTAssertEqualObjects(center.nowPlayingInfo[MPMediaItemPropertyPlaybackDuration], @34);
    XCTAssertEqualObjects(center.nowPlayingInfo[MPNowPlayingInfoPropertyPlaybackRate], @1.0);
    XCTAssertEqual(center.playbackState, MPNowPlayingPlaybackStatePlaying);

    [chan pause];
    [coordinator refreshForHandler:handler];
    XCTAssertEqualObjects(center.nowPlayingInfo[MPNowPlayingInfoPropertyPlaybackRate], @0.0);
    XCTAssertEqual(center.playbackState, MPNowPlayingPlaybackStatePaused);

    [chan stop];
    [coordinator refreshForHandler:handler];
    XCTAssertNil(center.nowPlayingInfo);
    XCTAssertEqual(center.playbackState, MPNowPlayingPlaybackStateStopped);
    XCTAssertNil(coordinator.activeHandler);
}

/// A loop has no length to show, however short one pass is.
- (void)testLoopingSoundClaimsWithoutDuration {
    FakeNowPlayingInfoCenter *center = [FakeNowPlayingInfoCenter new];
    NowPlayingCoordinator *coordinator = [[NowPlayingCoordinator alloc] initWithInfoCenter:center];
    RecordingSoundHandler *handler = [RecordingSoundHandler new];
    [self channelOn:handler seconds:2 looping:YES];
    [coordinator refreshForHandler:handler];
    XCTAssertNotNil(center.nowPlayingInfo);
    XCTAssertNil(center.nowPlayingInfo[MPMediaItemPropertyPlaybackDuration]);
}

/// Game A plays music, game B takes over, A stops: B keeps the session.
- (void)testSecondGameKeepsSessionWhenFirstStops {
    FakeNowPlayingInfoCenter *center = [FakeNowPlayingInfoCenter new];
    NowPlayingCoordinator *coordinator = [[NowPlayingCoordinator alloc] initWithInfoCenter:center];
    RecordingSoundHandler *a = [RecordingSoundHandler new];
    RecordingSoundHandler *b = [RecordingSoundHandler new];
    GlkSoundChannel *chanA = [self channelOn:a seconds:0 looping:YES];
    [coordinator refreshForHandler:a];
    [self channelOn:b seconds:0 looping:YES];
    [coordinator refreshForHandler:b];
    XCTAssertEqual(coordinator.activeHandler, b);

    [chanA stop];
    [coordinator refreshForHandler:a];
    XCTAssertEqual(coordinator.activeHandler, b);
    XCTAssertNotNil(center.nowPlayingInfo);
    XCTAssertEqual(center.playbackState, MPNowPlayingPlaybackStatePlaying);

    [coordinator clearForHandler:a];
    XCTAssertNotNil(center.nowPlayingInfo, @"only the owner clears the session");
    [coordinator clearForHandler:b];
    XCTAssertNil(center.nowPlayingInfo);
}

/// The owner going away (window closed) lets anyone clear the session.
- (void)testSessionOfFreedHandlerCanBeCleared {
    FakeNowPlayingInfoCenter *center = [FakeNowPlayingInfoCenter new];
    NowPlayingCoordinator *coordinator = [[NowPlayingCoordinator alloc] initWithInfoCenter:center];
    @autoreleasepool {
        RecordingSoundHandler *a = [RecordingSoundHandler new];
        [self channelOn:a seconds:0 looping:YES];
        [coordinator refreshForHandler:a];
        XCTAssertNotNil(coordinator.activeHandler);
        [a.glkchannels.allValues makeObjectsPerformSelector:@selector(stop)];
    }
    XCTAssertNil(coordinator.activeHandler);
    RecordingSoundHandler *other = [RecordingSoundHandler new];
    [coordinator clearForHandler:other];
    XCTAssertNil(center.nowPlayingInfo);
}

#pragma mark Playback through the handler

/// A format 0 MIDI file with one quarter-second note.
static NSData *TinyMIDI(void) {
    static const uint8_t bytes[] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0,
        'M', 'T', 'r', 'k', 0, 0, 0, 13,
        0x00, 0x90, 60, 64,
        0x83, 0x60, 0x80, 60, 0,
        0x00, 0xFF, 0x2F, 0x00,
    };
    return [NSData dataWithBytes:bytes length:sizeof bytes];
}

- (NSString *)writeSound:(NSData *)data named:(NSString *)name {
    NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:
                      [NSString stringWithFormat:@"SoundHandlerTests-%@-%@", NSUUID.UUID.UUIDString, name]];
    XCTAssertTrue([data writeToFile:path atomically:YES]);
    [self addTeardownBlock:^{
        [NSFileManager.defaultManager removeItemAtPath:path error:nil];
    }];
    return path;
}

- (void)loadSound:(glsi32)resno data:(NSData *)data named:(NSString *)name into:(SoundHandler *)handler {
    [handler handleLoadSoundNumber:resno from:[self writeSound:data named:name] offset:0 length:data.length];
    XCTAssertTrue([handler handleFindSoundNumber:resno]);
}

/// Wait until `handler` has seen `count` sound notifications.
- (void)waitForNotifications:(NSUInteger)count from:(RecordingSoundHandler *)handler {
    XCTestExpectation *seen = [self expectationWithDescription:@"sound notifications"];
    __weak RecordingSoundHandler *weakHandler = handler;
    void (^check)(void) = ^{
        if (weakHandler.soundNotifications.count == count) {
            weakHandler.onSoundNotification = nil;
            [seen fulfill];
        }
    };
    handler.onSoundNotification = check;
    if (handler.soundNotifications.count == count)
        check();
    [self waitForExpectations:@[seen] timeout:5];
}

/// Volume 0 throughout, so the tests make no sound.
- (void)testSampledSoundNotifiesAfterLastRepeat {
    RecordingSoundHandler *handler = [RecordingSoundHandler new];
    int name = [handler handleNewSoundChannel:0];
    [self loadSound:5 data:[SpectrumBeeper WAVWithFrequency:440 duration:50] named:@"beep.wav" into:handler];
    [handler handlePlaySoundOnChannel:name repeats:3 notify:9];
    GlkSoundChannel *chan = handler.glkchannels[@(name)];
    XCTAssertTrue(chan.isPlaying);
    [self waitForNotifications:1 from:handler];
    XCTAssertEqualObjects(handler.soundNotifications, (@[@[@9, @5]]));
    XCTAssertFalse(chan.isPlaying);
    [handler stopAllAndCleanUp];
}

- (void)testOneChannelPlaysMIDIThenSampled {
    RecordingSoundHandler *handler = [RecordingSoundHandler new];
    int name = [handler handleNewSoundChannel:0];
    GlkSoundChannel *chan = handler.glkchannels[@(name)];

    [self loadSound:1 data:TinyMIDI() named:@"note.mid" into:handler];
    [handler handlePlaySoundOnChannel:name repeats:1 notify:1];
    XCTAssertTrue(chan.isPlaying);
    [self waitForNotifications:1 from:handler];

    [self loadSound:2 data:[SpectrumBeeper WAVWithFrequency:440 duration:50] named:@"beep.wav" into:handler];
    [handler handlePlaySoundOnChannel:name repeats:1 notify:2];
    [self waitForNotifications:2 from:handler];

    XCTAssertEqual(handler.glkchannels[@(name)], chan, @"the channel is not swapped for another");
    XCTAssertEqualObjects(handler.soundNotifications, (@[@[@1, @1], @[@2, @2]]));
    [handler stopAllAndCleanUp];
}

/// A sound started on a paused channel, autosaved and restored, waits for
/// the unpause and then plays to its end.
- (void)testPausedSoundSurvivesAutosave {
    RecordingSoundHandler *handler = [RecordingSoundHandler new];
    int name = [handler handleNewSoundChannel:0];
    [self loadSound:4 data:[SpectrumBeeper WAVWithFrequency:440 duration:50] named:@"beep.wav" into:handler];
    [handler handlePauseOnChannel:name];
    [handler handlePlaySoundOnChannel:name repeats:1 notify:7];

    RecordingSoundHandler *restored = [self roundTrip:handler ofClass:[RecordingSoundHandler class]];
    [handler stopAllAndCleanUp];
    XCTAssertNotNil(restored);
    [restored restartAll];
    GlkSoundChannel *chan = restored.glkchannels[@(name)];
    XCTAssertTrue(chan.isPaused);
    XCTAssertTrue(chan.isPlaying);

    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
    XCTAssertEqual(restored.soundNotifications.count, 0U, @"nothing plays while paused");

    [restored handleUnpauseOnChannel:name];
    [self waitForNotifications:1 from:restored];
    XCTAssertEqualObjects(restored.soundNotifications, (@[@[@7, @4]]));
    XCTAssertEqual(handler.soundNotifications.count, 0U, @"the stopped original stays quiet");
    [restored stopAllAndCleanUp];
}

/// Format 1, 480 ticks a beat: a one-beat note on channel 1 and a three-beat
/// note on channel 2, each in its own track.
static NSData *UnevenTracksMIDI(void) {
    static const uint8_t bytes[] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x01, 0xE0,
        'M', 'T', 'r', 'k', 0, 0, 0, 13,
        0x00, 0x90, 60, 64,
        0x83, 0x60, 0x80, 60, 0,
        0x00, 0xFF, 0x2F, 0x00,
        'M', 'T', 'r', 'k', 0, 0, 0, 13,
        0x00, 0x91, 64, 64,
        0x8B, 0x20, 0x81, 64, 0,
        0x00, 0xFF, 0x2F, 0x00,
    };
    return [NSData dataWithBytes:bytes length:sizeof bytes];
}

/// Seconds from play to the finish call, after making sure there is only one.
- (NSTimeInterval)secondsToFinishMIDI:(NSData *)midi repeats:(glsi32)repeats {
    MIDISoundBackend *backend = [MIDISoundBackend new];
    XCTestExpectation *done = [self expectationWithDescription:@"finished"];
    NSDate *start = [NSDate date];
    __block NSTimeInterval took = 0;
    __block int calls = 0;
    XCTAssertTrue([backend playData:midi format:GlkSoundBlorbFormatMIDI repeats:repeats position:0 paused:NO volume:0 finished:^{
        if (calls++ == 0) {
            took = -start.timeIntervalSinceNow;
            [done fulfill];
        }
    }]);
    [self waitForExpectations:@[done] timeout:10];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.7]];
    XCTAssertEqual(calls, 1);
    [backend stop];
    return took;
}

/// A loop count of N is N passes, each half a second at the default tempo.
- (void)testMIDIRepeatsArePasses {
    XCTAssertGreaterThan([self secondsToFinishMIDI:TinyMIDI() repeats:3], 1.4);
}

/// The sequence ends with its longest track, not with the first.
- (void)testMIDIFinishesWithLongestTrack {
    XCTAssertGreaterThan([self secondsToFinishMIDI:UnevenTracksMIDI() repeats:1], 1.4);
}

/// Bytes that are not a MIDI file fail to play instead of leaving a player
/// with no sequence.
- (void)testMIDIBackendRejectsBadData {
    MIDISoundBackend *backend = [MIDISoundBackend new];
    NSData *junk = DataWithPrefix("MThd", 4, 0, 64);
    XCTAssertFalse([backend playData:junk format:GlkSoundBlorbFormatMIDI repeats:1 position:0 paused:NO volume:0 finished:nil]);
    XCTAssertFalse([backend resume]);
    XCTAssertEqual(backend.duration, 0);
}

#pragma mark Backends

/// Plays at volume 0, so the test makes no sound.
- (void)testSFBBackendFinishesEveryRepeat {
    NSData *wav = [SpectrumBeeper WAVWithFrequency:440 duration:50];
    SFBSoundBackend *backend = [SFBSoundBackend new];
    XCTestExpectation *done = [self expectationWithDescription:@"finished"];
    XCTAssertTrue([backend playData:wav format:GlkSoundBlorbFormatWave repeats:2 position:0 paused:NO volume:0 finished:^{
        [done fulfill];
    }]);
    XCTAssertEqualWithAccuracy(backend.duration, 0.05, 0.005);
    XCTAssertEqual(backend.resumeRepeats, 2);
    [self waitForExpectations:@[done] timeout:5];
    [backend stop];
}

- (void)testSFBBackendStopDropsFinish {
    NSData *wav = [SpectrumBeeper WAVWithFrequency:440 duration:50];
    SFBSoundBackend *backend = [SFBSoundBackend new];
    XCTestExpectation *never = [self expectationWithDescription:@"no finish after stop"];
    never.inverted = YES;
    XCTAssertTrue([backend playData:wav format:GlkSoundBlorbFormatWave repeats:1 position:0 paused:NO volume:0 finished:^{
        [never fulfill];
    }]);
    [backend stop];
    [self waitForExpectations:@[never] timeout:0.5];
}

- (void)testSFBBackendPausedStartWaitsForResume {
    NSData *wav = [SpectrumBeeper WAVWithFrequency:440 duration:50];
    SFBSoundBackend *backend = [SFBSoundBackend new];
    __block BOOL finished = NO;
    XCTestExpectation *done = [self expectationWithDescription:@"finished after resume"];
    XCTAssertTrue([backend playData:wav format:GlkSoundBlorbFormatWave repeats:1 position:0 paused:YES volume:0 finished:^{
        finished = YES;
        [done fulfill];
    }]);
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
    XCTAssertFalse(finished, @"a sound readied paused must not play");
    XCTAssertTrue([backend resume]);
    [self waitForExpectations:@[done] timeout:5];
    [backend stop];
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
