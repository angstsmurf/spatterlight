//
//  SideViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-16.
//
#import <QuartzCore/QuartzCore.h>

#import "SideViewController.h"
#import "Game.h"
#import "Metadata.h"
#import "SideInfoView.h"
#import "AppDelegate.h"

@interface SideViewController () {
    BOOL sideViewUpdatePending;
    BOOL needsUpdate;
}

@end

@implementation SideViewController

- (void)viewDidLoad {

    needsUpdate = YES;
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:self.managedObjectContext];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(updateSideView:)
                                                 name:@"UpdateSideView"
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(splitViewDidResizeSubviews:)
                                                 name:NSSplitViewDidResizeSubviewsNotification
                                               object:nil];
//    [_leftScrollView addFloatingSubview:[self gradientView] forAxis:NSEventGestureAxisVertical];
}

- (NSView *)gradientView {
    CAGradientLayer *gradientLayer = [CAGradientLayer layer];
    gradientLayer.zPosition = 10;

    CGFloat gradientHeight = 100;


    NSRect gradFrame = NSMakeRect(0, 0, NSWidth(self.view.frame), gradientHeight);
    gradientLayer.frame = gradFrame;


    NSColor *color = [NSColor windowBackgroundColor];

    gradientLayer.colors = @[
        (id)[[color colorWithAlphaComponent:0.0] CGColor],
        (id)[color CGColor]
    ];

    NSView *gradientView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(self.view.frame), gradientHeight)];

    gradientView.layer = gradientLayer;

    return gradientView;
}


- (void)splitViewDidResizeSubviews:(NSNotification *)notification {
    Metadata *meta = _currentSideView.metadata;
    NSNotification *currentGame = [NSNotification notificationWithName:@"UpdateSideView" object:(_currentSideView == nil) ? nil: @[_currentSideView]];
    if (!sideViewUpdatePending) {
        if (meta.blurb.length == 0 && meta.author.length == 0 && meta.headline.length == 0 && meta.cover == nil) {
            [self updateSideView:currentGame];

            if ([_leftScrollView.documentView isKindOfClass:[SideInfoView class]]) {
                SideInfoView *sideView = (SideInfoView *)_leftScrollView.documentView;
                [sideView deselectImage];
                [sideView updateTitle];
            }
        } else {
            sideViewUpdatePending = YES;
            needsUpdate = YES;
            double delayInSeconds = 0.3;
            dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
            SideViewController __weak *weakSelf = self;
            dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
                SideViewController *strongSelf = weakSelf;
                if (strongSelf && strongSelf->sideViewUpdatePending) {
                    [strongSelf updateSideView:currentGame];
                }
            });
        }
    }
}


- (NSManagedObjectContext *)managedObjectContext {
    if (_managedObjectContext == nil) {
        _managedObjectContext = ((AppDelegate*)NSApplication.sharedApplication.delegate).persistentContainer.viewContext;
    }
    return _managedObjectContext;
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {

    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSSet *insertedObjects = (notification.userInfo)[NSInsertedObjectsKey];
    NSSet *refreshedObjects = (notification.userInfo)[NSRefreshedObjectsKey];
    NSSet *deletedObjects = (notification.userInfo)[NSDeletedObjectsKey];

    if (!updatedObjects)
        updatedObjects = [NSSet new];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:insertedObjects];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];

    NSNotification *updatedGame = [NSNotification notificationWithName:@"UpdateSideView" object:(_currentSideView == nil) ? nil : @[_currentSideView]];

    if (_currentSideView && [deletedObjects containsObject:_currentSideView]) {
        needsUpdate = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateSideView:nil];
        });

    } else if (_currentSideView && [updatedObjects containsObject:_currentSideView]) {
        needsUpdate = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateSideView:updatedGame];
        });
    } else if (_currentSideView.metadata && ([updatedObjects containsObject:_currentSideView.metadata] || [updatedObjects containsObject:_currentSideView.metadata.cover]))
    {
        needsUpdate = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateSideView:updatedGame];
        });
    }
}

- (void)updateSideView:(NSNotification *)notification {

    Game *game = nil;
    NSString *string = nil;

    NSArray<Game *> *selectedGames = (NSArray<Game *> *)notification.object;

    sideViewUpdatePending = NO;

    if (!selectedGames || !selectedGames.count || selectedGames.count > 1) {
        string = (selectedGames.count > 1) ? @"Multiple selections" : @"No selection";
    } else {
        game = selectedGames[0];
        if (game.metadata.title.length == 0) {
            game = nil;
            string = @"No selection";
        }
    }

    if (needsUpdate == NO && game && game == _currentSideView) {
        // If game is already shown and needsUpdate is NO,
        // don't recreate the side view
        return;
    }

    needsUpdate = NO;

    [_leftScrollView.documentView removeFromSuperview];

    NSRect frame = _leftScrollView.bounds;
    frame.size.height = self.view.window.contentView.frame.size.height;
    CGFloat maxWidth = self.view.window.contentView.frame.size.width / 2;
    if (frame.size.width > maxWidth && maxWidth > 0)
        frame.size.width = maxWidth;
    SideInfoView *infoView = [[SideInfoView alloc] initWithFrame:frame];

    _leftScrollView.documentView = infoView;

    _sideIfid.stringValue = @"";


    if (game) {
        [infoView updateSideViewWithGame:game];
        //NSLog(@"\nUpdating info pane for %@ with ifid %@", game.metadata.title, game.ifid);
        //NSLog(@"Side view width: %f", NSWidth(_leftView.frame));

        if (game.ifid)
            _sideIfid.stringValue = game.ifid;
    } else if (string) {
        [infoView updateSideViewWithString:string];
    }

    _currentSideView = game;
}


@end
