//
//  IFDBDownloader.m
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFDBDownloader.h"
#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFDB.h"
#import "Metadata.h"
#import "Metadata+IFiction.h"
#import "Image.h"
#import "Game.h"
#import "Ifid.h"
#import "ImageCompareViewController.h"
#import "NotificationBezel.h"
#import "AppDelegate.h"
#import "TableViewController.h"
#import "DownloadOperation.h"
#import "NSManagedObjectContext+safeSave.h"
#import "ComparisonOperation.h"
#import "CoreDataManager.h"
#import "CoverImageStore.h"

typedef void (^DownloadHandler)(NSData * _Nullable, NSURLResponse * _Nullable, NSError * _Nullable, NSString * _Nullable);

@interface IFDBDownloader () {
    NSURLSession *defaultSession;
}

@end

// What downloadImageFor: needs to know from Core Data, read in one go on the
// queue of the caller's context
@interface IFDBCoverLookup : NSObject
@property (nullable) NSString *coverArtURL;
//! An Image already in the library with coverArtURL as its originalURL
@property (nullable) NSManagedObjectID *existingImageID;
@property (nullable) NSData *existingImageData;
@property (nullable) NSData *currentCoverData;
@end

@implementation IFDBCoverLookup
@end

@implementation IFDBDownloader

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController {
    self = [super init];
    if (self) {
        defaultSession = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration] delegate:nil delegateQueue:nil];
        _tableViewController = tableViewController;
    }
    return self;
}

- (nullable NSOperation *)downloadMetadataForGames:(NSArray<NSManagedObjectID *> *)games inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure completionHandler:(nullable void (^)(void))completionHandler {
    if (!games.count)
        return nil;

    NSOperation *lastoperation = nil;

    TableViewController *tableViewController = _tableViewController;

    DownloadHandler handler = ^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error, NSString * _Nullable identifier) {
        if (error) {
            if (!data) {
                NSLog(@"Error connecting: %@", [error localizedDescription]);
                [IFDBDownloader reportNoDataFoundIf:reportFailure];
            }
            return;
        }
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode != 200 || !data) {
            NSLog(@"httpResponse: url:%@ status code:%ld", httpResponse.URL, httpResponse.statusCode);
            [IFDBDownloader reportNoDataFoundIf:reportFailure];
            return;
        }
        [context performBlock:^{
            [self addIFictionData:data toMetadataWithURI:identifier inContext:context onQueue:queue imageOnly:imageOnly reportFailure:reportFailure];
        }];
    };

    for (NSManagedObjectID *gameID in games) {
        Game *game = [context objectWithID:gameID];
        Metadata *metadata = game.metadata;
        if (metadata == nil)
            continue;
        NSString *identifier = metadata.objectID.URIRepresentation.absoluteString;
        if (identifier.length == 0)
            continue;

        // Look the game up by TUID if we know it, otherwise by every IFID.
        NSMutableArray<NSURL *> *urls = [NSMutableArray new];
        if (metadata.tuid.length || !metadata.ifids.count) {
            NSURL *url = [IFDB viewGameURLForTUID:metadata.tuid IFID:game.ifid iFiction:YES];
            if (url)
                [urls addObject:url];
        } else {
            for (Ifid *ifid in metadata.ifids) {
                NSURL *url = [IFDB viewGameURLForTUID:nil IFID:ifid.ifidString iFiction:YES];
                if (url)
                    [urls addObject:url];
            }
        }

        for (NSURL *url in urls) {
            DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:defaultSession dataTaskURL:url identifier:identifier completionHandler:handler];
            [queue addOperation:operation];
            lastoperation = operation;
        }
    }

    if (completionHandler && lastoperation) {
        NSBlockOperation *finisher = [NSBlockOperation blockOperationWithBlock:completionHandler];
        [finisher addDependency:lastoperation];
        [queue addOperationWithBlock:^{
            NSOperation *op = tableViewController.lastImageDownloadOperation;
            if (op)
                [finisher addDependency:op];
            [queue addOperation:finisher];
        }];
        lastoperation = finisher;
    }

    if (lastoperation == nil)
        NSLog(@"Error! downloadMetadataForGames lastoperation == nil!");
    return lastoperation;
}

