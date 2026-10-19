//
//  CoverImageStore.h
//  Spatterlight
//
//  Cover image bookkeeping in Core Data: which Image is the cover of a
//  Metadata, sharing Images between games, and deleting orphaned ones.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Image, Metadata;

NS_ASSUME_NONNULL_BEGIN

@interface CoverImageStore : NSObject

//! Makes data the cover image of the metadata, reusing an Image object with
//! the same originalURL and data if there is one
+ (void)insertImageData:(NSData *)data inMetadataID:(NSManagedObjectID *)metaID context:(NSManagedObjectContext *)context;

//! Makes img the cover of metadata, copies the cover description whichever
//! way is needed, and deletes the previous cover if nothing else uses it.
//! Must be called on the queue of their context.
+ (void)setCover:(Image *)img onMetadata:(Metadata *)metadata;

//! The Image downloaded from imgurl, if there is one. Duplicates with
//! identical data are merged into it. Must be called on the queue of context.
+ (nullable Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context;

@end

NS_ASSUME_NONNULL_END
