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
#import "GameLauncher.h"
#import "GlkController.h"
#import "GlkController+Autorestore.h"
#import "Game.h"
#import "Theme.h"
#import "Metadata.h"
#import "CoreDataManager.h"
#import "Fetches.h"
#import "NSString+Categories.h"
#import "BuiltInThemes.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkGraphicsWindow.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "Preferences.h"
#import <BlorbFramework/BlorbFramework.h>
#include "glk.h"

// Little helpers for hand-assembling Blorb test fixtures.
static void blorbAppendFourCC(NSMutableData *data, const char *fourCC) {
    [data appendBytes:fourCC length:4];
}

static void blorbAppendBE32(NSMutableData *data, uint32_t value) {
    uint8_t bytes[4] = {
        (uint8_t)(value >> 24), (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),  (uint8_t)value
    };
    [data appendBytes:bytes length:4];
}

// Expose the private progressive-migration entry point for testing.
@interface CoreDataManager (ProgressiveMigrationTesting)
- (BOOL)progressivelyMigrateStoreAtURL:(NSURL *)sourceURL
                                ofType:(NSString *)type
                               toModel:(NSManagedObjectModel *)finalModel
                                 error:(NSError * __autoreleasing *)error;
@end

@interface GameImportXCTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) TableViewController *tableViewController;
@property (nonatomic, strong) NSManagedObjectContext *testContext;

// Helper methods
+ (NSURL *)tempDir;
- (NSURL *)cursesGameFileURL;
- (NSURL *)commandScriptFileURLForGame:(NSString *)gameName;
- (void)deleteGameAtPath:(NSString *)path;
- (NSUInteger)currentGameCount;
- (Game *)fetchGameAtPath:(NSString *)path;
- (void)observeImportCompletionWithInitialCount:(NSUInteger)initialCount
                                      gameFileURL:(NSURL *)gameFileURL
                                   fetchRequest:(NSFetchRequest *)fetchRequest
                                     onComplete:(void (^)(Game *importedGame))completionBlock;
- (void)verifyGame:(Game *)game hasPath:(NSString *)path;
- (GameImporter *)createGameImporter;
- (GameLauncher *)createAndSetupGameLauncher;
- (void)importAndRunGameFile:(NSString *)gameFileName
           commandScriptName:(NSString *)scriptName;

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

    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"LogInterpreterOutput"];

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

#pragma mark - Helper Methods

// Get the URL for curses.z5 in the test bundle
- (NSURL *)cursesGameFileURL {
    return [self gameFileURLForFileNamed:@"curses.z5"];
}

// Get the URL for the file name in the test bundle
- (NSURL *)gameFileURLForFileNamed:(NSString *)fileName {
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSString *name = [fileName stringByDeletingPathExtension];
    NSString *extension = [fileName pathExtension];
    NSURL *gameFileURL = [bundle URLForResource:name withExtension:extension];
    XCTAssertNotNil(gameFileURL, @"%@should exist in Supporting Files", fileName);
    XCTAssertTrue([[NSFileManager defaultManager] fileExistsAtPath:gameFileURL.path],
                  @"%@ file should be accessible", fileName);
    return gameFileURL;
}

// Get the URL for the command script file in the test bundle
- (NSURL *)commandScriptFileURLForGame:(NSString *)gameName {
    NSString *fileName = [NSString stringWithFormat:@"%@ command script", gameName];
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    return [bundle URLForResource:fileName withExtension:@"txt"];
}

+ (NSURL *)tempDir {
    NSURL *usersURL = [NSURL fileURLWithPath:@"/Users"
                                 isDirectory:YES];

    NSError *error = nil;

    NSURL *tempURL = [[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory
                                                            inDomain:NSUserDomainMask
                                                   appropriateForURL:usersURL
                                                              create:YES
                                                               error:&error];
    if (error)
        NSLog(@"tempDir: Error: %@", error);

    return tempURL;
}

// Delete a game at the given path if it exists
- (void)deleteGameAtPath:(NSString *)path {
    NSFetchRequest *gameFetch = [Game fetchRequest];
    NSString *hash = [path signatureFromFile];
    gameFetch.predicate = [NSPredicate predicateWithFormat:@"hashTag == %@", hash];

    NSError *error = nil;
    NSArray<Game *> *results = [self.testContext executeFetchRequest:gameFetch error:&error];
    
    XCTAssertNil(error, @"Should be able to fetch games for deletion");
    
    if (results.count) {
        [self.testContext deleteObject:results.firstObject];
        NSError *saveError = nil;
        [self.testContext save:&saveError];
        XCTAssertNil(saveError, @"Should be able to save after deletion");
    }
}

// Get the current count of games in the database
- (NSUInteger)currentGameCount {
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    NSError *error = nil;
    NSUInteger count = [self.testContext countForFetchRequest:fetchRequest error:&error];
    XCTAssertNil(error, @"Should be able to count games");
    return count;
}

// Fetch a game at the given path
- (Game *)fetchGameAtPath:(NSString *)path {
    NSFetchRequest *gameFetch = [Game fetchRequest];
    gameFetch.predicate = [NSPredicate predicateWithFormat:@"path == %@", path];
    
    NSError *error = nil;
    NSArray<Game *> *results = [self.testContext executeFetchRequest:gameFetch error:&error];
    
    XCTAssertNil(error, @"Should be able to fetch game");
    return results.firstObject;
}

// Observe import completion and call the completion block when done
- (void)observeImportCompletionWithInitialCount:(NSUInteger)initialCount
                                      gameFileURL:(NSURL *)gameFileURL
                                     fetchRequest:(NSFetchRequest *)fetchRequest
                                       onComplete:(void (^)(Game *importedGame))completionBlock {
    
    __block id observer = [[NSNotificationCenter defaultCenter] 
        addObserverForName:NSManagedObjectContextDidSaveNotification
                    object:self.testContext
                     queue:nil
                usingBlock:^(NSNotification * _Nonnull note) {
        // Verify on the context's queue to avoid priority inversion
        [self.testContext performBlock:^{
            NSError *countError = nil;
            NSUInteger finalCount = [self.testContext countForFetchRequest:fetchRequest error:&countError];
            
            if (finalCount > initialCount) {
                // Import completed, remove observer
                [[NSNotificationCenter defaultCenter] removeObserver:observer];
                
                XCTAssertNil(countError, @"Should be able to count games after import");
                XCTAssertEqual(finalCount, initialCount + 1, @"One game should have been added");
                
                // Fetch the imported game
                Game *game = [self fetchGameAtPath:gameFileURL.path];
                XCTAssertNotNil(game, @"Should be able to fetch the imported game");
                
                if (game && completionBlock) {
                    completionBlock(game);
                }
            }
        }];
    }];
}

// Verify standard game properties
- (void)verifyGame:(Game *)game hasPath:(NSString *)path {
    XCTAssertEqualObjects(game.path, path, @"Game path should match");
    XCTAssertNotNil(game.metadata, @"Game should have metadata");
    XCTAssertTrue(game.found, @"Game should be marked as found");
    
    if (game.metadata) {
        XCTAssertNotNil(game.metadata.title, @"Game should have a title");
        XCTAssertTrue(game.metadata.title.length > 0, @"Game title should not be empty");
    }
}

// Create a GameImporter instance
- (GameImporter *)createGameImporter {
    return [[GameImporter alloc] initWithTableViewController:self.tableViewController];
}

// Create and setup a GameLauncher instance
- (GameLauncher *)createAndSetupGameLauncher {
    GameLauncher *launcher = [[GameLauncher alloc] initWithTableViewController:self.tableViewController];
    
    // Set the launcher on the table view controller
    if ([self.tableViewController respondsToSelector:@selector(setGameLauncher:)]) {
        [self.tableViewController setValue:launcher forKey:@"gameLauncher"];
    }
    
    return launcher;
}

+ (NSString *)comparisonLogFor:(NSString *)name {
    name = [NSString stringWithFormat:@"%@ log output", name];

    NSBundle *bundle = [NSBundle bundleForClass:[self class]];

    NSURL *url = [bundle URLForResource:name
                              withExtension:@"txt"
                               subdirectory:nil];

    XCTAssertNotNil(url, @"%@ should exist in Supporting Files", name);

    NSError *error = nil;
    NSString *facit = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:&error];
    if (error)
        NSLog(@"Error: %@", error);
    return facit;
}