// Called on the queue of context with an iFiction record downloaded from IFDB.
// Starts a cover image download if the record has a new cover URL.
- (void)addIFictionData:(NSData *)data toMetadataWithURI:(NSString *)identifier inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure {
    NSURL *uri = identifier ? [NSURL URLWithString:identifier] : nil;
    NSManagedObjectID *objectID = uri ? [context.persistentStoreCoordinator managedObjectIDForURIRepresentation:uri] : nil;
    if (objectID == nil) {
        NSLog(@"ERROR! downloadMetadataForGames: found no metadata object with identifier \"%@\" to add metadata to!", identifier);
        [IFDBDownloader reportNoDataFoundIf:reportFailure];
        return;
    }
    Metadata *metadata = [context objectWithID:objectID];

    // IFDB should always send exactly one story
    IFStory *story = [[IFictionMetadata alloc] initWithData:data].firstStory;

    BOOL found = NO;
    if (!story) {
        NSLog(@"No metadata found!");
    } else {
        NSString *coverArtURL = story.ifdb.coverArtURL;
        if (coverArtURL.length && ![metadata.cover.originalURL isEqualToString:coverArtURL]) {
            metadata.coverArtURL = coverArtURL;
            // We are already on the queue of context, so look up the cover
            // here rather than through downloadImageFor:, which would
            // nest a performBlockAndWait: on the same queue
            NSOperation *op = [self downloadImageFor:objectID lookup:[self coverLookupFor:objectID inContext:context] onQueue:queue forceDialog:NO reportFailure:NO];
            if (op) {
                _tableViewController.lastImageDownloadOperation = op;
            }
        }
        // In image-only mode, a record with a cover URL counts as found
        // even if we already have that cover and nothing is downloaded.
        if (!imageOnly) {
            [metadata addInfoFromStory:story];
            found = YES;
        } else if (coverArtURL.length) {
            found = YES;
        }
    }

    if (!found)
        [IFDBDownloader reportNoDataFoundIf:reportFailure];
    [context safeSave];
}

+ (void)reportNoDataFoundIf:(BOOL)report {
    if (!report)
        return;
    [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:NSScreen.screens.firstObject];
        [bezel showStandardWithText:@"? No data found"];
    }];
}

// A private queue child of the main context, for cover changes that may
// race with the library window
- (NSManagedObjectContext *)newImageContext {
    NSManagedObjectContext *childContext = _tableViewController.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
    return childContext;
}

- (NSOperation *)downloadImageFor:(NSManagedObjectID *)metaID inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force reportFailure:(BOOL)report {
    IFDBCoverLookup __block *lookup;
    [context performBlockAndWait:^{
        lookup = [self coverLookupFor:metaID inContext:context];
    }];
    return [self downloadImageFor:metaID lookup:lookup onQueue:queue forceDialog:force reportFailure:report];
}

// Must be called on the queue of context
- (IFDBCoverLookup *)coverLookupFor:(NSManagedObjectID *)metaID inContext:(NSManagedObjectContext *)context {
    IFDBCoverLookup *lookup = [IFDBCoverLookup new];
    Metadata *metadata = [context objectWithID:metaID];
    lookup.coverArtURL = metadata.coverArtURL;
    if (!lookup.coverArtURL.length)
        return lookup;
    Image *img = [CoverImageStore fetchImageForURL:lookup.coverArtURL inContext:context];
    if (img) {
        lookup.existingImageID = img.objectID;
        lookup.existingImageData = (NSData *)img.data;
        lookup.currentCoverData = (NSData *)metadata.cover.data;
    }
    return lookup;
}

