//
//  IfidMigrationTests.m
//  SpatterlightTests
//
//  Tests for the one-time migration of library entries whose IFID has a form
//  babel no longer produces: Quest games that got the bare MD5 of the file
//  (now QUEST-<MD5>), and Z-code and Glulx games whose UUID:// marker was
//  copied as found (now validated and capitalised).
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "GameImporter.h"
#import "LibraryOrganizer.h"
#import "TableViewController+LibraryManagement.h"
#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "NSString+Categories.h"

static NSString *const kMigrationDoneKey = @"OutdatedIfidMigrationDone";
static NSString *const kMarkerFilename = @".spatterlightIdentity";

// Reach into the organiser's private custom-URL cache so the tests can point
// it at a temporary root without going through security-scoped bookmarks.
@interface LibraryOrganizer ()
@property (nonatomic) NSURL *accessedCustomURL;
@end

@interface IfidMigrationTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) GameImporter *importer;
@property (nonatomic, strong) NSURL *root;
@property (nonatomic, strong) NSURL *savedOrganizerURL;
@property (nonatomic, strong) NSData *savedBookmarkDefault;
@property (nonatomic, strong) id savedMigrationDefault;

@end

@implementation IfidMigrationTests

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

    // A fresh temporary folder for every test. It doubles as the library
    // root of the shared organiser, which is the one the migration uses.
    NSString *dirName = [@"IfidMigrationTests-" stringByAppendingString:NSUUID.UUID.UUIDString];
    self.root = [NSURL fileURLWithPath:
                 [NSTemporaryDirectory() stringByAppendingPathComponent:dirName]
                           isDirectory:YES].URLByResolvingSymlinksInPath;
    [NSFileManager.defaultManager createDirectoryAtURL:self.root
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];

    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    self.savedBookmarkDefault = [defaults dataForKey:kOrganiseDirBookmarkKey];
    [defaults setObject:[NSData dataWithBytes:"x" length:1] forKey:kOrganiseDirBookmarkKey];
    LibraryOrganizer *organizer = [LibraryOrganizer sharedOrganizer];
    self.savedOrganizerURL = organizer.accessedCustomURL;
    organizer.accessedCustomURL = self.root;

    self.savedMigrationDefault = [defaults objectForKey:kMigrationDoneKey];
    [defaults removeObjectForKey:kMigrationDoneKey];

    self.importer = [[GameImporter alloc] initWithTableViewController:nil];
}

- (void)tearDown {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if (self.savedBookmarkDefault)
        [defaults setObject:self.savedBookmarkDefault forKey:kOrganiseDirBookmarkKey];
    else
        [defaults removeObjectForKey:kOrganiseDirBookmarkKey];
    if (self.savedMigrationDefault)
        [defaults setObject:self.savedMigrationDefault forKey:kMigrationDoneKey];
    else
        [defaults removeObjectForKey:kMigrationDoneKey];
    [LibraryOrganizer sharedOrganizer].accessedCustomURL = self.savedOrganizerURL;

    [NSFileManager.defaultManager removeItemAtURL:self.root error:NULL];
    self.importer = nil;
    self.context = nil;
    [super tearDown];
}

#pragma mark - Helpers

- (NSString *)writeData:(NSData *)data to:(NSString *)relativePath {
    NSURL *url = [self.root URLByAppendingPathComponent:relativePath];
    [NSFileManager.defaultManager createDirectoryAtURL:url.URLByDeletingLastPathComponent
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    XCTAssertTrue([data writeToURL:url atomically:YES],
                  @"Could not write fixture %@", relativePath);
    return url.path;
}

// A minimal Quest 4 source file with no id of its own.
- (NSData *)questFile {
    return [@"define game <Migration test>\r\n"
            @"\tasl-version <400>\r\n"
            @"\tstart <room>\r\n"
            @"end define\r\n\r\n"
            @"define room <room>\r\n"
            @"end define\r\n" dataUsingEncoding:NSUTF8StringEncoding];
}

// A minimal version 5 Z-code file, release 1, serial 250101, checksum BEEF,
// with `marker` (if any) somewhere after the header.
- (NSData *)zcodeFileWithMarker:(nullable NSString *)marker {
    NSMutableData *data = [NSMutableData dataWithLength:0x48];
    uint8_t *header = data.mutableBytes;
    header[0] = 5;
    header[3] = 1;
    for (int i = 4; i <= 14; i += 2)
        header[i + 1] = 0x40;
    memcpy(header + 0x12, "250101", 6);
    header[0x1C] = 0xBE;
    header[0x1D] = 0xEF;
    if (marker)
        [data appendData:[marker dataUsingEncoding:NSASCIIStringEncoding]];
    [data increaseLengthBy:8];
    return data;
}

- (Game *)makeGameWithFormat:(NSString *)format
                        ifid:(NSString *)ifid
                        path:(NSString *)path {
    Metadata *metadata = [NSEntityDescription insertNewObjectForEntityForName:@"Metadata"
                                                       inManagedObjectContext:self.context];
    metadata.title = path.lastPathComponent;
    metadata.format = format;
    [metadata createIfid:ifid];
    Game *game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                               inManagedObjectContext:self.context];
    game.metadata = metadata;
    game.ifid = ifid;
    game.detectedFormat = format;
    game.path = path;
    return game;
}

