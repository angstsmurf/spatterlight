//
//  IfidToHashMigrationPolicy.m
//  Spatterlight
//
//  Created by Administrator on 2025-09-23.
//

#import "Ifid.h"
#import "Image.h"
#import "Fetches.h"
#import "Game.h"
#import "Metadata.h"
#import "FolderAccess.h"
#import "NSString+Categories.h"

#import "IfidToHashMigrationPolicy.h"

@implementation IfidToHashMigrationPolicy

+ (void)copyAttributesFrom:(NSManagedObject *)sourceObject to:(NSManagedObject *)destinationObject usingValues:(NSDictionary<NSString *, NSObject *> *)sourceValues andKeys:(NSArray<NSString *> *)destinationKeys {

    for (NSString *key in destinationKeys) {
        NSObject *val = sourceValues[key];
        if (val) {
            [destinationObject setValue:val forKey:key];
        }
    }
}

+ (void)copyAttributesFrom:(NSManagedObject *)sourceObject toMetadata:(Metadata *)destinationMetadata usingValues:(NSDictionary<NSString *, NSObject *> *)sourceValues andKeys:(NSArray<NSString *> *)destinationKeys ifid:(NSString *)sourceIfid {

    [IfidToHashMigrationPolicy copyAttributesFrom:sourceObject to:destinationMetadata usingValues:sourceValues andKeys:destinationKeys];

    NSSet<NSManagedObject *> *ifids = [sourceObject valueForKey:@"ifids"];
    NSManagedObject *cover = [sourceObject valueForKey:@"cover"];

    if (ifids.count) {
        for (NSManagedObject *ifidObj in ifids) {
            Ifid *destinationIfid = [NSEntityDescription insertNewObjectForEntityForName:@"Ifid" inManagedObjectContext:destinationMetadata.managedObjectContext];
            [destinationIfid setValue:[ifidObj valueForKey:@"ifidString"] forKey:@"ifidString"];
            destinationIfid.metadata = destinationMetadata;
        }
    }

    if (cover) {
        NSString *originalURL = [cover valueForKey:@"originalURL"];
        if (originalURL.length && ![destinationMetadata.coverArtURL isEqualToString:originalURL]) {
            destinationMetadata.coverArtURL = originalURL;
        }
    }

    if (destinationMetadata.ifids.count == 0) {
        [destinationMetadata createIfid:sourceIfid];
    }
}

- (BOOL)createDestinationInstancesForSourceInstance:(NSManagedObject *) sInstance
                                       entityMapping:(NSEntityMapping *) mapping
                                             manager:(NSMigrationManager *) manager
                                               error:(NSError * __autoreleasing *) error {

    NSArray<NSString *> *sourceKeys = sInstance.entity.attributesByName.allKeys;
    NSDictionary<NSString *, NSObject *> *sourceValues = [sInstance dictionaryWithValuesForKeys:sourceKeys];

    // Create new instance in target context
    NSManagedObject *destinationInstance = [NSEntityDescription insertNewObjectForEntityForName:mapping.destinationEntityName inManagedObjectContext:manager.destinationContext];
    // Get keys for attributes of entity in target model
    NSArray<NSString *> *destinationKeys = destinationInstance.entity.attributesByName.allKeys;

    // Copy attributes from old model to new
    [IfidToHashMigrationPolicy copyAttributesFrom:sInstance to:destinationInstance usingValues:sourceValues andKeys:destinationKeys];

    // Delete any old hash tags (because we use a new system)
    if ([destinationKeys indexOfObjectIdenticalTo:@"hashTag"] != NSNotFound) {
        if ([mapping.destinationEntityName isEqualToString:@"Game"]) {
            NSString *detectedFormat = [sInstance valueForKey:@"detectedFormat"];
            if (detectedFormat.length && [detectedFormat isEqualToString:@"glulx"]) {
                [destinationInstance setValue:[sInstance valueForKey:@"hashTag"] forKey:@"serialString"];
            }
        }
        [destinationInstance setValue:nil forKey:@"hashTag"];
    }

    // Let the system know which source context instance this target context instance corresponds to
    [manager associateSourceInstance:sInstance withDestinationInstance:destinationInstance forEntityMapping:mapping];

    if ([mapping.destinationEntityName isEqualToString:@"Metadata"]) {
        NSObject *title = sourceValues[@"title"];
        if ([title isEqual:[NSNull null]] || ![title respondsToSelector:NSSelectorFromString(@"title")] || [(NSString *)title length] == 0) {
            [destinationInstance setValue:@"" forKey:@"title"];
        }
        // if this Metadata entity has more than one games entity:
        // loop through remaining games (after the first)
        // and for each, create a new Metadata object,
        // with values copied from the current one.
        NSSet<NSManagedObject *> *games = (NSSet<NSManagedObject *> *)[sInstance valueForKey:@"games"];
        if (games.count) {
            // Note that we create one extra Metadata instance here. We delete it later. This simplifies things.
            for (NSManagedObject *game in games.allObjects) {
                Metadata *destinationMetadata = [NSEntityDescription insertNewObjectForEntityForName:@"Metadata" inManagedObjectContext:manager.destinationContext];

                NSString *ifid = [game valueForKey:@"ifid"];

                [IfidToHashMigrationPolicy copyAttributesFrom:sInstance toMetadata:destinationMetadata usingValues:sourceValues andKeys:destinationKeys ifid:ifid];

                destinationMetadata.hashTag = ifid;
            }
        }
    } else if ([mapping.destinationEntityName isEqualToString:@"Game"]) {
        Game *game = (Game *)destinationInstance;
        if (game.found && !game.hashTag.length) {
            if ([game respondsToSelector:NSSelectorFromString(@"urlForBookmark")])
                [FolderAccess grantAccessToFile:[game urlForBookmark]];
            game.hashTag = [game.path signatureFromFile];
        }
    }

    return YES;
}

