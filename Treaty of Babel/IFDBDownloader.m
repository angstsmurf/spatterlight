//
//  IFDBDownloader.m
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif


#import "IFDBDownloader.h"
#import "IFictionMetadata.h"
#import "IFStory.h"
#import "Metadata.h"
#import "Image.h"
#import "Game.h"
#import "Ifid.h"
#import "ImageCompareViewController.h"
#import "IFDB.h"
#import "NotificationBezel.h"
#import "AppDelegate.h"
#import "TableViewController.h"
#import "DownloadOperation.h"
#import "NSManagedObjectContext+safeSave.h"
#import "ComparisonOperation.h"
#import "NSData+Categories.h"
#import "CoreDataManager.h"


@interface IFDBDownloader () <NSURLSessionDelegate> {
    NSURLSession *defaultSession;
    NSURLSessionDataTask *dataTask;
}

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

- (void)URLSession:(NSURLSession *)session
didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler {
    NSLog(@"URLSession:didReceiveChallenge:");

    dispatch_async(dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSURLProtectionSpace *protectionSpace = challenge.protectionSpace;
        if (protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust) {
            SecTrustRef trust = protectionSpace.serverTrust;

            /***** Make specific changes to the trust policy here. *****/

            /* Re-evaluate the trust policy. */
            SecTrustResultType secresult = kSecTrustResultInvalid;
            if (SecTrustEvaluate(trust, &secresult) != errSecSuccess) {
                /* Trust evaluation failed. */

                // Perform other cleanup here, as needed.
                return;
            }

            NSLog(@"secresult: %u", secresult);

            switch (secresult) {
                case kSecTrustResultUnspecified: // The OS trusts this certificate implicitly.
                    NSLog(@"kSecTrustResultUnspecified");
                case kSecTrustResultRecoverableTrustFailure:
                    NSLog(@"kSecTrustResultRecoverableTrustFailure");
                case kSecTrustResultProceed: {
                    // The user explicitly told the OS to trust it.
                    NSLog(@"kSecTrustResultProceed");
                    NSURLCredential *credential =
                    [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust];
                    completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
                    return;
                }
                default: ;
                    /* It's somebody else's key. Fall through. */
            }
            /* The server sent a key other than the trusted key. */

            // Perform other cleanup here, as needed.

        }
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    });
}

+ (nullable DownloadOperation *)operationForIFID:(NSString*)ifid session:(NSURLSession *)session identifier:(NSString *)identifier completionHandler:(void (^)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable,  NSString * _Nullable))handler {
    if (!ifid || ifid.length == 0) {
        return nil;
    }

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid]];
    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:session dataTaskURL:url identifier:identifier completionHandler:handler];

    return operation;
}

+ (nullable DownloadOperation *)operationForTUID:(NSString*)tuid session:(NSURLSession *)session identifier:(NSString *)identifier completionHandler:(void (^)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable, NSString * _Nullable))handler {
    if (tuid.length == 0) {
        return nil;
    }

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.org/viewgame?ifiction&id=" stringByAppendingString:tuid]];
    ;
    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:session dataTaskURL:url identifier:identifier completionHandler:handler];

    return operation;
}

