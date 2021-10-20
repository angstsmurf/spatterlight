//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif


#import "IFStory.h"
#import "IFIdentification.h"
#import "IFZoomID.h"
#import "IFBibliographic.h"
#import "IFCoverDescription.h"
#import "IFDB.h"
#import "Metadata.h"
#import "Image.h"
#import "IFDBDownloader.h"

@implementation IFStory

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context andQueue:(NSOperationQueue *)queue {
    self = [super init];
    if (self) {
        NSXMLElement *idElement;
        if ([element elementsForName:@"identification"].count) {
            idElement = [element elementsForName:@"identification"][0];
            _identification = [[IFIdentification alloc] initWithXMLElement:idElement andContext:context];
        } else if ([element elementsForName:@"id"].count) {
            NSArray<NSXMLElement *> *elements = [element elementsForName:@"id"];
            _identification = [[IFZoomID alloc] initWithElements:elements andContext:context];
        } else {
            NSLog(@"Unsupported iFiction file!");
            return nil;
        }

        Metadata *metadata = _identification.metadata;

        if (!metadata)
            return nil;

        NSArray *biblioElements = [element elementsForName:@"bibliographic"];
        NSXMLElement *biblioElement;
        if (biblioElements.count == 0) {
            if ([_identification isKindOfClass:[IFZoomID class]]) {
                biblioElement = element;
            } else {
                biblioElement = nil;
            }
        } else {
            biblioElement = biblioElements.firstObject;
        }
        _bibliographic = [[IFBibliographic alloc] initWithXMLElement:biblioElement andMetadata:metadata];

        NSArray *elements = [element elementsForLocalName:@"ifdb"
                                                      URI:@"http://ifdb.org/api/xmlns"];
        if (elements.count > 0) {
            _ifdb = [[IFDB alloc] initWithXMLElement:elements[0] andMetadata:metadata];
            if (metadata.coverArtURL && ![metadata.cover.originalURL isEqualToString:metadata.coverArtURL]) {
                IFDBDownloader *downLoader = [[IFDBDownloader alloc] initWithContext:context];
                [downLoader downloadImageFor:metadata onQueue:queue forceDialog:NO];
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
