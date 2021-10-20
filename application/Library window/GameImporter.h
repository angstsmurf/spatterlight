//
//  GameImporter.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-27.
//

#import <Foundation/Foundation.h>

@class Game, LibController;

NS_ASSUME_NONNULL_BEGIN

@interface GameImporter : NSObject

- (instancetype)initWithLibController:(LibController *)libController;

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options;

@property (weak) LibController *libController;

@property NSMutableSet *downloadedMetadata;

@end

NS_ASSUME_NONNULL_END
