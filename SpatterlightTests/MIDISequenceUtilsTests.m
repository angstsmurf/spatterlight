//
//  MIDISequenceUtilsTests.m
//  SpatterlightTests
//
//  The sequence processing behind MIDIPlayer, on sequences built by hand.
//  No audio graph is opened.
//

#import <XCTest/XCTest.h>

#import "MIDISequenceUtils.h"

@interface MIDISequenceUtilsTests : XCTestCase {
    MusicSequence sequence;
}
@end

@implementation MIDISequenceUtilsTests

- (void)setUp {
    XCTAssertEqual(NewMusicSequence(&sequence), noErr);
}

- (void)tearDown {
    DisposeMusicSequence(sequence);
}

#pragma mark Helpers

- (MusicTrack)newTrack {
    MusicTrack track = NULL;
    XCTAssertEqual(MusicSequenceNewTrack(sequence, &track), noErr);
    return track;
}

static void AddNote(MusicTrack track, MusicTimeStamp at, MusicTimeStamp beats) {
    MIDINoteMessage note = { 0, 60, 100, 0, (Float32)beats };
    MusicTrackNewMIDINoteEvent(track, at, &note);
}

static void AddMessage(MusicTrack track, MusicTimeStamp at, UInt8 status, UInt8 data1, UInt8 data2) {
    MIDIChannelMessage message = { status, data1, data2, 0 };
    MusicTrackNewMIDIChannelEvent(track, at, &message);
}

static void AddSysEx(MusicTrack track, MusicTimeStamp at, const UInt8 *bytes, UInt32 length) {
    NSMutableData *buffer = [NSMutableData dataWithLength:offsetof(MIDIRawData, data) + length];
    MIDIRawData *raw = buffer.mutableBytes;
    raw->length = length;
    memcpy(raw->data, bytes, length);
    MusicTrackNewMIDIRawDataEvent(track, at, raw);
}

/// Each event of `track` as "stamp:type".
static NSArray<NSString *> *Events(MusicTrack track) {
    NSMutableArray *events = [NSMutableArray new];
    MIDITrackForEachEvent(track, ^BOOL(MusicTimeStamp stamp, MusicEventType type, const void *data, UInt32 size) {
        [events addObject:[NSString stringWithFormat:@"%.2f:%u", stamp, (unsigned)type]];
        return YES;
    });
    return events;
}

/// What the chase sends, one hex string per message.
- (NSArray<NSString *> *)chaseTo:(MusicTimeStamp)beats {
    NSMutableArray *sent = [NSMutableArray new];
    MIDISequenceChaseSynthState(sequence, beats, ^(const UInt8 *bytes, UInt32 length) {
        NSMutableString *hex = [NSMutableString new];
        for (UInt32 i = 0; i < length; i++)
            [hex appendFormat:@"%02x", bytes[i]];
        [sent addObject:hex];
    });
    return sent;
}

#pragma mark Iteration

- (void)testForEachTrackVisitsTempoTrackLast {
    MusicTrack first = [self newTrack];
    MusicTrack second = [self newTrack];
    MusicTrack tempo = NULL;
    MusicSequenceGetTempoTrack(sequence, &tempo);

    NSMutableArray *seen = [NSMutableArray new];
    MIDISequenceForEachTrack(sequence, YES, ^(MusicTrack track) {
        [seen addObject:[NSValue valueWithPointer:track]];
    });
    XCTAssertEqualObjects(seen, (@[[NSValue valueWithPointer:first], [NSValue valueWithPointer:second], [NSValue valueWithPointer:tempo]]));

    [seen removeAllObjects];
    MIDISequenceForEachTrack(sequence, NO, ^(MusicTrack track) {
        [seen addObject:[NSValue valueWithPointer:track]];
    });
    XCTAssertEqual(seen.count, 2U);
}