- (NSArray<NSString *> *)ifidStringsOf:(Game *)game {
    NSMutableArray<NSString *> *strings = [NSMutableArray new];
    for (Ifid *ifid in game.metadata.ifids)
        [strings addObject:ifid.ifidString];
    return [strings sortedArrayUsingSelector:@selector(compare:)];
}

- (BOOL)migrationIsDone {
    return [NSUserDefaults.standardUserDefaults boolForKey:kMigrationDoneKey];
}

#pragma mark - Quest

// The old IFID of a Quest game with no id of its own was the bare MD5 of
// the file. The new one is what babel says about the file now.
- (void)testQuestGameGetsPrefixedIfidFromFile {
    NSString *path = [self writeData:[self questFile] to:@"loose/test.asl"];
    NSString *newIfid = [TableViewController ifidFromFile:path];
    XCTAssertTrue([newIfid hasPrefix:@"QUEST-"], @"babel says %@", newIfid);
    XCTAssertEqual(newIfid.length, 38u);
    NSString *oldIfid = [newIfid substringFromIndex:6];

    Game *game = [self makeGameWithFormat:@"quest4" ifid:oldIfid path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, newIfid);
    XCTAssertEqualObjects([self ifidStringsOf:game], @[newIfid]);
    XCTAssertTrue([self migrationIsDone]);
}

// With the file gone, the old IFID is still the MD5 of the file we imported.
- (void)testQuestGameWithMissingFileGetsPrefix {
    NSString *path = [self.root URLByAppendingPathComponent:@"gone.cas"].path;
    Game *game = [self makeGameWithFormat:@"quest4"
                                     ifid:@"3a9d817fe7830c7341340baefee9498c"
                                     path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, @"QUEST-3A9D817FE7830C7341340BAEFEE9498C");
    XCTAssertEqualObjects([self ifidStringsOf:game],
                          @[@"QUEST-3A9D817FE7830C7341340BAEFEE9498C"]);
    XCTAssertTrue([self migrationIsDone]);
}

// A Quest 5 game with a <gameid>, and one already migrated, are left alone.
- (void)testQuestGamesWithProperIfidAreUntouched {
    Game *withId = [self makeGameWithFormat:@"quest5"
                                       ifid:@"1974a053-7db0-4103-93a1-767c1382c0b7"
                                       path:[self.root URLByAppendingPathComponent:@"a.quest"].path];
    Game *migrated = [self makeGameWithFormat:@"quest4"
                                         ifid:@"QUEST-3A9D817FE7830C7341340BAEFEE9498C"
                                         path:[self.root URLByAppendingPathComponent:@"b.asl"].path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(withId.ifid, @"1974a053-7db0-4103-93a1-767c1382c0b7");
    XCTAssertEqualObjects(migrated.ifid, @"QUEST-3A9D817FE7830C7341340BAEFEE9498C");
    XCTAssertTrue([self migrationIsDone]);
}

#pragma mark - Z-code

- (void)testLowercaseUuidIsCapitalised {
    NSString *lower = @"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    NSString *marker = [NSString stringWithFormat:@"UUID://%@//", lower];
    NSString *path = [self writeData:[self zcodeFileWithMarker:marker] to:@"loose/lower.z5"];
    Game *game = [self makeGameWithFormat:@"zcode" ifid:lower path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, lower.uppercaseString);
    XCTAssertEqualObjects([self ifidStringsOf:game], @[lower.uppercaseString]);
    XCTAssertTrue([self migrationIsDone]);
}

- (void)testLowercaseUuidIsCapitalisedWithoutFile {
    NSString *lower = @"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    Game *game = [self makeGameWithFormat:@"zcode"
                                     ifid:lower
                                     path:[self.root URLByAppendingPathComponent:@"gone.z5"].path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, lower.uppercaseString);
    XCTAssertTrue([self migrationIsDone]);
}

