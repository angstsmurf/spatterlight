//
//  IFDBDownloaderTests.m
//  SpatterlightTests
//
//  Drives IFDBDownloader end to end against a stubbed network: an iFiction
//  record download, then a cover image download or the reuse of an Image
//  already in the library. Each path is run from a private child context (as
//  MetadataHandler calls it) and from the main context (as ImageView does).
//  Run with -com.apple.CoreData.ConcurrencyDebug 1 to catch managed objects
//  touched off their context's queue.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "IFDBDownloader.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

static NSString * const kStubCoverURL = @"https://ifdb.org/coverart?id=stubtest";

#pragma mark - Network stub

@interface IFDBStubProtocol : NSURLProtocol
@end

@implementation IFDBStubProtocol

+ (NSData *)coverData {
    static NSData *data;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSMutableData *bytes = [NSMutableData dataWithLength:256];
        uint8_t *p = bytes.mutableBytes;
        for (NSUInteger i = 0; i < bytes.length; i++)
            p[i] = (uint8_t)(i * 7 + 3);
        data = bytes;
    });
    return data;
}

+ (BOOL)canInitWithRequest:(NSURLRequest *)request {
    return [request.URL.host isEqualToString:@"ifdb.org"];
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request {
    return request;
}

- (void)startLoading {
    NSURL *url = self.request.URL;
    NSData *body;
    if ([url.path isEqualToString:@"/viewgame"]) {
        NSString *xml = [NSString stringWithFormat:
                         @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                         "<ifindex version=\"1.0\" xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">"
                         "<story><identification><ifid>STUB-IFID-0001</ifid><format>zcode</format></identification>"
                         "<bibliographic><title>Stub Game</title><author>Nobody</author></bibliographic>"
                         "<ifdb xmlns=\"http://ifdb.org/api/xmlns\"><tuid>stubtuid</tuid>"
                         "<coverart><url>%@</url></coverart></ifdb>"
                         "</story></ifindex>",
                         [kStubCoverURL stringByReplacingOccurrencesOfString:@"&" withString:@"&amp;"]];
        body = [xml dataUsingEncoding:NSUTF8StringEncoding];
    } else {
        body = [IFDBStubProtocol coverData];
    }
    NSHTTPURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:url statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil];
    [self.client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:body];
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading {
}

@end

#pragma mark - Library stand-in

// The parts of TableViewController that IFDBDownloader uses
@interface IFDBStubLibrary : NSObject
@property (strong) CoreDataManager *coreDataManager;
@property (weak) NSOperation *lastImageDownloadOperation;
@property NSData *lastImageComparisonData;
@property BOOL downloadWasCancelled;
@end

@implementation IFDBStubLibrary
@end

#pragma mark - Tests

@interface IFDBDownloaderTests : XCTestCase
@property (nonatomic, strong) CoreDataManager *manager;
@property (nonatomic, strong) IFDBStubLibrary *library;
@property (nonatomic, strong) IFDBDownloader *downloader;
@property (nonatomic, strong) NSOperationQueue *queue;
@end

@implementation IFDBDownloaderTests

- (void)setUp {
    [super setUp];
    self.manager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    XCTAssertNotNil(self.manager.mainManagedObjectContext);
    self.library = [IFDBStubLibrary new];
    self.library.coreDataManager = self.manager;
    self.downloader = [[IFDBDownloader alloc] initWithTableViewController:(TableViewController *)self.library];

    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    configuration.protocolClasses = @[ [IFDBStubProtocol class] ];
    [self.downloader setValue:[NSURLSession sessionWithConfiguration:configuration] forKey:@"defaultSession"];

    self.queue = [NSOperationQueue new];
    self.queue.maxConcurrentOperationCount = 3;
}

- (void)tearDown {
    [self.queue cancelAllOperations];
    [self.queue waitUntilAllOperationsAreFinished];
    self.queue = nil;
    self.downloader = nil;
    self.library = nil;
    self.manager = nil;
    [super tearDown];
}

// A saved game with no cover, so its object IDs are permanent
- (NSManagedObjectID *)insertGame {
    NSManagedObjectContext *context = self.manager.mainManagedObjectContext;
    NSManagedObjectID __block *gameID;
    [context performBlockAndWait:^{
        Metadata *metadata = [NSEntityDescription insertNewObjectForEntityForName:@"Metadata" inManagedObjectContext:context];
        metadata.title = @"Stub Game";
        Game *game = [NSEntityDescription insertNewObjectForEntityForName:@"Game" inManagedObjectContext:context];
        game.metadata = metadata;
        game.ifid = @"STUB-IFID-0001";
        game.detectedFormat = @"zcode";
        NSError *error = nil;
        XCTAssertTrue([context save:&error], @"%@", error);
        gameID = game.objectID;
    }];
    return gameID;
}

- (NSManagedObjectID *)insertImageWithData:(NSData *)data {
    NSManagedObjectContext *context = self.manager.mainManagedObjectContext;
    NSManagedObjectID __block *imageID;
    [context performBlockAndWait:^{
        Image *image = [NSEntityDescription insertNewObjectForEntityForName:@"Image" inManagedObjectContext:context];
        image.data = data;
        image.originalURL = kStubCoverURL;
        NSError *error = nil;
        XCTAssertTrue([context save:&error], @"%@", error);
        imageID = image.objectID;
    }];
    return imageID;
}

- (NSManagedObjectContext *)childContext {
    NSManagedObjectContext *child = self.manager.privateChildManagedObjectContext;
    child.undoManager = nil;
    child.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
    return child;
}

- (void)download:(NSManagedObjectID *)gameID inContext:(NSManagedObjectContext *)context imageOnly:(BOOL)imageOnly {
    XCTestExpectation *finished = [self expectationWithDescription:@"completion handler"];
    IFDBDownloader *downloader = self.downloader;
    NSOperationQueue *queue = self.queue;
    [context performBlock:^{
        NSOperation *op = [downloader downloadMetadataForGames:@[gameID] inContext:context onQueue:queue imageOnly:imageOnly reportFailure:NO completionHandler:^{
            [finished fulfill];
        }];
        XCTAssertNotNil(op);
    }];
    [self waitForExpectations:@[finished] timeout:10];
}

// Waits for the store itself (not just the main context) to hold a cover for
// the game, and returns its data
- (NSData *)waitForCoverOf:(NSManagedObjectID *)gameID imageID:(NSManagedObjectID * __autoreleasing *)imageID {
    NSPersistentStoreCoordinator *coordinator = self.manager.mainManagedObjectContext.persistentStoreCoordinator;
    NSData __block *data;
    NSManagedObjectID __block *coverID;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
    while (!data && deadline.timeIntervalSinceNow > 0) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        NSManagedObjectContext *storeContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
        storeContext.persistentStoreCoordinator = coordinator;
        [storeContext performBlockAndWait:^{
            Game *game = [storeContext objectWithID:gameID];
            Image *cover = game.metadata.cover;
            data = (NSData *)cover.data;
            coverID = cover.objectID;
        }];
    }
    if (imageID)
        *imageID = coverID;
    return data;
}