- (BOOL)createRelationshipsForDestinationInstance:(NSManagedObject *)dInstance entityMapping:(NSEntityMapping *)mapping manager:(NSMigrationManager *)manager error:(NSError * __autoreleasing *)error {

    NSError *error3 = nil;

    [super createRelationshipsForDestinationInstance:dInstance entityMapping:mapping manager:manager error:&error3];

    NSManagedObjectContext *dContext = manager.destinationContext;

    NSFetchRequest *request =  [NSFetchRequest fetchRequestWithEntityName:@"Metadata"];

    // Create relationships for Game
    if ([dInstance.entity.name isEqualToString:@"Game"]) {
        Game *game = (Game *)dInstance;
        NSError *error2 = nil;
        NSString *ifidStr = game.ifid;
        if (ifidStr.length == 0) {
            NSLog(@"createRelationshipsForDestinationInstance: Error! Game has no ifid?");
        } else {
            request.predicate = [NSPredicate predicateWithFormat:@"hashTag LIKE %@", [dInstance valueForKey:@"ifid"]];
            NSArray<Metadata *> *result = [dContext executeFetchRequest:request error:&error2];
            if (result.count) {
                Metadata *metadata = result.firstObject;
                if (metadata.game) {
                    NSLog(@"createRelationshipsForDestinationInstance: Error! Metadata %@ already has game (%@)!", metadata.title, metadata.game.ifid);
                }
                game.metadata = metadata;
                metadata.hashTag = nil;
            } else {
                NSLog(@"createRelationshipsForDestinationInstance: Found no suitable Metadata instance for game!");
            }
        }
        if (game.metadata == nil) {
            NSLog(@"createRelationshipsForDestinationInstance: Game has no metadata!");
            NSSet<Metadata *> *metas = [Fetches fetchMetadataForIfid:ifidStr inContext:game.managedObjectContext];
            if (metas.count == 0) {
                NSLog(@"createRelationshipsForDestinationInstance: Game has no metadata and found no Ifid with ifidString %@", ifidStr);
            } else {
                for (Metadata *meta in metas) {
                    if (meta.game == nil) {
                        game.metadata = meta;
                        break;
                    }
                }
                if (game.metadata == nil) {
                    NSLog(@"Fail!");
                }
            }
        }
        if (game.metadata && game.metadata.title.length == 0) {
            if (game.path.length) {
                game.metadata.title = [game.path lastPathComponent];
            } else {
                game.metadata.title = @"Untitled";
            }
            NSLog(@"Set title of game to %@", game.metadata.title);
        }
    // Create relationships for Metadata
    } else if ([dInstance.entity.name isEqualToString:@"Metadata"]) {
        Metadata *destinationMeta = (Metadata *)dInstance;
        // Does this ever happen? I suppose the system handles entities alphabetically,
        // so when we get here, these were already taken care of when this method was called
        // with Image.
        if (destinationMeta.cover == nil && destinationMeta.coverArtURL.length) {
            request = [NSFetchRequest fetchRequestWithEntityName:@"Image"];
            request.predicate = [NSPredicate predicateWithFormat:@"originalURL LIKE %@", destinationMeta.coverArtURL];
            NSError *error2 = nil;
            NSArray<Image *> *result = [dContext executeFetchRequest:request error:&error2];
            if (result.count) {
                Image *image = result.firstObject;
                destinationMeta.cover = image;
            }
        }
        // Does this ever happen?
        if (destinationMeta.ifids.count == 0) {
            NSString *ifid = destinationMeta.game.ifid;
            if (ifid.length == 0) {
                ifid = [dInstance valueForKey:@"hashTag"];
            }
            if (ifid.length) {
                request = [Ifid fetchRequest];
                request.predicate = [NSPredicate predicateWithFormat:@"ifidString LIKE %@", ifid];
                NSError *error2 = nil;
                NSArray<NSManagedObject *> *result = [dContext executeFetchRequest:request error:&error2];
                if (result.count) {
                    Ifid *ifidObj = (Ifid *)result.firstObject;
                    [destinationMeta addIfidsObject:ifidObj];
                    ifidObj.metadata = destinationMeta;
                }
            }
        }
        if (destinationMeta.game == nil) {
            request = [Game fetchRequest];
            NSError *error2 = nil;
            NSArray<Game *> *result = nil;
            if (destinationMeta.hashTag.length) {
                request.predicate = [NSPredicate predicateWithFormat:@"ifidString LIKE %@", destinationMeta.hashTag];
                result = [dContext executeFetchRequest:request error:&error2];
            }
            if (!result.count) {
                for (Ifid *ifid in destinationMeta.ifids) {
                    request.predicate = [NSPredicate predicateWithFormat:@"metadata == NIL AND ifid LIKE %@", ifid.ifidString];
                    result = [dContext executeFetchRequest:request error:&error2];
                    if (result.count) {
                        destinationMeta.game = result.firstObject;
                        break;
                    }
                }
            }
        }
    // Create relationships for Image
    } else if ([dInstance.entity.name isEqualToString:@"Image"]) {
        request = [Metadata fetchRequest];
        request.predicate = [NSPredicate predicateWithFormat:@"coverArtURL LIKE %@", [dInstance valueForKey:@"originalURL"]];
        error3 = nil;
        NSArray<Metadata *> *metas = [dContext executeFetchRequest:request error:&error3];
        if (metas.count)
            [(Image *)dInstance addMetadata:[NSSet setWithArray:metas]];
    }
    return YES;
}

