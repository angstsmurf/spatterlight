//
//  IFIdentification.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFIdentification.h"

@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        NSMutableSet<NSString *> *ifids = [[NSMutableSet alloc] init];

        _format = @"";

        for (NSXMLNode *node in element.children) {
            NSString *name = node.name;
            if ([name isEqualToString:@"ifid"]) {
                [ifids addObject:node.stringValue];
            } else if ([name isEqualToString:@"format"]) {
                _format = node.stringValue;
            } else if ([name isEqualToString:@"bafn"]) {
                _bafn = node.stringValue;
            }
        }
        if (ifids.count == 0) {
            NSLog(@"IFIdentification: no Ifids found in document! Bailing!");
            return nil;
        }
        _ifids = ifids;
    }
    return self;
}

@end