- (void)testForEachEventStopsWhenAsked {
    MusicTrack track = [self newTrack];
    AddNote(track, 0, 1);
    AddNote(track, 1, 1);
    AddNote(track, 2, 1);
    __block int visited = 0;
    MIDITrackForEachEvent(track, ^BOOL(MusicTimeStamp stamp, MusicEventType type, const void *data, UInt32 size) {
        visited++;
        return stamp < 1;
    });
    XCTAssertEqual(visited, 2);
}

#pragma mark Length

- (void)testLengthIsLongestTrack {
    XCTAssertEqual(MIDISequenceLength(sequence), 0);
    AddNote([self newTrack], 0, 2);
    AddNote([self newTrack], 3, 2);
    XCTAssertEqualWithAccuracy(MIDISequenceLength(sequence), 5, 0.0001);
}

#pragma mark Trimming

/// At the default 120 bpm a beat is half a second.
- (void)testLeadingRestIsTrimmedAndSetupStays {
    MusicTrack track = [self newTrack];
    AddMessage(track, 0, 0xc0, 5, 0);
    AddNote(track, 4, 1);
    MIDISequenceTrimLeadingSilence(sequence);
    XCTAssertEqualObjects(Events(track), (@[@"0.00:7", @"0.01:6"]),
                          @"the note moves up to just after the program change, which stays put");
    XCTAssertEqualWithAccuracy(MIDISequenceLength(sequence), 1.01, 0.0001);
}

- (void)testRestIsMeasuredAcrossTracks {
    AddNote([self newTrack], 4, 1);
    MusicTrack early = [self newTrack];
    AddNote(early, 2, 1);
    MIDISequenceTrimLeadingSilence(sequence);
    XCTAssertEqualObjects(Events(early), (@[@"0.01:6"]));
}

- (void)testShortRestIsKept {
    MusicTrack track = [self newTrack];
    AddNote(track, 0.25, 1);
    MIDISequenceTrimLeadingSilence(sequence);
    XCTAssertEqualObjects(Events(track), (@[@"0.25:6"]));
}

- (void)testTrimStopsAtSetupAfterTimeZero {
    MusicTrack track = [self newTrack];
    AddMessage(track, 1, 0xc0, 5, 0);
    AddNote(track, 4, 1);
    MIDISequenceTrimLeadingSilence(sequence);
    XCTAssertEqualObjects(Events(track), (@[@"0.01:7", @"3.01:6"]),
                          @"the program change moves up with the music rather than being left behind it");
}

#pragma mark Chasing

- (void)testChaseSendsSysExThenControllersProgramsBends {
    MusicTrack track = [self newTrack];
    const UInt8 reset[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7 };
    AddMessage(track, 0.5, 0xb1, 7, 100);
    AddMessage(track, 1, 0xc1, 5, 0);
    AddMessage(track, 1.5, 0xe1, 0x00, 0x50);
    AddNote(track, 1.5, 1);
    AddMessage(track, 3, 0xc1, 9, 0);
    MusicTrack other = [self newTrack];
    AddMessage(other, 0.2, 0xb1, 0, 1);
    AddSysEx(other, 1.8, reset, sizeof reset);

    XCTAssertEqualObjects([self chaseTo:2], (@[@"f07e7f0901f7", @"b10001", @"b10764", @"c105", @"e10050"]),
                          @"the sysex reset first, the bank select before the volume and the program, nothing after beat 2, no notes");
}

- (void)testChaseKeepsLastValue {
    MusicTrack track = [self newTrack];
    AddMessage(track, 0.5, 0xb0, 7, 10);
    AddMessage(track, 1, 0xb0, 7, 20);
    XCTAssertEqualObjects([self chaseTo:2], (@[@"b00714"]));
}

- (void)testChaseToStartSendsNothing {
    AddMessage([self newTrack], 0, 0xc0, 5, 0);
    XCTAssertEqualObjects([self chaseTo:0], @[]);
}

@end
