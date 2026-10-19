//
//  IFictionParserTests.m
//  SpatterlightTests
//
//  Tests for the iFiction XML parsers in "iFiction/" (IFictionMetadata,
//  IFStory, IFIdentification, IFBibliographic, IFDB), for copying
//  their results into Core Data (Metadata+IFiction), and for the cover
//  bookkeeping in +[CoverImageStore insertImageData:inMetadataID:context:].
//  Everything runs on an in-memory store; nothing touches the network.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFIdentification.h"
#import "IFBibliographic.h"
#import "IFCoverDescription.h"
#import "IFDB.h"
#import "CoverImageStore.h"
#import "Game.h"
#import "Metadata.h"
#import "Metadata+IFiction.h"
#import "Ifid.h"
#import "Image.h"
#import "CoreDataManager.h"

@interface IFictionParserTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;

@end

@implementation IFictionParserTests

- (void)setUp {
    [super setUp];
    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    XCTAssertNotNil(self.context);
}

- (void)tearDown {
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

#pragma mark - Helpers

static NSData *IFiction(NSString *stories) {
    NSString *xml = [NSString stringWithFormat:
                     @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<ifindex version=\"1.0\" xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">%@</ifindex>",
                     stories];
    return [xml dataUsingEncoding:NSUTF8StringEncoding];
}

static NSXMLElement *Element(NSString *xml) {
    NSError *error = nil;
    NSXMLElement *element = [[NSXMLElement alloc] initWithXMLString:xml error:&error];
    NSCAssert(element, @"Bad fixture: %@", error);
    return element;
}

static IFBibliographic *Biblio(NSString *children) {
    return [[IFBibliographic alloc] initWithXMLElement:
            Element([NSString stringWithFormat:@"<bibliographic>%@</bibliographic>", children])];
}

- (Metadata *)newMetadata {
    return (Metadata *)[NSEntityDescription insertNewObjectForEntityForName:@"Metadata"
                                                     inManagedObjectContext:self.context];
}

- (Image *)newImageWithData:(NSData *)data url:(NSString *)url {
    Image *image = (Image *)[NSEntityDescription insertNewObjectForEntityForName:@"Image"
                                                          inManagedObjectContext:self.context];
    image.data = data;
    image.originalURL = url;
    return image;
}

// Lets the blocks insertImageData: queued with -performBlock: run.
- (void)drainContext {
    XCTestExpectation *done = [self expectationWithDescription:@"context drained"];
    [self.context performBlock:^{
        [done fulfill];
    }];
    [self waitForExpectations:@[done] timeout:5];
}

#pragma mark - IFictionMetadata / IFStory

- (void)testFullStory {
    NSData *data = IFiction(@"<story>"
                            "<identification><ifid>ZCODE-1-123456-ABCD</ifid><ifid>OTHER-IFID</ifid>"
                            "<format>zcode</format><bafn>42</bafn></identification>"
                            "<bibliographic><title>Foo &amp;amp; Bar</title><author>A. Writer</author></bibliographic>"
                            "<cover><format>jpg</format><description>A cover</description></cover>"
                            "<ifdb xmlns=\"http://ifdb.org/api/xmlns\"><tuid>abc123</tuid>"
                            "<coverart><url>https://ifdb.org/cover.jpg</url></coverart>"
                            "<starRating>4</starRating></ifdb>"
                            "</story>");
    IFictionMetadata *ifiction = [[IFictionMetadata alloc] initWithData:data];
    XCTAssertEqual(ifiction.stories.count, 1u);
    IFStory *story = ifiction.firstStory;

    NSSet *expected = [NSSet setWithArray:@[ @"ZCODE-1-123456-ABCD", @"OTHER-IFID" ]];
    XCTAssertEqualObjects(story.identification.ifids, expected);
    XCTAssertEqualObjects(story.identification.format, @"zcode");
    XCTAssertEqualObjects(story.identification.bafn, @"42");
    XCTAssertEqualObjects(story.bibliographic.title, @"Foo & Bar");
    XCTAssertEqualObjects(story.coverDescription.coverArtDescription, @"A cover");
    XCTAssertEqualObjects(story.ifdb.tuid, @"abc123");
    XCTAssertEqualObjects(story.ifdb.coverArtURL, @"https://ifdb.org/cover.jpg");
    XCTAssertEqualObjects(story.ifdb.starRating, @"4");

    Metadata *metadata = [self newMetadata];
    [metadata addInfoFromStory:story];
    XCTAssertEqualObjects([metadata.ifids valueForKey:@"ifidString"], expected);
    XCTAssertEqualObjects(metadata.title, @"Foo & Bar");
    XCTAssertEqualObjects(metadata.author, @"A. Writer");
    XCTAssertEqualObjects(metadata.bafn, @"42");
    XCTAssertEqualObjects(metadata.tuid, @"abc123");
    XCTAssertEqualObjects(metadata.coverArtURL, @"https://ifdb.org/cover.jpg");
    XCTAssertEqualObjects(metadata.coverArtDescription, @"A cover");

    // Applying the same story twice does not duplicate the IFIDs
    [metadata addInfoFromStory:story];
    XCTAssertEqual(metadata.ifids.count, 2u);
}

- (void)testNoUsableStoryGivesNil {
    XCTAssertNil([[IFictionMetadata alloc] initWithData:[NSData data]]);
    XCTAssertNil([[IFictionMetadata alloc] initWithData:[@"not xml at all <<<" dataUsingEncoding:NSUTF8StringEncoding]]);
    XCTAssertNil([[IFictionMetadata alloc] initWithData:IFiction(@"")]);
    // A story with neither <identification> nor <id> is skipped
    XCTAssertNil([[IFictionMetadata alloc] initWithData:IFiction(@"<story><bibliographic><title>T</title></bibliographic></story>")]);
}

- (void)testIdentificationWithoutIfidKeepsBibliographic {
    IFStory *story = [[IFictionMetadata alloc] initWithData:
                      IFiction(@"<story><identification><format>glulx</format></identification>"
                               "<bibliographic><title>Kept</title></bibliographic></story>")].firstStory;
    XCTAssertNotNil(story);
    XCTAssertNil(story.identification);
    XCTAssertEqualObjects(story.bibliographic.title, @"Kept");
}

- (void)testFirstStoryOfSeveral {
    IFictionMetadata *ifiction = [[IFictionMetadata alloc] initWithData:
                                  IFiction(@"<story><identification><ifid>ONE</ifid></identification></story>"
                                           "<story><identification><ifid>TWO</ifid></identification></story>")];
    XCTAssertEqual(ifiction.stories.count, 2u);
    XCTAssertEqualObjects(ifiction.firstStory.identification.ifids.anyObject, @"ONE");
}

#pragma mark - Legacy Zoom format

static NSXMLElement *ZoomID(NSString *serial, NSString *release, NSString *checksum) {
    NSMutableString *zcode = [NSMutableString string];
    if (serial)
        [zcode appendFormat:@"<serial>%@</serial>", serial];
    if (release)
        [zcode appendFormat:@"<release>%@</release>", release];
    if (checksum)
        [zcode appendFormat:@"<checksum>%@</checksum>", checksum];
    return Element([NSString stringWithFormat:@"<id><format>zcode</format><zcode>%@</zcode></id>", zcode]);
}

- (void)testZoomIfidMatchesBabel {
    // Date-like serials get the checksum, printed like babel's %04X
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"940726", @"88", @"a4b2")], @"ZCODE-88-940726-A4B2");
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"940726", @"88", @"3f")], @"ZCODE-88-940726-003F");
    // "000000", serials starting with 8 and non-digit serials do not
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"000000", @"1", @"ABCD")], @"ZCODE-1-000000");
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"870915", @"5", @"ABCD")], @"ZCODE-5-870915");
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"A10101", @"2", @"ABCD")], @"ZCODE-2-A10101");
    // Non-alphanumeric serial characters become dashes
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"12 4.6", @"3", @"0001")], @"ZCODE-3-12-4-6-0001");
    // A missing checksum is left out rather than printed as "(null)"
    XCTAssertEqualObjects([IFIdentification ifidStringFromZoomIDElement:ZoomID(@"940726", @"88", nil)], @"ZCODE-88-940726");
    // No serial, or not Z-code: no IFID
    XCTAssertNil([IFIdentification ifidStringFromZoomIDElement:ZoomID(nil, @"88", @"ABCD")]);
    XCTAssertNil([IFIdentification ifidStringFromZoomIDElement:Element(@"<id><format>glulx</format><zcode><serial>840726</serial></zcode></id>")]);
}

