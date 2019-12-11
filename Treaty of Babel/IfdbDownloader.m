//
//  IfdbDownloader.m
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import "IfdbDownloader.h"
#import "IFictionMetadata.h"
#import "Metadata.h"
#import "Image.h"


@implementation IfdbDownloader

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
            [[IFictionMetadata alloc] initWithData:data andContext:_context];
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

    fetchRequest.entity = [NSEntityDescription entityForName:@"Image" inManagedObjectContext:_context];;
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];

    fetchedObjects = [_context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one entry with url %@",imgurl);
    }
    else if (fetchedObjects.count == 0)
    {
        NSLog(@"fetchMetadataForIFID: Found no Image object with url %@", imgurl);
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

@end
