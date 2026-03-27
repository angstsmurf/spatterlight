//
//  GameImportXCTests.m
//  SpatterlightTests
//
//   Tests for importing game files into the Core Data database
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "TableViewController.h"
#import "GameImporter.h"
#import "Game.h"
#import "Metadata.h"
#import "CoreDataManager.h"
#import "Fetches.h"

@interface GameImportXCTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) TableViewController *tableViewController;
@property (nonatomic, strong) NSManagedObjectContext *testContext;

@end

@implementation GameImportXCTests

- (void)setUp {
    [super setUp];

    // Set up an in-memory Core Data stack for testing
    [self setUpInMemoryCoreDataStack];

    // Initialize TableViewController
    self.tableViewController = [[TableViewController alloc] init];

    // Set the test context on the table view controller
    // This ensures it uses our in-memory database instead of the real one
    if ([self.tableViewController respondsToSelector:@selector(setManagedObjectContext:)]) {
        [self.tableViewController setValue:self.testContext forKey:@"managedObjectContext"];
    }

    // Initialize other required properties
    if ([self.tableViewController respondsToSelector:@selector(setGameTableModel:)]) {
        [self.tableViewController setValue:[NSMutableArray new] forKey:@"gameTableModel"];
    }

    if ([self.tableViewController respondsToSelector:@selector(setGameSessions:)]) {
        [self.tableViewController setValue:[NSMutableDictionary new] forKey:@"gameSessions"];
    }

    self.tableViewController.currentlyAddingGames = YES;
}

- (void)tearDown {
    self.tableViewController = nil;
    self.testContext = nil;
    self.coreDataManager = nil;

    [super tearDown];
}

- (void)setUpInMemoryCoreDataStack {
    // Create a CoreDataManager with in-memory store for testing
    self.coreDataManager = [[CoreDataManager alloc] initWithModelName:@"Spatterlight"];

    // Get the managed object context
    self.testContext = self.coreDataManager.mainManagedObjectContext;

    // Configure for in-memory testing if CoreDataManager supports it
    // Alternative: Create a minimal in-memory stack manually
    if (!self.testContext) {
        // Manual setup if CoreDataManager doesn't provide a context
        NSURL *modelURL = [[NSBundle bundleForClass:[CoreDataManager class]] URLForResource:@"Spatterlight" withExtension:@"momd"];
        NSManagedObjectModel *model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];

        NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];

        NSError *error = nil;
        [coordinator addPersistentStoreWithType:NSInMemoryStoreType
                                  configuration:nil
                                            URL:nil
                                        options:nil
                                          error:&error];

        if (error) {
            NSLog(@"Failed to create in-memory store: %@", error);
        }

        self.testContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
        self.testContext.persistentStoreCoordinator = coordinator;
    }
}

#pragma mark - Tests