- (void)testZoomStoryUsesStoryElementAsBibliographic {
    IFStory *story = [[IFictionMetadata alloc] initWithData:
                      IFiction(@"<story><id><format>zcode</format><zcode><serial>940726</serial><release>88</release><checksum>A4B2</checksum></zcode></id>"
                               "<id><format>zcode</format><zcode><serial>940727</serial><release>89</release><checksum>0001</checksum></zcode></id>"
                               "<title>Zoomed</title></story>")].firstStory;
    NSSet *expected = [NSSet setWithArray:@[ @"ZCODE-88-940726-A4B2", @"ZCODE-89-940727-0001" ]];
    XCTAssertEqualObjects(story.identification.ifids, expected);
    XCTAssertEqualObjects(story.identification.format, @"zcode");
    XCTAssertEqualObjects(story.bibliographic.title, @"Zoomed");
}

#pragma mark - IFBibliographic

- (void)testFirstPublishedDoesNotOverwriteHeadline {
    IFBibliographic *biblio = Biblio(@"<headline>An Interactive Fiction</headline><firstpublished>1987</firstpublished>");
    XCTAssertEqualObjects(biblio.firstPublished, @"1987");

    Metadata *metadata = [self newMetadata];
    [metadata addInfoFromBibliographic:biblio];
    XCTAssertEqualObjects(metadata.headline, @"An Interactive Fiction");
    XCTAssertEqualObjects(metadata.firstpublished, @"1987");
    XCTAssertNotNil(metadata.firstpublishedDate);
}

