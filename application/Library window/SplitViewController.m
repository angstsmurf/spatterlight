//
//  SplitViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import "SplitViewController.h"
#import "SideInfoView.h"
#import "SideViewController.h"

@implementation SplitViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(showSideBarIfClosed:)
                                                 name:@"ShowSideBar"
                                               object:nil];
}

- (void)showSideBarIfClosed:(NSNotification *)notification {
    if (self.splitViewItems.firstObject.collapsed) {
        [self toggleSidebar:nil];
    }
}

- (void)toggleSidebar:sender {
    if (self.splitViewItems.firstObject.collapsed) {
        SideViewController *sideViewController = (SideViewController *)self.splitViewItems.firstObject.viewController;
        NSRect frame = sideViewController.view.frame;
        frame.size.width = 200;
        sideViewController.view.frame = frame;
    }
    [super toggleSidebar:sender];
}


@end
