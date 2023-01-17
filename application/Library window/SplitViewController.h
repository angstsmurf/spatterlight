//
//  SplitViewController.h
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class Game;

@interface SplitViewController : NSSplitViewController

@property Game *currentSideView;

@end

NS_ASSUME_NONNULL_END
