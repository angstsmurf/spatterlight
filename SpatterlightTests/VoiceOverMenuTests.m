//
//  VoiceOverMenuTests.m
//  SpatterlightTests
//
//  The parts of application/VoiceOver menu detection that work without a
//  game: the V6 menu strings, the Journey text formatter and archiving, and
//  the Bureaucracy form maths on a hand-built text storage.
//

#import <XCTest/XCTest.h>
#import <Cocoa/Cocoa.h>

#import "InfocomV6MenuHandler.h"
#import "JourneyMenuHandler.h"
#import "JourneyMenuItem.h"
#import "JourneyTextFormatter.h"
#import "BureaucracyForm.h"
#import "NSString+Categories.h"

#pragma mark - Private methods the tests call

@interface BureaucracyForm (VoiceOverMenuTests)
@property NSAttributedString *attrStr;
@property NSArray<NSValue *> *fields;
@property NSArray<NSString *> *fieldstrings;
- (BOOL)matchFormTitle;
- (NSArray *)extractFieldRanges;
- (NSRange)backgroundRun:(NSUInteger)run from:(NSUInteger)startpos;
- (NSRange)rangeFromIndex:(NSUInteger)index;
@end

#pragma mark - Latin-1 bytes

@interface Latin1BytesTests : XCTestCase
@end

@implementation Latin1BytesTests

- (void)testAsciiIsUnchanged {
    const char bytes[] = "Last name:";
    XCTAssertEqualObjects([NSString stringWithLatin1Bytes:bytes length:strlen(bytes)], @"Last name:");
}

// A signed char used to sign-extend 0xE9 to U+FFE9.
- (void)testHighBytesAreLatin1 {
    const char bytes[] = "\xe9t\xe9";
    XCTAssertEqualObjects([NSString stringWithLatin1Bytes:bytes length:3], @"été");
}

- (void)testNullOrEmptyGivesEmptyString {
    XCTAssertEqualObjects([NSString stringWithLatin1Bytes:NULL length:5], @"");
    XCTAssertEqualObjects([NSString stringWithLatin1Bytes:"abc" length:0], @"");
}

@end

#pragma mark - Infocom V6 menus

@interface InfocomV6MenuHandlerTests : XCTestCase
@property InfocomV6MenuHandler *handler;
@end

@implementation InfocomV6MenuHandlerTests

- (void)setUp {
    // No controller: speaking goes to nil, which is all these tests need.
    self.handler = [[InfocomV6MenuHandler alloc] initWithDelegate:(GlkController *)nil];
}

- (void)send:(InfocomV6MenuType)type index:(NSUInteger)index total:(NSUInteger)total text:(nullable const char *)text {
    [self.handler handleMenuItemOfType:type index:index total:total text:(char *)text length:text ? strlen(text) : 0];
}

- (NSString *)line {
    return [self.handler menuLineStringWithTitle:NO index:NO total:NO instructions:NO];
}

- (void)testTopicMenuLine {
    [self send:kV6MenuTitle index:0 total:kV6MenuTypeTopic text:"InvisiClues"];
    [self send:kV6MenuTypeTopic index:0 total:2 text:"The Start"];
    [self send:kV6MenuTypeTopic index:0 total:2 text:"The End"];
    [self send:kV6MenuSelectionChanged index:1 total:0 text:NULL];

    NSString *expected = @"We are in a menu, \"InvisiClues.\"\nThe End.\nMenu item 2 of 2.\n";
    XCTAssertEqualObjects([self.handler menuLineStringWithTitle:YES index:YES total:YES instructions:NO], expected);
}

- (void)testDefinitionEndingInSpaceIsSpokenAsSpace {
    [self send:kV6MenuTypeDefine index:0 total:1 text:"look "];
    XCTAssertEqualObjects(self.line, @"look (space)");
}

- (void)testTypingAndDeletingInADefinition {
    [self send:kV6MenuTypeDefine index:0 total:1 text:"ab"];
    [self send:kV6MenuCurrentItemChanged index:0 total:'c' text:NULL];
    XCTAssertEqualObjects(self.line, @"abc");
    [self send:kV6MenuCurrentItemChanged index:0 total:0 text:NULL];
    XCTAssertEqualObjects(self.line, @"ab");
}

// A1: deleting the last character, and deleting again from the empty
// definition, used to index -1.
- (void)testDeletingPastTheStartDoesNotCrash {
    [self send:kV6MenuTypeDefine index:0 total:1 text:"a"];
    [self send:kV6MenuCurrentItemChanged index:0 total:0 text:NULL];
    XCTAssertEqualObjects(self.line, @"");
    [self send:kV6MenuCurrentItemChanged index:0 total:0 text:NULL];
    XCTAssertEqualObjects(self.line, @"");
}

// A4: the typed character was formatted with %c.
- (void)testTypedLatin1Character {
    [self send:kV6MenuTypeDefine index:0 total:1 text:"caf"];
    [self send:kV6MenuCurrentItemChanged index:0 total:0xe9 text:NULL];
    XCTAssertEqualObjects(self.line, @"café");
}

