//
//  IFDBDownloader.m
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import "IFDBDownloader.h"
#import "IFictionMetadata.h"
#import "Metadata.h"
#import "Image.h"
#import "Game.h"
#import "Ifid.h"
#import "NSData+Categories.h"
#import "ImageCompareViewController.h"


@implementation IFDBDownloader

- (instancetype)initWithContext:(NSManagedObjectContext *)context {
    self = [super init];
    if (self) {
        _context = context;
    }
    return self;
}

- (BOOL)downloadMetadataForTUID:(NSString*)tuid {
    if (!tuid || tuid.length == 0)
        return NO;

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.tads.org/viewgame?ifiction&id=" stringByAppendingString:tuid]];
    return [self downloadMetadataFromURL:url];
}

- (BOOL)downloadMetadataForIFID:(NSString*)ifid {
    if (!ifid || ifid.length == 0)
        return NO;

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.tads.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid]];
    return [self downloadMetadataFromURL:url];
}

- (BOOL)downloadMetadataFor:(Game*)game {
    BOOL result = NO;
    if (game.metadata.tuid) {
        result = [self downloadMetadataForTUID:game.metadata.tuid];
    } else {
        result = [self downloadMetadataForIFID:game.ifid];
        if (!result) {
            for (Ifid *ifidObj in game.metadata.ifids) {
                result = [self downloadMetadataForIFID:ifidObj.ifidString];
                if (result)
                    return YES;
            }
        }
    }
    return result;
}

- (BOOL)downloadMetadataFromURL:(NSURL*)url {

    if (!url)
        return NO;

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSURLResponse *response = nil;
    NSError *error = nil;

    NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest returningResponse:&response
                                                     error:&error];
    if (error) {
        if (!data) {
            NSLog(@"Error connecting to url %@: %@", url, [error localizedDescription]);
            return NO;
        }
    }
    else
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode == 200 && data) {

            IFictionMetadata *result = [[IFictionMetadata alloc] initWithData:data andContext:_context];
            if (!result || result.stories.count == 0) {
//                NSLog(@"Could not convert downloaded iFiction XML data to Metadata!");
                return NO;
            }
        } else return NO;
    }
    return YES;
}

- (BOOL)downloadImageFor:(Metadata *)metadata
{
//    NSLog(@"libctl: download image from url %@", metadata.coverArtURL);

    if (!metadata.coverArtURL)
        return NO;

    Image *img = [self fetchImageForURL:metadata.coverArtURL];

    __block BOOL accepted = NO;

    if (img) {

        dispatch_sync(dispatch_get_main_queue(), ^{

            ImageCompareViewController *imageCompare = [[ImageCompareViewController alloc] initWithNibName:@"ImageCompareViewController" bundle:nil];

            if ([imageCompare userWantsImage:(NSData *)img.data ratherThanImage:(NSData *)metadata.cover.data]) {
                metadata.cover = img;
                metadata.coverArtDescription = img.imageDescription;
                accepted = YES;
            }
        });

        return accepted;
    }

    NSURL *url = [NSURL URLWithString:metadata.coverArtURL];

    if (!url) {
        NSLog(@"Could not create url from %@", metadata.coverArtURL);
        return NO;
    }

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSURLResponse *response = nil;
    NSError *error = nil;

    NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest returningResponse:&response
                                                     error:&error];
    if (error) {
        if (!data) {
            NSLog(@"Error connecting to url %@: %@", url, [error localizedDescription]);
            return NO;
        }
    }
    else
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode == 200 && data) {
            dispatch_sync(dispatch_get_main_queue(), ^{
                ImageCompareViewController *imageCompare = [[ImageCompareViewController alloc] initWithNibName:@"ImageCompareViewController" bundle:nil];

                if ([imageCompare userWantsImage:data ratherThanImage:(NSData *)metadata.cover.data]) {
                    [self insertImage:data inMetadata:metadata];
                    accepted = YES;
                }
            });

        }
    }
    return accepted;
}

- (Image *)insertImage:(NSData *)data inMetadata:(Metadata *)metadata {
    if ([data isPlaceHolderImage]) {
        NSLog(@"insertImage: image is placeholder!");
        return [self findPlaceHolderInMetadata:metadata imageData:data];
    }

   Image *img = (Image *) [NSEntityDescription
                     insertNewObjectForEntityForName:@"Image"
                     inManagedObjectContext:metadata.managedObjectContext];
    img.data = [data copy];
    img.originalURL = metadata.coverArtURL;
    metadata.cover = img;
    return img;
}

- (Image *)findPlaceHolderInMetadata:(Metadata *)metadata imageData:(NSData *)data {
    Image *placeholder = [self fetchImageForURL:@"Placeholder"];
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
    return placeholder;
}

- (Image *)fetchImageForURL:(NSString *)imgurl {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Image" inManagedObjectContext:_context];

    fetchRequest.includesPropertyValues = NO; //only fetch the managedObjectID
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];

    fetchedObjects = [_context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

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
