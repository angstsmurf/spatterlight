//
//  SideViewController.h
//  Spatterlight
//
//  Created by Administrator on 2023-01-16.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class Game;

@interface SideViewController : NSViewController

@property (weak) IBOutlet NSLayoutConstraint *widthConstraint;

@property (weak) IBOutlet NSTextField *sideIfid;
@property (weak) IBOutlet NSScrollView *leftScrollView;

@property (weak) Game *currentSideView;
@property (nonatomic, weak) NSManagedObjectContext *managedObjectContext;
@property (weak) IBOutlet NSClipView *clipView;

@end

NS_ASSUME_NONNULL_END
