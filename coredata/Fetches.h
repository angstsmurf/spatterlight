//
//  Fetches.h
//  Spatterlight
//
//  Created by Administrator on 2025-09-05.
//

#import <Foundation/Foundation.h>

@class Theme, Game, Metadata;

NS_ASSUME_NONNULL_BEGIN

@interface Fetches : NSObject

+ (nullable Metadata *)fetchMetadataForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context;
+ (nullable Game *)fetchGameForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context;
//! \c predicate is a format string with no arguments (e.g. @"hidden == YES"),
//! never user data: anything with a % in it would be read as a format
//! specifier. Pass nil to fetch every object of the entity.
+ (NSArray *)fetchObjects:(NSString *)entityName predicate:(nullable NSString *)predicate inContext:(NSManagedObjectContext *)context;
+ (nullable NSArray<Game *> *)fetchGamesForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
+ (nullable NSSet<Metadata *> *)fetchMetadataForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
//! Case-insensitive exact match (no wildcards).
+ (nullable Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context;
//! Every Game as a fault (no property values loaded), for setting relationships.
+ (NSArray<Game *> *)fetchAllGamesAsFaultsInContext:(NSManagedObjectContext *)context;
//! The themes the user made (editable == YES).
+ (NSArray<Theme *> *)fetchUserThemesInContext:(NSManagedObjectContext *)context;

@end

NS_ASSUME_NONNULL_END
