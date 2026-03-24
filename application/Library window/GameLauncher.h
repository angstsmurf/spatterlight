//
//  GameLauncher.h
//  Spatterlight
//
//  Extracted from TableViewController.m
//  Handles game launching, session management, and file access.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class Game, GlkController, InfoController, TableViewController;

@interface GameLauncher : NSObject

@property (weak) TableViewController *tableViewController;

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController;

- (nullable NSWindow *)playGame:(Game *)game restorationHandler:(nullable void (^)(NSWindow *, NSError *))completionHandler;
- (nullable NSWindow *)playGameWithHash:(NSString *)hash restorationHandler:(void (^)(NSWindow *, NSError *))completionHandler;

- (nullable NSWindow *)importAndPlayGame:(NSString *)path;
- (NSWindow *)selectAndPlayGame:(Game *)game;

- (nullable Game *)importGame:(NSString *)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (nullable GlkController *)activeGlkController;
- (BOOL)hasActiveGames;

- (void)runCommandsFromFile:(NSString *)filename;
- (void)restoreFromSaveFile:(NSString *)filename;

@end

NS_ASSUME_NONNULL_END
