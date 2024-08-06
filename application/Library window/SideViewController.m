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
#import "CoreDataManager.h"
#import "TableViewController.h"

@interface SideViewController () {
    BOOL sideViewUpdatePending;
    BOOL needsUpdate;
    NSArray<Game *> *selectedGames;
}

@end

@implementation SideViewController

- (void)viewDidLoad {

    needsUpdate = YES;
    selectedGames = @[];

    if (@available(macOS 11.0, *)) {
    } else {
        if (@available(macOS 10.14, *)) {
            _scrollViewTopPin.constant = 57;
        } else {
            _scrollViewTopPin.constant = 55;
        }
    }
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(updateSideView:)
                                                 name:@"UpdateSideView"
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(splitViewDidResizeSubviews:)
                                                 name:NSSplitViewDidResizeSubviewsNotification
                                               object:nil];
}

- (void)viewDidAppear {
    [super viewDidAppear];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:self.managedObjectContext];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification {

    if (selectedGames.count == 0 || selectedGames.count > 1)
        return;

    NSNotification *currentGame = [NSNotification notificationWithName:@"UpdateSideView" object:selectedGames];

    if (!sideViewUpdatePending) {
        sideViewUpdatePending = YES;
        needsUpdate = YES;
        _leftScrollView.documentView.needsLayout = YES;
        SideViewController __weak *weakSelf = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
            SideViewController *strongSelf = weakSelf;
            if (strongSelf && strongSelf->sideViewUpdatePending) {
                [strongSelf updateSideView:currentGame];
            }
        });
    }
}

- (NSManagedObjectContext *)managedObjectContext {
    if (_managedObjectContext == nil) {
        _managedObjectContext = ((AppDelegate*)NSApp.delegate).coreDataManager.mainManagedObjectContext;
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
    } else if (selectedGames.count > 1 && deletedObjects.count) {
        NSArray<Game *> *selected = ((AppDelegate*)NSApp.delegate).tableViewController.selectedGames;
        NSMutableArray *mutableSelected = selected.mutableCopy;
        for (Game *game in deletedObjects) {
            if ([selected containsObject:game])
                [mutableSelected removeObject:game];
        }
        if (mutableSelected.count > 2)
            mutableSelected = [mutableSelected subarrayWithRange:NSMakeRange(0, 2)].mutableCopy;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateSideView:[NSNotification notificationWithName:@"UpdateSideView" object:mutableSelected]];
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

    selectedGames = (NSArray<Game *> *)notification.object;

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

    SideInfoView *infoView = [[SideInfoView alloc] initWithFrame:_leftScrollView.bounds];

    [_leftScrollView.documentView removeFromSuperview];
    _leftScrollView.documentView = infoView;

    _sideIfid.stringValue = @"";


    if (game) {
        [infoView updateSideViewWithGame:game];
        if (game.ifid)
            _sideIfid.stringValue = game.ifid;
    } else if (string) {
        [infoView updateSideViewWithString:string];
    }

    _currentSideView = game;
}


@end