- (void)testDates {
    NSCalendar *gregorian = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    gregorian.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];

    IFBibliographic *biblio = Biblio(@"<firstpublished>2004-05-17</firstpublished>");
    XCTAssertEqualObjects(biblio.firstPublished, @"2004");
    NSDateComponents *components = [gregorian components:NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay fromDate:biblio.firstPublishedDate];
    XCTAssertEqual(components.year, 2004);
    XCTAssertEqual(components.month, 5);
    XCTAssertEqual(components.day, 17);

    XCTAssertEqualObjects(Biblio(@"<year>1999</year>").firstPublished, @"1999");

    // Neither YYYY nor YYYY-MM-DD: ignored
    for (NSString *bad in @[ @"2004-05", @"May 2004", @"", @"abcd" ]) {
        IFBibliographic *b = Biblio([NSString stringWithFormat:@"<firstpublished>%@</firstpublished>", bad]);
        XCTAssertNil(b.firstPublished, @"%@", bad);
        XCTAssertNil(b.firstPublishedDate, @"%@", bad);
    }
}

- (void)testLanguage {
    NSString *word = nil;
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"en", &word), @"en");
    XCTAssertEqualObjects(word, @"English");
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"en-US", &word), @"en");
    XCTAssertEqualObjects(word, @"English");
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"eng", &word), @"eng");
    XCTAssertEqualObjects(word, @"English");
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"sv", &word), @"sv");
    XCTAssertEqualObjects(word, @"Swedish");
    // Descriptive strings: the first code in parentheses, lowercased
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"French, English (fr, en)", &word), @"fr");
    XCTAssertEqualObjects(word, @"French");
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"German (DE)", &word), @"de");
    XCTAssertEqualObjects(word, @"German");
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"English, Castilian (en-GB, es)", &word), @"en");
    XCTAssertEqualObjects(word, @"English");
    // Without parentheses, the first two letters, lowercased
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"English", &word), @"en");
    XCTAssertEqualObjects(word, @"English");
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"EN", &word), @"en");
    XCTAssertEqualObjects(word, @"English");
    // Unknown codes: the whole string for both
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"xq", &word), @"xq");
    XCTAssertEqualObjects(word, @"xq");
    // A single letter is kept as is, with no word
    XCTAssertEqualObjects(IFLanguageCodeFromString(@"e", &word), @"e");
    XCTAssertNil(word);

    IFBibliographic *biblio = Biblio(@"<language>en-US</language>");
    XCTAssertEqualObjects(biblio.language, @"en");
    XCTAssertEqualObjects(biblio.languageAsWord, @"English");
}

- (void)testForgiveness {
    IFBibliographic *biblio = Biblio(@"<forgiveness>Polite</forgiveness>");
    XCTAssertEqualObjects(biblio.forgiveness, @"Polite");
    XCTAssertEqualObjects(biblio.forgivenessNumeric, @4);

    Metadata *metadata = [self newMetadata];
    [metadata addInfoFromBibliographic:biblio];
    XCTAssertEqualObjects(metadata.forgiveness, @"Polite");
    XCTAssertEqualObjects(metadata.forgivenessNumeric, @4);

    XCTAssertNil(Biblio(@"<forgiveness>Very nice</forgiveness>").forgiveness);
    XCTAssertNil(Biblio(@"<forgiveness>Very nice</forgiveness>").forgivenessNumeric);
}

