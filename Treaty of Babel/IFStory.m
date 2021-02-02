//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFStory.h"
#import "IFIdentification.h"
#import "IFBibliographic.h"
#import "IFCoverDescription.h"
#import "IFDB.h"
#import "Metadata.h"
#import "Image.h"
#import "IFDBDownloader.h"

@implementation IFStory

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context {
    self = [super init];
    if (self) {
        NSXMLElement *idElement;
        if ([element elementsForName:@"identification"].count) {
            idElement = [element elementsForName:@"identification"][0];
        } else {
            NSLog(@"Unsupported iFiction file!");
            return nil;
        }
        _identification = [[IFIdentification alloc] initWithXMLElement:idElement andContext:context];

        Metadata *metadata = _identification.metadata;

        NSXMLElement *biblioElement = [element elementsForName:@"bibliographic"][0];
        _bibliographic = [[IFBibliographic alloc] initWithXMLElement:biblioElement andMetadata:metadata];

        NSArray *elements = [element elementsForLocalName:@"ifdb"
                                                      URI:@"http://ifdb.tads.org/api/xmlns"];
        if (elements.count > 0) {
            _ifdb = [[IFDB alloc] initWithXMLElement:elements[0] andMetadata:metadata];
            if (metadata.coverArtURL && ![metadata.cover.originalURL isEqualToString:metadata.coverArtURL]) {
                IFDBDownloader *downLoader = [[IFDBDownloader alloc] initWithContext:context];
                [downLoader downloadImageFor:metadata];
            }
        }

        NSArray<NSXMLElement *> *coverElements = [element elementsForName:@"cover"];
        if (coverElements.count)
            _coverDescription = [[IFCoverDescription alloc] initWithXMLElement:coverElements[0] andMetadata:metadata];

        NSArray<NSXMLElement *> *formatElements = [element elementsForName:metadata.format];
        if (formatElements.count) {
            NSLog(@"Found %@ element", metadata.format);
            NSEnumerator *enumChildren = [formatElements[0].children objectEnumerator];
            NSXMLNode *node;
            while ((node = [enumChildren nextObject])) {
                if ([node.name compare:@"coverpicture"] == 0) {
                    NSString *coverArtIndex = node.stringValue;
                    if (coverArtIndex && coverArtIndex.length) {
                        NSLog(@"Found coverArtIndex %@", coverArtIndex);
                    }
                }
            }
        }

    }
    return self;
}

@end
