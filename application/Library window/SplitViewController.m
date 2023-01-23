//
//  SplitViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import "SplitViewController.h"
#import "SideInfoView.h"

@implementation SplitViewController

- (void)showSideBarIfClosed:(NSNotification *)notification {
    if (self.splitViewItems.firstObject.collapsed) {
        [self toggleSidebar:nil];
    }
}


@end