#pragma mark - Tests

- (void)testImportCursesGameFile {
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import completes"];

    // Get the game file URL
    NSURL *gameFileURL = [self cursesGameFileURL];
    
    // Delete any existing copy
    [self deleteGameAtPath:gameFileURL.path];
    
    // Get initial game count
    NSUInteger initialCount = [self currentGameCount];
    NSFetchRequest *fetchRequest = [Game fetchRequest];

    // Create GameImporter
    GameImporter *importer = [self createGameImporter];

    // Observe import completion
    [self observeImportCompletionWithInitialCount:initialCount
                                        gameFileURL:gameFileURL
                                       fetchRequest:fetchRequest
                                         onComplete:^(Game *game) {
        // Verify game properties
        [self verifyGame:game hasPath:gameFileURL.path];
        
        NSLog(@"Successfully imported game: %@", game.metadata.title);
        [importExpectation fulfill];
    }];

    // Set up options (with images and download)
    NSDictionary *options = @{
        @"lookForImages": @(YES),
        @"downloadInfo": @(YES),
        @"context": self.testContext
    };

    // Import the game
    [importer addFiles:@[gameFileURL] options:options];

    // Wait for expectations with a longer timeout for network operations
    [self waitForExpectationsWithTimeout:10.0 handler:^(NSError * _Nullable error) {
        if (error) {
            XCTFail(@"Import operation timed out: %@", error);
        }
    }];
}

- (void)testImportCursesWithoutImages {
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import without images"];

    // Get the game file URL
    NSURL *gameFileURL = [self cursesGameFileURL];
    
    // Delete any existing copy
    [self deleteGameAtPath:gameFileURL.path];
    
    // Get initial game count
    NSUInteger initialCount = [self currentGameCount];
    NSFetchRequest *fetchRequest = [Game fetchRequest];

    // Create GameImporter
    GameImporter *importer = [self createGameImporter];

    // Observe import completion
    [self observeImportCompletionWithInitialCount:initialCount
                                        gameFileURL:gameFileURL
                                       fetchRequest:fetchRequest
                                         onComplete:^(Game *game) {
        XCTAssertNotNil(game, @"Game should be imported");
        [importExpectation fulfill];
    }];

    // Set up options (no images, no download)
    NSDictionary *options = @{
        @"lookForImages": @(NO),
        @"downloadInfo": @(NO),
        @"context": self.testContext
    };

    [importer addFiles:@[gameFileURL] options:options];

    [self waitForExpectationsWithTimeout:10.0 handler:^(NSError * _Nullable error) {
        if (error) {
            XCTFail(@"Import operation timed out: %@", error);
        }
    }];
}

- (void)testImportAndStartCursesGame {
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import completes"];
    XCTestExpectation *gameStartExpectation = [self expectationWithDescription:@"Game starts running"];
    XCTestExpectation *commandScriptExpectation = [self expectationWithDescription:@"Command script completes"];

    // Get file URLs
    NSURL *gameFileURL = [self cursesGameFileURL];
    NSURL *scriptURL = [self commandScriptFileURLForGame:@"ScottFree"];

    // Delete any existing copy
    [self deleteGameAtPath:gameFileURL.path];
    
    // Get initial game count
    NSUInteger initialCount = [self currentGameCount];
    NSFetchRequest *fetchRequest = [Game fetchRequest];

    // Create GameImporter and GameLauncher
    GameImporter *importer = [self createGameImporter];
    GameLauncher *launcher = [self createAndSetupGameLauncher];

    // Observe command script completion notification
    __block id scriptObserver = [[NSNotificationCenter defaultCenter] 
        addObserverForName:@"GlkCommandScriptDidComplete"
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *note) {
        // Command script has completed
        NSLog(@"Command script completed successfully");
        
        // Get the game controller to verify it's still running
        GlkController *glkController = nil;
        for (GlkController *controller in self.tableViewController.gameSessions.allValues) {
            if (controller.isAlive) {
                glkController = controller;
                break;
            }
        }
        
        if (glkController) {
            XCTAssertTrue(glkController.isAlive, @"Game should still be running after command script");
            
            // Clean up: close the game window
            [glkController.window performClose:nil];
        }
        
        [commandScriptExpectation fulfill];
        [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
    }];

    // Observe import completion
    [self observeImportCompletionWithInitialCount:initialCount
                                        gameFileURL:gameFileURL
                                       fetchRequest:fetchRequest
                                         onComplete:^(Game *game) {
        // Verify game properties
        [self verifyGame:game hasPath:gameFileURL.path];
        [importExpectation fulfill];
        
        // Now try to start the game
        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *gameWindow = [launcher playGame:game restorationHandler:nil];
            
            // Give the game a moment to initialize
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                XCTAssertNotNil(gameWindow, @"Game window should be created");
                
                // Check if the game is actually running
                GlkController *glkController = self.tableViewController.gameSessions[game.hashTag];
                XCTAssertNotNil(glkController, @"GlkController should be created");
                
                if (glkController) {
                    XCTAssertTrue(glkController.isAlive, @"Game should be running");
                    XCTAssertNotNil(glkController.window, @"Game window should exist");
                    
                    NSLog(@"Successfully started game: %@", game.metadata.title);
                    [gameStartExpectation fulfill];
                    
                    // Run command script if available
                    if (scriptURL) {
                        NSLog(@"Running command script: %@", scriptURL.path);
                        
                        // Give the game a moment to be fully ready, then start the script
                        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                            [launcher runCommandsFromFile:scriptURL.path];
                            // The scriptObserver will handle fulfilling commandScriptExpectation
                            // when the GlkCommandScriptDidComplete notification is posted
                        });
                    } else {
                        NSLog(@"No command script found, skipping");
                        [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
                        [commandScriptExpectation fulfill];
                        
                        // Clean up: close the game window
                        [glkController.window performClose:nil];
                    }
                } else {
                    [gameStartExpectation fulfill];
                    [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
                    [commandScriptExpectation fulfill];
                }
            });
        });
    }];

    // Set up options (no images/download to speed up test)
    NSDictionary *options = @{
        @"lookForImages": @(NO),
        @"downloadInfo": @(NO),
        @"context": self.testContext
    };

    // Import the game
    [importer addFiles:@[gameFileURL] options:options];

    // Wait for expectations with a longer timeout for game startup and command script
    [self waitForExpectationsWithTimeout:30.0 handler:^(NSError * _Nullable error) {
        [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
        if (error) {
            XCTFail(@"Test timed out: %@", error);
        }
    }];
}

