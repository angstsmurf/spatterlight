//
//  ProgressiveMigrator.h
//  Spatterlight
//
//  Step-by-step migration of an old library store through the bundled model
//  versions.
//

#import <CoreData/CoreData.h>

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const ProgressiveMigrationErrorDomain;

@interface ProgressiveMigrator : NSObject

//! Migrates the store at sourceURL up to finalModel one hop at a time,
//! instead of in a single source->destination hop. At each step an explicit
//! (custom) mapping model to any later version wins - this is what lets the
//! IfidToHashMigrationPolicy run when an old store passes through
//! "Spatterlight 15" - and otherwise a mapping straight to the final model is
//! inferred. Each step migrates into a temporary store which then atomically
//! replaces the original, so a failure never corrupts the source store.
//! modelName names the .momd in the main bundle whose versions are the
//! candidates. Returns YES if the store ends up compatible with finalModel
//! (including when there is no store, or it already was), NO on failure.
+ (BOOL)migrateStoreAtURL:(NSURL *)sourceURL
                   ofType:(NSString *)type
                  toModel:(NSManagedObjectModel *)finalModel
                modelName:(NSString *)modelName
                    error:(NSError * __autoreleasing *)error;

@end

NS_ASSUME_NONNULL_END