// A marker that is not an IFID used to be copied anyway. Babel now builds
// the IFID from the header instead.
- (void)testJunkMarkerBecomesHeaderIfid {
    NSString *junk = @"11111111-2222-3333-4444-555555555555 ";
    NSString *marker = [NSString stringWithFormat:@"UUID://%@//", junk];
    NSString *path = [self writeData:[self zcodeFileWithMarker:marker] to:@"loose/padded.z5"];
    Game *game = [self makeGameWithFormat:@"zcode" ifid:junk path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, @"ZCODE-1-250101-BEEF");
    XCTAssertEqualObjects([self ifidStringsOf:game], @[@"ZCODE-1-250101-BEEF"]);
    XCTAssertTrue([self migrationIsDone]);
}

// Without the file there is no telling what the header says. The game keeps
// its IFID and the migration stays pending until the file can be read.
- (void)testJunkMarkerWithoutFileStaysPending {
    NSString *junk = @"11111111-2222-3333-4444-555555555555 ";
    NSString *path = [self.root URLByAppendingPathComponent:@"loose/padded.z5"].path;
    Game *game = [self makeGameWithFormat:@"zcode" ifid:junk path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, junk);
    XCTAssertEqualObjects([self ifidStringsOf:game], @[junk]);
    XCTAssertFalse([self migrationIsDone]);

    // The file turns up again: the next launch settles it.
    NSString *marker = [NSString stringWithFormat:@"UUID://%@//", junk];
    [self writeData:[self zcodeFileWithMarker:marker] to:@"loose/padded.z5"];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, @"ZCODE-1-250101-BEEF");
    XCTAssertTrue([self migrationIsDone]);
}

// Header-built IFIDs can hold small letters in the serial, and so can the
// IFIDs of other formats. Neither is ours to change.
- (void)testValidAndForeignIfidsAreUntouched {
    Game *header = [self makeGameWithFormat:@"zcode"
                                       ifid:@"ZCODE-1-ab0101-BEEF"
                                       path:[self.root URLByAppendingPathComponent:@"a.z5"].path];
    Game *uuid = [self makeGameWithFormat:@"glulx"
                                     ifid:@"11111111-2222-3333-4444-555555555555"
                                     path:[self.root URLByAppendingPathComponent:@"b.ulx"].path];
    Game *other = [self makeGameWithFormat:@"tads3"
                                      ifid:@"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
                                      path:[self.root URLByAppendingPathComponent:@"c.t3"].path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(header.ifid, @"ZCODE-1-ab0101-BEEF");
    XCTAssertEqualObjects(uuid.ifid, @"11111111-2222-3333-4444-555555555555");
    XCTAssertEqualObjects(other.ifid, @"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    XCTAssertTrue([self migrationIsDone]);
}

- (void)testMigrationRunsOnlyOnce {
    [self.importer migrateOutdatedIfidsInContext:self.context];
    XCTAssertTrue([self migrationIsDone]);

    NSString *lower = @"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    Game *game = [self makeGameWithFormat:@"zcode"
                                     ifid:lower
                                     path:[self.root URLByAppendingPathComponent:@"gone.z5"].path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, lower);
}

#pragma mark - IFID records

// Metadata that already lists the new IFID (from imported iFiction, say)
// must not end up with it twice, and other IFIDs it lists are kept.
- (void)testIfidRecordsAreNotDuplicated {
    NSString *lower = @"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    Game *game = [self makeGameWithFormat:@"zcode"
                                     ifid:lower
                                     path:[self.root URLByAppendingPathComponent:@"gone.z5"].path];
    [game.metadata createIfid:lower.uppercaseString];
    [game.metadata createIfid:@"ZCODE-1-250101-BEEF"];
    XCTAssertEqual(game.metadata.ifids.count, 3u);

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, lower.uppercaseString);
    XCTAssertEqualObjects([self ifidStringsOf:game],
                          (@[lower.uppercaseString, @"ZCODE-1-250101-BEEF"]));
}

#pragma mark - Organised library

// An organised game's folder is marked with the game's IFID, and the
// organiser only cleans up folders whose marker matches. The marker has to
// follow the IFID.
- (void)testIdentityMarkerFollowsIfid {
    NSString *path = [self writeData:[self questFile] to:@"Quest/Migration test/test.asl"];
    NSString *newIfid = [TableViewController ifidFromFile:path];
    NSString *oldIfid = [newIfid substringFromIndex:6];
    NSURL *marker = [self.root URLByAppendingPathComponent:
                     [@"Quest/Migration test/" stringByAppendingString:kMarkerFilename]];
    [oldIfid writeToURL:marker atomically:YES encoding:NSUTF8StringEncoding error:NULL];

    Game *game = [self makeGameWithFormat:@"quest4" ifid:oldIfid path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, newIfid);
    NSString *stored = [NSString stringWithContentsOfURL:marker
                                                encoding:NSUTF8StringEncoding
                                                   error:NULL];
    XCTAssertEqualObjects(stored, newIfid);
}