- (void)importAndRunGameFile:(NSString *)gameFileName
           commandScriptName:(NSString *)scriptName {
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import completes"];
    XCTestExpectation *gameStartExpectation = [self expectationWithDescription:@"Game starts running"];
    XCTestExpectation *commandScriptExpectation = [self expectationWithDescription:@"Command script completes"];
    XCTestExpectation *logsMatchedExpectation = [self expectationWithDescription:@"Interpreter log output matched"];

    // Get file URLs
    NSURL *gameFileURL = [self gameFileURLForFileNamed:gameFileName];
    NSURL *scriptURL = [self commandScriptFileURLForGame:scriptName];

    XCTAssertNotNil(scriptURL, @"%@ should exist in Supporting Files", scriptName);

    // Delete any existing copy
    [self deleteGameAtPath:gameFileURL.path];

    // Get initial game count
    NSUInteger initialCount = [self currentGameCount];
    NSFetchRequest *fetchRequest = [Game fetchRequest];

    // Create GameImporter and GameLauncher
    GameImporter *importer = [self createGameImporter];
    GameLauncher *launcher = [self createAndSetupGameLauncher];

    // Store original determinism setting to restore later
    __block BOOL originalDeterminismSetting = NO;
    __block BOOL originalSlowDrawSetting = NO;
    __block BOOL originalAutosaveSetting = NO;
    __block NSURL *tempDir = [GameImportXCTests tempDir];
    __block Theme *oldtheme = nil;

    // Observe command script completion notification
    __block id scriptObserver = [[NSNotificationCenter defaultCenter]
                                 addObserverForName:@"GlkCommandScriptDidComplete"
                                 object:nil
                                 queue:[NSOperationQueue mainQueue]
                                 usingBlock:^(NSNotification *note) {
        // Command script has completed
        NSLog(@"Command script completed successfully");

        // Get the game controller
        GlkController *glkController = self.tableViewController.gameSessions.allValues.firstObject;

        if (glkController) {
            // Restore original determinism setting
            Game *game = glkController.game;
            if (game && game.theme) {
                game.theme.determinism = originalDeterminismSetting;
                game.theme.slowDrawing = originalSlowDrawSetting;
                game.theme.autosave = originalAutosaveSetting;
                game.theme = oldtheme;
                NSLog(@"Restored determinism setting to %@", originalDeterminismSetting ? @"YES" : @"NO");
                NSLog(@"Restored slow draw setting to %@", originalSlowDrawSetting ? @"YES" : @"NO");
                NSLog(@"Restored autosave setting to %@", originalAutosaveSetting ? @"YES" : @"NO");
            }

            // Clean up: close the game window
            [glkController.window performClose:nil];
            // Compare log against comparison file in test bundle
            NSArray *contentsOfDir = [[NSFileManager defaultManager]
                                      contentsOfDirectoryAtURL:tempDir
                                      includingPropertiesForKeys:@[ NSURLNameKey ]
                                      options:NSDirectoryEnumerationSkipsHiddenFiles
                                      error:nil];

            NSURL *logURL = contentsOfDir.firstObject;

            XCTAssertNotNil(logURL, @"Log file should be created");

            NSError *error = nil;
            NSString *log = [NSString stringWithContentsOfURL:logURL encoding:NSUTF8StringEncoding error:&error];
            if (error)
                NSLog(@"Error: %@", error);
            XCTAssertNil(error, @"There should be no error");
            XCTAssert(log.length > 0, @"Log file should contain text");

            NSString *facit = [GameImportXCTests comparisonLogFor:scriptName];
            XCTAssert(facit.length > 0, @"Comparison file should contain text");
            XCTAssert([log isEqualToString:facit], @"Log file text should match comparison file text");
            [logsMatchedExpectation fulfill];
        }

        [commandScriptExpectation fulfill];
        [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
    }];

    // Observe import completion
    [self observeImportCompletionWithInitialCount:initialCount
                                      gameFileURL:gameFileURL
                                     fetchRequest:fetchRequest
                                       onComplete:^(Game *game) {
        // Verify game properties
        [self verifyGame:game hasPath:gameFileURL.path];
        [importExpectation fulfill];

        // Delete any existing autosaves
        GlkController *tempgctl = [[GlkController alloc] init];
        [tempgctl deleteAutosaveFilesForGame:game];

        // Set theme to default
        Theme *theme = [BuiltInThemes createDefaultThemeInContext:self.testContext forceRebuild:NO];
        Theme *oldtheme = game.theme;
        game.theme = theme;

        // Enable determinism setting for reproducible test results
        if (game.theme) {
            originalDeterminismSetting = game.theme.determinism;
            game.theme.determinism = YES;
            NSLog(@"Enabled determinism setting for test");
            originalSlowDrawSetting = game.theme.slowDrawing;
            game.theme.slowDrawing = NO;
            NSLog(@"Disabled slow drawing setting for test");
            originalAutosaveSetting = game.theme.autosave;
            game.theme.autosave = NO;
            NSLog(@"Disabled autosave setting for test");
        }

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        [defaults setBool:YES forKey:@"LogInterpreterOutput"];
        [defaults setObject:tempDir.path forKey:@"InterpreterLogDirectory"];

        // Now try to start the game
        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *gameWindow = [launcher playGame:game restorationHandler:nil];

            // Give the game a moment to initialize
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                XCTAssertNotNil(gameWindow, @"Game window should be created");

                // Check if the game is actually running
                GlkController *glkController = self.tableViewController.gameSessions[game.hashTag];
                XCTAssertNotNil(glkController, @"GlkController should be created");

                if (glkController) {
                    XCTAssertNotNil(glkController.window, @"Game window should exist");

                    NSLog(@"Successfully started game: %@", game.metadata.title);
                    [gameStartExpectation fulfill];

                    // Run command script if available
                    if (scriptURL) {
                        NSLog(@"Running command script: %@", scriptURL.path);

                        // Give the game a moment to be fully ready, then start the script
                        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                            [launcher runCommandsFromFile:scriptURL.path];
                        });
                    } else {
                        NSLog(@"No command script found, skipping");
                        [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
                        [logsMatchedExpectation fulfill];

                        // Clean up: close the game window
                        [glkController.window performClose:nil];
                    }
                } else {
                    [gameStartExpectation fulfill];
                    [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
                    [commandScriptExpectation fulfill];
                    [logsMatchedExpectation fulfill];
                }

                // Restore original determinism setting
                if (game.theme) {
                    game.theme.determinism = originalDeterminismSetting;
                    NSLog(@"Restored determinism setting to %@", originalDeterminismSetting ? @"YES" : @"NO");
                    game.theme.slowDrawing = originalSlowDrawSetting;
                    NSLog(@"Restored slow draw setting to %@", originalSlowDrawSetting ? @"YES" : @"NO");
                    game.theme.autosave = originalAutosaveSetting;
                    NSLog(@"Restored autosave setting to %@", originalAutosaveSetting ? @"YES" : @"NO");
                    game.theme = oldtheme;
                }
            });
        });
    }];

    // Set up options (no images/download to speed up test)
    NSDictionary *options = @{
        @"lookForImages": @(NO),
        @"downloadInfo": @(NO),
        @"context": self.testContext
    };

    // Import the game
    [importer addFiles:@[gameFileURL] options:options];

    // Wait for expectations with a longer timeout for game startup and command script
    [self waitForExpectationsWithTimeout:100.0 handler:^(NSError * _Nullable error) {
        [[NSNotificationCenter defaultCenter] removeObserver:scriptObserver];
        [self deleteGameAtPath:gameFileURL.path];
        if (error) {
            XCTFail(@"Test timed out: %@", error);
        }
    }];
}

