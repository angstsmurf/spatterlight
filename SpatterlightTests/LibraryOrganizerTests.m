//
//  LibraryOrganizerTests.m
//  SpatterlightTests
//
//  Regression tests for issue #167: organising a library rooted at a folder
//  that already contains the user's games must never move or delete anything
//  it does not own. The organiser may only empty and remove a source folder
//  that carries the game's own identity marker.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "LibraryOrganizer.h"
#import "Game.h"
#import "Metadata.h"

// The marker filename used by LibraryOrganizer (kept in sync by the tests
// that assert on it below).
static NSString *const kMarkerFilename = @".spatterlightIdentity";

// Reach into the organiser's private custom-URL cache so the tests can point
// it at a temporary root without going through security-scoped bookmarks.
@interface LibraryOrganizer ()
@property (nonatomic) NSURL *accessedCustomURL;
@end

@interface LibraryOrganizerTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) LibraryOrganizer *organizer;
@property (nonatomic, strong) NSURL *root;
@property (nonatomic, strong) NSData *savedBookmarkDefault;

@end

@implementation LibraryOrganizerTests

- (void)setUp {
    [super setUp];

    // In-memory Core Data stack for Game/Metadata objects.
    NSURL *modelURL = [[NSBundle bundleForClass:[Game class]] URLForResource:@"Spatterlight"
                                                               withExtension:@"momd"];
    NSManagedObjectModel *model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    XCTAssertNotNil(model, @"Could not load Core Data model");
    NSPersistentStoreCoordinator *coordinator =
    [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];
    NSError *error = nil;
    [coordinator addPersistentStoreWithType:NSInMemoryStoreType
                              configuration:nil
                                        URL:nil
                                    options:nil
                                      error:&error];
    XCTAssertNil(error);
    self.context = [[NSManagedObjectContext alloc]
                    initWithConcurrencyType:NSMainQueueConcurrencyType];
    self.context.persistentStoreCoordinator = coordinator;

    // A fresh temporary library root for every test.
    NSString *dirName = [@"LibraryOrganizerTests-" stringByAppendingString:NSUUID.UUID.UUIDString];
    self.root = [NSURL fileURLWithPath:
                 [NSTemporaryDirectory() stringByAppendingPathComponent:dirName]
                           isDirectory:YES];
    [NSFileManager.defaultManager createDirectoryAtURL:self.root
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];

    // Point a private organiser instance at the temp root. customLibraryURL
    // requires some bookmark data in defaults before it consults the cached
    // URL, so park a placeholder there (restored in tearDown).
    self.savedBookmarkDefault =
    [NSUserDefaults.standardUserDefaults dataForKey:kOrganiseDirBookmarkKey];
    [NSUserDefaults.standardUserDefaults setObject:[NSData dataWithBytes:"x" length:1]
                                            forKey:kOrganiseDirBookmarkKey];
    self.organizer = [[LibraryOrganizer alloc] init];
    self.organizer.accessedCustomURL = self.root;
}

- (void)tearDown {
    if (self.savedBookmarkDefault)
        [NSUserDefaults.standardUserDefaults setObject:self.savedBookmarkDefault
                                                forKey:kOrganiseDirBookmarkKey];
    else
        [NSUserDefaults.standardUserDefaults removeObjectForKey:kOrganiseDirBookmarkKey];

    [NSFileManager.defaultManager removeItemAtURL:self.root error:NULL];
    self.organizer = nil;
    self.context = nil;
    [super tearDown];
}

#pragma mark - Helpers

- (NSURL *)writeFile:(NSString *)relativePath contents:(NSString *)contents {
    NSURL *url = [self.root URLByAppendingPathComponent:relativePath];
    [NSFileManager.defaultManager createDirectoryAtURL:url.URLByDeletingLastPathComponent
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    XCTAssertTrue([contents writeToURL:url
                            atomically:YES
                              encoding:NSUTF8StringEncoding
                                 error:NULL],
                  @"Could not write fixture %@", relativePath);
    return url;
}

- (Game *)makeGameWithTitle:(NSString *)title
                       ifid:(NSString *)ifid
                       path:(NSString *)path {
    Metadata *metadata = [NSEntityDescription insertNewObjectForEntityForName:@"Metadata"
                                                       inManagedObjectContext:self.context];
    metadata.title = title;
    Game *game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                               inManagedObjectContext:self.context];
    game.metadata = metadata;
    game.ifid = ifid;
    game.detectedFormat = @"zcode";
    game.path = path;
    return game;
}

- (BOOL)fileExists:(NSString *)relativePath {
    return [NSFileManager.defaultManager fileExistsAtPath:
            [self.root URLByAppendingPathComponent:relativePath].path];
}

#pragma mark - Tests

