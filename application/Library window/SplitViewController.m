//
//  SplitViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import "SplitViewController.h"

@interface SplitViewController ()

@end

@implementation SplitViewController

- (void) viewDidLoad {
    [super viewDidLoad];
    self.splitView.wantsLayer = YES; // I think this is required if you use a left sidebar with vibrancy (which I do  below). Otherwise appkit complains and forces the use of CA layers anyway
    NSSplitViewItem *left =[NSSplitViewItem sidebarWithViewController:_leftController];
    [self addSplitViewItem:left];


    NSSplitViewItem *right =[NSSplitViewItem splitViewItemWithViewController:_rightController];
    right.minimumThickness = 420;
    [self addSplitViewItem:right];
}

@end