- (void)testAdrift {
    [self importAndRunGameFile:@"Hamper.taf"
             commandScriptName:@"To Hell in a Hamper"];
}

- (void)testAdvSys {
    [self importAndRunGameFile:@"onehand.dat"
             commandScriptName:@"The Sound of One Hand Clapping"];
}

- (void)testAGT {
    [self importAndRunGameFile:@"AGT-03201-0000E16C.agx"
             commandScriptName:@"Shades of Gray"];
}


- (void)testAlan2 {
    [self importAndRunGameFile:@"bugged.acd"
             commandScriptName:@"Bugged"];
}

- (void)testAlan3 {
    [self importAndRunGameFile:@"00 Wyldkynd Project.a3c"
             commandScriptName:@"The Wyldkynd Project"];
}

- (void)testArthur {
    [self importAndRunGameFile:@"arthur.zblorb"
             commandScriptName:@"Arthur"];
}

- (void)testCzech {
    [self importAndRunGameFile:@"czech.z5"
             commandScriptName:@"Czech"];
}

- (void)testGeas {
    [self importAndRunGameFile:@"Gatheredindarkness.cas"
             commandScriptName:@"Gathered in Darkness"];
}

- (void)testHugo {
    [self importAndRunGameFile:@"guilty.hex"
             commandScriptName:@"Guilty Bastards"];
}

- (void)testJacl {
    [self importAndRunGameFile:@"grail.j2"
             commandScriptName:@"Unholy Grail"];
}

- (void)testJourney {
    [self importAndRunGameFile:@"journey.zblorb"
             commandScriptName:@"Journey"];
}

- (void)testLevel9 {
    [self importAndRunGameFile:@"Q.L9"
             commandScriptName:@"Level 9"];
}

- (void)testMagnetic {
    [self importAndRunGameFile:@"mag.mag"
             commandScriptName:@"Magnetic"];
}

- (void)testPlus {
    [self importAndRunGameFile:@"SPL13P.plus"
             commandScriptName:@"Plus"];
}

- (void)testPraxix {
    [self importAndRunGameFile:@"praxix.z5"
             commandScriptName:@"Praxix"];
}

- (void)testScottFree {
    [self importAndRunGameFile:@"adv01.dat"
             commandScriptName:@"ScottFree"];
}

- (void)testShogun {
    [self importAndRunGameFile:@"shogun.zblorb"
             commandScriptName:@"Shogun"];
}

- (void)testTADS2 {
    [self importAndRunGameFile:@"tildeath.gam"
             commandScriptName:@"Till Death Makes a Monk-Fish out of Me"];
}

- (void)testTADS3 {
    [self importAndRunGameFile:@"Elysium.t3"
             commandScriptName:@"The Elysium Enigma"];
}

- (void)testTaylorMade {
    [self importAndRunGameFile:@"tot.tay"
             commandScriptName:@"TaylorMade"];
}

- (void)testTerpEtude {
    [self importAndRunGameFile:@"etude.z5"
             commandScriptName:@"TerpEtude"];
}

- (void)testZorkZero {
    [self importAndRunGameFile:@"zork0.zblorb"
             commandScriptName:@"Zork Zero"];
}

#pragma mark - Scrollback cap (#1)

// The cut-point helper must always land on a line boundary, respect the
// safe limit that protects the active input region, and decline to trim when
// there is nothing safe to remove.
- (void)testScrollbackCutPoint {
    // Build 10 lines of "line\n" (5 chars each => 50 chars, '\n' at 4,9,...,49).
    NSMutableString *text = [NSMutableString string];
    for (int i = 0; i < 10; i++)
        [text appendString:@"line\n"];
    XCTAssertEqual(text.length, 50u);

    // Under the keep target: nothing to trim.
    XCTAssertEqual([GlkTextBufferWindow scrollbackCutPointForString:text
                                                        targetKeep:1000
                                                         safeLimit:text.length],
                   0u);

    // Keep ~10 chars => want = 40, which already sits just after a newline,
    // so the cut is exactly 40 and the kept text starts at a line boundary.
    NSUInteger cut = [GlkTextBufferWindow scrollbackCutPointForString:text
                                                          targetKeep:10
                                                           safeLimit:text.length];
    XCTAssertEqual(cut, 40u);
    XCTAssertEqual([text characterAtIndex:cut - 1], (unichar)'\n');

    // want lands mid-line => scan forward to the next newline boundary.
    // targetKeep 8 => want = 42; next '\n' is at index 44, so cut = 45.
    NSUInteger cut2 = [GlkTextBufferWindow scrollbackCutPointForString:text
                                                           targetKeep:8
                                                            safeLimit:text.length];
    XCTAssertEqual(cut2, 45u);
    XCTAssertEqual([text characterAtIndex:cut2 - 1], (unichar)'\n');

    // A tight safe limit (e.g. the input fence) leaves no boundary to cut at:
    // decline rather than trim into protected text.
    XCTAssertEqual([GlkTextBufferWindow scrollbackCutPointForString:text
                                                        targetKeep:10
                                                         safeLimit:3],
                   0u);

    // No newline anywhere => no clean boundary => no trim.
    XCTAssertEqual([GlkTextBufferWindow scrollbackCutPointForString:@"a single very long line with no breaks"
                                                        targetKeep:5
                                                         safeLimit:38],
                   0u);
}

// After a front trim of `cut` characters, margin images and flow breaks whose
// anchor was removed must be dropped, and the survivors' positions shifted
// down by exactly `cut` so they stay attached to the same text.
- (void)testMarginContainerAnchorShift {
    MarginContainer *container =
        [[MarginContainer alloc] initWithContainerSize:NSMakeSize(100, 100)];
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(10, 10)];

    [container addImage:image alignment:imagealign_MarginLeft  at:10   linkid:0];
    [container addImage:image alignment:imagealign_MarginLeft  at:100  linkid:0];
    [container addImage:image alignment:imagealign_MarginRight at:1000 linkid:0];
    // Flow breaks exercise the parallel shift path (no assertion: just no crash).
    [container flowBreakAt:20];
    [container flowBreakAt:500];

    XCTAssertEqual(container.marginImages.count, 3u);

    [container shiftAnchorsAfterTrimOf:50];

    // The image anchored at pos 10 was inside the trimmed range and is gone.
    XCTAssertEqual(container.marginImages.count, 2u);
    XCTAssertEqual(container.marginImages[0].pos, 50u);   // 100 - 50
    XCTAssertEqual(container.marginImages[1].pos, 950u);  // 1000 - 50

    // A zero cut is a no-op.
    [container shiftAnchorsAfterTrimOf:0];
    XCTAssertEqual(container.marginImages.count, 2u);
    XCTAssertEqual(container.marginImages[0].pos, 50u);
}