// A marker that names something else is not ours to rewrite.
- (void)testForeignIdentityMarkerIsLeftAlone {
    NSString *path = [self writeData:[self questFile] to:@"Quest/Other/test.asl"];
    NSString *newIfid = [TableViewController ifidFromFile:path];
    NSString *oldIfid = [newIfid substringFromIndex:6];
    NSURL *marker = [self.root URLByAppendingPathComponent:
                     [@"Quest/Other/" stringByAppendingString:kMarkerFilename]];
    [@"SOMETHING-ELSE" writeToURL:marker atomically:YES encoding:NSUTF8StringEncoding error:NULL];

    Game *game = [self makeGameWithFormat:@"quest4" ifid:oldIfid path:path];

    [self.importer migrateOutdatedIfidsInContext:self.context];

    XCTAssertEqualObjects(game.ifid, newIfid);
    NSString *stored = [NSString stringWithContentsOfURL:marker
                                                encoding:NSUTF8StringEncoding
                                                   error:NULL];
    XCTAssertEqualObjects(stored, @"SOMETHING-ELSE");
}

#pragma mark - AGT hash repair

static NSString *const kAGTRepairDoneKey = @"AGTMigratedHashRepairDone";

- (Game *)makeAGTGameAt:(NSString *)path hash:(NSString *)hash {
    Metadata *metadata = [NSEntityDescription insertNewObjectForEntityForName:@"Metadata"
                                                       inManagedObjectContext:self.context];
    metadata.title = path.lastPathComponent;
    metadata.hashTag = hash;
    Game *game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                               inManagedObjectContext:self.context];
    game.metadata = metadata;
    game.ifid = @"AGT-01600-TESTTEST";
    game.detectedFormat = @"agt";
    game.path = path;
    game.hashTag = hash;
    game.found = YES;
    return game;
}

- (void)clearAGTRepairFlag {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    id saved = [defaults objectForKey:kAGTRepairDoneKey];
    [defaults removeObjectForKey:kAGTRepairDoneKey];
    [self addTeardownBlock:^{
        if (saved)
            [defaults setObject:saved forKey:kAGTRepairDoneKey];
        else
            [defaults removeObjectForKey:kAGTRepairDoneKey];
    }];
}

// A game the old migration re-pointed at its .D$$ still carries the hash of
// the converted .agx. The repair gives it the signature of the .D$$.
- (void)testAGTGameWithStaleHashGetsFileSignature {
    [self clearAGTRepairFlag];
    NSData *data = [@"An AGT game data file that is long enough to have a signature"
                    dataUsingEncoding:NSUTF8StringEncoding];
    NSString *path = [self writeData:data to:@"agt/HOBBS.D$$"];
    Game *game = [self makeAGTGameAt:path hash:@"HASHOFTHEDELETEDAGX"];

    [self.importer repairHashesOfMigratedAGTGamesInContext:self.context];

    NSString *signature = path.signatureFromFile;
    XCTAssertTrue(signature.length > 0);
    XCTAssertEqualObjects(game.hashTag, signature);
    XCTAssertEqualObjects(game.metadata.hashTag, signature);
    XCTAssertTrue([NSUserDefaults.standardUserDefaults boolForKey:kAGTRepairDoneKey]);
}

// When the original was added again in the meantime, the duplicate already
// has that hash. The repair must not give two entries the same one.
- (void)testAGTRepairLeavesExistingDuplicateAlone {
    [self clearAGTRepairFlag];
    NSData *data = [@"An AGT game data file that is long enough to have a signature"
                    dataUsingEncoding:NSUTF8StringEncoding];
    NSString *path = [self writeData:data to:@"agt/HOBBS.D$$"];
    Game *stale = [self makeAGTGameAt:path hash:@"HASHOFTHEDELETEDAGX"];
    Game *duplicate = [self makeAGTGameAt:path hash:path.signatureFromFile];

    [self.importer repairHashesOfMigratedAGTGamesInContext:self.context];

    XCTAssertEqualObjects(stale.hashTag, @"HASHOFTHEDELETEDAGX");
    XCTAssertEqualObjects(duplicate.hashTag, path.signatureFromFile);
}

// A file that cannot be read keeps the repair pending.
- (void)testAGTRepairStaysPendingWhileFileIsMissing {
    [self clearAGTRepairFlag];
    NSString *path = [self.root URLByAppendingPathComponent:@"agt/GONE.D$$"].path;
    Game *game = [self makeAGTGameAt:path hash:@"HASHOFTHEDELETEDAGX"];

    [self.importer repairHashesOfMigratedAGTGamesInContext:self.context];

    XCTAssertEqualObjects(game.hashTag, @"HASHOFTHEDELETEDAGX");
    XCTAssertFalse([NSUserDefaults.standardUserDefaults boolForKey:kAGTRepairDoneKey]);
}

@end