- (nullable NSOperation *)downloadMetadataForGames:(NSArray<NSManagedObjectID *> *)games inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure completionHandler:(nullable void (^)(void))completionHandler {
    if (!games.count)
        return nil;

    NSOperation __block *lastoperation = nil;

    IFDBDownloader __weak *weakSelf = self;

    TableViewController *tableViewController = _tableViewController;

    [context performBlockAndWait:^{
        IFDBDownloader *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        void (^internalHandler)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable,  NSString * _Nullable) = ^void(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error, NSString * _Nullable identifier) {
            if (error) {
                if (!data) {
                    NSLog(@"Error connecting: %@", [error localizedDescription]);
                    if (reportFailure)
                        [IFDBDownloader showNoDataFoundBezel];
                }
            } else {
                NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
                if (httpResponse.statusCode == 200 && data) {
                    [context performBlock:^{
                        BOOL success = NO;

                        NSURL *uri = [NSURL URLWithString:identifier];
                        NSManagedObjectID *objectID = [context.persistentStoreCoordinator managedObjectIDForURIRepresentation:uri];
                        Metadata *metadata = [context objectWithID:objectID];
                        if (metadata == nil) {
                            NSLog(@"ERROR! downloadMetadataForGames: found no metadata object with identifier \"%@\" to add metadata to!", identifier);
                        }

                        IFictionMetadata *result = [[IFictionMetadata alloc] initWithData:data];
                        if (result.stories.count == 0) {
                            NSLog(@"No metadata found!");
                        } else {
                            // IFictionMetadata does not directly touch the library
                            // database or download anything by itself anymore,
                            // so we take care of that here instead.
                            // XML data downloaded from IFDB should always contain 1 story only
                            if (result.stories.count > 1)  {
                                NSLog(@"Downloaded XML data contained more than one story! (%ld)", result.stories.count);
                                NSLog(@"Throwing away all but the first one.");
                            }
                            IFStory *story = result.stories.firstObject;
                            
                            NSString *coverArtURL = story.ifdb.coverArtURL;
                            if (coverArtURL.length && ![metadata.cover.originalURL isEqualToString:coverArtURL]) {
                                metadata.coverArtURL = coverArtURL;
                                NSOperation *op = [strongSelf downloadImageFor:objectID inContext:context onQueue:queue forceDialog:NO reportFailure:NO];
                                if (op) {
                                    tableViewController.lastImageDownloadOperation = op;
                                }
                            }
                            if (!imageOnly) {
                                [story addInfoToMetadata:metadata];
                                success = YES;
                            } else if (coverArtURL.length) {
                                success = YES;
                            }
                            result = nil;
                            story = nil;
                            metadata = nil;
                        }

                        if (!success && reportFailure) {
                            [IFDBDownloader showNoDataFoundBezel];
                        }
                        [context safeSave];
                    }];
                } else if (reportFailure) {
                    NSLog(@"httpResponse: url:%@ status code:%ld", httpResponse.URL, httpResponse.statusCode);
                    [IFDBDownloader showNoDataFoundBezel];
                }
            }
        };

        for (NSManagedObjectID *gameID in games) {
            Game *game = [context objectWithID:gameID];
            if (game.metadata == nil)
                continue;
            DownloadOperation *operation;
            NSString *identifier = game.metadata.objectID.URIRepresentation.absoluteString;
            if (identifier.length == 0)
                continue;
            if (game.metadata.tuid.length) {
                operation = [IFDBDownloader operationForTUID:game.metadata.tuid session:defaultSession identifier:identifier completionHandler:internalHandler];
                [queue addOperation:operation];
                lastoperation = operation;
            } else {
                if (game.metadata.ifids.count) {
                    for (Ifid *ifid in game.metadata.ifids) {
                        operation = [IFDBDownloader operationForIFID:ifid.ifidString session:defaultSession identifier:identifier completionHandler:internalHandler];
                        [queue addOperation:operation];
                        lastoperation = operation;
                    }
                } else {
                    operation = [IFDBDownloader operationForIFID:game.ifid session:defaultSession identifier:identifier completionHandler:internalHandler];
                    [queue addOperation:operation];
                    lastoperation = operation;
                }
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

+ (void)showNoDataFoundBezel {
    [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:NSScreen.screens.firstObject];
        [bezel showStandardWithText:@"? No data found"];
    }];
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
        if (report) {
            [IFDBDownloader showNoDataFoundBezel];
        }
        return nil;
    }

    Image __block *img = [IFDBDownloader fetchImageForURL:coverArtURL inContext:context];

    if (img) {
        NSData __block *newData;
        NSData __block *oldData;
        NSManagedObjectID *imgID = img.objectID;

        [context performBlockAndWait:^{
            // Replace img with corresponding Image object in localcontext
            img = [context objectWithID:imgID];

            newData = (NSData *)img.data;
            Metadata *metadata = [context objectWithID:metaID];
            oldData = (NSData *)metadata.cover.data;
        }];

        [IFDBDownloader checkIfUserWants:newData ratherThan:oldData force:force completionHandler:^{
            NSManagedObjectContext *childContext = manager.privateChildManagedObjectContext;
            childContext.undoManager = nil;
            childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
            [childContext performBlockAndWait:^{
                Metadata *blockmeta = [childContext objectWithID:metaID];
                Image *oldCover = blockmeta.cover;
                Image *blockImage = [childContext objectWithID:imgID];
                blockmeta.cover = blockImage;
                [Image deleteIfOrphan:oldCover];
                if (blockImage.imageDescription.length) {
                    blockmeta.coverArtDescription = blockImage.imageDescription;
                } else if (blockmeta.coverArtDescription.length) {
                    blockImage.imageDescription = blockmeta.coverArtDescription;
                }
                [childContext safeSave];
            }];
            [manager saveChanges];
        }];
        return nil;
    }

    NSURL *url = [NSURL URLWithString:coverArtURL];

    if (!url || !([url.scheme isEqualToString:@"http"] || [url.scheme isEqualToString:@"https"])) {
        NSLog(@"Could not create url from %@", coverArtURL);
        if (report) {
            [IFDBDownloader showNoDataFoundBezel];
        }
        return nil;
    }

    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:defaultSession dataTaskURL:url identifier:nil completionHandler:^(NSData * data, NSURLResponse * response, NSError * error, NSString * identifier) {
        if (error) {
            if (!data.length) {
                NSLog(@"Error connecting to url %@: %@", url, [error localizedDescription]);
                if (report) {
                    [IFDBDownloader showNoDataFoundBezel];
                }
                return;
            }
        } else {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
            if (httpResponse.statusCode == 200 && data) {
                NSManagedObjectContext *childContext = manager.privateChildManagedObjectContext;
                childContext.undoManager = nil;
                childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
                NSData __block *oldData;
                [childContext performBlockAndWait:^{
                    Metadata *metadata = [childContext objectWithID:metaID];
                    oldData = (NSData *)metadata.cover.data;
                }];

                [IFDBDownloader checkIfUserWants:data ratherThan:oldData force:force completionHandler:^{
                    NSManagedObjectContext *childChildContext = manager.privateChildManagedObjectContext;
                    childChildContext.undoManager = nil;
                    childChildContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
                    [IFDBDownloader insertImageData:data inMetadataID:metaID context:childChildContext];
                }];
            } else if (report) {
                [IFDBDownloader showNoDataFoundBezel];
            }
        }
    }];

    [queue addOperation:operation];
    return operation;
}