// Resizing a buffer window whose scrollback holds many margin images must
// remain responsive. Without the unoverlap: pos-distance early-exit, every
// laid-out line did an O(M) overlap scan against every margin image, making
// a single resize pass O(L * M^2). This test exercises that path without
// loading a game: stand up a minimal NSTextStorage / NSLayoutManager /
// MarginContainer / NSTextView pipeline, populate it with the shape of a
// long Zork Zero session (~500 short paragraphs, ~100 margin images), and
// measure a sequence of textview-frame resizes - the same code path a live
// window drag triggers.
- (void)testMarginContainerResizePerformance {
    NSTextStorage *storage = [[NSTextStorage alloc] init];
    NSLayoutManager *lm = [[NSLayoutManager alloc] init];
    [storage addLayoutManager:lm];

    MarginContainer *container =
        [[MarginContainer alloc] initWithContainerSize:NSMakeSize(600, CGFLOAT_MAX)];
    [lm addTextContainer:container];

    NSTextView *textview =
        [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 600, 10000)
                            textContainer:container];

    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(80, 80)];

    // 500 short paragraphs with a margin image every fifth one represents a
    // realistic long Zork Zero session (~18KB of text, ~100 inline margin
    // images held by the MarginContainer at once). Enough to demonstrate the
    // O(M^2) layout cost without dominating the rest of the test suite.
    NSUInteger imageCount = 0;
    for (int i = 0; i < 500; i++) {
        NSUInteger pos = storage.length;
        NSAttributedString *para = [[NSAttributedString alloc]
            initWithString:@"A short room description paragraph.\n"];
        [storage appendAttributedString:para];
        if ((i % 5) == 0) {
            [container addImage:image
                      alignment:(i & 1) ? imagealign_MarginLeft
                                        : imagealign_MarginRight
                             at:pos
                         linkid:0];
            imageCount++;
        }
    }
    XCTAssertGreaterThan(imageCount, 50u);

    // Establish a baseline layout and warm caches with one full resize cycle
    // before measuring, so a cold first iteration doesn't skew the average.
    [lm ensureLayoutForTextContainer:container];
    [textview setFrameSize:NSMakeSize(500, 10000)];
    [container invalidateLayout:nil];
    [lm ensureLayoutForTextContainer:container];
    [textview setFrameSize:NSMakeSize(700, 10000)];
    [container invalidateLayout:nil];
    [lm ensureLayoutForTextContainer:container];

    // Each iteration runs two full re-layouts at different widths. Per-pass
    // cost is O(L * M) just for lineFragmentRectForProposedRect:, so at this
    // scrollback size a single iteration already takes several seconds; cap
    // measure-block iterations to keep total test time bounded.
    XCTMeasureOptions *opts = [XCTMeasureOptions defaultOptions];
    opts.iterationCount = 3;
    [self measureWithMetrics:@[[[XCTClockMetric alloc] init]]
                     options:opts
                       block:^{
        [textview setFrameSize:NSMakeSize(500, 10000)];
        [container invalidateLayout:nil];
        [lm ensureLayoutForTextContainer:container];

        [textview setFrameSize:NSMakeSize(700, 10000)];
        [container invalidateLayout:nil];
        [lm ensureLayoutForTextContainer:container];
    }];

    XCTAssertEqual(container.marginImages.count, imageCount,
                   @"resize must not lose margin images");
}

// The scrollback limit is a per-theme value (the Theme.scrollbackLimit Core
// Data attribute), clamped via +clampScrollbackLimit:. A positive value is
// honoured, tiny values are clamped up to the floor, and 0 or negative means
// unlimited.
- (void)testScrollbackLimitSetting {
    // A positive value is honoured.
    XCTAssertEqual([GlkTextBufferWindow clampScrollbackLimit:50000], 50000u);

    // The built-in default value passes through unchanged.
    XCTAssertEqual([GlkTextBufferWindow clampScrollbackLimit:30000], 30000u);

    // Below the floor => clamped up.
    XCTAssertEqual([GlkTextBufferWindow clampScrollbackLimit:100], 2000u);

    // Explicit 0 => unlimited.
    XCTAssertEqual([GlkTextBufferWindow clampScrollbackLimit:0], 0u);

    // Negative is also treated as unlimited.
    XCTAssertEqual([GlkTextBufferWindow clampScrollbackLimit:-5], 0u);
}

// The Preferences nib must instantiate with the new scrollback controls wired
// up. A mismatched outlet name would raise NSUnknownKeyException here, and a
// missing action would leave the controller unable to respond - this guards
// the hand-edited PrefsWindow.xib against both.
- (void)testPreferencesNibConnectsScrollbackControls {
    Preferences *owner = [[Preferences alloc] init];
    NSNib *nib = [[NSNib alloc] initWithNibNamed:@"PrefsWindow"
                                          bundle:[NSBundle bundleForClass:[Preferences class]]];
    XCTAssertNotNil(nib, @"PrefsWindow nib should load");

    NSArray *topLevelObjects = nil;
    BOOL instantiated = NO;
    @try {
        instantiated = [nib instantiateWithOwner:owner topLevelObjects:&topLevelObjects];
    } @catch (NSException *exception) {
        XCTFail(@"Instantiating PrefsWindow raised: %@", exception);
    }
    XCTAssertTrue(instantiated, @"PrefsWindow should instantiate");

    // Connected outlets (would have thrown above on a name mismatch).
    XCTAssertNotNil(owner.scrollbackLimitTextField,
                    @"scrollbackLimitTextField outlet should be connected");
    XCTAssertNotNil(owner.scrollbackLimitStepper,
                    @"scrollbackLimitStepper outlet should be connected");
    XCTAssertNotNil(owner.scrollbackUnitLabel,
                    @"scrollbackUnitLabel outlet should be connected");

    // The action target must respond to the selector wired in the xib.
    XCTAssertTrue([owner respondsToSelector:@selector(changeScrollbackLimit:)],
                  @"changeScrollbackLimit: action should exist on Preferences");
}

- (void)testSecretDiaryScrolling {
    [self performMoleScrollingTestWithFileName:@"The Secret Diary Of Adrian Mole - Side 1.tzx" smoothScroll:YES];
}

- (void)testArchersScrolling {
    [self performMoleScrollingTestWithFileName:@"The Archers - Side 1.tzx" smoothScroll:YES];
}

- (void)testSecretDiaryScrollingNoSmooth {
    [self performMoleScrollingTestWithFileName:@"The Secret Diary Of Adrian Mole - Side 1.tzx" smoothScroll:NO];
}

- (void)testArchersScrollingNoSmooth {
    [self performMoleScrollingTestWithFileName:@"The Archers - Side 1.tzx" smoothScroll:NO];
}

