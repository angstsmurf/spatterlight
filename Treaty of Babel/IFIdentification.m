//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import "IFIdentification.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Game.h"

@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)aContext {
    self = [super init];
    if (self) {
        _context = aContext;
        NSMutableArray *ifids = [[NSMutableArray alloc] init];

        _format = @"";

        for (NSXMLNode *node in element.children) {
            if ([node.name compare:@"ifid"] == 0)
                [ifids addObject:node.stringValue];
            else if ([node.name compare:@"format"] == 0) {
                _format = node.stringValue;
            } else if ([node.name compare:@"bafn"] == 0)
                _bafn = node.stringValue;
        }
        if (ifids.count == 0) {
            NSLog(@"IFIdentification: no Ifids found in document! Bailing!");
            return nil;
        }
        _ifids = ifids;
        _metadata = [self metadataFromIfids:ifids];
        
        if (!_metadata.format)
            _metadata.format = _format;
        _metadata.bafn = _bafn;
    }
    return self;
}

+ (Ifid *)fetchIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;
    NSPredicate *predicate;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Ifid" inManagedObjectContext:context];

    fetchRequest.entity = entity;

    predicate = [NSPredicate predicateWithFormat:@"ifidString like[c] %@",ifid];
    fetchRequest.predicate = predicate;

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchMetadataForIFID: Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one Ifid with ifidString %@",ifid);
    }
    else if (fetchedObjects.count == 0)
    {
        return nil;
    }

    return fetchedObjects[0];
}

- (Metadata *)metadataFromIfids:(NSArray<NSString *> *)ifids {
    Ifid *ifidObj;
    Metadata *metadata;
    NSMutableSet *games = [[NSMutableSet alloc] init];
    NSMutableSet *ifidObjs = [[NSMutableSet alloc] init];

    for (NSString *ifid in ifids) {
        if ([ifid isEqualToString:@"DUMMYmanySelected"]  || [ifid isEqualToString:@"DUMMYnoneSelected"]) {
            continue;
        }
        ifidObj = [IFIdentification fetchIfid:ifid inContext:self.context];
        if (!ifidObj) {
            ifidObj = (Ifid *) [NSEntityDescription
                                insertNewObjectForEntityForName:@"Ifid"
                                inManagedObjectContext:self.context];
            ifidObj.ifidString = ifid;
        }

        [ifidObjs addObject:ifidObj];
        if (!metadata) {
            metadata = ifidObj.metadata;
        } else if (ifidObj.metadata && ifidObj.metadata != metadata) {
            NSLog(@"Competing metadata objects found!");
            metadata = [IFIdentification selectBestMetadataOf:metadata and:ifidObj.metadata];
            if (metadata != ifidObj.metadata) {
                Metadata *leftover = ifidObj.metadata;
                ifidObj.metadata = metadata;
                if (leftover.ifids.count == 0) {
                    [games unionSet:leftover.games];
                    ////                    NSLog(@"Deleting leftover metadata object with title %@", leftover.title);
                    ////                    [self.context deleteObject:leftover];
                }
            }
        }
        [games unionSet:ifidObj.metadata.games];
    }

    if (!metadata) {
        metadata = (Metadata *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Metadata"
                                 inManagedObjectContext:self.context];
    }

    [metadata addIfids:[NSSet setWithSet:ifidObjs]];
    [metadata addGames:[NSSet setWithSet:games]];

    return metadata;
}

+ (Metadata *)selectBestMetadataOf:(Metadata *)metadataA and:(Metadata *)metadataB {
    if (!metadataA)
        return metadataB;
    if (!metadataB)
        return metadataA;
    if (metadataB.games.count > metadataA.games.count) {
        return metadataB;
    }
    if (metadataB.games.count == metadataA.games.count) {
        if (metadataB.ifids.count > metadataA.ifids.count) {
            return metadataB;
        }
    }

    return metadataA;
}

@end
