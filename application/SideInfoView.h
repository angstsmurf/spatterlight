//
//  SideInfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

#import "Game.h"
#import "Image.h"
#import "LibController.h"

@class LibController;

@interface SideInfoView : NSView <NSTextFieldDelegate, NSControlTextEditingDelegate>
{
	Game *game;

	LibController *libctl;

	NSTextField *titleField;
	NSTextField *headlineField;
	NSTextField *authorField;
	NSTextField *blurbField;
	NSTextField *ifidField;

}

- (instancetype) initWithFrame:(NSRect)frameRect andIfid:(NSTextField *)ifid andController:(LibController *)sender;

- (NSTextField *) addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView;

- (void) updateSideViewForGame:(Game *)game;

@end