- (void)testImportCursesGameFile {
    // Create an expectation for the async import operation
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import completes"];

    // Get the URL for curses.z5 in the test bundle
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSURL *gameFileURL = [bundle URLForResource:@"curses" withExtension:@"z5"];

    XCTAssertNotNil(gameFileURL, @"curses.z5 should exist in Supporting Files");
    XCTAssertTrue([[NSFileManager defaultManager] fileExistsAtPath:gameFileURL.path],
                  @"curses.z5 file should be accessible");

    
    // Delete the game if it is already added
    NSFetchRequest *gameFetch = [Game fetchRequest];
    gameFetch.predicate = [NSPredicate predicateWithFormat:@"path == %@", gameFileURL.path];

    NSError *fetchError = nil;
    NSArray<Game *> *results = [self.testContext executeFetchRequest:gameFetch error:&fetchError];

    XCTAssertNil(fetchError);

    if (results.count) {
        [self.testContext deleteObject:results.firstObject];
    }

    // Get initial game count
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    NSError *error = nil;

    NSUInteger initialCount = [self.testContext countForFetchRequest:fetchRequest error:&error];
    XCTAssertNil(error, @"Should be able to count games");

    // Create GameImporter
    GameImporter *importer = [[GameImporter alloc] initWithTableViewController:self.tableViewController];

    // Observe context saves to know when import is complete
    __block id observer = [[NSNotificationCenter defaultCenter] addObserverForName:NSManagedObjectContextDidSaveNotification
                                                                             object:self.testContext
                                                                              queue:nil
                                                                         usingBlock:^(NSNotification * _Nonnull note) {
        // Wait a bit for all operations to complete, then verify on the context's queue
        [self.testContext performBlock:^{
            // Verify the game was added
            NSError *countError = nil;
            NSUInteger finalCount = [self.testContext countForFetchRequest:fetchRequest error:&countError];
            
            if (finalCount > initialCount) {
                // Import completed, remove observer and verify
                [[NSNotificationCenter defaultCenter] removeObserver:observer];
                
                XCTAssertNil(countError, @"Should be able to count games after import");
                XCTAssertEqual(finalCount, initialCount + 1, @"One game should have been added");

                // Fetch the imported game
                NSFetchRequest *gameFetch = [Game fetchRequest];
                gameFetch.predicate = [NSPredicate predicateWithFormat:@"path == %@", gameFileURL.path];

                NSError *fetchError = nil;
                NSArray<Game *> *results = [self.testContext executeFetchRequest:gameFetch error:&fetchError];

                XCTAssertNil(fetchError, @"Should be able to fetch the imported game");
                XCTAssertEqual(results.count, 1, @"Should find exactly one game with the given path");

                if (results.count > 0) {
                    Game *game = results.firstObject;

                    // Verify game properties
                    XCTAssertEqualObjects(game.path, gameFileURL.path, @"Game path should match");
                    XCTAssertNotNil(game.metadata, @"Game should have metadata");
                    XCTAssertTrue(game.found, @"Game should be marked as found");

                    if (game.metadata) {
                        XCTAssertNotNil(game.metadata.title, @"Game should have a title");
                        XCTAssertTrue(game.metadata.title.length > 0, @"Game title should not be empty");

                        NSLog(@"Successfully imported game: %@", game.metadata.title);
                    }
                }

                [importExpectation fulfill];
            }
        }];
    }];

    // Set up options
    NSDictionary *options = @{
        @"lookForImages": @(YES),
        @"downloadInfo": @(YES),
        @"context": self.testContext
    };

    // Import the game
    [importer addFiles:@[gameFileURL] options:options];

    // Wait for expectations with a longer timeout for network operations
    [self waitForExpectationsWithTimeout:10.0 handler:^(NSError * _Nullable error) {
        [[NSNotificationCenter defaultCenter] removeObserver:observer];
        if (error) {
            XCTFail(@"Import operation timed out: %@", error);
        }
    }];
}

- (void)testImportCursesWithoutImages {
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import without images"];

    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSURL *gameFileURL = [bundle URLForResource:@"curses" withExtension:@"z5"];

    XCTAssertNotNil(gameFileURL);

    GameImporter *importer = [[GameImporter alloc] initWithTableViewController:self.tableViewController];

    // Delete the game if it is already added
    NSFetchRequest *gameFetch = [Game fetchRequest];
    gameFetch.predicate = [NSPredicate predicateWithFormat:@"path == %@", gameFileURL.path];

    NSError *fetchError = nil;
    NSArray<Game *> *results = [self.testContext executeFetchRequest:gameFetch error:&fetchError];

    XCTAssertNil(fetchError);

    if (results.count) {
        [self.testContext deleteObject:results.firstObject];
    }


    // Get initial game count
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    NSError *error = nil;
    NSUInteger initialCount = [self.testContext countForFetchRequest:fetchRequest error:&error];
    XCTAssertNil(error, @"Should be able to count games");

    // Observe context saves to know when import is complete
    __block id observer = [[NSNotificationCenter defaultCenter] addObserverForName:NSManagedObjectContextDidSaveNotification
                                                                             object:self.testContext
                                                                              queue:nil
                                                                         usingBlock:^(NSNotification * _Nonnull note) {
        // Verify on the context's queue to avoid priority inversion
        [self.testContext performBlock:^{
            NSError *countError = nil;
            NSUInteger finalCount = [self.testContext countForFetchRequest:fetchRequest error:&countError];
            
            if (finalCount > initialCount) {
                // Import completed, remove observer and verify
                [[NSNotificationCenter defaultCenter] removeObserver:observer];
                
                NSFetchRequest *gameFetch = [Game fetchRequest];
                gameFetch.predicate = [NSPredicate predicateWithFormat:@"path == %@", gameFileURL.path];

                NSError *fetchError = nil;
                NSArray<Game *> *results = [self.testContext executeFetchRequest:gameFetch error:&fetchError];

                XCTAssertNil(fetchError);
                XCTAssertGreaterThan(results.count, 0, @"Game should be imported");

                [importExpectation fulfill];
            }
        }];
    }];

    NSDictionary *options = @{
        @"lookForImages": @(NO),  // Don't look for images
        @"downloadInfo": @(NO),   // Don't download info
        @"context": self.testContext
    };

    [importer addFiles:@[gameFileURL] options:options];

    [self waitForExpectationsWithTimeout:10.0 handler:^(NSError * _Nullable error) {
        [[NSNotificationCenter defaultCenter] removeObserver:observer];
        if (error) {
            XCTFail(@"Import operation timed out: %@", error);
        }
    }];
}

@end