// Shared scrolling test body for the Adrian Mole / Archers Level 9 games.
// These games rapidly request and cancel char events on a timer, and the GUI
// must distinguish that from real user keypresses so it only scrolls by one
// screenful when the user actually presses space.
- (void)performMoleScrollingTestWithFileName:(NSString *)fileName smoothScroll:(BOOL)smoothScroll {
    XCTestExpectation *importExpectation = [self expectationWithDescription:@"Game import completes"];
    XCTestExpectation *scrollTestExpectation = [self expectationWithDescription:@"Scroll test completes"];

    NSURL *gameFileURL = [self gameFileURLForFileNamed:fileName];

    [self deleteGameAtPath:gameFileURL.path];

    NSUInteger initialCount = [self currentGameCount];
    NSFetchRequest *fetchRequest = [Game fetchRequest];

    GameImporter *importer = [self createGameImporter];
    GameLauncher *launcher = [self createAndSetupGameLauncher];

    __block BOOL originalDeterminismSetting = NO;
    __block BOOL originalSlowDrawSetting = NO;
    __block BOOL originalAutosaveSetting = NO;
    __block BOOL originalSmoothScrollSetting = NO;
    __block Theme *oldTheme = nil;

    [self observeImportCompletionWithInitialCount:initialCount
                                      gameFileURL:gameFileURL
                                     fetchRequest:fetchRequest
                                       onComplete:^(Game *game) {
        [self verifyGame:game hasPath:gameFileURL.path];
        [importExpectation fulfill];

        GlkController *tempgctl = [[GlkController alloc] init];
        [tempgctl deleteAutosaveFilesForGame:game];

        Theme *theme = [BuiltInThemes createDefaultThemeInContext:self.testContext forceRebuild:NO];
        oldTheme = game.theme;
        game.theme = theme;

        originalDeterminismSetting = game.theme.determinism;
        game.theme.determinism = YES;
        originalSlowDrawSetting = game.theme.slowDrawing;
        game.theme.slowDrawing = NO;
        originalAutosaveSetting = game.theme.autosave;
        game.theme.autosave = NO;
        originalSmoothScrollSetting = game.theme.smoothScroll;
        game.theme.smoothScroll = smoothScroll;

        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *gameWindow = [launcher playGame:game restorationHandler:nil];

            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                XCTAssertNotNil(gameWindow, @"Game window should be created");

                GlkController *glkController = self.tableViewController.gameSessions[game.hashTag];
                XCTAssertNotNil(glkController, @"GlkController should be created");

                if (!glkController) {
                    [scrollTestExpectation fulfill];
                    return;
                }

                XCTAssertNotNil(glkController.window, @"Game window should exist");

                // Resize window to half the default content size
                NSSize defaultSize = gameWindow.contentView.frame.size;
                NSSize halfSize = NSMakeSize(defaultSize.width / 2.0, defaultSize.height / 2.0);
                NSRect contentRect = NSMakeRect(0, 0, halfSize.width, halfSize.height);
                NSRect newWindowFrame = [gameWindow frameRectForContentRect:contentRect];
                newWindowFrame.origin = gameWindow.frame.origin;
                [gameWindow setFrame:newWindowFrame display:YES];

                // Wait for the resize to take effect and the game to settle
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{

                    // Find the text buffer window
                    GlkTextBufferWindow *bufferWin = nil;
                    for (GlkWindow *win in glkController.gwindows.allValues) {
                        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                            bufferWin = (GlkTextBufferWindow *)win;
                            break;
                        }
                    }

                    XCTAssertNotNil(bufferWin, @"Should have a text buffer window");

                    if (!bufferWin) {
                        game.theme.determinism = originalDeterminismSetting;
                        game.theme.slowDrawing = originalSlowDrawSetting;
                        game.theme.autosave = originalAutosaveSetting;
                        game.theme = oldTheme;
                        [glkController.window performClose:nil];
                        [scrollTestExpectation fulfill];
                        return;
                    }

                    NSScrollView *scrollView = bufferWin.textview.enclosingScrollView;
                    XCTAssertNotNil(scrollView, @"Buffer window should have a scroll view");

                    CGFloat viewportHeight = scrollView.contentView.bounds.size.height;
                    NSLog(@"Viewport height: %f", viewportHeight);

                    CGFloat scrollPositionBefore = scrollView.contentView.bounds.origin.y;
                    NSLog(@"Scroll position before wait: %f", scrollPositionBefore);

                    // Wait 3 seconds and check that scroll position has not changed
                    // (The Adrian Mole games cycle char events rapidly, which should
                    // not cause scrolling)
                    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{

                        CGFloat scrollPositionAfterWait = scrollView.contentView.bounds.origin.y;
                        NSLog(@"Scroll position after wait: %f", scrollPositionAfterWait);

                        XCTAssertEqual(scrollPositionBefore, scrollPositionAfterWait,
                                       @"Scroll position should not change during idle wait (was %f, now %f)",
                                       scrollPositionBefore, scrollPositionAfterWait);

                        // Drive the game by repeatedly pressing '1' (picks the first
                        // multiple-choice option, also advances past "more"/"press
                        // any key" prompts in these Level 9 games). After each
                        // press, verify the scroll position did not advance past
                        // more than one viewport — that's the real "don't skip
                        // unread text" invariant.
                        const NSInteger kMaxPresses = 20;
                        const NSInteger kNoProgressLimit = 3;
                        const CGFloat scrollTolerance = 20.0;

                        __block NSInteger pressCount = 0;
                        __block NSInteger noProgressCount = 0;

                        void (^finishUp)(void) = ^{
                            game.theme.determinism = originalDeterminismSetting;
                            game.theme.slowDrawing = originalSlowDrawSetting;
                            game.theme.autosave = originalAutosaveSetting;
                            game.theme.smoothScroll = originalSmoothScrollSetting;
                            game.theme = oldTheme;

                            [glkController.window performClose:nil];
                            [scrollTestExpectation fulfill];
                        };

                        // Note: pressOneAndCheck self-references via __block, which
                        // forms a retain cycle. Don't break it by nil-ing inside the
                        // block — that releases the block we're currently executing
                        // and crashes on the next captured-variable access. The
                        // cycle is short-lived (one per test invocation) and
                        // released along with the dispatch_after task chain.
                        __block void (^pressOneAndCheck)(void) = nil;
                        pressOneAndCheck = ^{
                            if (pressCount >= kMaxPresses) {
                                NSLog(@"Reached press cap of %ld; ending playthrough.", (long)kMaxPresses);
                                finishUp();
                                return;
                            }

                            CGFloat scrollBefore = scrollView.contentView.bounds.origin.y;
                            CGFloat docBefore = scrollView.documentView.frame.size.height;

                            [bufferWin sendKeypress:'1'];
                            pressCount++;

                            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                                CGFloat scrollAfter = scrollView.contentView.bounds.origin.y;
                                CGFloat docAfter = scrollView.documentView.frame.size.height;
                                CGFloat scrollDelta = scrollAfter - scrollBefore;
                                CGFloat docDelta = docAfter - docBefore;

                                NSLog(@"Press %ld: scroll %f -> %f (delta %f), doc %f -> %f (delta %f)",
                                      (long)pressCount, scrollBefore, scrollAfter, scrollDelta,
                                      docBefore, docAfter, docDelta);

                                XCTAssertLessThanOrEqual(scrollDelta, viewportHeight + scrollTolerance,
                                                         @"Press %ld: scroll delta %f exceeds one viewport (%f)",
                                                         (long)pressCount, scrollDelta, viewportHeight);

                                if (scrollDelta == 0 && docDelta == 0) {
                                    noProgressCount++;
                                    if (noProgressCount >= kNoProgressLimit) {
                                        NSLog(@"%ld consecutive presses with no progress — assuming end of part 1.",
                                              (long)kNoProgressLimit);
                                        finishUp();
                                        return;
                                    }
                                } else {
                                    noProgressCount = 0;
                                }

                                pressOneAndCheck();
                            });
                        };

                        pressOneAndCheck();
                    });
                });
            });
        });
    }];

    NSDictionary *options = @{
        @"lookForImages": @(NO),
        @"downloadInfo": @(NO),
        @"context": self.testContext
    };

    [importer addFiles:@[gameFileURL] options:options];

    [self waitForExpectationsWithTimeout:90.0 handler:^(NSError * _Nullable error) {
        [self deleteGameAtPath:gameFileURL.path];
        if (error) {
            XCTFail(@"Test timed out: %@", error);
        }
    }];
}

#pragma mark - Progressive Core Data migration (real model-13 store)

