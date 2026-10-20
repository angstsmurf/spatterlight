//
//  TableViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-16.
//

#import "TableViewController.h"
#import "TableViewController+Internal.h"
#import "TableViewController+GameActions.h"
#import "TableViewController+LibraryManagement.h"
#import "TableViewController+TableDelegate.h"

#import "LibHelperTableView.h"

#import "LibController.h"
#import "AppDelegate.h"
#import "GlkController.h"
#import "Preferences.h"

#import "Constants.h"

#import "Game.h"
#import "Metadata.h"
#import "Fetches.h"
#import "FolderAccess.h"

#import "InfoController.h"
#import "CoreDataManager.h"

#import "MetadataHandler.h"
#import "GameLauncher.h"

@implementation TableViewController

@synthesize downloadQueue = _downloadQueue;
@synthesize alertQueue = _alertQueue;
@synthesize openGameQueue = _openGameQueue;

- (void)viewDidLoad {
    [super viewDidLoad];

    _infoWindows = [[NSMutableDictionary alloc] init];
    _gameSessions = [[NSMutableDictionary alloc] init];
    _gameTableModel = [[NSMutableArray alloc] init];

    _metadataHandler = [[MetadataHandler alloc] initWithTableViewController:self];
    _gameLauncher = [[GameLauncher alloc] initWithTableViewController:self];

    AppDelegate *appdelegate = (AppDelegate*)NSApp.delegate;
    appdelegate.tableViewController = self;

    _gameTableView.autosaveTableColumns = YES;
    _gameTableView.delegate = self;

    _gameTableView.action = @selector(doClick:);
    _gameTableView.doubleAction = @selector(doDoubleClick:);
    _gameTableView.target = self;

    if (@available(macOS 10.15, *)) {
    } else {
        [_gameTableView.horizontalLine removeFromSuperview];
        _gameTableView.horizontalLine = nil;
    }

    _currentlyAddingGames = NO;
    _ifictionMatches = [[NSMutableDictionary alloc] init];
    _ifictionPartialMatches = [[NSMutableDictionary alloc] init];

    _homepath = [FolderAccess sharedContainerURL];

    NSString *key;
    NSSortDescriptor *sortDescriptor;

    for (NSTableColumn *tableColumn in _gameTableView.tableColumns) {

        key = tableColumn.identifier;
        sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:key
                                                       ascending:YES];
        tableColumn.sortDescriptorPrototype = sortDescriptor;
    }

    NSArray<NSSortDescriptor *> *sortDescriptors = _gameTableView.sortDescriptors;

    if (!sortDescriptors.count)
        _gameTableView.sortDescriptors = @[ [NSSortDescriptor sortDescriptorWithKey:@"title" ascending:YES] ];

    // The "found" column seems to move about sometimes,
    // so we move it back to the front here.
    NSInteger foundColumnIndex = [_gameTableView columnWithIdentifier:@"found"];
    if (foundColumnIndex != 0) {
        [_gameTableView moveColumn:foundColumnIndex toColumn:0];
    }

    NSRect frame = _gameTableView.headerView.frame;
    frame.size.height = 23;
    _gameTableView.headerView.frame = frame;
}

