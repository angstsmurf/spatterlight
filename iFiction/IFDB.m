//
//  IFDB.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import "IFDB.h"

NSString * const kIFDBXMLNamespace = @"http://ifdb.org/api/xmlns";

static NSString *URLFromCoverArtElement(NSXMLElement *element) {
    for (NSXMLNode *node in element.children) {
        if ([node.name isEqualToString:@"url"])
            return node.stringValue;
    }
    return nil;
}

@implementation IFDB

+ (NSURL *)viewGameURLForTUID:(NSString *)tuid IFID:(NSString *)ifid iFiction:(BOOL)iFiction {
    NSURLQueryItem *key;
    if (tuid.length)
        key = [NSURLQueryItem queryItemWithName:@"id" value:tuid];
    else if (ifid.length)
        key = [NSURLQueryItem queryItemWithName:@"ifid" value:ifid];
    else
        return nil;
    NSURLComponents *components = [NSURLComponents componentsWithString:@"https://ifdb.org/viewgame"];
    components.queryItems = iFiction ? @[ [NSURLQueryItem queryItemWithName:@"ifiction" value:nil], key ] : @[ key ];
    return components.URL;
}

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        for (NSXMLNode *child in element.children) {
            NSString *name = child.name;
            if ([name isEqualToString:@"tuid"]) {
                _tuid = child.stringValue;
            } else if ([name isEqualToString:@"coverart"]) {
                _coverArtURL = URLFromCoverArtElement((NSXMLElement *)child);
            } else if ([name isEqualToString:@"starRating"]) {
                _starRating = child.stringValue;
            }
        }
    }
    return self;
}

@end
