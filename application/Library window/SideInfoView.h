//
//  SideInfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

@class Game;

@interface SideInfoView : NSView <NSDraggingDestination>

@property (weak) Game *game;
@property (weak) NSString *string;
@property NSButton *downloadButton;

@property BOOL animatingScroll;

- (instancetype)initWithFrame:(NSRect)frameRect;
- (void)updateSideViewWithGame:(Game *)somegame;
- (void)updateSideViewWithString:(NSString *)string;
- (void)deselectImage;
- (void)updateTitle;

@end
