//
//  IFIdentification.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFIdentification.h"
#import "Metadata.h"
#import "Ifid.h"

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

// Add new Ifid objects to a Metadata object
+ (void)addIfids:(NSSet<NSString *> *)ifids toMetadata:(Metadata *)metadata {
    if (metadata == nil || metadata.managedObjectContext == nil) {
        return;
    }

    NSMutableSet<NSString *> *existing = [[metadata.ifids valueForKey:@"ifidString"] mutableCopy];

    for (NSString *ifid in ifids) {
        // Placeholders used by the info panel, never real IFIDs
        if ([ifid isEqualToString:@"DUMMYmanySelected"] || [ifid isEqualToString:@"DUMMYnoneSelected"])
            continue;
        if ([existing containsObject:ifid])
            continue;
        [existing addObject:ifid];

        // Create a new Ifid object. The library can contain any number of Ifid objects with identical ifidStrings,
        // as long as they are attached to different Metadata objects.
        Ifid *ifidObj = (Ifid *) [NSEntityDescription
                                  insertNewObjectForEntityForName:@"Ifid"
                                  inManagedObjectContext:metadata.managedObjectContext];
        ifidObj.ifidString = ifid;
        ifidObj.metadata = metadata;
    }
}

- (void)addInfoToMetadata:(Metadata *)metadata {
    if (_bafn)
        metadata.bafn = _bafn;
    if (_ifids.count) {
        [IFIdentification addIfids:_ifids toMetadata:metadata];
    }
}

@end
