//
//  AppSupportDirTests.m
//  SpatterlightTests
//
//  Tests for GlkController's -appSupportDir method.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "CoreDataManager.h"
#import "GlkController.h"
#import "Game.h"
#import "Metadata.h"
#import "Theme.h"

@interface AppSupportDirTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, copy) NSString *tempFilePath;
@property (nonatomic, copy) NSString *createdDir;

@end

@implementation AppSupportDirTests

- (void)setUp {
    [super setUp];

    _coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    _context = _coreDataManager.mainManagedObjectContext;
    XCTAssertNotNil(_context);

    // Create a temporary file with enough bytes for signatureFromFile (needs >= 64 bytes)
    NSString *tempDir = NSTemporaryDirectory();
    _tempFilePath = [tempDir stringByAppendingPathComponent:@"appSupportDirTest_gamefile.dat"];
    NSMutableData *dummyData = [NSMutableData dataWithLength:128];
    // Fill with non-zero data so the signature is meaningful
    uint8_t *bytes = dummyData.mutableBytes;
    for (NSUInteger i = 0; i < 128; i++) {
        bytes[i] = (uint8_t)(i & 0xFF);
    }
    [dummyData writeToFile:_tempFilePath atomically:YES];
}

- (void)tearDown {
    // Remove the temporary game file
    if (_tempFilePath) {
        [[NSFileManager defaultManager] removeItemAtPath:_tempFilePath error:nil];
    }

    // Remove the directory that was created by appSupportDir, if any
    if (_createdDir) {
        [[NSFileManager defaultManager] removeItemAtPath:_createdDir error:nil];
    }

    _context = nil;
    _coreDataManager = nil;

    [super tearDown];
}

- (void)testAppSupportDirCreatesDirectoryWithTimeout {
    XCTestExpectation *expectation = [self expectationWithDescription:@"appSupportDir completes in time"];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self testAppSupportDirCreatesDirectory];
        [expectation fulfill];
    });

    [self waitForExpectations:@[expectation] timeout:5.0];
}

- (void)testAppSupportDirCreatesDirectory {
    // Create Core Data objects
    Game *game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                              inManagedObjectContext:_context];
    Metadata *metadata = [NSEntityDescription insertNewObjectForEntityForName:@"Metadata"
                                                      inManagedObjectContext:_context];
    Theme *theme = [NSEntityDescription insertNewObjectForEntityForName:@"Theme"
                                                inManagedObjectContext:_context];

    metadata.title = @"Test Game";
    game.metadata = metadata;
    game.detectedFormat = @"zcode";
    theme.autosave = YES;

    // Create a GlkController and configure it
    GlkController *controller = [[GlkController alloc] init];

    // Set properties using KVC to reach readonly/private ivars
    [controller setValue:game forKey:@"game"];
    [controller setValue:_tempFilePath forKey:@"gamefile"];
    [controller setValue:theme forKey:@"theme"];
    [controller setValue:@YES forKey:@"supportsAutorestore"];

    // Clear any cached value
    controller.appSupportDir = nil;

    // Call the method under test
    NSString *result = controller.appSupportDir;

    // Store the result for cleanup
    _createdDir = result;

    XCTAssertNotNil(result, @"appSupportDir should return a non-nil path");
    XCTAssertGreaterThan(result.length, 0, @"appSupportDir should return a non-empty path");

    // Verify the directory was actually created on disk
    BOOL isDirectory = NO;
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:result isDirectory:&isDirectory];
    XCTAssertTrue(exists, @"The directory at appSupportDir path should exist");
    XCTAssertTrue(isDirectory, @"The path returned by appSupportDir should be a directory");

    // Verify the path contains expected components
    XCTAssertTrue([result containsString:@"Spatterlight"], @"Path should contain 'Spatterlight'");
    XCTAssertTrue([result containsString:@"Bocfel Files"], @"Path should contain 'Bocfel Files' for zcode format");
    XCTAssertTrue([result containsString:@"Autosaves"], @"Path should contain 'Autosaves'");
}

@end