// Copy the file at url (plus its -wal/-shm sidecars, if present) to dstURL.
// A purely read-only snapshot of the source - we never open or modify it.
- (BOOL)copyStoreFilesFrom:(NSURL *)url to:(NSURL *)dstURL {
    NSFileManager *fm = NSFileManager.defaultManager;
    for (NSString *suffix in @[@"", @"-wal", @"-shm"]) {
        NSURL *src = [NSURL fileURLWithPath:[url.path stringByAppendingString:suffix]];
        if (![fm fileExistsAtPath:src.path])
            continue;
        NSURL *dst = [NSURL fileURLWithPath:[dstURL.path stringByAppendingString:suffix]];
        [fm removeItemAtURL:dst error:NULL];
        if (![fm copyItemAtURL:src toURL:dst error:NULL])
            return NO;
    }
    return YES;
}

// End-to-end check that progressive migration chains a real, pre-hash store all
// the way to the current model AND actually runs the custom
// IfidToHashMigrationPolicy (i.e. passes through "Spatterlight 15"), rather than
// taking a single inferred hop. Uses the on-disk model-13 group-container store
// as a read-only fixture; skips cleanly if it is not present.
//
// Discriminator: the policy nils every Game.hashTag (and recomputes it from the
// file only for "found" games). We set found=NO first, so after a policy-driven
// migration every hashTag is empty. Plain inference would instead have copied
// the old non-empty hashTags straight across. Zero non-empty hashTags therefore
// proves the policy ran.
- (void)testProgressiveMigrationFromRealV13Store {
    NSFileManager *fm = NSFileManager.defaultManager;

    // The app momd (in the test host bundle) holds every model version.
    NSURL *momd = [[NSBundle mainBundle] URLForResource:@"Spatterlight" withExtension:@"momd"];
    XCTAssertNotNil(momd, @"Spatterlight.momd must be in the test host bundle");
    NSManagedObjectModel *v13 = [[NSManagedObjectModel alloc]
                                 initWithContentsOfURL:[momd URLByAppendingPathComponent:@"Spatterlight 13.mom"]];
    XCTAssertNotNil(v13, @"model 13 must exist in the momd");

    // Locate the real model-13 fixture store.
    NSURL *realStore = nil;
    NSMutableArray<NSURL *> *candidates = [NSMutableArray array];
    for (NSString *group in @[@"group.net.ccxvii.spatterlight", @"6U7YY3724Y.group.net.ccxvii.spatterlight"]) {
        NSURL *c = [fm containerURLForSecurityApplicationGroupIdentifier:group];
        if (c)
            [candidates addObject:[c URLByAppendingPathComponent:@"Spatterlight.storedata"]];
    }
    [candidates addObject:[NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:
                           @"Library/Group Containers/group.net.ccxvii.spatterlight/Spatterlight.storedata"]]];
    for (NSURL *s in candidates) {
        if (![fm fileExistsAtPath:s.path])
            continue;
        NSDictionary *m = [NSPersistentStoreCoordinator metadataForPersistentStoreOfType:NSSQLiteStoreType URL:s options:nil error:NULL];
        if (m && [v13 isConfiguration:nil compatibleWithStoreMetadata:m]) { realStore = s; break; }
    }
    if (!realStore) {
        NSLog(@"Skipping testProgressiveMigrationFromRealV13Store: no model-13 fixture store found.");
        return;
    }

    // Snapshot it to a temp location; the original is never opened/modified.
    NSURL *tmpStore = [[NSURL fileURLWithPath:NSTemporaryDirectory()]
                       URLByAppendingPathComponent:[NSString stringWithFormat:@"v13fixture-%@.storedata",
                                                    NSProcessInfo.processInfo.globallyUniqueString]];
    XCTAssertTrue([self copyStoreFilesFrom:realStore to:tmpStore], @"failed to snapshot fixture store");

    NSManagedObjectModel *finalModel = [[CoreDataManager alloc] initWithModelName:@"Spatterlight"].managedObjectModel;
    XCTAssertNotNil(finalModel);

    // Open the snapshot at v13: set found=NO everywhere (so the policy does no
    // file I/O) and record the discriminator baseline.
    NSUInteger preGames = 0, preMeta = 0, preNonEmptyHash = 0;
    @autoreleasepool {
        NSPersistentStoreCoordinator *c13 = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:v13];
        NSError *e = nil;
        XCTAssertNotNil([c13 addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:tmpStore options:nil error:&e], @"open v13 snapshot: %@", e);
        NSManagedObjectContext *ctx = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
        ctx.persistentStoreCoordinator = c13;
        NSArray<NSManagedObject *> *games = [ctx executeFetchRequest:[NSFetchRequest fetchRequestWithEntityName:@"Game"] error:&e];
        for (NSManagedObject *g in games) {
            if (((NSString *)[g valueForKey:@"hashTag"]).length)
                preNonEmptyHash++;
            [g setValue:@NO forKey:@"found"];
        }
        preGames = games.count;
        preMeta = [ctx countForFetchRequest:[NSFetchRequest fetchRequestWithEntityName:@"Metadata"] error:&e];
        XCTAssertTrue([ctx save:&e], @"save found=NO: %@", e);
        for (NSPersistentStore *s in c13.persistentStores.copy)
            [c13 removePersistentStore:s error:NULL];
    }
    XCTAssertGreaterThan(preGames, 0u, @"fixture should contain games");
    // The discriminator below relies on the policy reconciling Metadata to one
    // row per Game, so the fixture must start out NOT already 1:1.
    XCTAssertNotEqual(preMeta, preGames, @"fixture should have a different Metadata and Game count");
    // ...and on the policy nilling hashTags, so the fixture must have some.
    XCTAssertGreaterThan(preNonEmptyHash, 0u, @"fixture needs some non-empty hashTags");

    // The snapshot must NOT already be at the current model.
    NSDictionary *preMetaDict = [NSPersistentStoreCoordinator metadataForPersistentStoreOfType:NSSQLiteStoreType URL:tmpStore options:nil error:NULL];
    XCTAssertFalse([finalModel isConfiguration:nil compatibleWithStoreMetadata:preMetaDict]);

    // Run the real progressive migration.
    CoreDataManager *cdm = [[CoreDataManager alloc] initWithModelName:@"Spatterlight"];
    NSError *migErr = nil;
    BOOL ok = [cdm progressivelyMigrateStoreAtURL:tmpStore ofType:NSSQLiteStoreType toModel:finalModel error:&migErr];
    XCTAssertTrue(ok, @"progressive migration failed: %@", migErr);

    // It must now be at the current model.
    NSDictionary *postMetaDict = [NSPersistentStoreCoordinator metadataForPersistentStoreOfType:NSSQLiteStoreType URL:tmpStore options:nil error:NULL];
    XCTAssertTrue([finalModel isConfiguration:nil compatibleWithStoreMetadata:postMetaDict], @"store is not at the current model after migration");

    // Verify data survived and the policy ran.
    NSUInteger postGames = 0, postMeta = 0, postNonEmptyHash = 0;
    @autoreleasepool {
        NSPersistentStoreCoordinator *c16 = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:finalModel];
        NSError *e = nil;
        XCTAssertNotNil([c16 addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:tmpStore options:nil error:&e], @"open migrated store: %@", e);
        NSManagedObjectContext *ctx = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
        ctx.persistentStoreCoordinator = c16;
        NSArray<NSManagedObject *> *games = [ctx executeFetchRequest:[NSFetchRequest fetchRequestWithEntityName:@"Game"] error:&e];
        for (NSManagedObject *g in games)
            if (((NSString *)[g valueForKey:@"hashTag"]).length)
                postNonEmptyHash++;
        postGames = games.count;
        postMeta = [ctx countForFetchRequest:[NSFetchRequest fetchRequestWithEntityName:@"Metadata"] error:&e];
        for (NSPersistentStore *s in c16.persistentStores.copy)
            [c16 removePersistentStore:s error:NULL];
    }

    NSLog(@"Progressive migration: games %lu->%lu, metadata %lu->%lu, non-empty hashTags %lu->%lu",
          (unsigned long)preGames, (unsigned long)postGames, (unsigned long)preMeta, (unsigned long)postMeta,
          (unsigned long)preNonEmptyHash, (unsigned long)postNonEmptyHash);

    XCTAssertEqual(postGames, preGames, @"Game rows must be preserved across migration");
    XCTAssertGreaterThan(postMeta, 0u, @"Metadata must survive migration");
    // Discriminator that the custom IfidToHashMigrationPolicy actually ran (i.e.
    // the chain passed through model 15) rather than a single inferred hop: the
    // policy rebuilds Game<->Metadata relationships and reconciles Metadata to
    // one row per Game. A plain inferred migration is structurally incapable of
    // deleting rows, so it would have preserved the original Metadata count.
    // Reaching one-Metadata-per-Game from a non-1:1 source therefore proves the
    // policy ran.
    XCTAssertNotEqual(postMeta, preMeta,
                      @"Metadata count must change - inference cannot delete rows, so an unchanged count would mean the policy did not run");
    XCTAssertEqual(postMeta, postGames,
                   @"policy should reconcile Metadata to exactly one row per Game (got %lu Metadata for %lu Games)",
                   (unsigned long)postMeta, (unsigned long)postGames);
    // With found=NO set above, the (now-fixed) policy nils every Game.hashTag
    // and does not recompute it from a file. Plain inference would have copied
    // the old non-empty values across, so zero proves the hashTag-deletion path
    // (IfidToHashMigrationPolicy.m line 77) actually runs now.
    XCTAssertEqual(postNonEmptyHash, 0u,
                   @"the policy should have nilled every hashTag (got %lu non-empty of %lu before the migration)",
                   (unsigned long)postNonEmptyHash, (unsigned long)preNonEmptyHash);

    // Clean up the temp snapshot.
    [[[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:finalModel]
     destroyPersistentStoreAtURL:tmpStore withType:NSSQLiteStoreType options:nil error:NULL];
}

