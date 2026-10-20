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
#import "JourneyMenuHandler_Private.h"
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
    self.handler = [[InfocomV6MenuHandler alloc] initWithGlkController:(GlkController *)nil];
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

- (void)testHintsWithoutTextGiveACount {
    [self send:kV6MenuTitle index:0 total:kV6MenuTypeHint text:"How do I get in?"];
    [self send:kV6MenuTypeHint index:0 total:3 text:NULL];
    NSString *line = [self.handler menuLineStringWithTitle:YES index:YES total:YES instructions:NO];
    XCTAssertEqualObjects(line, @"Showing hints for, \"How do I get in?.\"\n3 hints available.\n");
}

// Only a selection change, not building a line, marks the menu spoken.
- (void)testSelectionChangeMarksTheMenuSpoken {
    [self send:kV6MenuTypeTopic index:0 total:2 text:"The Start"];
    (void)[self.handler menuLineStringWithTitle:YES index:YES total:YES instructions:YES];
    XCTAssertFalse(self.handler.haveSpokenMenu);
    [self send:kV6MenuSelectionChanged index:0 total:0 text:NULL];
    XCTAssertTrue(self.handler.haveSpokenMenu);
}

- (void)testHintInstructions {
    [self send:kV6MenuTypeHint index:0 total:3 text:"First hint"];
    XCTAssertEqualObjects([self.handler instructionString], @"M for hint menu. Return for a hint. Q to resume story. ");
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

// Archives the way JourneyMenuHandler did before tagCounter was added.
@interface OldJourneyMenuHandlerArchive : NSObject <NSCoding>
@end

@implementation OldJourneyMenuHandlerArchive

- (instancetype)initWithCoder:(NSCoder *)decoder {
    return [super init];
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    JourneyMenuItem *lore = [[JourneyMenuItem alloc] initWithTitle:@"Lore" column:3 line:1 tag:3];
    lore.actor = @"Praxix";
    JourneyMenuItem *bergon = [[JourneyMenuItem alloc] initWithTitle:@"Bergon" column:2 line:2 tag:9];
    bergon.actor = @"Bergon";
    [encoder encodeObject:[@{@3 : lore, @9 : bergon} mutableCopy] forKey:@"journeyVerbMenuItems"];
    [encoder encodeBool:NO forKey:@"shouldShowDialog"];
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
    JourneyMenuHandler *handler = [[JourneyMenuHandler alloc] initWithGlkController:(GlkController *)nil gridWindow:(GlkTextGridWindow *)nil bufferWindow:(GlkTextBufferWindow *)nil];
    NSMutableArray *dialog = [@[[[JourneyMenuItem alloc] initWithTitle:@"Proceed" column:1 line:0 tag:0],
                                [[JourneyMenuItem alloc] initWithTitle:@"[cancel]" column:2 line:0 tag:1]] mutableCopy];
    [handler setValue:dialog forKey:@"dialogMenuItems"];
    JourneyMenuItem *verb = [[JourneyMenuItem alloc] initWithTitle:@"Lore" column:3 line:1 tag:4];
    verb.actor = @"Praxix";
    [handler setValue:[@{@4 : verb} mutableCopy] forKey:@"verbMenuItems"];
    [handler setValue:@5 forKey:@"tagCounter"];

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:handler requiringSecureCoding:NO error:&error];
    XCTAssertNotNil(data, @"%@", error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    unarchiver.requiresSecureCoding = NO;
    JourneyMenuHandler *copy = [unarchiver decodeObjectOfClass:[JourneyMenuHandler class] forKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    XCTAssertNotNil(copy);

    NSArray<JourneyMenuItem *> *items = [copy valueForKey:@"dialogMenuItems"];
    XCTAssertTrue([items isKindOfClass:[NSMutableArray class]]);
    XCTAssertEqualObjects([items valueForKey:@"title"], (@[@"Proceed", @"[cancel]"]));
    NSDictionary<NSNumber *, JourneyMenuItem *> *verbs = [copy valueForKey:@"verbMenuItems"];
    XCTAssertEqualObjects(verbs[@4].actor, @"Praxix");
    XCTAssertEqualObjects([copy valueForKey:@"tagCounter"], @5);
}

// Autosaves from before tagCounter was archived start new tags above the
// highest restored verb tag, so that new verbs don't replace restored ones.
- (void)testOldArchiveWithoutTagCounterStartsAboveRestoredVerbs {
    NSError *error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [archiver setClassName:@"JourneyMenuHandler" forClass:[OldJourneyMenuHandlerArchive class]];
    [archiver encodeObject:[OldJourneyMenuHandlerArchive new] forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:archiver.encodedData error:&error];
    XCTAssertNotNil(unarchiver, @"%@", error);
    unarchiver.requiresSecureCoding = NO;
    JourneyMenuHandler *copy = [unarchiver decodeObjectOfClass:[JourneyMenuHandler class] forKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    XCTAssertTrue([copy isKindOfClass:[JourneyMenuHandler class]]);

    NSDictionary<NSNumber *, JourneyMenuItem *> *verbs = [copy valueForKey:@"verbMenuItems"];
    XCTAssertEqual(verbs.count, 2u);
    XCTAssertEqualObjects([copy valueForKey:@"tagCounter"], @10);
}

- (JourneyMenuHandler *)roundTrip:(JourneyMenuHandler *)handler {
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:handler requiringSecureCoding:NO error:&error];
    XCTAssertNotNil(data, @"%@", error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    unarchiver.requiresSecureCoding = NO;
    JourneyMenuHandler *copy = [unarchiver decodeObjectOfClass:[JourneyMenuHandler class] forKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    return copy;
}

- (JourneyMenuHandler *)newHandler {
    return [[JourneyMenuHandler alloc] initWithGlkController:(GlkController *)nil gridWindow:nil bufferWindow:nil];
}

// The dialog state is archived as the old shouldShowDialog flag, so a
// dialog that was showing comes back as one to show again.
- (void)testShowingDialogIsArchivedAsShouldShowDialog {
    JourneyMenuHandler *handler = [self newHandler];
    [handler prepareDialogWithText:@"Praxix, Cast elevation at:" type:JourneyDialogTypeMultipleChoice];
    handler.dialogState = JourneyDialogStateShowing;
    handler.dialogIncludesLastMove = YES;

    JourneyMenuHandler *copy = [self roundTrip:handler];
    XCTAssertEqual(copy.dialogState, JourneyDialogStateRestored);
    XCTAssertTrue(copy.hasPendingDialog);
    XCTAssertEqual(copy.dialogType, JourneyDialogTypeMultipleChoice);
    XCTAssertEqualObjects(copy.dialogText, @"Praxix, Cast elevation at:");
    XCTAssertTrue(copy.restoredDialogIncludesLastMove);
    XCTAssertTrue(copy.startNewPartyMenu);
    XCTAssertTrue(copy.startNewMembersMenu);

    handler.dialogState = JourneyDialogStateIdle;
    XCTAssertEqual([self roundTrip:handler].dialogState, JourneyDialogStateIdle);
}

// A restored question survives the menus being rebuilt; a pending one
// doesn't. Choosing a menu item answers either.
- (void)testRestoredDialogSurvivesCancelUntilAMenuItemIsChosen {
    JourneyMenuHandler *handler = [self newHandler];
    handler.dialogState = JourneyDialogStateRestored;
    [handler cancelPendingDialog];
    XCTAssertEqual(handler.dialogState, JourneyDialogStateRestored);
    [handler sendMouseEventForMenuItem:nil];
    XCTAssertEqual(handler.dialogState, JourneyDialogStateIdle);

    [handler prepareDialogWithText:@"Please Select One:" type:JourneyDialogTypeMultipleChoice];
    XCTAssertEqual(handler.dialogState, JourneyDialogStatePending);
    [handler cancelPendingDialog];
    XCTAssertEqual(handler.dialogState, JourneyDialogStateIdle);
}

// A dialog on screen stays until it is answered.
- (void)testShowingDialogIgnoresNewQuestionsAndCancels {
    JourneyMenuHandler *handler = [self newHandler];
    handler.dialogState = JourneyDialogStateShowing;
    [handler prepareDialogWithText:@"Bergon says..." type:JourneyDialogTypeTextEntryElvish];
    XCTAssertEqual(handler.dialogState, JourneyDialogStateShowing);
    XCTAssertEqualObjects(handler.dialogText, @"Bergon says...");
    [handler cancelPendingDialog];
    [handler sendMouseEventForMenuItem:nil];
    XCTAssertEqual(handler.dialogState, JourneyDialogStateShowing);
    XCTAssertFalse(handler.hasPendingDialog);
    XCTAssertFalse([handler shouldRecreateDialog]);
}

// A restored question becomes pending at the first chance to show it,
// even without VoiceOver, so that later menus can drop it.
- (void)testRestoredDialogBecomesPendingWhenItCouldBeShown {
    JourneyMenuHandler *handler = [self newHandler];
    handler.dialogState = JourneyDialogStateRestored;
    XCTAssertFalse([handler shouldRecreateDialog]);
    XCTAssertEqual(handler.dialogState, JourneyDialogStatePending);
}

@end

#pragma mark - Journey click geometry

@interface JourneyClickTests : XCTestCase
@end

@implementation JourneyClickTests

- (NSPoint)clickAtColumn:(NSUInteger)column line:(NSUInteger)line inGrid:(NSSize)size bordered:(BOOL)bordered inBounds:(BOOL *)inBounds {
    JourneyMenuItem *item = [[JourneyMenuItem alloc] initWithTitle:@"Proceed" column:column line:line tag:0];
    NSPoint point = NSZeroPoint;
    *inBounds = [item getClickPoint:&point inGridOfSize:size bordered:bordered];
    return point;
}

// The party commands are in column 0, the rest in five equal columns
// starting after width % 5.
- (void)testColumnsAndLines {
    BOOL inBounds = NO;
    NSPoint point = [self clickAtColumn:0 line:0 inGrid:NSMakeSize(82, 25) bordered:NO inBounds:&inBounds];
    XCTAssertTrue(inBounds);
    XCTAssertTrue(NSEqualPoints(point, NSMakePoint(2, 20)));

    point = [self clickAtColumn:4 line:4 inGrid:NSMakeSize(82, 25) bordered:NO inBounds:&inBounds];
    XCTAssertTrue(inBounds);
    XCTAssertTrue(NSEqualPoints(point, NSMakePoint(2 + 16 * 4 + 2, 24)));
}

- (void)testBorderMovesTheMenusUpALine {
    BOOL inBounds = NO;
    NSPoint point = [self clickAtColumn:2 line:4 inGrid:NSMakeSize(80, 25) bordered:YES inBounds:&inBounds];
    XCTAssertTrue(inBounds);
    XCTAssertTrue(NSEqualPoints(point, NSMakePoint(16 * 2 + 2, 23)));
}

// Column 4 reaches the right edge only in a grid narrower than 15.
- (void)testNarrowAndShortGridsAreOutOfBounds {
    BOOL inBounds = YES;
    [self clickAtColumn:4 line:0 inGrid:NSMakeSize(15, 25) bordered:NO inBounds:&inBounds];
    XCTAssertTrue(inBounds);
    [self clickAtColumn:4 line:0 inGrid:NSMakeSize(14, 25) bordered:NO inBounds:&inBounds];
    XCTAssertFalse(inBounds);
    [self clickAtColumn:0 line:0 inGrid:NSMakeSize(80, 4) bordered:NO inBounds:&inBounds];
    XCTAssertFalse(inBounds);
    [self clickAtColumn:0 line:0 inGrid:NSZeroSize bordered:NO inBounds:&inBounds];
    XCTAssertFalse(inBounds);
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
