//
//  SideInfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

@class Game;

@interface SideInfoView : NSView <NSTextFieldDelegate>

@property (weak) Game *game;
@property (weak) NSString *string;


- (void) updateSideViewWithGame:(Game *)somegame scroll:(BOOL)shouldScroll;
- (void) updateSideViewWithString:(NSString *)string;

@end