// Issue #167: the user points the library at the folder their games already
// live in. Organising must copy the game into <root>/<group>/<title>/ and
// leave everything else - the folder itself, the original file, unrelated
// files and subfolders - completely untouched.
- (void)testOrganisingLooseGameInRootDeletesNothing {
    NSURL *gameURL = [self writeFile:@"curses.z5" contents:@"fake z-code"];
    [self writeFile:@"unrelated.txt" contents:@"precious notes"];
    [self writeFile:@"Other stuff/keep.txt" contents:@"also precious"];

    Game *game = [self makeGameWithTitle:@"Curses"
                                    ifid:@"TEST-IFID-167"
                                    path:gameURL.path];

    NSError *error = nil;
    XCTAssertTrue([self.organizer organiseGame:game error:&error],
                  @"organiseGame failed: %@", error);

    // The library root and everything that was in it must survive.
    XCTAssertTrue([self fileExists:@""], @"library root was deleted");
    XCTAssertTrue([self fileExists:@"curses.z5"], @"original game file was removed");
    XCTAssertTrue([self fileExists:@"unrelated.txt"], @"unrelated file was removed");
    XCTAssertTrue([self fileExists:@"Other stuff/keep.txt"], @"unrelated subfolder was touched");

    // And the organised copy must exist where the game now points.
    XCTAssertTrue([self fileExists:@"Z-code games/Curses/curses.z5"],
                  @"game was not copied into its library folder");
    XCTAssertTrue([self fileExists:@"Z-code games/Curses/.spatterlightIdentity"],
                  @"identity marker missing from game folder");
    XCTAssertEqualObjects(game.path,
                          [self.root URLByAppendingPathComponent:@"Z-code games/Curses/curses.z5"].path);
}

// Even if the root itself somehow carries an identity marker naming the game,
// the destination folder is nested inside the source, so the move-and-delete
// path must still be refused.
- (void)testOrganisingNeverDeletesRootEvenWithMarker {
    NSURL *gameURL = [self writeFile:@"curses.z5" contents:@"fake z-code"];
    [self writeFile:kMarkerFilename contents:@"TEST-IFID-167"];
    [self writeFile:@"unrelated.txt" contents:@"precious notes"];

    Game *game = [self makeGameWithTitle:@"Curses"
                                    ifid:@"TEST-IFID-167"
                                    path:gameURL.path];

    NSError *error = nil;
    XCTAssertTrue([self.organizer organiseGame:game error:&error],
                  @"organiseGame failed: %@", error);

    XCTAssertTrue([self fileExists:@""], @"library root was deleted");
    XCTAssertTrue([self fileExists:@"curses.z5"], @"original game file was removed");
    XCTAssertTrue([self fileExists:@"unrelated.txt"], @"unrelated file was removed");
}

// The legitimate case must keep working: a game folder the organiser owns
// (identity marker present) is moved wholesale when the game's group
// changes, and the old, now-empty folder is removed.
- (void)testReorganisingOwnedGameFolderMovesAndCleansUp {
    NSURL *gameURL = [self writeFile:@"Z-code games/Curses/curses.z5"
                            contents:@"fake z-code"];
    [self writeFile:@"Z-code games/Curses/cover.jpg" contents:@"art"];
    NSString *marker = [@"Z-code games/Curses/" stringByAppendingString:kMarkerFilename];
    [self writeFile:marker contents:@"TEST-IFID-167"];

    Game *game = [self makeGameWithTitle:@"Curses"
                                    ifid:@"TEST-IFID-167"
                                    path:gameURL.path];
    game.group = @"Favorites";

    NSError *error = nil;
    XCTAssertTrue([self.organizer organiseGame:game error:&error],
                  @"organiseGame failed: %@", error);

    XCTAssertTrue([self fileExists:@"Favorites/Curses/curses.z5"],
                  @"game file was not moved to the new group folder");
    XCTAssertTrue([self fileExists:@"Favorites/Curses/cover.jpg"],
                  @"companion file was not moved along");
    XCTAssertFalse([self fileExists:@"Z-code games/Curses"],
                   @"emptied game folder was not removed");
    XCTAssertTrue([self fileExists:@""], @"library root was deleted");
}

// A folder that exists but carries no identity marker (someone else's
// folder) must never be emptied or deleted, even when it sits inside the
// library and is not the destination.
- (void)testUnmarkedFolderInsideLibraryIsNotMoved {
    NSURL *gameURL = [self writeFile:@"My imports/curses.z5" contents:@"fake z-code"];
    [self writeFile:@"My imports/diary.txt" contents:@"private"];

    Game *game = [self makeGameWithTitle:@"Curses"
                                    ifid:@"TEST-IFID-167"
                                    path:gameURL.path];

    NSError *error = nil;
    XCTAssertTrue([self.organizer organiseGame:game error:&error],
                  @"organiseGame failed: %@", error);

    XCTAssertTrue([self fileExists:@"My imports/curses.z5"],
                  @"original game file was removed from unmarked folder");
    XCTAssertTrue([self fileExists:@"My imports/diary.txt"],
                  @"unrelated file was removed from unmarked folder");
    XCTAssertTrue([self fileExists:@"Z-code games/Curses/curses.z5"],
                  @"game was not copied into its library folder");
}

@end
