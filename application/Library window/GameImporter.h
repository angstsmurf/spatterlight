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

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (void)addFiles:(NSArray*)urls options:(NSDictionary *)options;

@property (weak) LibController *libController;

@end

NS_ASSUME_NONNULL_END
