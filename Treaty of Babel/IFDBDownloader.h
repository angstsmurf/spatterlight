//
//  IFDBDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Image, DownloadOperation, TableViewController;

NS_ASSUME_NONNULL_BEGIN

@interface IFDBDownloader : NSObject

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController;
- (nullable NSOperation *)downloadMetadataForGames:(NSArray<NSManagedObjectID *> *)games inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure completionHandler:(nullable void (^)(void))completionHandler;
- (nullable NSOperation *)downloadImageFor:(NSManagedObjectID *)metaID inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force reportFailure:(BOOL)report;
+ (nullable Image *)fetchImageForURL:(NSString *)imgurl inContext:(NSManagedObjectContext *)context;
+ (void)insertImageData:(NSData *)data inMetadataID:(NSManagedObjectID *)metadata context:(NSManagedObjectContext *)context;

@property (weak) TableViewController *tableViewController;

@end

NS_ASSUME_NONNULL_END
