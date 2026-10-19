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
#import "Image.h"
#import "Metadata.h"
#import "NSString+Categories.h"
#import "ReadOnlyStore.h"

@implementation ThumbnailProvider

// App-level categories are only available if their .m file is compiled into
// this extension target. A missing one otherwise shows up as an unrecognized
// selector deep inside thumbnail generation, so check the ones we use at
// launch. If this fires, add the named file to the SpatterlightThumbnailer
// target's Compile Sources build phase.
+ (void)initialize {
    if (self != [ThumbnailProvider class])
        return;

    [ReadOnlyStore checkCategoryMethods:@[
        @[NSString.class, @"signatureFromFile", @"NSString+Categories.m"]
    ] target:@"SpatterlightThumbnailer"];
}

#pragma mark - Core Data stack

@synthesize persistentContainer = _persistentContainer;

- (NSPersistentContainer *)persistentContainer {
    @synchronized (self) {
        if (_persistentContainer == nil) {
            _persistentContainer = [ReadOnlyStore persistentContainerWithLogName:@"SpatterlightThumbnailer"];
        }
    }
    return _persistentContainer;
}

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request completionHandler:(void (^)(QLThumbnailReply * _Nullable, NSError * _Nullable))handler  API_AVAILABLE(macos(10.15)) {

    NSData __block *imgdata = nil;
    NSImage __block *image;

    NSURL __block *url = request.fileURL;
    NSSize __block newImageSize;
    NSSize __block contextSize;

    NSError *error;

    // Blorb parsing doesn't touch Core Data, so keep it outside the
    // context's perform block. A malformed file only means no embedded
    // cover, and we fall back to the database lookup.
    imgdata = [Blorb coverImageDataAtURL:url];

    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;

    if (!context) {
        NSLog(@"context is nil!");
        handler(nil, error);
        return;
    }

    [context performBlockAndWait:^{

        NSError *blockerror = nil;

        if (!imgdata) {

            Game *game = [ReadOnlyStore gameForURL:url inContext:context hashTag:NULL error:&blockerror];
            if (blockerror) {
                handler(nil, blockerror);
                return;
            }
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

        if (@available(macOS 10.15, *)) {
            handler([QLThumbnailReply replyWithContextSize:contextSize currentContextDrawingBlock:^BOOL {

                // draw the image centered
                [image drawInRect:NSMakeRect(0,
                                             0,
                                             newImageSize.width,
                                             newImageSize.height)];

                return YES;
            }], nil);
        }
    }];
}

@end
