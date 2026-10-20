//
//  PreviewControllerTests.m
//  SpatterlightTests
//
//  The sample text of the Preferences preview, built without a view from
//  built-in themes in an in-memory store.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "Theme.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "PreviewController.h"

@interface PreviewControllerTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;

@end

@implementation PreviewControllerTests

- (void)setUp {
    [super setUp];
    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
}

- (void)tearDown {
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

- (Theme *)themeNamed:(NSString *)name {
    NSFetchRequest *request = [Theme fetchRequest];
    request.predicate = [NSPredicate predicateWithFormat:@"name == %@", name];
    NSError *error = nil;
    Theme *theme = [self.context executeFetchRequest:request error:&error].firstObject;
    XCTAssertNil(error);
    XCTAssertNotNil(theme, @"No built-in theme named %@", name);
    return theme;
}

- (void)testPreviewUsesTheThreeStyles {
    for (NSString *name in @[ @"Default", @"Zoom", @"MS-DOS", @"Lectrote Dark" ]) {
        Theme *theme = [self themeNamed:name];
        NSAttributedString *preview = [PreviewController attributedPreviewForTheme:theme];

        NSString *heading = @"Palace Gate";
        NSString *body = @" A tide of perambulators surges north along the crowded Broad Walk. ";
        NSString *credit = @"(Trinity, Brian Moriarty, Infocom 1986)";
        XCTAssertEqualObjects(preview.string, ([NSString stringWithFormat:@"%@%@%@", heading, body, credit]));

        NSDictionary *headingAttrs = [preview attributesAtIndex:0 effectiveRange:nil];
        NSDictionary *bodyAttrs = [preview attributesAtIndex:heading.length + 1 effectiveRange:nil];
        NSDictionary *creditAttrs = [preview attributesAtIndex:preview.length - 1 effectiveRange:nil];

        // The heading has the subheader font and colour, but the body's paragraph style
        XCTAssertEqualObjects(headingAttrs[NSFontAttributeName], theme.bufSubH.attributeDict[NSFontAttributeName], @"%@", name);
        XCTAssertEqualObjects(headingAttrs[NSForegroundColorAttributeName], theme.bufSubH.attributeDict[NSForegroundColorAttributeName], @"%@", name);
        XCTAssertEqualObjects(headingAttrs[NSParagraphStyleAttributeName], theme.bufferNormal.attributeDict[NSParagraphStyleAttributeName], @"%@", name);

        XCTAssertEqualObjects(bodyAttrs, theme.bufferNormal.attributeDict, @"%@", name);
        XCTAssertEqualObjects(creditAttrs, theme.bufEmph.attributeDict, @"%@", name);
    }
}

- (void)testPreviewOfNilThemeHasNoAttributes {
    NSAttributedString *preview = [PreviewController attributedPreviewForTheme:(Theme * _Nonnull)nil];
    XCTAssertTrue(preview.length > 0);
    XCTAssertEqual([preview attributesAtIndex:0 effectiveRange:nil].count, 0u);
}

@end
