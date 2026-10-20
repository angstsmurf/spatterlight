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

@property (weak) IBOutlet NSTextField *sideIfid;
@property (weak) IBOutlet NSScrollView *leftScrollView;

@property (nonatomic, weak) NSManagedObjectContext *managedObjectContext;
@property (strong) IBOutlet NSLayoutConstraint *scrollViewTopPin;

@end

NS_ASSUME_NONNULL_END
