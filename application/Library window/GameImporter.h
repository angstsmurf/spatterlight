//
//  GameImporter.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-27.
//

#import <Foundation/Foundation.h>

@class Game, TableViewController, Blorb;
@class NSManagedObjectContext;

NS_ASSUME_NONNULL_BEGIN

@interface GameImporter : NSObject

- (instancetype)initWithTableViewController:(TableViewController *)libController;

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options;
- (void)updateImageFromBlorb:(Blorb *)blorb inGame:(Game *)game;
- (void)lookForImagesForGame:(Game *)game;

// One-time migration: re-point AGT games that were imported the old way (stored
// as a converted Converted/<IFID>.agx in the app container) at their original
// .D$$ file, when it can be located in a folder we still have access to. Games
// whose original .D$$ can't be found are left on the AGX path (still playable).
- (void)migrateConvertedAGTGamesInContext:(NSManagedObjectContext *)context;

@property (weak) TableViewController *tableViewController;


@end

NS_ASSUME_NONNULL_END