- (void)viewDidAppear {
    [super viewDidAppear];

    // The window is not up yet in viewDidLoad
    [self updateWindowSubtitle];

    // viewDidAppear runs again each time the library window is reopened
    // after being closed. Everything up to the table refresh below must only
    // happen once, or observers would fire twice, the verify timer would be
    // restarted and the one-time migrations re-run.
    if (_didAppear) {
        [self updateTableViews];
        [self postSideViewUpdate];
        return;
    }
    _didAppear = YES;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:self.managedObjectContext];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(download:)
                                                 name:@"SideviewDownload"
                                               object:nil];

    _gameTableModel = [[Fetches fetchObjects:@"Game" predicate:@"hidden == NO" inContext:self.managedObjectContext] mutableCopy];

    [self rebuildThemesSubmenu];

    [self verifyInBackground:nil];

    if ([[NSUserDefaults standardUserDefaults] boolForKey:kRecheckForMissingKey]) {
        [self startVerifyTimer];
    }

    // One-time migration of old AGT games (converted to .agx in the container)
    // to play directly from their original .D$$ files where they can be found.
    [self migrateConvertedAGTGames];

    // One-time migration of Quest, Z-code and Glulx games whose stored IFID
    // has a form babel no longer produces.
    [self migrateOutdatedIfids];

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:SpatterlightStartIndexingNotification object:nil]];

    [self updateTableViews];

    [self addTestGames];

    _gameTableView.autosaveName = @"GameTable";
    NSString *key;
    for (NSTableColumn *tableColumn in _gameTableView.tableColumns) {
        key = tableColumn.identifier;
        for (NSMenuItem *menuitem in _headerMenu.itemArray) {
            if ([menuitem.identifier isEqualToString:key]) {
                menuitem.state = (tableColumn.hidden) ? NSControlStateValueOff : NSControlStateValueOn;
                break;
            }
        }
    }
    [self postSideViewUpdate];

    // Restored info windows can turn up a while after launch
    for (int64_t seconds = 0; seconds <= 2; seconds++)
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, seconds * (int64_t)NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            [InfoController closeStrayInfoWindows];
        });
}

//! Tells the side view to show the selection: the selected game, or a
//! comparison of the first two when more than one is selected.
- (void)postSideViewUpdate {
    NSArray<Game *> *selected = _selectedGames ?: @[];
    if (selected.count > 2)
        selected = [selected subarrayWithRange:NSMakeRange(0, 2)];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"UpdateSideView" object:selected]];
}

//! The UI tests launch the app with SPATTERLIGHT_TEST_ADD_GAMES, a
//! colon-separated list of game paths, to add games to their test library
//! (see CoreDataManager +testStoreURL) without going through the open panel,
//! and select them, or with SPATTERLIGHT_TEST_PLAY_GAME, the path of a game to
//! add, select and start. Both are ignored on the user's own library.
+ (BOOL)launchedWithTestGames {
    NSDictionary<NSString *, NSString *> *environment = NSProcessInfo.processInfo.environment;
    return [CoreDataManager testStoreURL] &&
    (environment[@"SPATTERLIGHT_TEST_PLAY_GAME"].length || environment[@"SPATTERLIGHT_TEST_ADD_GAMES"].length);
}

- (void)addTestGames {
    static BOOL added = NO;
    if (added || ![TableViewController launchedWithTestGames])
        return;
    added = YES;

    NSDictionary<NSString *, NSString *> *environment = NSProcessInfo.processInfo.environment;

    NSString *gameToPlay = environment[@"SPATTERLIGHT_TEST_PLAY_GAME"];
    NSString *gamesToAdd = environment[@"SPATTERLIGHT_TEST_ADD_GAMES"];

    // Not before the library window is up
    dispatch_async(dispatch_get_main_queue(), ^{
        NSMutableSet<Game *> *games = [NSMutableSet new];
        for (NSString *path in [gamesToAdd componentsSeparatedByString:@":"]) {
            if (!path.length)
                continue;
            Game *game = [self.gameLauncher importGame:path
                                             inContext:self.managedObjectContext
                                         reportFailure:YES
                                                  hide:NO];
            if (game)
                [games addObject:game];
        }
        if (games.count) {
            self.gameTableDirty = YES;
            [self updateTableViews];
            [self selectGames:games];
        }

        if (gameToPlay.length) {
            Game *game = [self.gameLauncher importGame:gameToPlay
                                             inContext:self.managedObjectContext
                                         reportFailure:YES
                                                  hide:NO];
            if (game)
                [self.gameLauncher selectAndPlayGame:game];
        }

        // Some tests relaunch the app on the same library
        [self.coreDataManager saveChanges];
    });
}

#pragma mark - Property Accessors

- (LibController *)windowController {
    if (_windowController == nil) {
        _windowController = ((AppDelegate*)NSApp.delegate).libctl;
        if (!_windowController) {
            _windowController = (LibController *)self.view.window.delegate;
        }
        if (_windowController) {
            _windowController.tableViewController = self;
        }
    }
    return _windowController;
}

- (NSManagedObjectContext *)managedObjectContext {
    if (_managedObjectContext == nil) {
        _managedObjectContext = self.coreDataManager.mainManagedObjectContext;
    }
    return _managedObjectContext;
}

