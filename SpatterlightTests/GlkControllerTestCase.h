//
//  GlkControllerTestCase.h
//  SpatterlightTests
//
//  A base class for tests that drive a GlkController without an interpreter:
//  an in-memory Core Data store with the Default theme in it, controllers
//  wired the way runTerp wires one minus the interpreter process, and
//  requests fed to them the way the interpreter sends them.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#include "glk.h"
#include "protocol.h"

@class CoreDataManager, GlkController, Theme, Game;

NS_ASSUME_NONNULL_BEGIN

@interface GlkControllerTestCase : XCTestCase

// Always an in-memory store: createDefaultThemeInContext with forceRebuild
// would rewrite the real library's Default theme.
@property (nonatomic, strong, nullable) CoreDataManager *coreDataManager;
@property (nonatomic, strong, nullable) NSManagedObjectContext *context;
// The Default theme, created fresh by setUp.
@property (nonatomic, strong, nullable) Theme *theme;
@property (nonatomic, strong, nullable) Game *game;
// GlkWindow.glkctl and GlkController.game are weak, so the test owns these.
// tearDown cancels any perform requests left on them, their windows and
// their quote boxes.
@property (nonatomic, strong, nullable) NSMutableArray<GlkController *> *controllers;

// Inserts a Game with this ifid and theme, and makes it self.game.
- (Game *)makeGameWithIfid:(NSString *)ifid theme:(Theme *)theme;

// A controller with a theme and empty style hint matrices for both window
// types, and nothing else: no game, no window, not kept in self.controllers.
- (GlkController *)makeBareControllerWithTheme:(Theme *)theme;

// A bare controller with self.game, empty window and event lists, events
// going to /dev/null, and a window (never shown) holding its game view:
// GlkTextGridWindow's setFrame: will not lay out a grid without a screen to
// measure it against. Kept in self.controllers.
- (GlkController *)makeControllerWithTheme:(Theme *)theme;
- (GlkController *)makeController;  // With self.theme.

// Hands a request to the controller, the payload copied into a buffer with
// room for the terminator the request handlers write after it.
- (struct message)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3 data:(nullable NSData *)data;
// The same with the text as UTF-16, returning a1 of the reply.
- (int)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3 text:(nullable NSString *)text;
- (int)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3;

// The interpreter asks for the next event: the end of a turn.
- (void)nextEventIn:(GlkController *)ctl;

@end

NS_ASSUME_NONNULL_END
