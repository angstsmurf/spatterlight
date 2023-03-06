//
//  ThumbnailProvider.m
//  SpatterlightThumbnails
//
//  Created by Administrator on 2020-12-29.
//

#import "ThumbnailProvider.h"
#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>
#import <BlorbFramework/Blorb.h>

#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#include "babel_handler.h"

@implementation ThumbnailProvider

#pragma mark - Core Data stack

@synthesize persistentContainer = _persistentContainer;

- (NSPersistentContainer *)persistentContainer {
    @synchronized (self) {
        if (_persistentContainer == nil) {
            _persistentContainer = [[NSPersistentContainer alloc] initWithName:@"Spatterlight"];

            NSString *groupIdentifier =
            [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

            NSURL *url = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];

            url = [url URLByAppendingPathComponent:@"Spatterlight.storedata"];

            NSPersistentStoreDescription *description = [[NSPersistentStoreDescription alloc] initWithURL:url];

            description.readOnly = YES;
            description.shouldMigrateStoreAutomatically = NO;

            _persistentContainer.persistentStoreDescriptions = @[ description ];

            NSLog(@"persistentContainer url path:%@", url.path);

            [_persistentContainer loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *aDescription, NSError *error) {
                if (error != nil) {
                    NSLog(@"Failed to load Core Data stack: %@", error);
                    abort();
                }
            }];
        }
    }
    return _persistentContainer;
}

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request completionHandler:(void (^)(QLThumbnailReply * _Nullable, NSError * _Nullable))handler  API_AVAILABLE(macos(10.15)) {

    // There are three ways to provide a thumbnail through a QLThumbnailReply. Only one of them should be used.

    NSData __block *imgdata = nil;
    NSImage __block *image;

    NSURL __block *url = request.fileURL;
    NSSize __block newImageSize;
    NSSize __block contextSize;


    NSError *error;

    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;

    if (!context) {
        NSLog(@"context is nil!");
        handler(nil, error);
        return;
    }

    [context performBlockAndWait:^{

        NSError *blockerror = nil;
        NSArray *fetchedObjects;

        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path like[c] %@", url.path];

        fetchedObjects = [context executeFetchRequest:fetchRequest error:&blockerror];
        if (fetchedObjects == nil) {
            NSLog(@"ThumbnailProvider: %@",blockerror);
            handler(nil, blockerror);
            return;
        }

        if (!fetchedObjects.count) {
            NSString *ifid = [self ifidFromFile:url.path];
            if (ifid.length) {
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@", ifid];
                fetchedObjects = [context executeFetchRequest:fetchRequest error:&blockerror];
            }
        }

        if ([Blorb isBlorbURL:url]) {
            Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
            imgdata = [blorb coverImageData];
        }

        if (fetchedObjects.count) {
            Game *game = fetchedObjects[0];
            if (!imgdata)
                imgdata = (NSData *)game.metadata.cover.data;
        }

        if (!imgdata || imgdata.length == 0) {
            handler(nil, blockerror);
            return;
        }

        image = [[NSImage alloc] initWithData:imgdata];
        NSSize maximumSize = request.maximumSize;
        NSSize imageSize = [image size];

        // calculate `newImageSize` and `contextSize` such that the image fits perfectly and respects the constraints
        newImageSize = maximumSize;
        contextSize = maximumSize;
        CGFloat aspectRatio = imageSize.height / imageSize.width;
        CGFloat proposedHeight = aspectRatio * maximumSize.width;

        if (proposedHeight <= maximumSize.height) {
            newImageSize.height = proposedHeight;
            contextSize.height = MAX(floor(proposedHeight), request.minimumSize.height);
        } else {
            newImageSize.width = maximumSize.height / aspectRatio;
            contextSize.width = MAX(floor(newImageSize.width), request.minimumSize.width);
        }
        if (isnan(newImageSize.width))
            newImageSize.width = maximumSize.width;
        if (isnan(contextSize.width))
            contextSize.width = maximumSize.width;

        // First way: Draw the thumbnail into the current context, set up with AppKit's coordinate system.
        if (@available(macOS 10.15, *)) {
            handler([QLThumbnailReply replyWithContextSize:contextSize currentContextDrawingBlock:^BOOL {
                // Draw the thumbnail here.

                // draw the image centered
                [image drawInRect:NSMakeRect(0,
                                             0,
                                             newImageSize.width,
                                             newImageSize.height)];

                // Return YES if the thumbnail was successfully drawn inside this block.
                return YES;
            }], nil);
        }
    }];
    /*

     // Second way: Draw the thumbnail into a context passed to your block, set up with Core Graphics's coordinate system.
     handler([QLThumbnailReply replyWithContextSize:request.maximumSize drawingBlock:^BOOL(CGContextRef  _Nonnull context) {
     // Draw the thumbnail here.

     // Return YES if the thumbnail was successfully drawn inside this block.
     return YES;
     }], nil);

     // Third way: Set an image file URL.
     handler([QLThumbnailReply replyWithImageFileURL:[NSBundle.mainBundle URLForResource:@"fileThumbnail" withExtension:@"jpg"]], nil);

     */
}

- (NSString *)ifidFromFile:(NSString *)path {
    void *context = get_babel_ctx();
    char *format = babel_init_ctx((char*)path.UTF8String, context);
    if (!format || !babel_get_authoritative_ctx(context))
    {
        babel_release_ctx(context);
        free(context);
        return nil;
    }

    char buf[TREATY_MINIMUM_EXTENT];

    int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, context);
    if (rv <= 0)
    {
        babel_release_ctx(context);
        free(context);
        return nil;
    }

    babel_release_ctx(context);
    free(context);
    return @(buf);
}

@end
