//
//  Fetches.h
//  Spatterlight
//
//  Created by Administrator on 2025-09-05.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface Fetches : NSObject

+ (nullable Metadata *)fetchMetadataForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context;
+ (nullable Game *)fetchGameForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context;
+ (NSArray *)fetchObjects:(NSString *)entityName predicate:(nullable NSString *)predicate inContext:(NSManagedObjectContext *)context;
+ (nullable NSArray<Game *> *)fetchGamesForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
+ (nullable NSSet<Metadata *> *)fetchMetadataForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
+ (nullable Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context;

@end

NS_ASSUME_NONNULL_END
