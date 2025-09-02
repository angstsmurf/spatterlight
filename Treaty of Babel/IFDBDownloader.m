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
#import "NSData+Categories.h"
#import "ImageCompareViewController.h"
#import "IFDB.h"
#import "NotificationBezel.h"
#import "AppDelegate.h"
#import "LibController.h"
#import "TableViewController.h"
#import "DownloadOperation.h"
#import "NSManagedObjectContext+safeSave.h"
#import "ComparisonOperation.h"
#import "NSData+Categories.h"


@interface IFDBDownloader () <NSURLSessionDelegate> {
    NSURLSession *defaultSession;
    NSURLSessionDataTask *dataTask;
}

@end

@implementation IFDBDownloader

- (instancetype)init {
    self = [super init];
    if (self) {
        defaultSession = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration] delegate:nil delegateQueue:nil];
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

    [context performBlockAndWait:^{
        IFDBDownloader *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        void (^internalHandler)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable,  NSString * _Nullable) = ^void(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error, NSString * _Nullable identifier) {

            if (error) {
                if (!data) {
                    [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                        NSLog(@"Error connecting: %@", [error localizedDescription]);
                        if (reportFailure)
                            [IFDBDownloader showNoDataFoundBezel];
                    }];
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

                        if (imageOnly) {
                            NSString *coverArtUrl = [IFDBDownloader coverArtUrlFromXMLData:data];
                            if (coverArtUrl.length) {
                                metadata.coverArtURL = coverArtUrl;
                                NSOperation *op = [strongSelf downloadImageFor:objectID inContext:context onQueue:queue forceDialog:NO];
                                if (op) {
                                    TableViewController *libController = ((AppDelegate*)NSApp.delegate).tableViewController;
                                    libController.lastImageDownloadOperation = op;
                                }

                                success = YES;
                            }
                        } else {
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
                                [story addInfoToMetadata:metadata];
                                if (metadata.coverArtURL && ![metadata.cover.originalURL isEqualToString:metadata.coverArtURL]) {
                                    [strongSelf downloadImageFor:objectID inContext:context onQueue:queue forceDialog:NO];
                                }
                                result = nil;
                                story = nil;
                                metadata = nil;
                                success = YES;
                            }
                        }

                        if (!success && reportFailure) {
                            [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                                [IFDBDownloader showNoDataFoundBezel];
                            }];
                        }
                        [context safeSave];
                    }];
                } else if (reportFailure) {
                    [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                        [IFDBDownloader showNoDataFoundBezel];
                    }];
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
            [queue addOperation:finisher];
            lastoperation = finisher;
        }
    }];

    if (lastoperation == nil)
        NSLog(@"Error! downloadMetadataForGames lastoperation == nil!");
    return lastoperation;
}

+ (void)showNoDataFoundBezel {
    dispatch_async(dispatch_get_main_queue(), ^{
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:NSScreen.screens.firstObject];
        [bezel showStandardWithText:@"? No data found"];
    });
}

+ (NSString *)coverArtUrlFromXMLData:(NSData *)data {
    NSString *result = @"";
    NSError *error = nil;
    NSXMLDocument *xml =
    [[NSXMLDocument alloc] initWithData:data
                                options:NSXMLDocumentTidyXML
                                  error:&error];
    NSXMLElement *root = xml.rootElement;
    NSXMLElement *namespace = [NSXMLElement namespaceWithName:@"ns" stringValue:@"http://ifdb.org/api/xmlns"];
    [root addNamespace:namespace];
    NSArray *urls = [root nodesForXPath:@"//ns:url" error:&error];
    if (urls.count) {
        NSXMLElement *url = urls.firstObject;
        result = url.stringValue;
    }
    return result;
}