- (CoreDataManager *)coreDataManager {
    if (_coreDataManager == nil) {
        _coreDataManager =  ((AppDelegate*)NSApp.delegate).coreDataManager;
    }
    return _coreDataManager;
}

- (NSMenuItem *)mainThemesSubMenu {
    if (_mainThemesSubMenu == nil) {
        _mainThemesSubMenu = ((AppDelegate*)NSApp.delegate).themesMenuItem;
    }
    return _mainThemesSubMenu;
}

- (NSOperationQueue *)downloadQueue {
    if (_downloadQueue == nil) {
        _downloadQueue = [NSOperationQueue new];
        _downloadQueue.maxConcurrentOperationCount = 3;
        _downloadQueue.qualityOfService = NSQualityOfServiceUtility;
    }

    return _downloadQueue;
}

// Serial: IFDBDownloader queues ComparisonOperations here, so that only one
// "replace the cover image?" prompt is up at a time.
- (NSOperationQueue *)alertQueue {
    if (_alertQueue == nil) {
        _alertQueue = [NSOperationQueue new];
        _alertQueue.maxConcurrentOperationCount = 1;
        _alertQueue.qualityOfService = NSQualityOfServiceUtility;
    }

    return _alertQueue;
}

- (NSOperationQueue *)openGameQueue {
    if (_openGameQueue == nil) {
        _openGameQueue = [NSOperationQueue new];
        _openGameQueue.maxConcurrentOperationCount = 3;
        _openGameQueue.qualityOfService = NSQualityOfServiceUserInteractive;
    }

    return _openGameQueue;
}

#pragma mark - Importing state

- (void)beginImporting {
    TableViewController * __weak weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!weakSelf.spinnerSpinning) {
            weakSelf.spinnerSpinning = YES;
            weakSelf.windowController.progIndicator.hidden = NO;
            [weakSelf.windowController.progIndicator startAnimation:self];
        }
    });
}

- (void)endImporting {
    TableViewController * __weak weakSelf = self;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        if (weakSelf.spinnerSpinning) {
            weakSelf.spinnerSpinning = NO;
            weakSelf.windowController.progIndicator.hidden = YES;
            [weakSelf.windowController.progIndicator stopAnimation:self];
        }

        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:SpatterlightStartIndexingNotification object:nil]];

        [weakSelf.managedObjectContext performBlock:^{
            while (weakSelf.undoGroupingCount > 0) {
                [weakSelf.managedObjectContext.undoManager endUndoGrouping];
                weakSelf.undoGroupingCount--;
            }
        }];

        [weakSelf.windowController.window.toolbar validateVisibleItems];
    });
}

- (void)performFindPanelAction:(id<NSValidatedUserInterfaceItem>)sender {
    [self.windowController.searchField selectText:self];
}

#pragma mark - Release Controller

- (void)releaseGlkControllerSoon:(GlkController *)glkctl {
    NSString *key = [_gameSessions allKeysForObject:glkctl].firstObject;
    if (key) {
        glkctl.window.delegate = nil;
        [_gameSessions removeObjectForKey:key];
        _gameTableDirty = YES;
        [self updateTableViews];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)NSEC_PER_SEC), dispatch_get_main_queue(), ^(void){
            [glkctl terminateTask];
        });
    }
}

#pragma mark - Forwarding to MetadataHandler

- (void)askAboutImportingMetadata:(NSDictionary<NSString *, IFStory *> *)storyDict indirectMatches:(NSDictionary<NSString *, IFStory *> *)indirectDict {
    [_metadataHandler askAboutImportingMetadata:storyDict indirectMatches:indirectDict];
}

- (void)downloadMetadataForGames:(NSArray<Game *> *)games {
    [_metadataHandler downloadMetadataForGames:games];
}

- (IBAction)importMetadata:(id)sender {
    [_metadataHandler importMetadataInContext:_managedObjectContext window:self.view.window];
}

- (IBAction)exportMetadata:(id)sender {
    [_metadataHandler exportMetadataInContext:_managedObjectContext window:self.view.window accessoryView:_exportTypeView popUpButton:_exportTypeControl];
}

@end
