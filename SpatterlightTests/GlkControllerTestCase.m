//
//  GlkControllerTestCase.m
//  SpatterlightTests
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "Theme.h"
#import "Game.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"


@implementation GlkControllerTestCase

- (void)setUp {
    [super setUp];

    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    XCTAssertNotNil(self.context);

    self.theme = [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];
    XCTAssertNotNil(self.theme);

    self.controllers = [NSMutableArray array];
}

- (void)tearDown {
    for (GlkController *ctl in self.controllers) {
        [NSObject cancelPreviousPerformRequestsWithTarget:ctl];
        for (GlkWindow *win in ctl.gwindows.allValues)
            [NSObject cancelPreviousPerformRequestsWithTarget:win];
        for (GlkTextGridWindow *box in ctl.quoteBoxes)
            [NSObject cancelPreviousPerformRequestsWithTarget:box];
    }
    self.controllers = nil;
    self.game = nil;
    self.theme = nil;
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

- (Game *)makeGameWithIfid:(NSString *)ifid theme:(Theme *)theme {
    Game *game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                               inManagedObjectContext:self.context];
    game.ifid = ifid;
    game.theme = theme;
    self.game = game;
    return game;
}

#pragma mark - A controller without an interpreter

- (GlkController *)makeBareControllerWithTheme:(Theme *)theme {
    GlkController *ctl = [[GlkController alloc] init];
    ctl.theme = theme;

    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];
    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
        [nullarray addObject:[NSNull null]];

    ctl.gridStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    ctl.bufferStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSInteger i = 0; i < style_NUMSTYLES; i++) {
        [ctl.gridStyleHints addObject:[nullarray mutableCopy]];
        [ctl.bufferStyleHints addObject:[nullarray mutableCopy]];
    }
    return ctl;
}

- (GlkController *)makeControllerWithTheme:(Theme *)theme {
    GlkController *ctl = [self makeBareControllerWithTheme:theme];
    [ctl setValue:self.game forKey:@"game"];

    ctl.gwindows = [NSMutableDictionary dictionary];
    ctl.windowsToBeAdded = [NSMutableArray array];
    ctl.windowsToBeRemoved = [NSMutableArray array];
    ctl.queue = [NSMutableArray array];
    ctl->sendfh = [NSFileHandle fileHandleWithNullDevice];

    ctl.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600)
                                             styleMask:NSWindowStyleMaskTitled
                                               backing:NSBackingStoreBuffered
                                                 defer:YES];
    XCTAssertNotNil(ctl.window.screen, @"the test needs a screen to lay out text grids");
    ctl.gameView = [[GlkHelperView alloc] initWithFrame:ctl.window.contentView.bounds];
    [ctl.window.contentView addSubview:ctl.gameView];

    [self.controllers addObject:ctl];
    return ctl;
}

- (GlkController *)makeController {
    return [self makeControllerWithTheme:self.theme];
}

#pragma mark - Requests, as the interpreter sends them

- (struct message)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3 data:(NSData *)data {
    struct message request = { .cmd = cmd, .a1 = a1, .a2 = a2, .a3 = a3, .len = data.length };
    struct message reply = { 0 };
    NSMutableData *buffer = [NSMutableData dataWithLength:data.length + sizeof(unichar)];
    if (data.length)
        memcpy(buffer.mutableBytes, data.bytes, data.length);
    [ctl handleRequest:&request reply:&reply buffer:buffer.mutableBytes];
    return reply;
}

- (int)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3 text:(NSString *)text {
    NSMutableData *data = nil;
    if (text.length) {
        data = [NSMutableData dataWithLength:text.length * sizeof(unichar)];
        [text getCharacters:data.mutableBytes range:NSMakeRange(0, text.length)];
    }
    return [self send:cmd to:ctl a1:a1 a2:a2 a3:a3 data:data].a1;
}

- (int)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3 {
    return [self send:cmd to:ctl a1:a1 a2:a2 a3:a3 text:nil];
}

- (void)nextEventIn:(GlkController *)ctl {
    [self send:NEXTEVENT to:ctl a1:1 a2:0 a3:0];
}

@end
