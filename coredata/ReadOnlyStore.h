//
//  ReadOnlyStore.h
//  Spatterlight
//
//  Shared by the Quick Look and thumbnail extensions, which read the app's
//  library store but never write to it.
//

#import <CoreData/CoreData.h>

@class Game;

NS_ASSUME_NONNULL_BEGIN

@interface ReadOnlyStore : NSObject

// A container for the library store in the app group container, opened
// read-only and without migration. logName names the extension in log lines.
+ (NSPersistentContainer *)persistentContainerWithLogName:(NSString *)logName;

// The first Game matching predicate, logging a failed fetch (and setting
// *error) or more than one match. Must be called on the queue of context.
+ (nullable Game *)gameMatching:(NSPredicate *)predicate
                      inContext:(NSManagedObjectContext *)context
                          error:(NSError * _Nullable * _Nullable)error;

// The Game whose path is url's or, failing that, whose hashTag is the file's
// signature. The signature is returned in hashTag when it was computed.
// Returns nil and sets *error if the path fetch fails. Must be called on the
// queue of context.
+ (nullable Game *)gameForURL:(NSURL *)url
                    inContext:(NSManagedObjectContext *)context
                      hashTag:(NSString * _Nullable * _Nullable)hashTag
                        error:(NSError * _Nullable * _Nullable)error;

// App-level categories only exist in an extension if their .m file is in its
// Compile Sources phase; a missing one shows up as an unrecognized selector
// deep inside a preview. Each entry is @[class, selector name, file name].
// Logs and asserts for any instance method that is missing.
+ (void)checkCategoryMethods:(NSArray<NSArray *> *)entries target:(NSString *)targetName;

@end

NS_ASSUME_NONNULL_END
