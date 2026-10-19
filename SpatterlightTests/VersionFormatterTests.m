//
//  VersionFormatterTests.m
//  SpatterlightTests
//
//  The Z-machine interpreter version field accepts a single uppercase
//  letter. VersionFormatter rewrites typed or pasted text to that.
//

#import <XCTest/XCTest.h>

#import "VersionFormatter.h"

@interface VersionFormatterTests : XCTestCase
@end

@implementation VersionFormatterTests

// Runs isPartialStringValid: as an NSTextField would after an edit of
// `original` (selection `selection`) that produced `proposed`.
- (BOOL)edit:(NSString *)original selection:(NSRange)selection
    proposed:(NSString *)proposed result:(NSString **)result {
    VersionFormatter *formatter = [VersionFormatter new];
    NSString *partial = proposed;
    NSRange proposedRange = NSMakeRange(selection.location + 1, 0);
    NSString *error = nil;
    BOOL valid = [formatter isPartialStringValid:&partial
                           proposedSelectedRange:&proposedRange
                                  originalString:original
                           originalSelectedRange:selection
                                errorDescription:&error];
    *result = partial;
    return valid;
}

- (void)testUppercaseLetterIsAccepted {
    NSString *result;
    XCTAssertTrue([self edit:@"" selection:NSMakeRange(0, 0) proposed:@"S" result:&result]);
    XCTAssertEqualObjects(result, @"S");
}

- (void)testLowercaseLetterIsUppercased {
    NSString *result;
    XCTAssertFalse([self edit:@"" selection:NSMakeRange(0, 0) proposed:@"s" result:&result]);
    XCTAssertEqualObjects(result, @"S");
}

- (void)testNonLettersAreRejected {
    NSString *result;
    XCTAssertFalse([self edit:@"" selection:NSMakeRange(0, 0) proposed:@"1" result:&result]);
    XCTAssertFalse([self edit:@"" selection:NSMakeRange(0, 0) proposed:@"-" result:&result]);
}

- (void)testEmptyStringIsAccepted {
    NSString *result;
    XCTAssertTrue([self edit:@"S" selection:NSMakeRange(0, 1) proposed:@"" result:&result]);
    XCTAssertEqualObjects(result, @"");
}

- (void)testReplacingSelectedLetter {
    NSString *result;
    XCTAssertTrue([self edit:@"S" selection:NSMakeRange(0, 1) proposed:@"B" result:&result]);
    XCTAssertEqualObjects(result, @"B");
}

- (void)testPasteKeepsFirstLetter {
    NSString *result;
    XCTAssertFalse([self edit:@"" selection:NSMakeRange(0, 0) proposed:@"12xyz" result:&result]);
    XCTAssertEqualObjects(result, @"X");
}

// A field that already holds more than one character (set from code)
// made the insert length negative before it was clamped.
- (void)testOverlongOriginalStringGivesOneLetter {
    NSString *result;
    XCTAssertFalse([self edit:@"AB" selection:NSMakeRange(2, 0) proposed:@"ABC" result:&result]);
    XCTAssertEqual(result.length, 1u);
    XCTAssertEqualObjects(result, @"A");
}

@end