#pragma mark - Blorb parser hardening (corrupt / truncated files)

// Build a minimal but structurally valid Blorb: FORM/IFRS wrapper + an RIdx
// index containing a single Picture resource whose `start` offset points exactly
// at the end of the data. Reading the 4-byte chunk type at that offset used to
// walk off the end of the buffer and raise NSRangeException in
// -[Blorb stringFromChunkWithPtr:] (Blorb.m:81 -> subdataWithRange:).
- (NSData *)truncatedBlorbWithResourceStartAtEOF {
    NSMutableData *data = [NSMutableData data];

    // FORM header (12 bytes): 'FORM' + length + 'IFRS'.
    blorbAppendFourCC(data, "FORM");
    blorbAppendBE32(data, 0);            // form length is not consulted by the parser
    blorbAppendFourCC(data, "IFRS");

    // RIdx chunk header (8 bytes) + count (4) + one 12-byte entry.
    blorbAppendFourCC(data, "RIdx");
    blorbAppendBE32(data, 4 + 12);       // chunk length: count field + one entry
    blorbAppendBE32(data, 1);            // resource count

    // Single index entry: usage 'Pict', number 1, start == total length (EOF).
    blorbAppendFourCC(data, "Pict");
    blorbAppendBE32(data, 1);
    blorbAppendBE32(data, 36);           // start offset == final data length

    XCTAssertEqual(data.length, 36u, @"fixture layout changed; update the start offset");
    return data;
}

// The reported crash: an RIdx entry whose start offset sits at the end of the
// file. Parsing must succeed (degrading gracefully) instead of throwing.
- (void)testTruncatedBlorbResourceStartAtEOFDoesNotCrash {
    NSData *data = [self truncatedBlorbWithResourceStartAtEOF];

    Blorb *blorb = [[Blorb alloc] initWithData:data];
    XCTAssertNotNil(blorb, @"parsing a truncated Blorb should not return nil for a valid-length buffer");
    XCTAssertEqual(blorb.resources.count, 1u, @"the single index entry should still be recorded");

    BlorbResource *res = blorb.resources.firstObject;
    XCTAssertNil(res.chunkType, @"an out-of-range chunk type must be nil, not a crash");

    // The readers that dereference `start` must also degrade to nil, not crash.
    XCTAssertNil([blorb dataForResource:res], @"reading past EOF should return nil");
    XCTAssertNil(blorb.coverImageData, @"cover extraction must tolerate a missing chunk");
}

// An RIdx that claims far more entries than the file actually contains must not
// over-read past the buffer while walking the index.
- (void)testBlorbWithOverlongResourceCountDoesNotOverread {
    NSMutableData *data = [NSMutableData data];
    blorbAppendFourCC(data, "FORM");
    blorbAppendBE32(data, 0);
    blorbAppendFourCC(data, "IFRS");
    blorbAppendFourCC(data, "RIdx");
    blorbAppendBE32(data, 4 + 12);
    blorbAppendBE32(data, 1000);         // lies: claims 1000 entries...
    blorbAppendFourCC(data, "Pict");     // ...but only one 12-byte entry follows
    blorbAppendBE32(data, 1);
    blorbAppendBE32(data, 36);

    Blorb *blorb = [[Blorb alloc] initWithData:data];
    XCTAssertNotNil(blorb);
    XCTAssertEqual(blorb.resources.count, 1u,
                   @"only the entries that actually fit in the buffer should be read");
}

// A metadata (IFmd) chunk whose declared length runs past the end of the file
// must be skipped rather than read out of bounds.
- (void)testBlorbWithOverlongMetadataLengthDoesNotCrash {
    NSMutableData *data = [NSMutableData data];
    blorbAppendFourCC(data, "FORM");
    blorbAppendBE32(data, 0);
    blorbAppendFourCC(data, "IFRS");
    // Empty RIdx (zero resources) so the parser moves on to the trailing chunks.
    blorbAppendFourCC(data, "RIdx");
    blorbAppendBE32(data, 4);
    blorbAppendBE32(data, 0);
    // IFmd chunk header claiming 10000 bytes of payload that isn't there.
    blorbAppendFourCC(data, "IFmd");
    blorbAppendBE32(data, 10000);

    Blorb *blorb = [[Blorb alloc] initWithData:data];
    XCTAssertNotNil(blorb);
    XCTAssertNil(blorb.metaData, @"an out-of-range IFmd chunk must not be extracted");
}

// isBlorbData: reads the first 12 bytes of the FORM header, so it must reject
// buffers shorter than that instead of reading past the end.
- (void)testIsBlorbDataRejectsShortBuffers {
    XCTAssertFalse([Blorb isBlorbData:[NSData data]]);
    XCTAssertFalse([Blorb isBlorbData:[@"FO" dataUsingEncoding:NSASCIIStringEncoding]]);
    XCTAssertFalse([Blorb isBlorbData:[@"FORM????????" dataUsingEncoding:NSASCIIStringEncoding]],
                   @"a 12-byte non-IFRS FORM is not a Blorb");
}

@end
