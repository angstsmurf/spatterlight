//
//  IFDBDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata, Image, Game, DownloadOperation;

NS_ASSUME_NONNULL_BEGIN

@interface IFDBDownloader : NSObject

- (instancetype)initWithContext:(NSManagedObjectContext *)context;
- (nullable NSOperation *)downloadMetadataForGames:(NSArray<Game *> *)games onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure completionHandler:(nullable void (^)(void))completionHandler;
- (void)downloadImageFor:(Metadata *)metadata onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force;
+ (nullable Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context;
+ (void)insertImageData:(NSData *)data inMetadata:(Metadata *)metadata;

@property (strong) NSManagedObjectContext *context;

@end

NS_ASSUME_NONNULL_END
