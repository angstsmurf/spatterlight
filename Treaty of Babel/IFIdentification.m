//
//  IFIdentification.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import "IFIdentification.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Game.h"

@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        NSMutableSet *ifids = [[NSMutableSet alloc] init];

        _format = @"";

        for (NSXMLNode *node in element.children) {
            if ([node.name compare:@"ifid"] == 0) {
                [ifids addObject:node.stringValue];
            } else if ([node.name compare:@"format"] == 0) {
                _format = node.stringValue;
            } else if ([node.name compare:@"bafn"] == 0) {
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
+ (void)addIfids:(NSSet<NSString *> *)ifids toMetadata:(Metadata *)metadata{
    Ifid *ifidObj;

    if (metadata == nil || metadata.managedObjectContext == nil) {
        return;
    }


    NSMutableSet *ifidsToAdd = [NSMutableSet setWithCapacity:ifids.count];

    for (NSString *ifid in ifids) {
        if ([ifid isEqualToString:@"DUMMYmanySelected"]  || [ifid isEqualToString:@"DUMMYnoneSelected"]) {
            continue;
        }
        BOOL skip = NO;
        for (Ifid *existing in metadata.ifids) {
            if ([existing.ifidString isEqualToString:ifid]) {
                skip = YES;
                break;
            }
        }
        if (skip)
            continue;
        [ifidsToAdd addObject:ifid];
    }

    for (NSString *ifid in ifidsToAdd) {
        // Create a new Ifid object. The library can contain any number of Ifid objects with identical ifidStrings,
        // as long as they are attached to different Metadata objects.
        ifidObj = (Ifid *) [NSEntityDescription
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