- (NSOperation *)downloadImageFor:(NSManagedObjectID *)metaID inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force {

    NSString __block *coverArtURL = nil;
    BOOL __block giveup = NO;

    [context performBlockAndWait:^{
        Metadata *metadata = [context objectWithID:metaID];
        coverArtURL = metadata.coverArtURL;
        if (!coverArtURL.length)
            giveup = YES;
    }];

    if (giveup) {
        NSLog(@"IFDBDownloader downloadImageFor: found no coverArtURL!");
        return nil;
    }

    Image __block *img = [IFDBDownloader fetchImageForURL:coverArtURL inContext:context];

    if (img) {
        NSData __block *newData;
        NSData __block *oldData;

        [context performBlockAndWait:^{
            // Replace img with corresponding Image object in localcontext
            img = [context objectWithID:img.objectID];

            newData = (NSData *)img.data;
            Metadata *metadata = [context objectWithID:metaID];
            oldData = (NSData *)metadata.cover.data;
        }];

        [IFDBDownloader checkIfUserWants:newData ratherThan:oldData force:force completionHandler:^{
            [context performBlock:^{
                Metadata *metadata = [context objectWithID:metaID];
                Image *oldCover = metadata.cover;
                metadata.cover = img;
                [Image deleteIfOrphan:oldCover];
                metadata.coverArtDescription = img.imageDescription;
                [context safeSave];
            }];
        }];

        return nil;
    }

    NSURL *url = [NSURL URLWithString:coverArtURL];

    if (!url || !([url.scheme isEqualToString:@"http"] || [url.scheme isEqualToString:@"https"])) {
        NSLog(@"Could not create url from %@", coverArtURL);
        return nil;
    }

    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:defaultSession dataTaskURL:url identifier:nil completionHandler:^(NSData * data, NSURLResponse * response, NSError * error, NSString * identifier) {
        if (error) {
            if (!data.length) {
                NSLog(@"Error connecting to url %@: %@", url, [error localizedDescription]);
                return;
            }
        } else {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
            if (httpResponse.statusCode == 200 && data) {

                NSData __block *oldData;
                [context performBlockAndWait:^{
                    Metadata *metadata = [context objectWithID:metaID];
                    oldData = (NSData *)metadata.cover.data;
                }];

                [IFDBDownloader checkIfUserWants:data ratherThan:oldData force:force completionHandler:^{
                    [IFDBDownloader insertImageData:data inMetadataID:metaID context:context];
                }];
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
        img = [IFDBDownloader fetchImageForURL:metadata.coverArtURL inContext:context];
        if (img && [((NSData *)img.data).sha256String isEqualToString:data.sha256String]) {
            // Image with same originalURL already existed in database
            if (img.imageDescription.length) {
                metadata.coverArtDescription = img.imageDescription;
            } else if (metadata.coverArtDescription.length) {
                img.imageDescription = metadata.coverArtDescription;
            }
            metadata.cover = img;
            [context safeSave];
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
    NSArray __block *fetchedObjects;
    
    [context performBlockAndWait:^{
        NSError *error = nil;
        NSFetchRequest *fetchRequest = [Image fetchRequest];
        
        fetchRequest.includesPropertyValues = NO; //only fetch the managedObjectID
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];
        
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects == nil) {
            NSLog(@"fetchImageForURL: Could not execute fetch request! %@",error);
        }
    }];
    
    if (fetchedObjects.count > 1) {
        // Found more than one Image object with the same originalURL string.
        // Delete all but the first one.
        NSMutableArray *objectsToDelete = fetchedObjects.mutableCopy;
        Image *toKeep = fetchedObjects[0];
        [objectsToDelete removeObject:toKeep];
        for (Image *image in objectsToDelete) {
            // Check if data is the same
            if ([((NSData *)image.data).sha256String isEqualToString:((NSData *)toKeep.data).sha256String]) {
                [toKeep addMetadata:image.metadata];
                [context deleteObject:image];
            }
        }
    } else if (fetchedObjects.count == 0) {
        // Found no previously loaded Image object with url imgurl
        return nil;
    }
    return fetchedObjects[0];
}

@end
