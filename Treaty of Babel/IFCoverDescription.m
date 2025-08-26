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

@implementation IFCoverDescription

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        for (NSXMLNode *node in element.children) {
            if ([node.name compare:@"description"] == 0) {
                _coverArtDescription = node.stringValue;
            }
        }
    }
    return self;
}

@end
