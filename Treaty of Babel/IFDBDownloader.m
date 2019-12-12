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


@implementation IFDBDownloader

- (instancetype)initWithContext:(NSManagedObjectContext *)context {
    self = [super init];
    if (self) {
        _context = context;
    }
    return self;
}

- (BOOL)downloadMetadataForIFID:(NSString*)ifid {
    NSLog(@"libctl: downloadMetadataForIFID %@", ifid);

    if (!ifid)
        return NO;

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.tads.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid]];

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSURLResponse *response = nil;
    NSError *error = nil;

    NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest returningResponse:&response
                                                     error:&error];
    if (error) {
        if (!data) {
            NSLog(@"Error connecting: %@", [error localizedDescription]);
            return NO;
        }
    }
    else
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode == 200 && data) {
//            cursrc = kIfdb;
//            currentIfid = ifid;
            IFictionMetadata *result = [[IFictionMetadata alloc] initWithData:data andContext:_context];
            if (!result) {
                NSLog(@"downloadMetadataForIFID: Could not convert downloaded iFiction XML data to Metadata!");
                return NO;
            }
//            currentIfid = nil;
//            cursrc = 0;
        } else return NO;
    }
    return YES;
}

- (BOOL)downloadImageFor:(Metadata *)metadata
{
    NSLog(@"libctl: download image from url %@", metadata.coverArtURL);

    Image *img = [self fetchImageForURL:metadata.coverArtURL];

    if (img) {
        metadata.cover = img;
        return YES;
    }

    NSURL *url = [NSURL URLWithString:metadata.coverArtURL];

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSURLResponse *response = nil;
    NSError *error = nil;

    NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest returningResponse:&response
                                                     error:&error];
    if (error) {
        if (!data) {
            NSLog(@"Error connecting: %@", [error localizedDescription]);
            return NO;
        }
    }
    else
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode == 200 && data) {
            img = (Image *) [NSEntityDescription
                                    insertNewObjectForEntityForName:@"Image"
                                    inManagedObjectContext:_context];
            img.data = [data copy];
            img.originalURL = metadata.coverArtURL;
            metadata.cover = img;
        } else return NO;
    }
    return YES;
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

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one Image with originalURL %@",imgurl);
    }
    else if (fetchedObjects.count == 0)
    {
        NSLog(@"fetchImageForURL: Found no Image object with url %@", imgurl);
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

@end
