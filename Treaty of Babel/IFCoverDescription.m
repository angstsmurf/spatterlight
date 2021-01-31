//
//  IFCoverDescription.m
//  Spatterlight
//
//  Created by Administrator on 2021-01-31.
//

#import "IFCoverDescription.h"
#import "Metadata.h"
#import "Image.h"
#import "NSString+Categories.h"
#import "main.h"

@implementation IFCoverDescription

- (instancetype)initWithXMLElement:(NSXMLElement *)element andMetadata:(Metadata *)metadata {
    self = [super init];
    if (self) {
        NSEnumerator *enumChildren = [element.children objectEnumerator];
        NSXMLNode *node;
        while ((node = [enumChildren nextObject])) {
            if ([node.name compare:@"description"] == 0) {
                NSString *description = node.objectValue;
                if (description.length) {
                    metadata.coverArtDescription = description;
                    if (metadata.cover != nil)
                        metadata.cover.imageDescription = description;
                }
            }
        }
    }
    return self;
}

@end
