//
//  GameImporter.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-27.
//

#import <Foundation/Foundation.h>

@class Game, TableViewController;

NS_ASSUME_NONNULL_BEGIN

@interface GameImporter : NSObject

- (instancetype)initWithLibController:(TableViewController *)libController;

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options;
- (void)updateImageFromBlorb:(Blorb *)blorb inGame:(Game *)game;
- (void)lookForImagesForGame:(Game *)game;

@property (weak) TableViewController *libController;

@property NSMutableSet *downloadedMetadata;

@end

NS_ASSUME_NONNULL_END