+ (void)checkIfUserWants:(NSData *)newData ratherThan:(NSData *)oldData force:(BOOL)force completionHandler:(nullable void (^)(void))completionHandler {
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

+ (void)insertImageData:(NSData *)data inMetadataID:(NSManagedObjectID *)metaID context:(NSManagedObjectContext *)context {
    if (!data.length) {
        NSLog(@"IFDBDownloader insertImageData called with empty image data");
        return;
    }
    Image __block *img;

    if (!context) {
        NSLog(@"Error! No context!");
        return;
    }

    if ([data isPlaceHolderImage]) {
        [context performBlock:^{
            img = [IFDBDownloader findPlaceHolderInMetadata:metaID imageData:data context:context];
            Metadata *metadata = [context objectWithID:metaID];
            metadata.cover = img;
        }];

        return;
    }

    [context performBlockAndWait:^{
        Metadata *metadata = [context objectWithID:metaID];
        if (!metadata || metadata.coverArtURL.length == 0)
            return;
        img = [IFDBDownloader fetchImageForURL:metadata.coverArtURL inContext:context];
        if (img) {
            if ([((NSData *)img.data) isEqual:data]) {
                // Image with same originalURL already existed in database
                if (img.imageDescription.length) {
                    metadata.coverArtDescription = img.imageDescription;
                } else if (metadata.coverArtDescription.length) {
                    img.imageDescription = metadata.coverArtDescription;
                }
                metadata.cover = img;
                [context safeSave];
            } else {
                img = nil;
            }
        }
    }];

    if (img)
        return;

    [context performBlock:^{
        img = (Image *) [NSEntityDescription
                         insertNewObjectForEntityForName:@"Image"
                         inManagedObjectContext:context];
        img.data = [data copy];
        Metadata *metadata = [context objectWithID:metaID];
        img.originalURL = metadata.coverArtURL;
        img.imageDescription = metadata.coverArtDescription;
        Image *oldCover = metadata.cover;
        metadata.cover = img;
        [Image deleteIfOrphan:oldCover];
        [context safeSave];
    }];
}

+ (Image *)findPlaceHolderInMetadata:(NSManagedObjectID *)metaID imageData:(NSData *)data context:(NSManagedObjectContext *)context {
    Image __block *placeholder;
    [context performBlockAndWait:^{
        placeholder = [IFDBDownloader fetchImageForURL:@"Placeholder" inContext:context];
        if (!placeholder) {
            NSLog(@"IFDBDownLoader insertImageData: Creating new  placeholder Image object in Core Data");
            placeholder = (Image *) [NSEntityDescription
                                     insertNewObjectForEntityForName:@"Image"
                                     inManagedObjectContext:context];
            placeholder.data = [data copy];
            placeholder.originalURL = @"Placeholder";
            placeholder.imageDescription = @"Inform 7 placeholder cover image: The worn spine of an old book, laying open on top of another open book. Caption: \"Interactive fiction by Inform. Photograph: Scot Campbell\".";
            Metadata *metadata = [context objectWithID:metaID];
            metadata.cover = placeholder;
            metadata.coverArtDescription = placeholder.imageDescription;
        }
    }];

    return placeholder;
}

+ (Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context {

    if (imgurl.length == 0)
        return nil;

    NSArray __block *fetchedObjects;

    NSError *error = nil;
    NSFetchRequest *fetchRequest = [Image fetchRequest];

    fetchRequest.includesPropertyValues = NO; //only fetch the managedObjectID
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchImageForURL: Could not execute fetch request! %@",error);
    }

    if (fetchedObjects.count > 1) {
        // Found more than one Image object with the same originalURL string.
        // Delete all but the first one.
        NSMutableArray *objectsToDelete = fetchedObjects.mutableCopy;
        Image *toKeep = fetchedObjects[0];
        [objectsToDelete removeObject:toKeep];
        [context performBlockAndWait:^{
            for (Image *image in objectsToDelete) {
                // Check if data is the same
                if ([(NSData *)image.data isEqual:(NSData *)toKeep.data]) {
                    [toKeep addMetadata:image.metadata];
                    [context deleteObject:image];
                }
            }
        }];
    } else if (fetchedObjects.count == 0) {
        // Found no previously loaded Image object with url imgurl
        return nil;
    }
    return fetchedObjects[0];
}

@end
