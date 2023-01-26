//
//  SplitViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import "SplitViewController.h"
#import "SideInfoView.h"

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


@end
