//
//  InputHistoryTests.m
//  SpatterlightTests
//
//  Command history travel, its archive format (shared with autosaves
//  written by older versions) and the input field length formatter.
//

#import <XCTest/XCTest.h>

#import "InputHistory.h"
#import "InputTextField.h"

#define HISTORYLEN 100

@interface InputHistoryTests : XCTestCase
@end

@implementation InputHistoryTests

static InputHistory *HistoryWithLines(NSUInteger n) {
    InputHistory *history = [InputHistory new];
    for (NSUInteger i = 0; i < n; i++)
        [history saveHistory:[NSString stringWithFormat:@"line %lu", i]];
    [history reset];
    return history;
}

// Walks back to the oldest line and returns every line seen, newest first.
static NSArray<NSString *> *WalkBack(InputHistory *history, NSString *editLine) {
    NSMutableArray *seen = [NSMutableArray new];
    NSString *line;
    while ((line = [history travelBackward:editLine]) != nil) {
        [seen addObject:line];
        if (seen.count > 2 * HISTORYLEN)
            break;
    }
    return seen;
}

static InputHistory *RoundTrip(id<NSSecureCoding> object) {
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object requiringSecureCoding:YES error:&error];
    if (!data)
        return nil;
    return [NSKeyedUnarchiver unarchivedObjectOfClass:[InputHistory class] fromData:data error:&error];
}

- (void)testWalkBackAndForwardAtVariousFillLevels {
    for (NSNumber *fill in @[@0, @1, @98, @99, @100, @101, @199, @200, @250]) {
        NSUInteger n = fill.unsignedIntegerValue;
        InputHistory *history = HistoryWithLines(n);
        NSUInteger expected = MIN(n, HISTORYLEN - 1);
        XCTAssertEqual(history.count, expected, @"fill %lu", n);
        XCTAssertEqual(history.isEmpty, n == 0);

        NSArray *back = WalkBack(history, @"typing");
        XCTAssertEqual(back.count, expected, @"fill %lu", n);
        if (n) {
            XCTAssertEqualObjects(back.firstObject, ([NSString stringWithFormat:@"line %lu", n - 1]));
            XCTAssertEqualObjects(back.lastObject, ([NSString stringWithFormat:@"line %lu", n - expected]));
        }
        // At the oldest line, going back stays put.
        XCTAssertNil([history travelBackward:@"x"]);

        NSMutableArray *forward = [NSMutableArray new];
        NSString *line;
        while ((line = [history travelForward]) != nil && forward.count <= HISTORYLEN)
            [forward addObject:line];
        if (n) {
            XCTAssertEqual(forward.count, expected);
            // Arriving at the present brings back the edited line.
            XCTAssertEqualObjects(forward.lastObject, @"typing");
        }
        XCTAssertNil([history travelForward]);
    }
}

- (void)testEditLineIsTakenWhenLeavingThePresent {
    InputHistory *history = HistoryWithLines(3);
    XCTAssertEqualObjects([history travelBackward:@"half"], @"line 2");
    // Further back does not overwrite the edit line.
    XCTAssertEqualObjects([history travelBackward:@"line 2"], @"line 1");
    XCTAssertEqualObjects([history travelForward], @"line 2");
    XCTAssertEqualObjects([history travelForward], @"half");
    XCTAssertNil([history travelForward]);
}

- (void)testSaveMovesToThePresent {
    InputHistory *history = HistoryWithLines(3);
    [history travelBackward:@""];
    [history travelBackward:@""];
    [history saveHistory:@"new"];
    XCTAssertNil([history travelForward]);
    XCTAssertEqualObjects([history travelBackward:@""], @"new");
}

- (void)testArchiveRoundTrip {
    InputHistory *history = HistoryWithLines(150);
    [history travelBackward:@"edit"];
    [history travelBackward:@""];
    InputHistory *copy = RoundTrip(history);
    XCTAssertNotNil(copy);
    XCTAssertEqual(copy.count, HISTORYLEN - 1);
    XCTAssertEqualObjects([copy travelBackward:@""], @"line 147");
    XCTAssertEqualObjects([copy travelForward], @"line 148");
    XCTAssertEqualObjects([copy travelForward], @"line 149");
    XCTAssertEqualObjects([copy travelForward], @"edit");
    XCTAssertNil([copy travelForward]);
}

// An archive in the old ring format, as written by older versions.
static NSData *LegacyArchive(NSArray *ring, NSInteger pos, NSInteger first, NSInteger present) {
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [archiver encodeObject:ring forKey:@"history"];
    [archiver encodeInteger:pos forKey:@"historypos"];
    [archiver encodeInteger:first forKey:@"historyfirst"];
    [archiver encodeInteger:present forKey:@"historypresent"];
    [archiver finishEncoding];
    return archiver.encodedData;
}

static InputHistory *DecodeLegacy(NSData *data) {
    NSError *error = nil;
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    return [[InputHistory alloc] initWithCoder:unarchiver];
}

