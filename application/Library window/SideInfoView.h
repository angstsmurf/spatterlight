//
//  SideInfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

@class Game, ImageView;

@interface SideInfoView : NSView <NSDraggingDestination>

@property (weak) Game *game;

@property (readonly) ImageView *imageView;

- (instancetype)initWithFrame:(NSRect)frameRect;
- (void)updateSideViewWithGame:(Game *)somegame;
- (void)updateSideViewWithString:(NSString *)string;


@end
