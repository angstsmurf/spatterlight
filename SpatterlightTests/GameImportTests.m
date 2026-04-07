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
    [self waitForExpectationsWithTimeout:60.0 handler:^(NSError * _Nullable error) {
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

- (void)testCzech {
    [self importAndRunGameFile:@"czech.z5"
             commandScriptName:@"Czech"];
}

- (void)testHugo {
    [self importAndRunGameFile:@"guilty.hex"
             commandScriptName:@"Guilty Bastards"];
}

- (void)testJacl {
    [self importAndRunGameFile:@"grail.j2"
             commandScriptName:@"Unholy Grail"];
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

@end