// A2: the selected line comes from the interpreter unchecked.
- (void)testEditingALineThatDoesNotExistIsIgnored {
    [self send:kV6MenuTypeDefine index:0 total:1 text:"ab"];
    [self send:kV6MenuCurrentItemChanged index:5 total:'x' text:NULL];
    [self send:kV6MenuSelectionChanged index:0 total:0 text:NULL];
    XCTAssertEqualObjects(self.line, @"ab");
}

- (void)testEmptyMenuLine {
    NSString *line = [self.handler menuLineStringWithTitle:YES index:YES total:YES instructions:NO];
    XCTAssertEqualObjects(line, @"We are in a menu.\n.\nMenu item 1 of 0.\n");
}

- (void)testHintInstructions {
    [self send:kV6MenuTypeHint index:0 total:3 text:"First hint"];
    XCTAssertEqualObjects([self.handler constructMenuInstructionString], @"M for hint menu. Return for a hint. Q to resume story. ");
}

@end

#pragma mark - Journey

@interface JourneyTextFormatterTests : XCTestCase
@end

@implementation JourneyTextFormatterTests

- (NSString *)format:(NSString *)proposed elvish:(BOOL)elvish valid:(BOOL *)valid {
    JourneyTextFormatter *formatter = [[JourneyTextFormatter alloc] initWithMaxLength:13 elvish:elvish];
    NSString *string = proposed;
    NSRange selection = NSMakeRange(proposed.length, 0);
    NSString *error = nil;
    *valid = [formatter isPartialStringValid:&string proposedSelectedRange:&selection originalString:@"" originalSelectedRange:NSMakeRange(0, 0) errorDescription:&error];
    return string;
}

- (void)testNameIsCapitalised {
    BOOL valid;
    XCTAssertEqualObjects([self format:@"bERGON" elvish:NO valid:&valid], @"Bergon");
    XCTAssertFalse(valid);
    XCTAssertEqualObjects([self format:@"Bergon" elvish:NO valid:&valid], @"Bergon");
    XCTAssertTrue(valid);
}

// The first letter used to be lower-cased along with later copies of it.
- (void)testRepeatedFirstLetterStaysUpperCase {
    BOOL valid;
    XCTAssertEqualObjects([self format:@"AnnA" elvish:NO valid:&valid], @"Anna");
}

- (void)testNameDropsEverythingButLetters {
    BOOL valid;
    XCTAssertEqualObjects([self format:@"Al 2-x" elvish:NO valid:&valid], @"Alx");
    XCTAssertFalse(valid);
}

// A34: isalpha() on a unichar above 255 is undefined.
- (void)testNonAsciiIsDropped {
    BOOL valid;
    XCTAssertEqualObjects([self format:@"Élan中" elvish:NO valid:&valid], @"Lan");
    XCTAssertFalse(valid);
}

- (void)testElvishKeepsPunctuationAndCase {
    BOOL valid;
    XCTAssertEqualObjects([self format:@"Ai na, mE?" elvish:YES valid:&valid], @"Ai na, mE?");
    XCTAssertTrue(valid);
    XCTAssertEqualObjects([self format:@"a1b" elvish:YES valid:&valid], @"ab");
    XCTAssertFalse(valid);
}

@end

@interface JourneyCodingTests : XCTestCase
@end

@implementation JourneyCodingTests

- (void)testMenuItemRoundTripsSecurely {
    JourneyMenuItem *item = [[JourneyMenuItem alloc] initWithTitle:@"Bergon" column:2 line:3 tag:7];
    item.actor = @"Praxix";
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:item requiringSecureCoding:YES error:&error];
    XCTAssertNotNil(data, @"%@", error);
    JourneyMenuItem *copy = [NSKeyedUnarchiver unarchivedObjectOfClass:[JourneyMenuItem class] fromData:data error:&error];
    XCTAssertNotNil(copy, @"%@", error);
    XCTAssertEqualObjects(copy.title, @"Bergon");
    XCTAssertEqual(copy.column, 2u);
    XCTAssertEqual(copy.line, 3u);
    XCTAssertEqual(copy.tag, 7);
    XCTAssertEqualObjects(copy.actor, @"Praxix");
}

