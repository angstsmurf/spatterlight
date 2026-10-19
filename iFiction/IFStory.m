//
//  IFStory.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFStory.h"
#import "IFIdentification.h"
#import "IFBibliographic.h"
#import "IFCoverDescription.h"
#import "IFDB.h"

@implementation IFStory

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        NSXMLElement *idElement = [element elementsForName:@"identification"].firstObject;
        NSArray<NSXMLElement *> *zoomIdElements = [element elementsForName:@"id"];
        // The legacy Zoom format has no <bibliographic> element either:
        // the bibliographic tags are children of the story element itself.
        BOOL legacyZoomFormat = NO;

        if (idElement) {
            // Nil if there is no <ifid>. We still keep the bibliographic
            // data, which callers that already know the game can use.
            _identification = [[IFIdentification alloc] initWithXMLElement:idElement];
        } else if (zoomIdElements.count) {
            _identification = [[IFIdentification alloc] initWithZoomIDElements:zoomIdElements];
            legacyZoomFormat = YES;
        } else {
            NSLog(@"Unsupported iFiction file!");
            return nil;
        }

        NSXMLElement *biblioElement = [element elementsForName:@"bibliographic"].firstObject;
        if (!biblioElement && legacyZoomFormat)
            biblioElement = element;

        if (biblioElement) {
            _bibliographic = [[IFBibliographic alloc] initWithXMLElement:biblioElement];
        } else {
            NSLog(@"IFStory initWithXMLElement: No bibliographic tag found!");
        }

        NSXMLElement *ifdbElement = [element elementsForLocalName:@"ifdb" URI:kIFDBXMLNamespace].firstObject;
        if (ifdbElement)
            _ifdb = [[IFDB alloc] initWithXMLElement:ifdbElement];

        NSXMLElement *coverElement = [element elementsForName:@"cover"].firstObject;
        if (coverElement)
            _coverDescription = [[IFCoverDescription alloc] initWithXMLElement:coverElement];
    }
    return self;
}

@end