// Does not touch the context the lookup was made in, so it can be called
// from any queue
- (NSOperation *)downloadImageFor:(NSManagedObjectID *)metaID lookup:(IFDBCoverLookup *)lookup onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force reportFailure:(BOOL)report {

    NSString *coverArtURL = lookup.coverArtURL;
    NSManagedObjectID *imgID = lookup.existingImageID;
    NSData *newData = lookup.existingImageData;
    NSData *oldData = lookup.currentCoverData;

    CoreDataManager *manager = _tableViewController.coreDataManager;

    if (!coverArtURL.length) {
        NSLog(@"IFDBDownloader downloadImageFor: found no coverArtURL!");
        [IFDBDownloader reportNoDataFoundIf:report];
        return nil;
    }

    if (imgID) {
        // We already have an image from this URL in the library
        [IFDBDownloader checkIfUserWants:newData ratherThan:oldData force:force completionHandler:^{
            NSManagedObjectContext *childContext = [self newImageContext];
            [childContext performBlockAndWait:^{
                [CoverImageStore setCover:[childContext objectWithID:imgID] onMetadata:[childContext objectWithID:metaID]];
                [childContext safeSaveAndWait];
            }];
            [manager saveChanges];
        }];
        return nil;
    }

    NSURL *url = [NSURL URLWithString:coverArtURL];

    if (!url || !([url.scheme isEqualToString:@"http"] || [url.scheme isEqualToString:@"https"])) {
        NSLog(@"Could not create url from %@", coverArtURL);
        [IFDBDownloader reportNoDataFoundIf:report];
        return nil;
    }

    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:defaultSession dataTaskURL:url identifier:nil completionHandler:^(NSData * data, NSURLResponse * response, NSError * error, NSString * identifier) {
        if (error) {
            if (!data.length) {
                NSLog(@"Error connecting to url %@: %@", url, [error localizedDescription]);
                [IFDBDownloader reportNoDataFoundIf:report];
            }
            return;
        }
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode != 200 || !data) {
            [IFDBDownloader reportNoDataFoundIf:report];
            return;
        }
        NSManagedObjectContext *childContext = [self newImageContext];
        NSData __block *currentData;
        [childContext performBlockAndWait:^{
            Metadata *metadata = [childContext objectWithID:metaID];
            currentData = (NSData *)metadata.cover.data;
        }];

        [IFDBDownloader checkIfUserWants:data ratherThan:currentData force:force completionHandler:^{
            NSManagedObjectContext *imageContext = [self newImageContext];
            [CoverImageStore insertImageData:data inMetadataID:metaID context:imageContext];
            // Queued after insertImageData's blocks, which save the child
            // context before they return
            [imageContext performBlock:^{
                [manager saveChanges];
            }];
        }];
    }];

    [queue addOperation:operation];
    return operation;
}

+ (void)checkIfUserWants:(NSData *)newData ratherThan:(NSData *)oldData force:(BOOL)force completionHandler:(void (^)(void))completionHandler {
    kImageComparisonResult comparisonResult = [ImageCompareViewController chooseImageA:newData orB:oldData source:kImageComparisonDownloaded force:force];

    if (comparisonResult == kImageComparisonResultA) {
        completionHandler();
    } else if (comparisonResult == kImageComparisonResultWantsUserInput) {
        dispatch_async(dispatch_get_main_queue(), ^{
            TableViewController *libController = ((AppDelegate*)NSApp.delegate).tableViewController;
            if (!force && [libController.lastImageComparisonData isEqual:newData])
                return;
            if (libController.downloadWasCancelled)
                return;
            ComparisonOperation *comparisonOperation = [[ComparisonOperation alloc] initWithNewData:newData oldData:oldData force:force completionHandler:completionHandler];
            NSOperationQueue *alertQueue = libController.alertQueue;
            [alertQueue addOperation:comparisonOperation];
            libController.lastImageComparisonData = newData;
        });
    }
}

@end
