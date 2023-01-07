//
//  SplitViewController.h
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface SplitViewController : NSSplitViewController
@property (weak) IBOutlet NSViewController *leftController;
@property (weak) IBOutlet NSViewController *rightController;

@end

NS_ASSUME_NONNULL_END