- (BOOL)performCustomValidationForEntityMapping:(NSEntityMapping *)mapping manager:(NSMigrationManager *)manager error:(NSError * __autoreleasing *)error {

    NSManagedObjectContext *context = manager.destinationContext;

    // Delete any hidden games
    NSArray *gameEntriesToDelete =
    [Fetches fetchObjects:@"Game" predicate:@"hidden == YES" inContext:context];
    for (Game *game in gameEntriesToDelete) {
        [context deleteObject:game];
    }

    // Does this ever happen anymore?
    NSArray<Game *> *gamesWithoutMetadata =
    [Fetches fetchObjects:@"Game" predicate:@"metadata == NIL" inContext:context];
    if (gamesWithoutMetadata.count)
        NSLog(@"performCustomValidationForEntityMapping: Found %ld Game instances without metadata. Leaving them for now", gamesWithoutMetadata.count);

    // Remove orphaned metadata
    NSArray *metadataEntriesToDelete =
    [Fetches fetchObjects:@"Metadata" predicate:@"game == NIL" inContext:context];

    for (Metadata *meta in metadataEntriesToDelete) {
        [context deleteObject:meta];
    }

    // Remove any orphaned images
    NSArray *imageEntriesToDelete = [Fetches fetchObjects:@"Image" predicate:@"ANY metadata == NIL" inContext:context];

    for (Image *img in imageEntriesToDelete) {
        [context deleteObject:img];
    }
    return YES;
}


@end