- (void)testLegacyWrappedRing {
    // A full ring: slots 0..99, oldest at 43, edit slot at 42.
    NSMutableArray *ring = [NSMutableArray new];
    for (NSUInteger i = 0; i < HISTORYLEN; i++)
        [ring addObject:[NSString stringWithFormat:@"slot %lu", i]];
    InputHistory *history = DecodeLegacy(LegacyArchive(ring, 42, 43, 42));
    XCTAssertEqual(history.count, HISTORYLEN - 1);
    NSArray *back = WalkBack(history, @"");
    XCTAssertEqualObjects(back.firstObject, @"slot 41");
    XCTAssertEqualObjects(back.lastObject, @"slot 43");
}

- (void)testLegacyBrokenStatesAreRepaired {
    NSMutableArray *ring = [NSMutableArray new];
    for (NSUInteger i = 0; i < HISTORYLEN; i++)
        [ring addObject:[NSString stringWithFormat:@"slot %lu", i]];
    // historyfirst == HISTORYLEN, from the old off-by-one wrap.
    InputHistory *history = DecodeLegacy(LegacyArchive(ring, 99, 100, 99));
    XCTAssertEqual(WalkBack(history, @"").count, HISTORYLEN - 1);

    // A ring that lost slots, with indices past its end.
    NSArray *shrunk = [ring subarrayWithRange:NSMakeRange(0, 90)];
    history = DecodeLegacy(LegacyArchive(shrunk, 95, 96, 95));
    XCTAssertEqual(WalkBack(history, @"").count, 90U);

    // Garbage.
    history = DecodeLegacy(LegacyArchive(@[@1, @"a"], 0, 0, 1));
    XCTAssertTrue(history.isEmpty);
    XCTAssertNil([history travelBackward:@""]);
    history = DecodeLegacy(LegacyArchive(@[], 7, 3, 5));
    XCTAssertTrue(history.isEmpty);
}

#pragma mark MyTextFormatter

static NSString *Edit(NSFormatter *formatter, NSString *original, NSRange selection, NSString *typed, BOOL *valid) {
    NSString *partial = [original stringByReplacingCharactersInRange:selection withString:typed];
    NSRange proposedRange = NSMakeRange(selection.location + typed.length, 0);
    NSString *error = nil;
    *valid = [formatter isPartialStringValid:&partial
                       proposedSelectedRange:&proposedRange
                              originalString:original
                       originalSelectedRange:selection
                            errorDescription:&error];
    return partial;
}

- (void)testFormatterTruncatesPaste {
    MyTextFormatter *formatter = [[MyTextFormatter alloc] initWithMaxLength:5];
    BOOL valid;
    XCTAssertEqualObjects(Edit(formatter, @"ab", NSMakeRange(2, 0), @"c", &valid), @"abc");
    XCTAssertTrue(valid);
    XCTAssertEqualObjects(Edit(formatter, @"", NSMakeRange(0, 0), @"1234567", &valid), @"12345");
    XCTAssertFalse(valid);
    XCTAssertEqualObjects(Edit(formatter, @"abcd", NSMakeRange(1, 2), @"1234567", &valid), @"a123d");
    XCTAssertEqualObjects(Edit(formatter, @"abcde", NSMakeRange(5, 0), @"x", &valid), @"abcde");
    XCTAssertFalse(valid);
}

- (void)testFormatterOverTheLimit {
    MyTextFormatter *formatter = [[MyTextFormatter alloc] initWithMaxLength:3];
    BOOL valid;
    // Text set in code can be longer than the limit: typing is refused,
    // deleting is allowed, and nothing throws.
    XCTAssertEqualObjects(Edit(formatter, @"abcdef", NSMakeRange(6, 0), @"x", &valid), @"abcdef");
    XCTAssertFalse(valid);
    XCTAssertEqualObjects(Edit(formatter, @"abcdef", NSMakeRange(5, 1), @"", &valid), @"abcde");
    XCTAssertTrue(valid);
    XCTAssertEqualObjects(Edit(formatter, @"abcdef", NSMakeRange(1, 1), @"xyz", &valid), @"abcdef");
    XCTAssertFalse(valid);
}

- (void)testFormatterKeepsComposedCharactersWhole {
    MyTextFormatter *formatter = [[MyTextFormatter alloc] initWithMaxLength:4];
    BOOL valid;
    // U+1F600 is two UTF-16 units and must not be split at the limit.
    NSString *result = Edit(formatter, @"abc", NSMakeRange(3, 0), @"\U0001F600", &valid);
    XCTAssertEqualObjects(result, @"abc");
    result = Edit(formatter, @"ab", NSMakeRange(2, 0), @"\U0001F600x", &valid);
    XCTAssertEqualObjects(result, @"ab\U0001F600");
}

- (void)testFormatterZeroAllowsNothing {
    MyTextFormatter *formatter = [[MyTextFormatter alloc] initWithMaxLength:0];
    BOOL valid;
    XCTAssertEqualObjects(Edit(formatter, @"", NSMakeRange(0, 0), @"a", &valid), @"");
    XCTAssertFalse(valid);
}

- (void)testFormatterCopyAndArchiveKeepTheLimit {
    MyTextFormatter *formatter = [[MyTextFormatter alloc] initWithMaxLength:7];
    XCTAssertEqual(((MyTextFormatter *)[formatter copy]).maxLength, 7U);
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:formatter requiringSecureCoding:NO error:nil];
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
    unarchiver.requiresSecureCoding = NO;
    MyTextFormatter *decoded = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    XCTAssertEqual(decoded.maxLength, 7U);
}

@end
