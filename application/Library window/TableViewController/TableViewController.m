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

#import "InfoController.h"
#import "SplitViewController.h"
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

    NSError *error;
    NSURL *appSuppDir = [[NSFileManager defaultManager]
                         URLForDirectory:NSApplicationSupportDirectory
                         inDomain:NSUserDomainMask
                         appropriateForURL:nil
                         create:YES
                         error:&error];
    if (error)
        NSLog(@"libctl: Could not find Application Support directory! %@",
              error);

    _homepath = [NSURL URLWithString:@"Spatterlight" relativeToURL:appSuppDir];

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

    if (@available(macOS 11.0, *)) {
        NSArray *games = [Fetches fetchObjects:@"Game" predicate:@"hidden == NO" inContext:self.managedObjectContext];
        if (games.count == 0)
            self.view.window.subtitle = @"No games";
        else if (games.count == 1)
            self.view.window.subtitle = @"1 game";
        self.view.window.subtitle = [NSString stringWithFormat:@"%ld games", games.count];
    }
}

- (void)viewDidAppear {
    [super viewDidAppear];

    // Set up a dictionary to match language codes to languages
    _englishUSLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    NSArray *ISOLanguageCodes = [NSLocale ISOLanguageCodes];
    NSMutableDictionary *mutablelanguageCodes = [NSMutableDictionary dictionaryWithCapacity:ISOLanguageCodes.count];
    NSString *languageWord;
    for (NSString *languageCode in ISOLanguageCodes) {
        languageWord = [_englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
        if (languageWord) {
            mutablelanguageCodes[[_englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode].lowercaseString] = languageCode;
        }
    }

    _languageCodes = mutablelanguageCodes;

    _forgiveness = @{
        @"" : @(0),
        @"Cruel" : @(1),
        @"Nasty" : @(2),
        @"Tough" : @(3),
        @"Polite" : @(4),
        @"Merciful" : @(5)
    };

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

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"RecheckForMissing"]) {
        [self startVerifyTimer];
    }

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StartIndexing" object:nil]];

    [self updateTableViews];

    _gameTableView.autosaveName = @"GameTable";
    NSString *key;
    for (NSTableColumn *tableColumn in _gameTableView.tableColumns) {
        key = tableColumn.identifier;
        for (NSMenuItem *menuitem in _headerMenu.itemArray) {
            if ([[menuitem valueForKey:@"identifier"] isEqualToString:key]) {
                menuitem.state = (tableColumn.hidden) ? NSOffState : NSOnState;
                break;
            }
        }
    }
    NSArray<Game *> *selected = _selectedGames;
    if (selected.count > 2)
        selected = @[_selectedGames[0], _selectedGames[1]];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"UpdateSideView" object:selected]];

    [InfoController closeStrayInfoWindows];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)NSEC_PER_SEC), dispatch_get_main_queue(), ^(void) {
        [InfoController closeStrayInfoWindows];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)NSEC_PER_SEC), dispatch_get_main_queue(), ^(void) {
            [InfoController closeStrayInfoWindows];
        });
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
         postNotification:[NSNotification notificationWithName:@"StartIndexing" object:nil]];

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
    NSLog(@"performFindPanelAction sender class %@", NSStringFromClass([(id)sender class]));
    [self.windowController.searchField selectText:self];
}

#pragma mark - Release Controller

- (void)releaseGlkControllerSoon:(GlkController *)glkctl {
    NSString *key = nil;
    for (GlkController *controller in _gameSessions.allValues)
        if (controller == glkctl) {
            NSArray *temp = [_gameSessions allKeysForObject:controller];
            key = temp[0];
            break;
        }
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
