//
//  SplitViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-05.
//

#import "SplitViewController.h"
#import "SideInfoView.h"

#import "Game.h"
#import "Metadata.h"
#import "SideViewController.h"
#import "TableViewController.h"

#define RIGHT_VIEW_MIN_WIDTH 350.0
#define PREFERRED_LEFT_VIEW_MIN_WIDTH 200.0
#define ACTUAL_LEFT_VIEW_MIN_WIDTH 50.0

@interface SplitViewController () {
//    NSView *leftView;
//    NSView *rightView;
//    NSScrollView *leftScrollView;
//    NSArray<Game *> *selectedGames;
    NSTextField *sideIfid;
}

@end

@implementation SplitViewController

- (void)viewDidLoad {
    [super viewDidLoad];
//    leftView = self.splitViewItems.firstObject.viewController.view;
//    rightView = self.splitViewItems.lastObject.viewController.view;
    if (@available(macOS 11.0, *)) {
        self.splitViewItems.firstObject.allowsFullHeightLayout = YES;
//        self.view.frame = self.view.window.contentView.frame;
//        self.splitView.frame = self.view.window.contentView.frame;
    }

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(showSideBarIfClosed:)
                                                 name:@"ShowSideBar"
                                               object:nil];

}

//- (void)windowDidResignKey:(NSNotification *)notification {
//    if ([leftScrollView.documentView isKindOfClass:[SideInfoView class]]) {
//        SideInfoView *sideView = (SideInfoView *)leftScrollView.documentView;
//        [sideView deselectImage];
//    }
//}
//
//- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMaximumPosition ofSubviewAt:(NSInteger)dividerIndex
//{
//    CGFloat result = NSWidth(splitView.frame) / 2;
//    if (result < RIGHT_VIEW_MIN_WIDTH)
//        result = NSWidth(splitView.frame) - RIGHT_VIEW_MIN_WIDTH;
//
//    return result;
//}
//
//- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
//{
//    CGFloat result = PREFERRED_LEFT_VIEW_MIN_WIDTH;
//
//    if (NSWidth(splitView.frame) - PREFERRED_LEFT_VIEW_MIN_WIDTH < RIGHT_VIEW_MIN_WIDTH)
//        result = ACTUAL_LEFT_VIEW_MIN_WIDTH;
//
//    return result;
//}
////
////-(void)collapseLeftView
////{
////    lastSideviewWidth = leftView.frame.size.width;
////    lastSideviewPercentage = lastSideviewWidth / self.window.frame.size.width;
////
////    _leftView.hidden = YES;
////    [_splitView setPosition:0
////           ofDividerAtIndex:0];
////    NSSize minSize = self.window.minSize;
////    minSize.width =  RIGHT_VIEW_MIN_WIDTH;
////    self.window.minSize = minSize;
////    _leftViewConstraint.priority = NSLayoutPriorityDefaultLow;
////    [self.splitView display];
////    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"ShowSidebar"];
////}
////
////- (NSSize)windowWillResize:(NSWindow *)sender
////                    toSize:(NSSize)frameSize {
////    if (![_splitView isSubviewCollapsed:_leftView] &&
////        (_leftView.frame.size.width < PREFERRED_LEFT_VIEW_MIN_WIDTH - 1
////         || _rightView.frame.size.width < RIGHT_VIEW_MIN_WIDTH - 1)) {
////        [self collapseLeftView];
////    }
////
////    return frameSize;
////}
//
//-(void)uncollapseLeftView
//{
////    lastSideviewWidth = lastSideviewPercentage *  self.window.frame.size.width;
////    _leftViewConstraint.priority = 999;
////
////    leftView.hidden = NO;
////
////    CGFloat dividerThickness = _splitView.dividerThickness;
////
////    // make sideview at least PREFERRED_LEFT_VIEW_MIN_WIDTH
////    if (lastSideviewWidth < PREFERRED_LEFT_VIEW_MIN_WIDTH)
////        lastSideviewWidth = PREFERRED_LEFT_VIEW_MIN_WIDTH;
////
////    if (self.window.frame.size.width < PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + dividerThickness) {
////        [self.window setContentSize:NSMakeSize(PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + dividerThickness, ((NSView *)self.window.contentView).frame.size.height)];
////    }
////
////    NSSize minSize = self.window.minSize;
////    minSize.width = RIGHT_VIEW_MIN_WIDTH + PREFERRED_LEFT_VIEW_MIN_WIDTH + dividerThickness;
////    if (minSize.width > self.window.frame.size.width)
////        minSize.width  = self.window.frame.size.width;
////    self.window.minSize = minSize;
////
////    [_splitView setPosition:lastSideviewWidth
////           ofDividerAtIndex:0];
////
////    [_splitView display];
////    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"ShowSidebar"];
//}

- (void)splitViewWillResizeSubviews:(NSNotification *)notification {

}

//- (BOOL)splitView:(NSSplitView *)splitView shouldAdjustSizeOfSubview:(NSView *)view {
//    if (NSWidth(leftView.frame) >  NSWidth(self.view.window.frame)) {
//        return NO;
//    }
//    return YES;
//}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
//    if (NSWidth(self.splitView.frame) < ACTUAL_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH && ![self.splitView isSubviewCollapsed:leftView]) {
//        NSRect frame = self.view.window.frame;
//        frame.size.width = PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + 10;
//        [self.splitView setPosition:PREFERRED_LEFT_VIEW_MIN_WIDTH - 10
//               ofDividerAtIndex:0];
////        [self.view.window setFrame:frame display:NO animate:YES];
//    }
}

- (void)showSideBarIfClosed:(NSNotification *)notification {
    if (self.splitViewItems.firstObject.collapsed) {
        [self toggleSidebar:nil];
    }
}


//- (CGFloat)splitView:(NSSplitView *)splitView
//constrainMaxCoordinate:(CGFloat)proposedMaximumPosition
//         ofSubviewAt:(NSInteger)dividerIndex {
//    CGFloat maxWidth = NSWidth(self.view.window.frame);
//    if (maxWidth == 0)
//        maxWidth = 300;
//    if (proposedMaximumPosition > maxWidth)
//        return maxWidth;
//    return proposedMaximumPosition;
//}


@end
