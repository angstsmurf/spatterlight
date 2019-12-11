//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFStory.h"
#import "IFIdentification.h"
#import "IFBibliographic.h"
#import "IFDB.h"
#import "Metadata.h"
#import "Image.h"

@implementation IFStory

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context {
  self = [super init];
  if (self) {
      NSXMLElement *idElement = [[element elementsForName:@"identification"] objectAtIndex:0];
    _identification = [[IFIdentification alloc] initWithXMLElement:idElement andContext:context];

      Metadata *metadata = _identification.metadata;
    NSXMLElement *biblioElement = [[element elementsForName:@"bibliographic"] objectAtIndex:0];
    _bibliographic = [[IFBibliographic alloc] initWithXMLElement:biblioElement andMetadata:metadata];

    NSArray *elements = [element elementsForLocalName:@"ifdb"
                                         URI:@"http://ifdb.tads.org/api/xmlns"];
    if (elements.count > 0) {
      _ifdb = [[IFDB alloc] initWithXMLElement:[elements objectAtIndex:0] andMetadata:metadata];
        if (metadata.coverArtURL && ![metadata.cover.originalURL isEqualToString:metadata.coverArtURL]) {
            metadata.cover = [self fetchImageForURL:metadata.coverArtURL inContext:context];
            if (!metadata.cover)
                [self downloadImageFor:metadata inContext:context];
        }
    }
  }
  return self;
}

- (Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Image" inManagedObjectContext:context];;
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"originalURL like[c] %@",imgurl];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
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

- (BOOL) downloadImageFor:(Metadata *)metadata inContext:(NSManagedObjectContext *)context
{
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
            Image *img = (Image *) [NSEntityDescription
                                    insertNewObjectForEntityForName:@"Image"
                                    inManagedObjectContext:context];
            img.data = [data copy];
            img.originalURL = metadata.coverArtURL;
            metadata.cover = img;
        } else return NO;
    }
    return YES;
}

@end
