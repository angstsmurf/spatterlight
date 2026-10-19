//
//  IFDBDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class TableViewController;

NS_ASSUME_NONNULL_BEGIN

@interface IFDBDownloader : NSObject

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController;
// Must be called on the queue of context. The downloaded records are added
// to the games in context on its queue later, with performBlock:
- (nullable NSOperation *)downloadMetadataForGames:(NSArray<NSManagedObjectID *> *)games inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue imageOnly:(BOOL)imageOnly reportFailure:(BOOL)reportFailure completionHandler:(nullable void (^)(void))completionHandler;
// Can be called from any queue: reads the cover URL with
// performBlockAndWait: on the queue of context
- (nullable NSOperation *)downloadImageFor:(NSManagedObjectID *)metaID inContext:(NSManagedObjectContext *)context onQueue:(NSOperationQueue *)queue forceDialog:(BOOL)force reportFailure:(BOOL)report;

@property (weak) TableViewController *tableViewController;

@end

NS_ASSUME_NONNULL_END
