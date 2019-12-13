//
//  SideInfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

@class Game;
@class Preferences;

@interface SideInfoView : NSView <NSTextFieldDelegate> //, NSControlTextEditingDelegate>
{
	NSTextField *titleField;
	NSTextField *headlineField;
	NSTextField *authorField;
	NSTextField *blurbField;
	NSTextField *ifidField;

}

//@property (weak) LibController *libctl;
@property (weak) Game *game;
@property (weak) NSString *string;


- (void) updateSideViewWithGame:(Game *)somegame;

- (void) updateSideViewWithString:(NSString *)string;

@end