- (void)assertCoverURLOf:(NSManagedObjectID *)gameID {
    NSManagedObjectContext *context = self.manager.mainManagedObjectContext;
    [context performBlockAndWait:^{
        Game *game = [context objectWithID:gameID];
        XCTAssertEqualObjects(game.metadata.coverArtURL, kStubCoverURL);
        XCTAssertEqualObjects(game.metadata.cover.originalURL, kStubCoverURL);
    }];
}

- (void)testNewCoverFromChildContext {
    NSManagedObjectID *gameID = [self insertGame];
    [self download:gameID inContext:[self childContext] imageOnly:NO];
    XCTAssertEqualObjects([self waitForCoverOf:gameID imageID:NULL], [IFDBStubProtocol coverData]);
    [self assertCoverURLOf:gameID];
    NSManagedObjectContext *context = self.manager.mainManagedObjectContext;
    [context performBlockAndWait:^{
        Game *game = [context objectWithID:gameID];
        XCTAssertEqualObjects(game.metadata.tuid, @"stubtuid");
    }];
}

- (void)testNewCoverFromMainContext {
    NSManagedObjectID *gameID = [self insertGame];
    [self download:gameID inContext:self.manager.mainManagedObjectContext imageOnly:YES];
    XCTAssertEqualObjects([self waitForCoverOf:gameID imageID:NULL], [IFDBStubProtocol coverData]);
    [self assertCoverURLOf:gameID];
}

- (void)testExistingImageFromChildContext {
    NSManagedObjectID *gameID = [self insertGame];
    NSManagedObjectID *existingID = [self insertImageWithData:[IFDBStubProtocol coverData]];
    [self download:gameID inContext:[self childContext] imageOnly:NO];
    NSManagedObjectID *coverID = nil;
    XCTAssertEqualObjects([self waitForCoverOf:gameID imageID:&coverID], [IFDBStubProtocol coverData]);
    XCTAssertEqualObjects(coverID, existingID);
}

- (void)testExistingImageFromMainContext {
    NSManagedObjectID *gameID = [self insertGame];
    NSManagedObjectID *existingID = [self insertImageWithData:[IFDBStubProtocol coverData]];
    [self download:gameID inContext:self.manager.mainManagedObjectContext imageOnly:YES];
    NSManagedObjectID *coverID = nil;
    XCTAssertEqualObjects([self waitForCoverOf:gameID imageID:&coverID], [IFDBStubProtocol coverData]);
    XCTAssertEqualObjects(coverID, existingID);
}

@end