- (void)testTitleAndHeadline {
    XCTAssertEqualObjects(Biblio(@"<headline>One [em dash] Two</headline>").headline, @"One - Two");
    // An empty title does not replace one we have
    XCTAssertNil(Biblio(@"<title></title>").title);
}

- (void)testDescription {
    XCTAssertEqualObjects(Biblio(@"<description>First<br/>Second</description>").blurb, @"First\n\nSecond");
    XCTAssertEqualObjects(Biblio(@"<description>  One&amp;lt;br&amp;gt;Two  </description>").blurb, @"One\nTwo");
    XCTAssertEqualObjects(Biblio(@"<description>Line\\nbreak</description>").blurb, @"Line\nbreak");
    XCTAssertEqualObjects(Biblio(@"<teaser>Teaser</teaser>").blurb, @"Teaser");
}

#pragma mark - IFDB

- (void)testViewGameURL {
    XCTAssertEqualObjects([IFDB viewGameURLForTUID:@"abc" IFID:@"IGNORED" iFiction:NO].absoluteString,
                          @"https://ifdb.org/viewgame?id=abc");
    XCTAssertEqualObjects([IFDB viewGameURLForTUID:@"" IFID:@"ZCODE-88-840726" iFiction:YES].absoluteString,
                          @"https://ifdb.org/viewgame?ifiction&ifid=ZCODE-88-840726");
    XCTAssertEqualObjects([IFDB viewGameURLForTUID:nil IFID:@"a b&c" iFiction:NO].absoluteString,
                          @"https://ifdb.org/viewgame?ifid=a%20b%26c");
    XCTAssertNil([IFDB viewGameURLForTUID:nil IFID:@"" iFiction:NO]);
}

#pragma mark - Metadata+IFiction

- (void)testAddIfidsSkipsDuplicatesAndPlaceholders {
    Metadata *metadata = [self newMetadata];
    [metadata createIfid:@"EXISTING"];
    [metadata addIfidStrings:[NSSet setWithArray:@[ @"EXISTING", @"NEW", @"DUMMYmanySelected", @"DUMMYnoneSelected" ]]];
    NSSet *expected = [NSSet setWithArray:@[ @"EXISTING", @"NEW" ]];
    XCTAssertEqualObjects([metadata.ifids valueForKey:@"ifidString"], expected);
}

#pragma mark - Cover images

- (void)testInsertImageDataCreatesImageAndDropsOrphanedCover {
    Metadata *metadata = [self newMetadata];
    metadata.coverArtURL = @"https://example.com/new.png";
    metadata.coverArtDescription = @"New cover";
    Image *old = [self newImageWithData:[@"old" dataUsingEncoding:NSUTF8StringEncoding] url:@"https://example.com/old.png"];
    metadata.cover = old;

    NSData *data = [@"new" dataUsingEncoding:NSUTF8StringEncoding];
    [CoverImageStore insertImageData:data inMetadataID:metadata.objectID context:self.context];
    [self drainContext];

    XCTAssertEqualObjects((NSData *)metadata.cover.data, data);
    XCTAssertEqualObjects(metadata.cover.originalURL, @"https://example.com/new.png");
    XCTAssertEqualObjects(metadata.cover.imageDescription, @"New cover");
    XCTAssertTrue(old.isDeleted || old.managedObjectContext == nil);
}

- (void)testInsertImageDataReusesImageWithSameURLAndData {
    NSData *data = [@"same" dataUsingEncoding:NSUTF8StringEncoding];
    Image *existing = [self newImageWithData:data url:@"https://example.com/same.png"];
    existing.imageDescription = @"From the image";
    Image *old = [self newImageWithData:[@"old" dataUsingEncoding:NSUTF8StringEncoding] url:@"https://example.com/old.png"];

    Metadata *metadata = [self newMetadata];
    metadata.coverArtURL = @"https://example.com/same.png";
    metadata.cover = old;

    [CoverImageStore insertImageData:data inMetadataID:metadata.objectID context:self.context];
    [self drainContext];

    XCTAssertEqual(metadata.cover, existing);
    XCTAssertEqualObjects(metadata.coverArtDescription, @"From the image");
    // The previous cover is no longer used by anything
    XCTAssertTrue(old.isDeleted || old.managedObjectContext == nil);

    NSFetchRequest *request = [Image fetchRequest];
    request.predicate = [NSPredicate predicateWithFormat:@"originalURL == %@", @"https://example.com/same.png"];
    XCTAssertEqual([self.context countForFetchRequest:request error:nil], 1u);
}

@end
