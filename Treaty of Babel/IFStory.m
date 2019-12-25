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
#import "IFDBDownloader.h"

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
                IFDBDownloader *downLoader = [[IFDBDownloader alloc] initWithContext:context];
                [downLoader downloadImageFor:metadata];
            }
        }
    }
    return self;
}

@end
