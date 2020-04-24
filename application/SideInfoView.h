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

@interface SideInfoView : NSView <NSTextFieldDelegate>
{
    NSBox *topSpacer;
    NSImageView *imageView;
	NSTextField *titleField;
	NSTextField *headlineField;
	NSTextField *authorField;
	NSTextField *blurbField;
	NSTextField *ifidField;

    CGFloat totalHeight;
}

@property (weak) Game *game;
@property (weak) NSString *string;


- (void) updateSideViewWithGame:(Game *)somegame scroll:(BOOL)shouldScroll;
- (void) updateSideViewWithString:(NSString *)string;

@end
