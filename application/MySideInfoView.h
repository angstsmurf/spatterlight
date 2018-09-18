//
//  MySideInfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

#import "Game.h"
#import "Image+CoreDataProperties.h"

@interface MySideInfoView : NSView

- (NSTextField *) addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView;

- (void) updateSideViewForGame:(Game *)game;

@property (strong) Game *game;


@end
