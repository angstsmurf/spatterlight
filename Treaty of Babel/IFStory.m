//
//  IFStory.m
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
#import "constants.h"

@implementation IFStory

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        NSXMLElement *idElement;
        if ([element elementsForName:@"identification"].count) {
            idElement = [element elementsForName:@"identification"][0];
            _identification = [[IFIdentification alloc] initWithXMLElement:idElement];
        } else if ([element elementsForName:@"id"].count) {
            NSArray<NSXMLElement *> *elements = [element elementsForName:@"id"];
            _identification = [[IFZoomID alloc] initWithElements:elements];
        } else {
            NSLog(@"Unsupported iFiction file!");
            return nil;
        }

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

        if (biblioElement) {
            _bibliographic = [[IFBibliographic alloc] initWithXMLElement:biblioElement];
        } else {
            NSLog(@"IFStory initWithXMLElement: No bibliographic tag found!");
        }

        NSArray *elements = [element elementsForLocalName:@"ifdb"
                                                      URI:@"http://ifdb.org/api/xmlns"];
        if (elements.count > 0) {
            _ifdb = [[IFDB alloc] initWithXMLElement:elements[0]];
        }

        NSArray<NSXMLElement *> *coverElements = [element elementsForName:@"cover"];
        if (coverElements.count)
            _coverDescription = [[IFCoverDescription alloc] initWithXMLElement:coverElements[0]];

        // The metadata in blorbs sometimes contain tags corresponding to the game
        // format (e.g. <zcode> for a Z-code game.)
        // The code below simply looks for these tags and prints them and their values
        // for informational (debug?) purposes.

//        if (gFormatMap.count) {
//            for (NSString *key in gFormatMap.allKeys) {
//                NSArray<NSXMLElement *> *formatElements = [element elementsForName:key];
//                if (formatElements.count) {
//                    NSLog(@"Found <%@> element", key);
//                    NSEnumerator *enumChildren = [formatElements[0].children objectEnumerator];
//                    NSXMLNode *node;
//                    while ((node = [enumChildren nextObject])) {
//                        NSLog(@"Found %@ : %@ under the \"%@\" element", node.name, node.stringValue, key);
//                    }
//                }
//            }
//        }
    }
    return self;
}

- (void)addInfoToMetadata:(Metadata * _Nonnull)metadata {
    if (_identification) {
        [_identification addInfoToMetadata:metadata];
    }
    if (_bibliographic) {
        [_bibliographic addInfoToMetadata:metadata];
    }
    if (_ifdb) {
        [_ifdb addInfoToMetadata:metadata];
    }
    if (_coverDescription && _coverDescription.coverArtDescription.length)
        metadata.coverArtDescription = _coverDescription.coverArtDescription;
}

@end
