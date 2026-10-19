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
#import "NSData+Categories.h"
#import "CoreDataManager.h"

typedef void (^DownloadHandler)(NSData * _Nullable, NSURLResponse * _Nullable, NSError * _Nullable, NSString * _Nullable);

@interface IFDBDownloader () {
    NSURLSession *defaultSession;
}

+ (nullable Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context;

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

    NSOperation __block *lastoperation = nil;

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

    [context performBlockAndWait:^{
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
    }];

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
            NSOperation *op = [self downloadImageFor:objectID inContext:context onQueue:queue forceDialog:NO reportFailure:NO];
            if (op) {
                _tableViewController.lastImageDownloadOperation = op;
            }
        }
        // In image-only mode, a record with a cover URL counts as found
        // even if we already have that cover and nothing is downloaded.
        if (!imageOnly) {
            [story addInfoToMetadata:metadata];
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

    NSString __block *coverArtURL = nil;

    CoreDataManager *manager = _tableViewController.coreDataManager;

    [context performBlockAndWait:^{
        Metadata *metadata = [context objectWithID:metaID];
        coverArtURL = metadata.coverArtURL;
    }];

    if (!coverArtURL.length) {
        NSLog(@"IFDBDownloader downloadImageFor: found no coverArtURL!");
        [IFDBDownloader reportNoDataFoundIf:report];
        return nil;
    }

    NSData __block *newData;
    NSData __block *oldData;
    NSManagedObjectID __block *imgID;

    [context performBlockAndWait:^{
        Image *img = [IFDBDownloader fetchImageForURL:coverArtURL inContext:context];
        if (img) {
            imgID = img.objectID;
            newData = (NSData *)img.data;
            Metadata *metadata = [context objectWithID:metaID];
            oldData = (NSData *)metadata.cover.data;
        }
    }];

    if (imgID) {
        // We already have an image from this URL in the library
        [IFDBDownloader checkIfUserWants:newData ratherThan:oldData force:force completionHandler:^{
            NSManagedObjectContext *childContext = [self newImageContext];
            [childContext performBlockAndWait:^{
                [IFDBDownloader setCover:[childContext objectWithID:imgID] onMetadata:[childContext objectWithID:metaID]];
                [childContext safeSave];
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
            [IFDBDownloader insertImageData:data inMetadataID:metaID context:[self newImageContext]];
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

// Makes img the cover of metadata, copies the cover description whichever
// way is needed, and deletes the previous cover if nothing else uses it.
// Must be called on the queue of their context.
+ (void)setCover:(Image *)img onMetadata:(Metadata *)metadata {
    Image *oldCover = metadata.cover;
    metadata.cover = img;
    if (oldCover != img)
        [Image deleteIfOrphan:oldCover];
    if (img.imageDescription.length) {
        metadata.coverArtDescription = img.imageDescription;
    } else if (metadata.coverArtDescription.length) {
        img.imageDescription = metadata.coverArtDescription;
    }
}

+ (void)insertImageData:(NSData *)data inMetadataID:(NSManagedObjectID *)metaID context:(NSManagedObjectContext *)context {
    if (!data.length) {
        NSLog(@"IFDBDownloader insertImageData called with empty image data");
        return;
    }

    if (!context) {
        NSLog(@"Error! No context!");
        return;
    }

    if ([data isPlaceHolderImage]) {
        [context performBlock:^{
            Image *placeholder = [IFDBDownloader placeholderImageWithData:data inContext:context];
            [IFDBDownloader setCover:placeholder onMetadata:[context objectWithID:metaID]];
            [context safeSave];
        }];
        return;
    }

    BOOL __block reused = NO;

    [context performBlockAndWait:^{
        Metadata *metadata = [context objectWithID:metaID];
        if (!metadata || metadata.coverArtURL.length == 0)
            return;
        Image *img = [IFDBDownloader fetchImageForURL:metadata.coverArtURL inContext:context];
        if (img && [((NSData *)img.data) isEqual:data]) {
            // Image with same originalURL and data already existed in database
            [IFDBDownloader setCover:img onMetadata:metadata];
            [context safeSave];
            reused = YES;
        }
    }];

    if (reused)
        return;

    [context performBlock:^{
        Image *img = (Image *) [NSEntityDescription
                                insertNewObjectForEntityForName:@"Image"
                                inManagedObjectContext:context];
        img.data = [data copy];
        Metadata *metadata = [context objectWithID:metaID];
        img.originalURL = metadata.coverArtURL;
        img.imageDescription = metadata.coverArtDescription;
        [IFDBDownloader setCover:img onMetadata:metadata];
        [context safeSave];
    }];
}

// The shared Image for the Inform 7 placeholder cover, created on first use.
// Must be called on the queue of context.
+ (Image *)placeholderImageWithData:(NSData *)data inContext:(NSManagedObjectContext *)context {
    Image *placeholder = [IFDBDownloader fetchImageForURL:kImagePlaceholderOriginalURL inContext:context];
    if (!placeholder) {
        NSLog(@"IFDBDownloader: Creating new placeholder Image object in Core Data");
        placeholder = (Image *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Image"
                                 inManagedObjectContext:context];
        placeholder.data = [data copy];
        placeholder.originalURL = kImagePlaceholderOriginalURL;
        placeholder.imageDescription = kImagePlaceholderDescription;
    }
    return placeholder;
}

// Must be called on the queue of context.
+ (Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context {

    if (imgurl.length == 0)
        return nil;

    NSError *error = nil;
    NSFetchRequest *fetchRequest = [Image fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];

    NSArray<Image *> *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchImageForURL: Could not execute fetch request! %@",error);
    }

    if (fetchedObjects.count == 0) {
        // Found no previously loaded Image object with url imgurl
        return nil;
    }

    // Found more than one Image object with the same originalURL string.
    // Merge those with identical data into the first one. Ones with
    // different data are kept.
    Image *toKeep = fetchedObjects.firstObject;
    for (Image *image in fetchedObjects) {
        if (image != toKeep && [(NSData *)image.data isEqual:(NSData *)toKeep.data]) {
            [toKeep addMetadata:image.metadata];
            [context deleteObject:image];
        }
    }
    return toKeep;
}

@end
