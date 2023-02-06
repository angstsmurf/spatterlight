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


@interface IFDBDownloader () <NSURLSessionDelegate> {
    NSURLSession *defaultSession;
    NSURLSessionDataTask *dataTask;
    NSOperation *lastImageDownloadOperation;
}

@property NSString *coverArtUrl;

@end

@implementation IFDBDownloader

- (instancetype)initWithContext:(NSManagedObjectContext *)context {
    self = [super init];
    if (self) {
        _context = context;
        defaultSession = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration] delegate:nil delegateQueue:nil];

        lastImageDownloadOperation = nil;
    }
    return self;
}

- (void)URLSession:(NSURLSession *)session
didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler {
//    NSLog(@"URLSession:didReceiveChallenge:");

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

//            NSLog(@"secresult: %u", secresult);

            switch (secresult) {
                case kSecTrustResultUnspecified: // The OS trusts this certificate implicitly.
//                    NSLog(@"kSecTrustResultUnspecified");
                case kSecTrustResultRecoverableTrustFailure:
//                    NSLog(@"kSecTrustResultRecoverableTrustFailure");
                case kSecTrustResultProceed: // The user explicitly told the OS to trust it.
                {
//                    NSLog(@"kSecTrustResultProceed");
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

+ (nullable DownloadOperation *)operationForIFID:(NSString*)ifid session:(NSURLSession *)session completionHandler:(void (^)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable))handler {
    if (!ifid || ifid.length == 0) {
        return nil;
    }
    
    NSURL *url = [NSURL URLWithString:[@"https://ifdb.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid]];
    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:session dataTaskURL:url completionHandler:handler];
    
    return operation;
}

+ (nullable DownloadOperation *)operationForTUID:(NSString*)tuid session:(NSURLSession *)session completionHandler:(void (^)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable))handler {
    if (tuid.length == 0) {
        return nil;
    }
    
    NSURL *url = [NSURL URLWithString:[@"https://ifdb.org/viewgame?ifiction&id=" stringByAppendingString:tuid]];
    ;
    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:session dataTaskURL:url completionHandler:handler];
    
    return operation;
}

- (nullable NSOperation *)downloadMetadataForGames:(NSArray<Game *> *)games onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure completionHandler:(nullable void (^)(void))completionHandler {
    if (!games.count)
        return nil;
    
    NSManagedObjectContext *localContext = games.firstObject.managedObjectContext;

    NSOperation __block *lastoperation = nil;
    
    IFDBDownloader __weak *weakSelf = self;
    
    [localContext performBlockAndWait:^{
        IFDBDownloader *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        void (^internalHandler)(NSData * _Nullable,  NSURLResponse * _Nullable,  NSError * _Nullable) = ^void(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
            
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
                    [localContext performBlock:^{
                        BOOL success = NO;
                        
                        if (imageOnly) {
                            strongSelf.coverArtUrl = [IFDBDownloader coverArtUrlFromXMLData:data];
                            if (strongSelf.coverArtUrl.length) {
                                success = YES;
                            }
                        } else {
                            
                            IFictionMetadata *result = [[IFictionMetadata alloc] initWithData:data andContext:localContext andQueue:queue];
                            
                            if (!result || result.stories.count == 0) {
//                                NSLog(@"No metadata found!");
                            } else {
//                                NSLog(@"Downloaded metadata successfully!");
                                success = YES;
                            }
                        }
                        [localContext safeSave];

                        if (!success && reportFailure) {
                            [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                                [IFDBDownloader showNoDataFoundBezel];
                            }];
                        }
                    }];
                } else if (reportFailure) {
                    [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                        [IFDBDownloader showNoDataFoundBezel];
                    }];
                }
            }
        };
        
        NSMutableSet *downloadedMetadata = [NSMutableSet new];
        
        for (Game *game in games) {
            if (game.metadata == nil || [downloadedMetadata containsObject:game.metadata])
                continue;
            [downloadedMetadata addObject:game.metadata];
            DownloadOperation *operation;
            if (game.metadata.tuid.length) {
                operation = [IFDBDownloader operationForTUID:game.metadata.tuid session:defaultSession completionHandler:internalHandler];
                [queue addOperation:operation];
                lastoperation = operation;
            } else {
                for (Ifid *ifid in game.metadata.ifids) {
                    operation = [IFDBDownloader operationForIFID:ifid.ifidString session:defaultSession completionHandler:internalHandler];
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

    if (lastImageDownloadOperation)
        lastoperation = lastImageDownloadOperation;
    lastImageDownloadOperation = nil;
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

- (void)downloadImageFor:(Metadata *)metadata onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force {
    NSManagedObjectContext *localcontext = metadata.managedObjectContext;

    NSString __block *coverArtURL = _coverArtUrl;
    BOOL __block giveup = NO;
    
    if (!coverArtURL.length) {
        [localcontext performBlockAndWait:^{
            coverArtURL = metadata.coverArtURL;
            if (!coverArtURL.length)
                giveup = YES;
        }];
    }
    
    if (giveup)
        return;
    
    Image __block *img = [IFDBDownloader fetchImageForURL:coverArtURL inContext:localcontext];
    
    if (img) {
        NSData __block *newData;
        NSData __block *oldData;
        
        [localcontext performBlockAndWait:^{
            // Replace img with corresponding Image object in localcontext
            img = [localcontext objectWithID:img.objectID];
            
            newData = (NSData *)img.data;
            oldData = (NSData *)metadata.cover.data;
        }];
        
        [self checkIfUserWants:newData ratherThan:oldData force:force completionHandler:^{
            [localcontext performBlock:^{
                metadata.cover = img;
                metadata.coverArtDescription = img.imageDescription;
                [localcontext safeSave];
            }];
        }];
        
        return;
    }
    
    NSURL *url = [NSURL URLWithString:coverArtURL];
    
    if (!url || !([url.scheme isEqualToString:@"http"] || [url.scheme isEqualToString:@"https"])) {
        NSLog(@"Could not create url from %@", coverArtURL);
        return;
    }

    IFDBDownloader __weak *weakSelf = self;

    DownloadOperation *operation = [[DownloadOperation alloc] initWithSession:defaultSession dataTaskURL:url completionHandler:^(NSData * data, NSURLResponse * response, NSError * error) {
        IFDBDownloader *strongSelf = weakSelf;
        if (!strongSelf)
            strongSelf = [[IFDBDownloader alloc] initWithContext:localcontext];
        if (error) {
            if (!data) {
                NSLog(@"Error connecting to url %@: %@", url, [error localizedDescription]);
                return;
            }
        }
        else
        {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
            if (httpResponse.statusCode == 200 && data) {
                
                NSData __block *oldData;
                [localcontext performBlockAndWait:^{
                    oldData = (NSData *)metadata.cover.data;
                }];
                
                [strongSelf checkIfUserWants:data ratherThan:oldData force:force completionHandler:^{
                    [IFDBDownloader insertImageData:data inMetadata:metadata];
                }];
            }
        }
    }];
    
    [queue addOperation:operation];
    lastImageDownloadOperation = operation;
}

- (void)checkIfUserWants:(NSData *)newData ratherThan:(NSData *)oldData force:(BOOL)force completionHandler:(nullable void (^)(void))completionHandler {
    kImageComparisonResult comparisonResult = [ImageCompareViewController chooseImageA:newData orB:oldData source:kImageComparisonDownloaded force:force];
    
    if (comparisonResult == kImageComparisonResultA) {
        completionHandler();
    } else if (comparisonResult == kImageComparisonResultWantsUserInput) {
        dispatch_async(dispatch_get_main_queue(), ^{
            TableViewController *libController = ((AppDelegate*)NSApplication.sharedApplication.delegate).libctl.tableViewController;
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

+ (void)insertImageData:(NSData *)data inMetadata:(Metadata *)metadata {
    if (!data) {
        NSLog(@"No data");
        return;
    }
    Image __block *img;
    NSManagedObjectContext *localcontext = metadata.managedObjectContext;

    if (!localcontext) {
        NSLog(@"Error! No context!");
        return;
    }

    if ([data isPlaceHolderImage]) {
        [localcontext performBlock:^{
            img = [IFDBDownloader findPlaceHolderInMetadata:metadata imageData:data];
            metadata.cover = img;
        }];

        return;
    }
    
    [localcontext performBlock:^{
        img = (Image *) [NSEntityDescription
                         insertNewObjectForEntityForName:@"Image"
                         inManagedObjectContext:localcontext];
        img.data = [data copy];
        img.originalURL = metadata.coverArtURL;
        img.imageDescription = metadata.coverArtDescription;
        metadata.cover = img;
        [localcontext safeSave];
    }];
}

+ (Image *)findPlaceHolderInMetadata:(Metadata *)metadata imageData:(NSData *)data {
//    NSLog(@"findPlaceHolderInMetadata");
    Image __block *placeholder;
    NSManagedObjectContext *context = metadata.managedObjectContext;
    [context performBlockAndWait:^{
        placeholder = [IFDBDownloader fetchImageForURL:@"Placeholder" inContext:context];
        if (!placeholder) {
            placeholder = (Image *) [NSEntityDescription
                                     insertNewObjectForEntityForName:@"Image"
                                     inManagedObjectContext:metadata.managedObjectContext];
            placeholder.data = [data copy];
            placeholder.originalURL = @"Placeholder";
            placeholder.imageDescription = @"Inform 7 placeholder cover image: The worn spine of an old book, laying open on top of another open book. Caption: \"Interactive fiction by Inform. Photograph: Scot Campbell\".";
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
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        
        fetchRequest.entity = [NSEntityDescription entityForName:@"Image" inManagedObjectContext:context];
        
        fetchRequest.includesPropertyValues = NO; //only fetch the managedObjectID
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];
        
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects == nil) {
            NSLog(@"Problem! %@",error);
        }
    }];
    
    if (fetchedObjects.count > 1) {
        NSLog(@"Found more than one Image with originalURL %@",imgurl);
    }
    else if (fetchedObjects.count == 0) {
        // Found no previously loaded Image object with url imgurl
        return nil;
    }
    return fetchedObjects[0];
}

@end
