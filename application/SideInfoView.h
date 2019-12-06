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
#import "AppDelegate.h"

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
@property (weak) Metadata *metadata;

- (void) updateSideViewForMetadata:(Metadata *)somedata;

@end
