//
//  CoverImageStore.m
//  Spatterlight
//
//  Cover image bookkeeping in Core Data: which Image is the cover of a
//  Metadata, sharing Images between games, and deleting orphaned ones.
//

#import "CoverImageStore.h"
#import "Image.h"
#import "Metadata.h"
#import "NSData+Categories.h"
#import "NSManagedObjectContext+safeSave.h"

@implementation CoverImageStore

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
        NSLog(@"CoverImageStore insertImageData called with empty image data");
        return;
    }

    if (!context) {
        NSLog(@"Error! No context!");
        return;
    }

    if ([data isPlaceHolderImage]) {
        [context performBlock:^{
            Image *placeholder = [CoverImageStore placeholderImageWithData:data inContext:context];
            [CoverImageStore setCover:placeholder onMetadata:[context objectWithID:metaID]];
            [context safeSaveAndWait];
        }];
        return;
    }

    BOOL __block reused = NO;

    [context performBlockAndWait:^{
        Metadata *metadata = [context objectWithID:metaID];
        if (!metadata || metadata.coverArtURL.length == 0)
            return;
        Image *img = [CoverImageStore fetchImageForURL:metadata.coverArtURL inContext:context];
        if (img && [((NSData *)img.data) isEqual:data]) {
            // Image with same originalURL and data already existed in database
            [CoverImageStore setCover:img onMetadata:metadata];
            [context safeSaveAndWait];
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
        [CoverImageStore setCover:img onMetadata:metadata];
        [context safeSaveAndWait];
    }];
}

// The shared Image for the Inform 7 placeholder cover, created on first use.
// Must be called on the queue of context.
+ (Image *)placeholderImageWithData:(NSData *)data inContext:(NSManagedObjectContext *)context {
    Image *placeholder = [CoverImageStore fetchImageForURL:kImagePlaceholderOriginalURL inContext:context];
    if (!placeholder) {
        NSLog(@"CoverImageStore: Creating new placeholder Image object in Core Data");
        placeholder = (Image *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Image"
                                 inManagedObjectContext:context];
        placeholder.data = [data copy];
        placeholder.originalURL = kImagePlaceholderOriginalURL;
        placeholder.imageDescription = kImagePlaceholderDescription;
    }
    return placeholder;
}

+ (Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context {

    if (imgurl.length == 0)
        return nil;

    NSError *error = nil;
    NSFetchRequest *fetchRequest = [Image fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL ==[c] %@", imgurl];

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