// A27/A29: the dialog items were decoded as a dictionary and the tag
// counter was not archived. Archived the way autorestore does it.
- (void)testHandlerRoundTripKeepsDialogItemsAndTags {
    JourneyMenuHandler *handler = [[JourneyMenuHandler alloc] initWithDelegate:(GlkController *)nil gridWindow:(GlkTextGridWindow *)nil bufferWindow:(GlkTextBufferWindow *)nil];
    NSMutableArray *dialog = [@[[[JourneyMenuItem alloc] initWithTitle:@"Proceed" column:1 line:0 tag:0],
                                [[JourneyMenuItem alloc] initWithTitle:@"[cancel]" column:2 line:0 tag:1]] mutableCopy];
    [handler setValue:dialog forKey:@"journeyDialogMenuItems"];
    JourneyMenuItem *verb = [[JourneyMenuItem alloc] initWithTitle:@"Lore" column:3 line:1 tag:4];
    verb.actor = @"Praxix";
    [handler setValue:[@{@4 : verb} mutableCopy] forKey:@"journeyVerbMenuItems"];
    [handler setValue:@5 forKey:@"tagCounter"];

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:handler requiringSecureCoding:NO error:&error];
    XCTAssertNotNil(data, @"%@", error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    unarchiver.requiresSecureCoding = NO;
    JourneyMenuHandler *copy = [unarchiver decodeObjectOfClass:[JourneyMenuHandler class] forKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    XCTAssertNotNil(copy);

    NSArray<JourneyMenuItem *> *items = [copy valueForKey:@"journeyDialogMenuItems"];
    XCTAssertTrue([items isKindOfClass:[NSMutableArray class]]);
    XCTAssertEqualObjects([items valueForKey:@"title"], (@[@"Proceed", @"[cancel]"]));
    NSDictionary<NSNumber *, JourneyMenuItem *> *verbs = [copy valueForKey:@"journeyVerbMenuItems"];
    XCTAssertEqualObjects(verbs[@4].actor, @"Praxix");
    XCTAssertEqualObjects([copy valueForKey:@"tagCounter"], @5);
}

@end

#pragma mark - Bureaucracy

@interface BureaucracyFormTests : XCTestCase
@property BureaucracyForm *form;
@end

@implementation BureaucracyFormTests

- (void)setUp {
    self.form = [[BureaucracyForm alloc] initWithGlkController:(GlkController *)nil];
}

// Text in runs with alternating background colours, like the form
// header, followed by the field labels with no background.
- (NSTextStorage *)formWithRuns:(NSArray<NSString *> *)runs fields:(NSString *)fields {
    NSTextStorage *storage = [NSTextStorage new];
    NSArray *colors = @[NSColor.blackColor, NSColor.whiteColor];
    [runs enumerateObjectsUsingBlock:^(NSString *run, NSUInteger i, BOOL *stop) {
        [storage appendAttributedString:[[NSAttributedString alloc] initWithString:run attributes:@{NSBackgroundColorAttributeName : colors[i % 2]}]];
    }];
    [storage appendAttributedString:[[NSAttributedString alloc] initWithString:fields]];
    return storage;
}

- (NSString *)depositFields {
    return @"Last name: Smith\nFirst name: Al\nMiddle initial: B\nAmount of deposit: $ 10\nFrom illegal activity? (y/n): n\nIf yes, which one: ";
}

- (NSArray<NSString *> *)depositHeader {
    // Run 3 is the title. The info line is run 5 counted from the end of
    // the title text, which is inside run 3.
    return @[@"  ", @"|", @" ", @"  DEPOSIT SLIP  ", @" ", @"|", @" ", @"|", @"Please fill in the form", @"\n"];
}

- (void)testDepositSlip {
    self.form.attrStr = [self formWithRuns:self.depositHeader fields:self.depositFields];
    XCTAssertTrue([self.form matchFormTitle]);
    self.form.fields = [self.form extractFieldRanges];
    XCTAssertEqual(self.form.fields.count, 6u);

    NSString *string = self.form.attrStr.string;
    NSRange info = [self.form backgroundRun:5 from:NSMaxRange([string rangeOfString:@"DEPOSIT SLIP"])];
    XCTAssertEqualObjects([string substringWithRange:info], @"Please fill in the form");

    XCTAssertEqualObjects([string substringWithRange:[self.form rangeFromIndex:0]], @"Last name: Smith\n");
    // A23: the last field runs to the end of the text.
    NSRange last = [self.form rangeFromIndex:5];
    XCTAssertEqual(NSMaxRange(last), string.length);
    XCTAssertEqualObjects([string substringWithRange:last], @"If yes, which one: ");
}

- (void)testUnknownTitle {
    NSMutableArray *header = [self.depositHeader mutableCopy];
    header[3] = @"  TAX RETURN  ";
    self.form.attrStr = [self formWithRuns:header fields:self.depositFields];
    XCTAssertFalse([self.form matchFormTitle]);
}

// A21: a missing field used to give an empty field list that was then indexed.
- (void)testMissingFieldGivesNoFields {
    self.form.attrStr = [self formWithRuns:self.depositHeader fields:@"Last name: Smith\nFirst name: Al\n"];
    XCTAssertTrue([self.form matchFormTitle]);
    XCTAssertEqual([self.form extractFieldRanges].count, 0u);
}

// A20: too few runs used to return an uninitialised range.
- (void)testTooFewRunsGivesNotFound {
    self.form.attrStr = [self formWithRuns:@[@"  ", @"DEPOSIT SLIP"] fields:self.depositFields];
    XCTAssertEqual([self.form backgroundRun:5 from:0].location, (NSUInteger)NSNotFound);
    XCTAssertFalse([self.form matchFormTitle]);
}

- (void)testNoFieldsAtAll {
    self.form.attrStr = [self formWithRuns:self.depositHeader fields:@"nothing here"];
    XCTAssertEqual([self.form backgroundRun:3 from:0].location, (NSUInteger)NSNotFound);
}

@end
