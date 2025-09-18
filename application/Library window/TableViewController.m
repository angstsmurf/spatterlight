//
//  TableViewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-01-16.
//

#import "TableViewController.h"

#import <BlorbFramework/Blorb.h>
#import <CoreSpotlight/CoreSpotlight.h>

#import "LibController.h"
#import "AppDelegate.h"
#import "GlkController.h"
#import "Preferences.h"

#import "Constants.h"

#import "NSString+Categories.h"
#import "NSDate+relative.h"
#import "NSData+Categories.h"

#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Theme.h"
#import "Image.h"
#import "Fetches.h"

#import "SideInfoView.h"
#import "InfoController.h"

#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFIdentification.h"
#import "IFDBDownloader.h"

#import "MissingFilesFinder.h"
#import "GameImporter.h"

#import "CommandScriptHandler.h"
#import "NotificationBezel.h"

#import "FolderAccess.h"
#import "DownloadOperation.h"

#import "NSManagedObjectContext+safeSave.h"
#import "SplitViewController.h"

#import "CoreDataManager.h"

#import "OpenGameOperation.h"

enum { X_EDITED, X_LIBRARY, X_DATABASE }; // export selections

enum  {
    FORGIVENESS_NONE,
    FORGIVENESS_CRUEL,
    FORGIVENESS_NASTY,
    FORGIVENESS_TOUGH,
    FORGIVENESS_POLITE,
    FORGIVENESS_MERCIFUL
};

#define RIGHT_VIEW_MIN_WIDTH 350.0
#define PREFERRED_LEFT_VIEW_MIN_WIDTH 200.0
#define ACTUAL_LEFT_VIEW_MIN_WIDTH 50.0

#include <ctype.h>

/* the treaty of babel headers */
#include "babel_handler.h"
#include "ifiction.h"
#include "treaty.h"


@interface LibHelperTableView () {
    NSTrackingArea *trackingArea;
    BOOL mouseOverView;
    NSInteger lastOverRow;
}

@property NSInteger mouseOverRow;
@property IBOutlet NSBox *horizontalLine;
@end

@implementation LibHelperTableView

-(BOOL)becomeFirstResponder {
    BOOL flag = [super becomeFirstResponder];
    if (flag) {
        [(TableViewController *)self.delegate enableClickToRenameAfterDelay];
    }
    return flag;
}

- (void)keyDown:(NSEvent *)event {
    NSString *pressed = event.characters;
    if ([pressed isEqualToString:@" "])
        [(TableViewController *)self.delegate showGameInfo:nil];
    else
        [super keyDown:event];
}

- (void)awakeFromNib {
    [super awakeFromNib];
    trackingArea = [[NSTrackingArea alloc] initWithRect:self.frame options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways) owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
    mouseOverView = NO;
    _mouseOverRow = -1;
    lastOverRow = -1;
}

- (void)mouseEntered:(NSEvent*)theEvent {
    mouseOverView = YES;
}

- (void)mouseMoved:(NSEvent*)theEvent {
    if (mouseOverView) {
        _mouseOverRow = [self rowAtPoint:[self convertPoint:theEvent.locationInWindow fromView:nil]];
        if (lastOverRow == _mouseOverRow)
            return;
        else {
            [(TableViewController *)self.delegate mouseOverChangedFromRow:lastOverRow toRow:_mouseOverRow];
            lastOverRow = _mouseOverRow;
        }
    }
}

- (void)mouseExited:(NSEvent *)theEvent {
    mouseOverView = NO;
    [(TableViewController *)self.delegate mouseOverChangedFromRow:lastOverRow toRow:-1];
    _mouseOverRow = -1;
    lastOverRow = -1;
}

- (void)updateTrackingAreas {
    [self removeTrackingArea:trackingArea];
    trackingArea = [[NSTrackingArea alloc] initWithRect:self.frame options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways) owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

@end

@implementation CellViewWithConstraint
@end

@implementation MyIndicator
- (void)mouseDown:(NSEvent *)event {
    if (_tableView) {
        [_tableView mouseDown:event];
    } else {
        [super mouseDown:event];
    }
}

@end

@implementation RatingsCellView
@end

@implementation ForgivenessCellView
@end

@implementation LikeCellView
@end

@interface TableViewController () {
    BOOL canEdit;
    NSString *searchString;

    NSLocale *englishUSLocale;
    NSDictionary *languageCodes;
    NSMenuItem *enabledThemeItem;

    BOOL countingMetadataChanges;
    NSUInteger insertedMetadataCount, updatedMetadataCount;

    BOOL verifyIsCancelled;
    NSTimer *verifyTimer;

    NSDictionary *forgiveness;
}

@end

@implementation TableViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    _infoWindows = [[NSMutableDictionary alloc] init];
    _gameSessions = [[NSMutableDictionary alloc] init];
    _gameTableModel = [[NSMutableArray alloc] init];

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

    NSString* _imageDirName = @"Cover Art";
    _imageDirName = [_imageDirName stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];
    _imageDir = [NSURL URLWithString:_imageDirName relativeToURL:_homepath];

    [[NSFileManager defaultManager] createDirectoryAtURL:_imageDir
                             withIntermediateDirectories:YES
                                              attributes:NULL
                                                   error:NULL];

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
    // To be used when a user enters a language for a game
    englishUSLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    NSArray *ISOLanguageCodes = [NSLocale ISOLanguageCodes];
    NSMutableDictionary *mutablelanguageCodes = [NSMutableDictionary dictionaryWithCapacity:ISOLanguageCodes.count];
    NSString *languageWord;
    for (NSString *languageCode in ISOLanguageCodes) {
        languageWord = [englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
        if (languageWord) {
            mutablelanguageCodes[[englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode].lowercaseString] = languageCode;
        }
    }

    languageCodes = mutablelanguageCodes;

    forgiveness = @{
        @"" : @(FORGIVENESS_NONE),
        @"Cruel" : @(FORGIVENESS_CRUEL),
        @"Nasty" : @(FORGIVENESS_NASTY),
        @"Tough" : @(FORGIVENESS_TOUGH),
        @"Polite" : @(FORGIVENESS_POLITE),
        @"Merciful" : @(FORGIVENESS_MERCIFUL)
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

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"RecheckForMissing"]) {
        [self startVerifyTimer];
        [self verifyInBackground:nil];
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
    NSArray<Game *> *selected = self.selectedGames;
    if (selected.count > 2)
        selected = @[self.selectedGames[0], self.selectedGames[1]];
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

- (IBAction)deleteLibrary:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:NSLocalizedString(@"Do you really want to delete the library?", nil)];
    [alert setInformativeText:NSLocalizedString(@"This will empty your game list and delete all metadata. The original game files will not be affected.", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Delete Library", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    if (_gameSessions.count) {
        alert.accessoryView = _forceQuitView;
        _forceQuitCheckBox.state = NSOffState;
    }

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {

        [self cancel:nil];
        [self.coreDataManager saveChanges];
        BOOL forceQuit = _forceQuitCheckBox.state == NSOnState;

        CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
        [index deleteSearchableItemsWithDomainIdentifiers:@[@"net.ccxvii.spatterlight"] completionHandler:^(NSError *blockerror){
            if (blockerror) {
                NSLog(@"Deleting all searchable items failed: %@", blockerror);
            }
        }];

        NSArray *entitiesToDelete = @[@"Metadata", @"Game", @"Ifid", @"Image"];

        NSMutableSet<NSManagedObjectID *> *gamesToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet<NSManagedObjectID *> *metadataToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet<NSManagedObjectID *> *imagesToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet<NSManagedObjectID *> *ifidsToKeep = [[NSMutableSet alloc] init];

        for (GlkController *ctl in _gameSessions.allValues) {
            if (forceQuit || !ctl.alive) {
                if (_infoWindows[ctl.game.hashTag]) {
                    _infoWindows[ctl.game.hashTag].inDeletion = YES;
                }
                [ctl.window close];
            } else {
                Game *game = ctl.game;
                [gamesToKeep addObject:game.objectID];
                [metadataToKeep addObject:game.metadata.objectID];
                if (game.metadata.cover)
                    [imagesToKeep addObject:game.metadata.cover.objectID];
                for (Ifid *ifid in game.metadata.ifids)
                    [ifidsToKeep addObject:ifid.objectID];
            }
        }

        if (forceQuit) {
            _gameSessions = [NSMutableDictionary new];
        }

        NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;

        [childContext performBlock:^{
            for (NSString *entity in entitiesToDelete) {

                NSFetchRequest *fetchEntities = [[NSFetchRequest alloc] init];
                fetchEntities.entity = [NSEntityDescription entityForName:entity inManagedObjectContext:childContext];
                fetchEntities.resultType = NSManagedObjectIDResultType;
                fetchEntities.includesPropertyValues = NO; //only fetch the managedObjectID

                NSError *error = nil;
                NSArray *objectsToDelete = [childContext executeFetchRequest:fetchEntities error:&error];
                if (error)
                    NSLog(@"deleteLibrary: %@", error);

                NSMutableSet *set = [NSMutableSet setWithArray:objectsToDelete];

                if ([entity isEqualToString:@"Game"]) {
                    [set minusSet:gamesToKeep];
                } else if ([entity isEqualToString:@"Metadata"]) {
                    [set minusSet:metadataToKeep];
                } else if ([entity isEqualToString:@"Image"]) {
                    [set minusSet:imagesToKeep];
                } else if ([entity isEqualToString:@"Ifid"]) {
                    [set minusSet:ifidsToKeep];
                }

                for (NSManagedObjectID *objectID in set) {
                    [childContext deleteObject:[childContext objectWithID:objectID]];
                }
            }
            [childContext safeSave];
        }];
    }
}

- (IBAction)pruneLibrary:(id)sender {

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(@"Do you really want to prune the library?", nil);
    alert.informativeText = NSLocalizedString(@"Pruning will delete the information about games that are not in the library at the moment.", nil);
    [alert addButtonWithTitle:NSLocalizedString(@"Prune", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {

        [self cancel:nil];

        NSArray *gameEntriesToDelete =
        [Fetches fetchObjects:@"Game" predicate:@"hidden == YES" inContext:self.managedObjectContext];
        NSUInteger counter = gameEntriesToDelete.count;
        for (Game *game in gameEntriesToDelete) {
            if (game.metadata)
                NSLog(@"pruneLibrary: Deleting Game object with title %@", game.metadata.title);
            [self.managedObjectContext deleteObject:game];
        }

        NSArray *metadataEntriesToDelete =
        [Fetches fetchObjects:@"Metadata" predicate:@"game == NIL" inContext:self.managedObjectContext];
        counter += metadataEntriesToDelete.count;

        for (Metadata *meta in metadataEntriesToDelete) {
            NSLog(@"pruneLibrary: Deleting Metadata object with title %@", meta.title);
            [self.managedObjectContext deleteObject:meta];
        }

        // Now we removed any orphaned images
        NSArray *imageEntriesToDelete = [Fetches fetchObjects:@"Image" predicate:@"ANY metadata == NIL" inContext:self.managedObjectContext];

        counter += imageEntriesToDelete.count;
        for (Image *img in imageEntriesToDelete) {
            NSLog(@"pruneLibrary: Deleting Image object with url %@", img.originalURL);
            [self.managedObjectContext deleteObject:img];
        }

        [self.coreDataManager saveChanges];

        // And then any orphaned ifids
        NSArray *ifidEntriesToDelete = [Fetches fetchObjects:@"Ifid" predicate:@"metadata == NIL" inContext:self.managedObjectContext];

        counter += ifidEntriesToDelete.count;
        for (Ifid *ifid in ifidEntriesToDelete) {
            [self.managedObjectContext deleteObject:ifid];
        }

        [self.coreDataManager saveChanges];

        NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.view.window.screen];
        [notification showStandardWithText:[NSString stringWithFormat:@"%ld entit%@ pruned", counter, counter == 1 ? @"y" : @"ies"]];

        [self.coreDataManager saveChanges];
    }
}

#pragma mark Check library for missing files
- (IBAction)verifyLibrary:(id)sender{
    if ([self verifyAlert] == NSAlertFirstButtonReturn) {
        verifyIsCancelled = NO;
        [self verifyInBackground:nil];
    }
}

- (NSModalResponse)verifyAlert {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults boolForKey:@"VerifyAlertSuppression"])
        return NSAlertFirstButtonReturn;

    NSAlert *alert = [NSAlert new];
    alert.messageText = NSLocalizedString(@"Verify Library", nil);
    alert.informativeText = NSLocalizedString(@"Check the library for missing game files.", nil);
    alert.showsSuppressionButton = YES;
    alert.suppressionButton.title = NSLocalizedString(@"Remember these choices", nil);

    _verifyDeleteMissingCheckbox.state = [defaults boolForKey:@"DeleteMissing"];
    _verifyReCheckbox.state = [defaults boolForKey:@"RecheckForMissing"];
    _verifyFrequencyTextField.integerValue = [defaults integerForKey:@"RecheckFrequency"];

    alert.accessoryView = _verifyView;
    [alert addButtonWithTitle:NSLocalizedString(@"Check", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        [defaults setBool:alert.suppressionButton.state ? YES : NO forKey:@"VerifyAlertSuppression"];
        [defaults setBool:_verifyReCheckbox.state ? YES : NO forKey:@"RecheckForMissing"];
        if (_verifyReCheckbox.state)
            [self startVerifyTimer];
        else
            [self stopVerifyTimer];
        [defaults setBool:_verifyDeleteMissingCheckbox.state ? YES : NO forKey:@"DeleteMissing"];
        [defaults setInteger:_verifyFrequencyTextField.integerValue forKey:@"RecheckFrequency"];
    }
    return choice;
}

- (void)verifyInBackground:(id)sender {
    if (verifyIsCancelled) {
        verifyIsCancelled = NO;
        return;
    }

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;

    TableViewController * __weak weakSelf = self;

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    [childContext performBlock:^{
        TableViewController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        BOOL deleteMissing = [[NSUserDefaults standardUserDefaults] boolForKey:@"DeleteMissing"];
        for (Game *gameInMain in strongSelf.gameTableModel) {
            if (strongSelf->verifyIsCancelled) {
                strongSelf->verifyIsCancelled = NO;
                return;
            }
            Game *game = [childContext objectWithID:gameInMain.objectID];
            if (game) {
                if (![[NSFileManager defaultManager] fileExistsAtPath:game.path] &&
                    ![[NSFileManager defaultManager] fileExistsAtPath:game.urlForBookmark.path]) {
                    if (deleteMissing) {
                        [childContext deleteObject:game];
                    } else if (game.found) {
                        game.found = NO;
                    }
                } else {
                    if (!game.found)
                        game.found = YES;
                }
            }
        }
        [childContext safeSave];

        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"StartIndexing" object:nil]];
    }];
}

- (void)startVerifyTimer {
    if (verifyTimer)
        [verifyTimer invalidate];
    verifyIsCancelled = NO;
    NSInteger minutes = [[NSUserDefaults standardUserDefaults] integerForKey:@"RecheckFrequency"];
    NSTimeInterval seconds = minutes * 60;
    verifyTimer = [NSTimer scheduledTimerWithTimeInterval:seconds
                                                   target:self
                                                 selector:@selector(verifyInBackground:)
                                                 userInfo:nil
                                                  repeats:YES];
    verifyTimer.tolerance = 10;
}

- (void)stopVerifyTimer {
    if (verifyTimer)
        [verifyTimer invalidate];
    verifyIsCancelled = YES;
}

- (void)lookForMissingFile:(Game *)game {
    MissingFilesFinder *look = [MissingFilesFinder new];
    [look lookForMissingFile:game libController:self];
}

#pragma mark End of check for missing files

- (void)beginImporting {
    //    NSLog(@"Beginning importing");
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
    //    NSLog(@"Ending importing");

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

        // This is the only way I have found to immediately enable the "Add to library…" button again.
        // (The Apple header for validateVisibleItems says: "Typically you should not invoke this method.")
        [weakSelf.windowController.window.toolbar validateVisibleItems];
    });
}

- (IBAction)importMetadata:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.prompt = NSLocalizedString(@"Import", nil);

    panel.allowedFileTypes = @[ @"iFiction", @"ifiction" ];

    TableViewController * __weak weakSelf = self;

    [panel beginSheetModalForWindow:self.view.window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;
            [weakSelf.managedObjectContext performBlock:^{
                [weakSelf importMetadataFromFile:url.path inContext:weakSelf.managedObjectContext];
            }];
        }
    }];
}

- (IBAction)exportMetadata:(id)sender {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.accessoryView = _exportTypeView;
    panel.allowedFileTypes = @[ @"iFiction" ];
    panel.prompt = NSLocalizedString(@"Export", nil);
    panel.nameFieldStringValue = NSLocalizedString(@"Interactive Fiction Metadata.iFiction", nil);

    NSPopUpButton *localExportTypeControl = _exportTypeControl;

    [panel beginSheetModalForWindow:self.view.window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;
            [self exportMetadataToFile:url.path
                                  what:localExportTypeControl
             .indexOfSelectedItem];
        }
    }];
}

- (IBAction)addGamesToLibrary:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseDirectories:YES];
    panel.prompt = NSLocalizedString(@"Add", nil);

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    _lookForCoverImagesCheckBox.state = [defaults boolForKey:@"LookForImagesWhenImporting"];
    _downloadGameInfoCheckBox.state = [defaults boolForKey:@"DownloadInfoWhenImporting"];

    panel.accessoryView = _downloadCheckboxView;
    panel.allowedFileTypes = gGameFileTypes;

    TableViewController * __weak weakSelf = self;

    [panel beginSheetModalForWindow:self.view.window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSArray *urls = panel.URLs;

            BOOL lookForImages = weakSelf.lookForCoverImagesCheckBox.state ? YES : NO;
            BOOL downloadInfo = weakSelf.downloadGameInfoCheckBox.state ? YES : NO;

            [defaults setBool:lookForImages forKey:@"LookForImagesWhenImporting"];
            [defaults setBool:downloadInfo forKey:@"DownloadInfoWhenImporting"];

            [FolderAccess askForAccessToURL:urls.firstObject andThenRunBlock:^() {
                [weakSelf addInBackground:urls lookForImages:lookForImages downloadInfo:downloadInfo];
            }];
            return;
        }
    }];
}

- (void)addInBackground:(NSArray<NSURL *> *)files lookForImages:(BOOL)lookForImages downloadInfo:(BOOL)downloadInfo {
    //    NSLog(@"addInBackground %ld files lookForImages:%@ downloadInfo:%@", files.count, lookForImages ? @"YES" : @"NO", downloadInfo ? @"YES" : @"NO");

    if (_currentlyAddingGames)
        return;

    _downloadWasCancelled = NO;

    [self.coreDataManager saveChanges];
    [self.managedObjectContext.undoManager beginUndoGrouping];
    _undoGroupingCount++;

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    verifyIsCancelled = YES;

    [self beginImporting];

    _lastImageComparisonData = nil;

    _currentlyAddingGames = YES;

    [childContext performBlock:^{
        GameImporter *importer = [[GameImporter alloc] initWithTableViewController:self];
        NSDictionary *options = @{ @"context":childContext,
                                   @"lookForImages":@(lookForImages),
                                   @"downloadInfo":@(downloadInfo) };
        [importer addFiles:files options:options];
    }];
}

- (void)performFindPanelAction:(id<NSValidatedUserInterfaceItem>)sender {
    NSLog(@"performFindPanelAction sender class %@", NSStringFromClass([(id)sender class]));
    [self.windowController.searchField selectText:self];
}

- (void)runCommandsFromFile:(NSString *)filename {
    GlkController *gctl = [self activeGlkController];
    if (gctl) {
        [gctl.commandScriptHandler runCommandsFromFile:filename];
    }
}

- (void)restoreFromSaveFile:(NSString *)filename {
    GlkController *gctl = [self activeGlkController];
    if (gctl) {
        [gctl.commandScriptHandler restoreFromSaveFile:filename];
    }
}

- (nullable GlkController *)activeGlkController {
    Game *game = [Preferences instance].currentGame;
    if (!game)
        return nil;
    GlkController *gctl = _gameSessions[game.hashTag];
    if (!gctl.alive) {
        // If the current game is finished, look for another
        gctl = nil;
        for (GlkController *g in  _gameSessions.allValues) {
            if (g.alive) {
                gctl = g;
                break;
            }
        }
    }
    return gctl;
}

- (BOOL)hasActiveGames {
    for (GlkController *gctl in _gameSessions.allValues)
        if (gctl.alive)
            return YES;
    return NO;
}

#pragma mark Contextual menu

- (IBAction)play:(id)sender {
    NSInteger rowidx;

    if (_gameTableView.clickedRow != -1)
        rowidx = _gameTableView.clickedRow;
    else
        rowidx = _gameTableView.selectedRow;

    if (rowidx >= 0) {
        [self playGame:_gameTableModel[(NSUInteger)rowidx]];
    }
}

- (IBAction)revealGameInFinder:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only reveal game in clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    NSUInteger i;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = _gameTableModel[i];
        NSURL *url = game.urlForBookmark;
        if (![[NSFileManager defaultManager] fileExistsAtPath:url.path]) {
            game.found = NO;
            [self lookForMissingFile:game];
            return;
        } else game.found = YES;
        [[NSWorkspace sharedWorkspace] selectFile:url.path
                         inFileViewerRootedAtPath:@""];
    }
}

- (IBAction)deleteGame:(id)sender {
    if (_currentlyAddingGames) {
        NSBeep();
        return;
    }
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only delete game in clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    NSMutableArray __block *running = [NSMutableArray new];
    Game __block *game;

    TableViewController * __weak weakSelf = self;

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        game = weakSelf.gameTableModel[idx];
        if (weakSelf.gameSessions[game.ifid] == nil) {
            [weakSelf.managedObjectContext deleteObject:game];
        } else {
            [running addObject:game];
        }
    }];

    NSUInteger count = running.count;
    if (count) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:NSLocalizedString(@"Are you sure?", nil)];
        alert.informativeText = [NSString stringWithFormat:@"%@ %@ currently in use. Do you want to close and delete %@?", [NSString stringWithSummaryOfGames:running], (count == 1) ? @"is" : @"are", (count == 1) ? @"it" : @"them"];
        [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSInteger choice = [alert runModal];

        if (choice == NSAlertFirstButtonReturn) {
            for (Game *toDelete in running) {
                [_gameSessions[toDelete.hashTag].window close];
                [self.managedObjectContext deleteObject:toDelete];
            }
        }
    }
}

- (IBAction)delete:(id)sender {
    [self deleteGame:sender];
}

- (IBAction)openIfdb:(id)sender {

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow != -1) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    Game __block *game;
    NSString __block *urlString;

    TableViewController * __weak weakSelf = self;

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        game = weakSelf.gameTableModel[idx];
        if (game.metadata.tuid)
            urlString = [@"https://ifdb.tads.org/viewgame?id=" stringByAppendingString:game.metadata.tuid];
        else
            urlString = [@"https://ifdb.tads.org/viewgame?ifid=" stringByAppendingString:game.ifid];
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: urlString]];
    }];
}

- (IBAction)download:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow != -1) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    if (rows.count > 0)
    {
        [self downloadMetadataForGames:[_gameTableModel objectsAtIndexes:rows]];
    }
}

@synthesize downloadQueue = _downloadQueue;

- (NSOperationQueue *)downloadQueue {
    if (_downloadQueue == nil) {
        _downloadQueue = [NSOperationQueue new];
        _downloadQueue.maxConcurrentOperationCount = 3;
        _downloadQueue.qualityOfService = NSQualityOfServiceUtility;
    }

    return _downloadQueue;
}

@synthesize alertQueue = _alertQueue;

- (NSOperationQueue *)alertQueue {
    if (_alertQueue == nil) {
        _alertQueue = [NSOperationQueue new];
        _alertQueue.maxConcurrentOperationCount = 1;
        _alertQueue.qualityOfService = NSQualityOfServiceUtility;
    }

    return _alertQueue;
}

@synthesize openGameQueue = _openGameQueue;

- (NSOperationQueue *)openGameQueue {
    if (_openGameQueue == nil) {
        _openGameQueue = [NSOperationQueue new];
        _openGameQueue.maxConcurrentOperationCount = 3;
        _openGameQueue.qualityOfService = NSQualityOfServiceUserInteractive;
    }

    return _openGameQueue;
}

- (void)downloadMetadataForGames:(NSArray<Game *> *)games {

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    _downloadWasCancelled = NO;

    [self.managedObjectContext performBlockAndWait:^{
        [self.managedObjectContext.undoManager beginUndoGrouping];
        self.undoGroupingCount++;
    }];

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    _downloadWasCancelled = NO;

    [self beginImporting];

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    [self.coreDataManager saveChanges];

    _lastImageComparisonData = nil;

    // This deselects all games
    if (_gameTableView.selectedRowIndexes.count > 10 && _gameTableView.selectedRowIndexes.count == _gameTableModel.count)
        [_gameTableView selectRowIndexes:[NSIndexSet new] byExtendingSelection:NO];

    _nestedDownload = _currentlyAddingGames;

    if (_nestedDownload)
        NSLog(@"downloadMetadataForGames: _nestedDownload:YES");

    verifyIsCancelled = YES;
    _currentlyAddingGames = YES;

    NSMutableArray<NSManagedObjectID *> __block *blockGames = [NSMutableArray new];

    for (Game *gameInMain in games) {
        [blockGames addObject:gameInMain.objectID];
    }

    NSUInteger numberOfGames = games.count;

    games = nil;

    TableViewController * __weak weakSelf = self;

    [childContext performBlock:^{

        IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithTableViewController:weakSelf];
        NSOperationQueue *queue = weakSelf.downloadQueue;

        int64_t pause = 0;
        if (numberOfGames > 1) {
            pause = (int64_t)(0.5 * NSEC_PER_SEC);
            if (numberOfGames > 10) {
                pause = 2 * NSEC_PER_SEC;
            }
        }

        // A block that will run when all
        // all metadata and all images are downloaded
        void (^finisherBlock)(void) = ^void() {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, pause), dispatch_get_main_queue(), ^{
                if (weakSelf.nestedDownload) {
                    NSLog(@"nestedDownload in finisherBlock!");
                    weakSelf.nestedDownload = NO;
                } else {
                    weakSelf.currentlyAddingGames = NO;
                }
                [childContext performBlock:^{
                    for (NSManagedObjectID *objectID in blockGames) {
                        Game *game = [childContext objectWithID:objectID];
                        game.hasDownloaded = YES;
                    }
                    [childContext safeSave];
                }];
                [weakSelf endImporting];
                [weakSelf.coreDataManager saveChanges];
            });
        };

        if (numberOfGames > 1) {
            pause = (int64_t)(0.5 * NSEC_PER_SEC);
            if (numberOfGames > 10) {
                pause = NSEC_PER_SEC;
            }
        }

        NSBlockOperation *finisher = [NSBlockOperation blockOperationWithBlock:finisherBlock];
        NSBlockOperation *preFinisher = [NSBlockOperation blockOperationWithBlock:^{
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, pause), dispatch_get_main_queue(), ^{
                if (weakSelf.lastImageDownloadOperation) {
                    [finisher addDependency:weakSelf.lastImageDownloadOperation];
                }
                [queue addOperation:finisher];
            });
        }];

        [finisher addDependency:preFinisher];

        BOOL reportFailure = NO;
        if (blockGames.count == 1) {
            Game *game = [childContext objectWithID:blockGames.firstObject];
            if (game.metadata.ifids.count == 1)
                reportFailure = YES;
        }

        NSOperation *lastOperation = [downloader downloadMetadataForGames:blockGames inContext:childContext onQueue:weakSelf.downloadQueue imageOnly:NO reportFailure:reportFailure completionHandler:^{
            if (weakSelf.lastImageDownloadOperation) {
                [preFinisher addDependency:weakSelf.lastImageDownloadOperation];
            }
            [queue addOperation:preFinisher];
        }];

        if (lastOperation) {
            [preFinisher addDependency:lastOperation];
        }
    }];
}

- (void)rebuildThemesSubmenu {

    enabledThemeItem = nil;
    NSMenu *themesMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Apply Theme", nil)];

    themesMenu.showsStateColumn = YES;

    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    fetchRequest.sortDescriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES], [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES selector:@selector(localizedStandardCompare:)]];
    NSError *error = nil;

    NSArray *themes = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (themes == nil) {
        NSLog(@"Problem! %@",error);
    }

    NSMenu *themesMenu2 = themesMenu.copy;

    for (Theme *theme in themes) {
        [themesMenu addItemWithTitle:theme.name action:@selector(applyTheme:) keyEquivalent:@""];
        [themesMenu2 addItemWithTitle:theme.name action:@selector(applyTheme:) keyEquivalent:@""];
    }

    _themesSubMenu.submenu = themesMenu;
    self.mainThemesSubMenu.submenu = themesMenu2;
}

- (IBAction) applyTheme:(id)sender {

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow != -1) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    NSString *name = ((NSMenuItem *)sender).title;

    Theme *theme = [Fetches findTheme:name inContext:self.managedObjectContext];

    if (!theme) {
        NSLog(@"applyTheme: found no theme with name %@", name);
        return;
    }

    NSSet *games = [NSSet setWithArray:[_gameTableModel objectsAtIndexes:rows]];
    [theme addGames:games];

    Preferences *prefwin = [Preferences instance];
    if (prefwin) {
        // Unless we are changing the theme of all games simultaneously,
        // uncheck the "Changes apply to all games" box.
        if (games.count < _gameTableModel.count) {
            prefwin.oneThemeForAll = NO;
        }
        if ([games containsObject:prefwin.currentGame])
            [prefwin restoreThemeSelection:theme];
    }

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
}

- (IBAction) selectSameTheme:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow != -1) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    Game *selectedGame = _gameTableModel[rows.firstIndex];

    NSSet *gamesWithTheme = selectedGame.theme.games;

    NSIndexSet *matchingIndexes = [_gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
        return [gamesWithTheme containsObject:obj];
    }];

    [_gameTableView selectRowIndexes:matchingIndexes byExtendingSelection:NO];
}

- (IBAction)deleteSaves:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;
    TableViewController * __weak weakSelf = self;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        GlkController *gctl = weakSelf.gameSessions[game.ifid];

        if (!gctl) {
            gctl = [[GlkController alloc] init];
        }
        [gctl deleteAutosaveFilesForGame:game];
    }];
}

- (IBAction)like:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;
    TableViewController * __weak weakSelf = self;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    __block NSUInteger count = 0;
    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (game.like == 1)
            count++;
    }];

    BOOL allLiked = (count == rows.count);

    // If all are liked: unlike all. If only some or none are liked: like all
    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (allLiked)
            game.like = 0;
        else
            game.like = 1;
    }];
}

- (IBAction)dislike:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;
    TableViewController * __weak weakSelf = self;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    __block NSUInteger count = 0;
    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (game.like == 2)
            count++;
    }];

    BOOL allDisliked = (count == rows.count);

    // If all are disliked: undislike all. If only some or none are disliked: dislike all

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (allDisliked)
            game.like = 0;
        else
            game.like = 2;
    }];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;
    NSInteger count = _gameTableView.numberOfSelectedRows;
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if (_gameTableView.clickedRow != -1 &&
        (![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])) {
        count = 1;
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];
    }

    if (count == 0 &&
        (action == @selector(delete:) ||
         action == @selector(deleteGame:) ||
         action == @selector(openIfdb:) ||
         action == @selector(applyTheme:) ||
         action == @selector(selectSameTheme:) ||
         action == @selector(like:) ||
         action == @selector(download:) ||
         action == @selector(deleteSaves:) ||
         action == @selector(dislike:))) {
        return NO;
    }

    if (action == @selector(delete:) || action == @selector(deleteGame:)) {
        return !_currentlyAddingGames;
    }

    if (action == @selector(applyTheme:)) {
        if ([Preferences instance].darkOverrideActive || [Preferences instance].lightOverrideActive)
            return NO;

        if (enabledThemeItem != nil) {
            for (NSMenuItem *item in _themesSubMenu.submenu.itemArray) {
                item.state = NSOffState;
            }
            for (NSMenuItem *item in self.mainThemesSubMenu.submenu.itemArray) {
                item.state = NSOffState;
            }
            enabledThemeItem = nil;
        }

        if (count > 0 && count < 10000) {
            NSMutableSet<NSString *> __block *themeNamesToSelect = [NSMutableSet new];
            [rows enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
                NSString *name = _gameTableModel[idx].theme.name;
                if (name)
                    [themeNamesToSelect addObject:name];
            }];
            NSControlStateValue state = NSOnState;
            if (themeNamesToSelect.count > 1)
                state = NSMixedState;
            for (NSString *name in themeNamesToSelect) {
                enabledThemeItem = [self.mainThemesSubMenu.submenu itemWithTitle:name];
                if (enabledThemeItem)
                    enabledThemeItem.state = state;
                enabledThemeItem = [_themesSubMenu.submenu itemWithTitle:name];
                if (enabledThemeItem)
                    enabledThemeItem.state = state;
            }
        }
    }

    if (action == @selector(performFindPanelAction:)) {
        if (menuItem.tag == NSTextFinderActionShowFindInterface)
            return YES;
        else
            return NO;
    }

    if (action == @selector(addGamesToLibrary:))
        return !_currentlyAddingGames;

    //    if (action == @selector(importMetadata:))
    //         return !_currentlyAddingGames;

    //    if (action == @selector(download:))
    //        return !_currentlyAddingGames;

    if (action == @selector(showGameInfo:)
        || action == @selector(reset:))
        return (count > 0 && count < 20);

    if (action == @selector(revealGameInFinder:))
        return count > 0 && count < 10;

    if (action == @selector(play:))
        return count == 1;

    if (action == @selector(selectSameTheme:)) {
        // Check if all selected games use the same theme
        NSArray *selection = [_gameTableModel objectsAtIndexes:rows];
        Theme *selectionTheme = nil;
        for (Game *game in selection) {
            if (selectionTheme == nil) {
                selectionTheme = game.theme;
            } else {
                if (game.theme != selectionTheme) {
                    return NO;
                }
            }
        }
    }

    if (action == @selector(deleteSaves:)) {
        NSArray *selection = [_gameTableModel objectsAtIndexes:rows];
        for (Game *game in selection) {
            if (game.autosaved) return YES;
        }
        return NO;
    }

    if (action == @selector(like:) || action == @selector(dislike:)) {
        BOOL like = (action == @selector(like:));
        NSArray *selection = [_gameTableModel objectsAtIndexes:rows];
        int likecount[3] = { 0,0,0 };
        for (Game *game in selection) {
            likecount[game.like]++;
        }
        if (like) {
            if (likecount[1] > 0) {
                menuItem.title = NSLocalizedString(@"Liked", nil);
                if (likecount[0] > 0 || likecount[2] > 0) {
                    menuItem.state = NSMixedState;
                } else {
                    menuItem.state = NSOnState;
                }
            } else {
                menuItem.title = NSLocalizedString(@"Like", nil);
                menuItem.state = NSOffState;
            }
        } else {
            if (likecount[2] > 0) {
                menuItem.title = NSLocalizedString(@"Disliked", nil);
                if (likecount[0] > 0 || likecount[1] > 0) {
                    menuItem.state = NSMixedState;
                } else {
                    menuItem.state = NSOnState;
                }
            } else {
                menuItem.title = NSLocalizedString(@"Dislike", nil);
                menuItem.state = NSOffState;
            }
        }
    }
    return YES;
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)item {
    BOOL enable = YES;
    NSUInteger count = _selectedGames.count;
    if ([item.itemIdentifier isEqual:@"showInfo"] ) {
        return (count > 0 && count < 20);
    } else if ([item.itemIdentifier isEqual:@"playGame"]) {
        enable = (count == 1);
    } else if ([item.itemIdentifier isEqual:@"addToLibrary"]) {
        enable = (_currentlyAddingGames == NO);
    }
    return enable;
}

#pragma mark Game info windows

- (IBAction)showGameInfo:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    NSUInteger i;
    NSUInteger counter = 0;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = _gameTableModel[i];
        [self showInfoForGame:game toggle:![sender isKindOfClass:[NSMenuItem class]]];
        // Don't open more than 20 info windows at once
        if (counter++ > 20)
            break;
    }
}

- (void)showInfoForGame:(Game *)game toggle:(BOOL)toggle {
    InfoController *infoctl;

    NSString *hashTag = game.hashTag;
    if (hashTag.length == 0) {
        hashTag = game.path.signatureFromFile;
        game.hashTag = hashTag;
    }
    if (hashTag.length == 0) {
        NSLog(@"showInfoForGame: Could not create hash from game file!");
        return;
    }
    // First, we check if we have created this info window already
    infoctl = _infoWindows[hashTag];

    if (!infoctl || infoctl.inDeletion) {
        infoctl = [[InfoController alloc] initWithGame:game];
        infoctl.libcontroller = self;
        _infoWindows[hashTag] = infoctl;
        NSWindow *infoWindow = infoctl.window;
        infoWindow.restorable = YES;
        infoWindow.restorationClass = [AppDelegate class];
        infoWindow.identifier = [NSString stringWithFormat:@"infoWin%@", hashTag];

        NSRect targetFrame = infoctl.window.frame;
        NSRect visibleFrame = infoctl.window.screen.visibleFrame;
        if (targetFrame.origin.y < visibleFrame.origin.y)
            targetFrame.origin.y = visibleFrame.origin.y;

        [infoctl hideWindow];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [infoctl animateIn:targetFrame];
        });
    } else if (toggle) {
        //[infoctl.window makeKeyAndOrderFront:nil];
        if (!infoctl.inAnimation)
            [infoctl.window performClose:nil];
    } else {
        [infoctl showWindow:nil];
    }
}

- (void)closeAndOpenNextAbove:(InfoController *)infocontroller {
    NSUInteger index = [_gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound && index > 0) {
        [_gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index - 1] byExtendingSelection:NO];
        [infocontroller.window performClose:nil];
        [self showInfoForGame:_gameTableModel[index - 1] toggle:NO];
        [_gameTableView scrollRowToVisible:(NSInteger)index - 1];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [InfoController closeStrayInfoWindows];
    });
}

- (void)closeAndOpenNextBelow:(InfoController *)infocontroller {
    NSUInteger index = [_gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound && index < _gameTableModel.count - 1) {
        [_gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index + 1] byExtendingSelection:NO];
        [infocontroller.window performClose:nil];
        [self showInfoForGame:_gameTableModel[index + 1] toggle:NO];
        [_gameTableView scrollRowToVisible:(NSInteger)index + 1];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [InfoController closeStrayInfoWindows];
    });
}



#pragma mark Drag-n-drop destination handler

- (NSDragOperation)draggingEntered:sender {
    extern NSString *terp_for_filename(NSString * path);

    NSFileManager *mgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSUInteger i;

    if (_currentlyAddingGames)
        return NSDragOperationNone;

    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([pboard.types containsObject:NSFilenamesPboardType]) {
        NSArray *paths = [pboard propertyListForType:NSFilenamesPboardType];
        NSUInteger count = paths.count;
        for (i = 0; i < count; i++) {
            NSString *path = paths[i];
            if ([gGameFileTypes
                 containsObject:path.pathExtension.lowercaseString])
                return NSDragOperationCopy;
            if ([path.pathExtension isEqualToString:@"iFiction"])
                return NSDragOperationCopy;
            if ([mgr fileExistsAtPath:path isDirectory:&isdir])
                if (isdir)
                    return NSDragOperationCopy;
        }
    }

    return NSDragOperationNone;
}

- (void)draggingExited:sender {
    // unhighlight window
}

- (BOOL)prepareForDragOperation:sender {
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard *pboard = sender.draggingPasteboard;
    if ([pboard.types containsObject:NSFilenamesPboardType]) {
        NSArray *files = [pboard propertyListForType:NSFilenamesPboardType];
        NSMutableArray *urls =
        [[NSMutableArray alloc] initWithCapacity:files.count];
        for (id tempObject in files) {
            [urls addObject:[NSURL fileURLWithPath:tempObject]];
        }
        [self addInBackground:urls lookForImages:NO downloadInfo:NO];
        return YES;
    }
    return NO;
}

#pragma mark Metadata

/*
 * Metadata is not tied to games in the library.
 * We keep it around even if the games come and go.
 * Metadata that has been edited by the user is flagged,
 * and can be exported.
 */

- (void)askToDownload {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"HasAskedToDownload"])
        return;

    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hasDownloaded = YES"];

    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"askToDownload: Could not fetch Game entities: %@",error);
        return;
    }

    if (fetchedObjects.count == 0)
    {
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
        fetchRequest.predicate = nil;
        fetchRequest.includesPropertyValues = NO;
        fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects.count < 5)
            return;

        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasAskedToDownload"];

        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Do you want to download game info from IFDB in the background?", nil);
        alert.informativeText = NSLocalizedString(@"This will only be done once. You can do this later by selecting games in the library and choosing Download Info from the contextual menu.", nil);
        [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result) {
            if (result == NSAlertFirstButtonReturn) {
                [self downloadMetadataForGames:fetchedObjects];
            }
        }];
    }
}

typedef NS_ENUM(int32_t, kImportResult) {
    kImportResultNoneFound,
    kImportResultExactMatchesOnly,
    kImportResultPartialMatchesOnly,
    kImportExactAndPartialMatchesBoth
};

+ (NSDictionary<NSString *, IFStory *> *)importMetadataFromXML:(NSData *)mdbuf indirectMatches:(NSDictionary<NSString *, IFStory *> * __autoreleasing *)indirectDict inContext:(NSManagedObjectContext *)context {
    NSMutableDictionary<NSString *, IFStory *> *directMatches, *indirectMatches;
    directMatches = [[NSMutableDictionary alloc] init];

    IFictionMetadata *ifictionmetadata = [[IFictionMetadata alloc] initWithData:mdbuf];

    if (!ifictionmetadata || ifictionmetadata.stories.count == 0)
        return directMatches;

    indirectMatches = [[NSMutableDictionary alloc] init];

    for (IFStory *story in ifictionmetadata.stories) {
        for (NSString *ifid in story.identification.ifids) {
            NSArray *result = [Fetches fetchGamesForIfid:ifid inContext:context];
            if (result.count) {
                for (Game *game in result) {
                    NSString *idString = game.objectID.URIRepresentation.absoluteString;
                    directMatches[idString] = story;
                }
            } else {
                NSSet *resultSet = [Fetches fetchMetadataForIfid:ifid inContext:context];
                if (resultSet.count) {
                    for (Metadata *meta in resultSet) {
                        NSString *idString = meta.game.objectID.URIRepresentation.absoluteString;
                        if (idString == nil)
                            continue;
                        indirectMatches[idString] = story;
                    }
                }
            }
        }
    }

    if (indirectMatches.count && indirectDict != nil)
        *indirectDict = indirectMatches;

    return directMatches;
}

+ (void)addInfoToMetadata:(NSSet<NSManagedObjectID *>*)gameIDs fromStories:(NSDictionary<NSString *, IFStory *> *)stories coreDataManager:(CoreDataManager *)manager{
    NSManagedObjectContext *childContext = manager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
    [childContext performBlock:^{
        for (NSManagedObjectID *gameID in gameIDs) {
            IFStory *story = stories[gameID.URIRepresentation.absoluteString];
            Game *game = [childContext objectWithID:gameID];
            [story addInfoToMetadata:game.metadata];
            game.metadata.source = @(kExternal);
        }
        [childContext safeSave];
    }];
}

- (void)askAboutImportingMetadata:(NSDictionary<NSString *, IFStory *> *)storyDict indirectMatches:(NSDictionary<NSString *, IFStory *> *)indirectDict {

    kImportResult __block importResult = kImportResultNoneFound;

    NSSet<Game *> __block *games = [[NSSet alloc] init];
    NSSet<Game *> __block *indirectGames = [[NSSet alloc] init];
    NSSet<NSManagedObjectID *> __block *gameIDs = [[NSSet alloc] init];
    NSSet<NSManagedObjectID *> __block *indirectGameIDs = [[NSSet alloc] init];

    NSString __block *msg = @"There were no library matches for the games in the iFiction file.";
    NSString __block *msg2;

    NSLog(@"storyDict.count: %ld indirectDict.count: %ld", storyDict.count, indirectDict.count);

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    [childContext performBlockAndWait:^{
        for (NSString *identifier in storyDict.allKeys) {
            NSManagedObjectID *objectID = [childContext.persistentStoreCoordinator managedObjectIDForURIRepresentation:[NSURL URLWithString:identifier]];
            Game *result = [childContext objectWithID:objectID];
            if (result) {
                games = [games setByAddingObject:result];
                gameIDs = [gameIDs setByAddingObject:objectID];
            }
        }

        for (NSString *identifier in indirectDict.allKeys) {
            NSManagedObjectID *objectID = [childContext.persistentStoreCoordinator managedObjectIDForURIRepresentation:[NSURL URLWithString:identifier]];
            Game *result = [childContext objectWithID:objectID];
            if (result && ![games containsObject:result]) {
                indirectGames = [indirectGames setByAddingObject:result];
                indirectGameIDs = [indirectGameIDs setByAddingObject:objectID];
            }
        }

        if (games.count > 0) {
            importResult = kImportResultExactMatchesOnly;
            msg = [NSString stringWithFormat:@"Found details matching %@.",
                   [NSString stringWithSummaryOfGames:games.allObjects]];
        } else if (storyDict.count) {
            NSLog(@"askAboutImportingMetadata: None of the %ld direct matches found in database", storyDict.count);
        }

        if (indirectGames.count > 0) {
            if (importResult == kImportResultExactMatchesOnly)
                importResult = kImportExactAndPartialMatchesBoth;
            else
                importResult = kImportResultPartialMatchesOnly;

            msg2 = [NSString stringWithFormat:@"%@ound partial match%@ for %@.", importResult == kImportExactAndPartialMatchesBoth ? @"Also f" : @"F", indirectGames.count > 1 ? @"es" : @"", [NSString stringWithSummaryOfGames:indirectGames.allObjects]];
        } else if (indirectDict.count) {
            NSLog(@"askAboutImportingMetadata: None of the %ld indirect matches found in database (or they were also exact matches)", indirectDict.count);
        }
    }];

    dispatch_async(dispatch_get_main_queue(), ^{

        NSAlert *anAlert = [[NSAlert alloc] init];
        [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        anAlert.messageText = NSLocalizedString(msg, nil);

        if (importResult != kImportResultNoneFound) {
            [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
            anAlert.informativeText = NSLocalizedString(@"Would you like to replace existing information?", nil);
        }

        [anAlert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result){
            if (importResult == kImportResultNoneFound)
                return;
            if (result == NSAlertFirstButtonReturn) {
                if (importResult == kImportResultExactMatchesOnly ||
                    importResult == kImportExactAndPartialMatchesBoth) {
                    [TableViewController addInfoToMetadata:gameIDs fromStories:storyDict coreDataManager:self.coreDataManager];
                } else if (importResult == kImportResultPartialMatchesOnly) {
                    [TableViewController addInfoToMetadata:indirectGameIDs fromStories:indirectDict coreDataManager:self.coreDataManager];
                }
            }
            if (importResult == kImportExactAndPartialMatchesBoth) {
                NSAlert *alert2 = [[NSAlert alloc] init];
                alert2.messageText = NSLocalizedString(msg2, nil);
                [alert2 addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
                [alert2 addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
                alert2.informativeText = anAlert.informativeText;
                [alert2 beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result2){
                    if (result2 == NSAlertFirstButtonReturn) {
                        [TableViewController addInfoToMetadata:indirectGameIDs fromStories:indirectDict coreDataManager:self.coreDataManager];
                    }
                }];
            }
        }];
    });
}


//When importing iFiction (XML) data:
//• If we are downloading from IFDB, the iFiction file should only contain data for one single game. If it does not, show an error. Check which one has a matching ifid.
//
//If we open an iFiction file directly, we may have one or more games selected, or opened it via the context menu of one or more games.
//• Overwrite the metadata of any matching selected games. If the iFiction contains metadata for unselected games (and any games are selected) ask if we want to use this metadata as well.

// We don't create a Metadata object if there is no suitable existant Game object to attach it to.

- (void)importMetadataFromFile:(NSString *)filename inContext:(NSManagedObjectContext *)context {
    NSLog(@"libctl: importMetadataFromFile %@", filename);

    NSData *data = [NSData dataWithContentsOfFile:filename];
    if (!data)
        return;

    NSDictionary<NSString *, IFStory *> *indirectMatches = nil;
    NSDictionary<NSString *, IFStory *> *exactMatches = [TableViewController importMetadataFromXML:data indirectMatches:&indirectMatches inContext:context];
    if (exactMatches.count > 0 || indirectMatches.count > 0) {
        [self askAboutImportingMetadata:exactMatches indirectMatches:indirectMatches];
    }
}

static void write_xml_text(FILE *fp, Metadata *info, NSString *key) {
    NSString *val;
    const char *tagname;
    const char *s;

    val = [info valueForKey:key];
    if (!val)
        return;

    //"description" is a reserved word in core data,
    // so we use "blurb" internally. We change this to
    // "description" in XML output.
    if ([key isEqualToString:@"blurb"])
        key = @"description";

    NSLog(@"write_xml_text: key:%@ val:%@", key, val);

    tagname = key.UTF8String;
    s = [NSString stringWithFormat:@"%@", val].UTF8String;

    fprintf(fp, "<%s>", tagname);
    while (*s) {
        if (*s == '&')
            fprintf(fp, "&amp;");
        else if (*s == '<')
            fprintf(fp, "&lt;");
        else if (*s == '>')
            fprintf(fp, "&gt;");
        else if (*s == '\n') {
            fprintf(fp, "<br/>");
            while (s[1] == '\n')
                s++;
        } else {
            putc(*s, fp);
        }
        s++;
    }
    fprintf(fp, "</%s>\n", tagname);
}

/*
 * Export metadata (ie, with kSource == kUser)
 */
- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what {
    NSEnumerator *enumerator;
    Metadata *meta;

    NSLog(@"libctl: exportMetadataToFile %@", filename);

    FILE *fp;

    fp = fopen(filename.UTF8String, "w");
    if (!fp)
        return NO;

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<ifindex version=\"1.0\">\n\n");


    NSFetchRequest *fetchRequest = [Metadata fetchRequest];

    switch (what) {
        case X_EDITED:
            fetchRequest.predicate = [NSPredicate predicateWithFormat: @"(userEdited == YES)"];
            break;
        case X_LIBRARY:
            fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ANY games != NIL"];
            break;
        case X_DATABASE:
            // No fetchRequest - we want all of them
            break;
        default:
            NSLog(@"exportMetadataToFile: Unhandled switch case!");
            break;
    }

    NSError *error = nil;
    NSArray *metadata = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (!metadata) {
        NSLog(@"exportMetadataToFile: Could not fetch metadata list. Error: %@", error);
    }

    if (metadata.count == 0) {
        fclose(fp);
        NSAlert *alert = [NSAlert new];
        alert.messageText = NSLocalizedString(@"No metadata found", nil);
        alert.informativeText = NSLocalizedString(@"No matching metadata to export was found in the library.", nil);
        [alert runModal];
        return NO;
    }

    enumerator = [metadata objectEnumerator];
    while ((meta = [enumerator nextObject]))
    {
        fprintf(fp, "<story>\n");

        fprintf(fp, "<identification>\n");
        for (Ifid *ifid in meta.ifids) {
            fprintf(fp, "<ifid>%s</ifid>\n", ifid.ifidString.UTF8String);
        }
        write_xml_text(fp, meta, @"format");
        write_xml_text(fp, meta, @"bafn");
        fprintf(fp, "</identification>\n");

        fprintf(fp, "<bibliographic>\n");

        write_xml_text(fp, meta, @"title");
        write_xml_text(fp, meta, @"author");
        write_xml_text(fp, meta, @"language");
        write_xml_text(fp, meta, @"headline");
        write_xml_text(fp, meta, @"firstpublished");
        write_xml_text(fp, meta, @"genre");
        write_xml_text(fp, meta, @"group");
        write_xml_text(fp, meta, @"series");
        write_xml_text(fp, meta, @"seriesnumber");
        write_xml_text(fp, meta, @"forgiveness");
        write_xml_text(fp, meta, @"blurb");

        fprintf(fp, "</bibliographic>\n");

        if (meta.cover) {
            NSData *data = (NSData *)meta.cover.data;
            NSString *type = @"png";
            if (data.JPEG)
                type = @"jpg";
            else if (!data.PNG)
                NSLog(@"Illegal image type!");

            NSImage *image = [[NSImage alloc] initWithData:data];
            NSString *description = meta.coverArtDescription;
            if (!description.length) {
                description = meta.cover.imageDescription;
            }
            if (!description.length) {
                description = @"";
            }
            fprintf(fp, "<cover>\n");
            fprintf(fp, "<format>%s</format>\n", type.UTF8String);
            fprintf(fp, "<height>%ld</height>\n", (NSInteger)image.size.height);
            fprintf(fp, "<width>%ld</width>\n", (NSInteger)image.size.width);
            fprintf(fp, "<description>%s</description>\n", description.UTF8String);
            fprintf(fp, "</cover>\n");
        }
        fprintf(fp, "</story>\n\n");
    }

    fprintf(fp, "</ifindex>\n");

    fclose(fp);
    return YES;
}

- (void)handleSpotlightSearchResult:(id)object {
    Metadata *meta = nil;
    Game *game = nil;

    NSManagedObjectContext *context = [object managedObjectContext];

    if (!context)
        return;

    if (context.hasChanges)
        [context save:nil];

    if ([object isKindOfClass:[Metadata class]]) {
        meta = (Metadata *)object;
        game = meta.game;
    } else if ([object isKindOfClass:[Game class]]) {
        game = (Game *)object;
        meta = game.metadata;
    } else if ([object isKindOfClass:[Image class]]) {
        Image *image = (Image *)object;
        meta = image.metadata.anyObject;
        game = meta.game;
    } else {
        NSLog(@"Object: %@", [object class]);
    }

    if (game && [_gameTableModel indexOfObject:game] == NSNotFound) {
        self.windowController.searchField.stringValue = @"";
        [self searchForGames:nil];
    }

    if (game) {
        [self selectAndPlayGame:game];
    } else if (meta) {
        [self createDummyGameForMetadata:meta];
    }
}

- (void)createDummyGameForMetadata:(Metadata *)metadata {

    Game *game = [NSEntityDescription
                  insertNewObjectForEntityForName:@"Game"
                  inManagedObjectContext:metadata.managedObjectContext];

    game.ifid = metadata.ifids.anyObject.ifidString;
    game.metadata = metadata;
    game.added = [NSDate date];
    game.detectedFormat = metadata.format;
    game.found = NO;

    [self updateTableViews];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        [self selectGames:[NSSet setWithObject:game]];

        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"ShowSideBar" object:nil]];
    });
}

#pragma mark Actually starting the game

- (nullable NSWindow *) playGame:(Game *)game {
    return [self playGame:game restorationHandler:nil];
}

- (nullable NSWindow *)playGameWithHash:(NSString *)hash restorationHandler:(void (^)(NSWindow *, NSError *))completionHandler  {
    Game *game = [Fetches fetchGameForHash:hash inContext:self.managedObjectContext];
    if (!game) return nil;
    return [self playGame:game restorationHandler:completionHandler];
}

- (nullable NSWindow *)playGame:(Game *)game restorationHandler:(nullable void (^)(NSWindow *, NSError *))completionHandler {

    // The systemWindowRestoration flag is just to let us know
    // if this is called from restoreWindowWithIdentifier in
    // AppDelegate.m.
    // The playGameWithIFID method passes this flag on
    // to the GlkController runTerp method.

    // The main difference is that then we will enter
    // fullscreen automatically if we autosaved in fullscreen,
    // so we don't have to worry about that, unlike when
    // autorestoring by clicking the game in the library view
    // or similar.

    NSURL *url = game.urlForBookmark;
    NSString *path = url.path;
    NSString *terp;
    GlkController *gctl = _gameSessions[game.hashTag];

    BOOL systemWindowRestoration = (completionHandler != nil);

    NSFileManager *fileManager = [NSFileManager defaultManager];

    if (gctl) {
        NSLog(@"A game with this hash is already in session");
        [gctl.window makeKeyAndOrderFront:nil];
        return gctl.window;
    }

    if (![fileManager fileExistsAtPath:path]) {
        game.found = NO;
        if (!systemWindowRestoration) // Everything will break if we throw up a dialog during system window restoration
            [self lookForMissingFile:game];
        return nil;
    } else game.found = YES;

    //Check if the file is in trash
    NSError *error = nil;
    NSURL *trashUrl = [fileManager URLForDirectory:NSTrashDirectory inDomain:NSUserDomainMask appropriateForURL:[NSURL fileURLWithPath:path isDirectory:NO] create:NO error:&error];
    if (error)
        NSLog(@"Error getting Trash path: %@", error);
    if (trashUrl && [path hasPrefix:trashUrl.path]) {
        if (!systemWindowRestoration) {
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = NSLocalizedString(@"This game is in Trash.", nil);
            alert.informativeText = NSLocalizedString(@"You won't be able to play the game until you move the file out of there.", nil);
            [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
            [alert addButtonWithTitle:NSLocalizedString(@"Delete entry", nil)];
            NSInteger choice = [alert runModal];
            if (choice == NSAlertSecondButtonReturn) {
                [game.managedObjectContext deleteObject:game];
            }
        }
        return nil;
    }

    if (!game.detectedFormat) {
        game.detectedFormat = game.metadata.format;
    }

    terp = gExtMap[path.pathExtension.lowercaseString];

    if (!terp)
        terp = gFormatMap[game.detectedFormat];

    if (!terp) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Cannot play the file.", nil);
        alert.informativeText = NSLocalizedString(@"The game is not in a recognized file format; cannot find a suitable interpreter.", nil);
        [alert runModal];
        return nil;
    }

    gctl = [[GlkController alloc] initWithWindowNibName:@"GameWindow"];
    if ([terp isEqualToString:@"glulxe"] || [terp isEqualToString:@"fizmo"] || [terp isEqualToString:@"bocfel"]) {
        gctl.window.restorable = YES;
        gctl.window.restorationClass = [AppDelegate class];
        gctl.window.identifier = [NSString stringWithFormat:@"gameWin%@", game.hashTag];
    } else {
        gctl.window.restorable = NO;
    }

    if (game.metadata.title.length)
        gctl.window.title = game.metadata.title;
    else
        NSLog(@"Game has no metadata?");

    TableViewController __weak *weakSelf = self;

    GlkController __block *blockgctl = gctl;
    Game __block *blockGame = game;
    NSString *hashTag = game.hashTag;
    if (hashTag.length == 0) {
        hashTag = game.path.signatureFromFile;
        game.hashTag = hashTag;
    }
    if (hashTag.length == 0) {
        NSLog(@"Could not hash game data?");
        return nil;
    }

    [gctl askForAccessToURL:url showDialog:!systemWindowRestoration andThenRunBlock:^{
        OpenGameOperation *operation = [[OpenGameOperation alloc] initWithURL:url completionHandler:^(NSData * _Nullable newData, NSURL * _Nullable newURL) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockgctl.slowReadAlert.window close];
                blockgctl.slowReadAlert = nil;

                if (newData.length == 0) {
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Failed to open the game.", nil);
                    alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"File access to \"%@\" was not permitted.", nil), newURL.lastPathComponent];
                    [alert runModal];
                } else {
                    weakSelf.gameSessions[hashTag] = blockgctl;
                    blockGame.lastPlayed = [NSDate date];
                    blockgctl.gameData = newData;
                    blockgctl.gameFileURL = newURL;

                    [blockgctl runTerp:terp withGame:blockGame reset:NO restorationHandler:completionHandler];

                    [((AppDelegate *)NSApp.delegate)
                     addToRecents:@[ newURL ]];

                    if ([Blorb isBlorbURL:newURL]) {
                        if (!blockGame.metadata.cover || [Blorb isBlorbURL:[NSURL fileURLWithPath:blockGame.metadata.cover.originalURL]]) {
                            Blorb *blorb = [[Blorb alloc] initWithData:newData];
                            GameImporter *importer = [[GameImporter alloc] initWithTableViewController:weakSelf];
                            blockGame.metadata.coverArtURL = newURL.absoluteString;
                            [importer updateImageFromBlorb:blorb inGame:blockGame];
                        }
                    }
                }
            });
        }];

        [weakSelf.openGameQueue addOperation:operation];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (gctl.gameData == nil && !systemWindowRestoration) {
                gctl.slowReadAlert = [[NSAlert alloc] init];
                NSAlert __weak *alert = gctl.slowReadAlert;
                alert.messageText =
                [NSString stringWithFormat:NSLocalizedString(@"The game \"%@\" is taking a long time to load.", nil), blockGame.metadata.title];
                [alert addButtonWithTitle:NSLocalizedString(@"Cancel loading", nil)];

                [alert beginSheetModalForWindow:gctl.window completionHandler:^(NSModalResponse result){
                    if (result == NSAlertFirstButtonReturn) {
                        [operation cancel];
                    }
                }];
            }
        });
    }];

    return gctl.window;
}

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

- (void)releaseInfoController:(InfoController *)infoctl {
    NSString *key = nil;
    for (InfoController *controller in _infoWindows.allValues)
        if (controller == infoctl) {
            NSArray *temp = [_infoWindows allKeysForObject:controller];
            key = temp[0];
            break;
        }
    if (key) {
        infoctl.window.delegate = nil;
        [_infoWindows removeObjectForKey:key];
    }
}


- (nullable NSWindow *)importAndPlayGame:(NSString *)path {
    BOOL hide = ![[NSUserDefaults standardUserDefaults] boolForKey:@"AddToLibrary"];
    Game *game = [self importGame:path inContext:self.managedObjectContext reportFailure:YES hide:hide];
    if (game) {
        return [self selectAndPlayGame:game];
    }
    return nil;
}

- (NSWindow *)selectAndPlayGame:(Game *)game {
    NSWindow *result = [self playGame:game];
    NSUInteger gameIndex = [self.gameTableModel indexOfObject:game];
    if (gameIndex >= (NSUInteger)self.gameTableView.numberOfRows) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.6 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
            [self selectGames:[NSSet setWithObject:game]];
            [self.gameTableView scrollRowToVisible:(NSInteger)[self.gameTableModel indexOfObject:game]];
        });
    } else {
        [self selectGames:[NSSet setWithObject:game]];
        [self.gameTableView scrollRowToVisible:(NSInteger)gameIndex];
    }
    return result;
}

- (Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide {
    GameImporter *importer = [[GameImporter alloc] initWithTableViewController:self];
    Game *result = [importer importGame:path inContext:context reportFailure:report hide:hide];
    if (result && !result.metadata.cover)
        [importer lookForImagesForGame:result];
    return result;
}

- (IBAction)cancel:(id)sender {
    _currentlyAddingGames = NO;
    _nestedDownload = NO;
    _downloadWasCancelled = YES;
    verifyIsCancelled = YES;
    [_alertQueue cancelAllOperations];
    [_downloadQueue cancelAllOperations];
    [_openGameQueue cancelAllOperations];
    [self endImporting];
}


#pragma mark -
#pragma mark Table magic

- (IBAction)searchForGames:(nullable id)sender {
    searchString = [[sender stringValue] stringByReplacingOccurrencesOfString:@"\"" withString:@""];
    _gameTableDirty = YES;
    [self updateTableViews];
}

- (IBAction)toggleColumn:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSTableColumn *column;
    for (NSTableColumn *tableColumn in _gameTableView.tableColumns) {
        if ([tableColumn.identifier
             isEqualToString:[item valueForKey:@"identifier"]]) {
            column = tableColumn;
            break;
        }
    }
    if (item.state == YES) {
        [column setHidden:YES];
        item.state = NO;
    } else {
        [column setHidden:NO];
        item.state = YES;
    }
}

- (NSRect)rectForLineWithHash:(NSString*)hashTag {
    Game *game;
    NSRect frame = NSZeroRect;
    NSRect myFrame = self.view.window.frame;
    frame.origin.x = myFrame.origin.x + myFrame.size.width / 2;
    frame.origin.y = myFrame.origin.y + myFrame.size.height / 2;

    if (hashTag.length) {
        game = [Fetches fetchGameForHash:hashTag inContext:self.managedObjectContext];
        if ([_gameTableModel containsObject:game]) {
            NSUInteger index = [_gameTableModel indexOfObject:game];
            frame = [_gameTableView rectOfRow:(NSInteger)index];
            frame = [_gameTableView convertRect:frame toView:nil];
            frame = [self.view.window convertRectToScreen:frame];
        }
    }
    frame.origin.x += 12;
    frame.size.width -= 24;
    return frame;
}

- (void)selectGamesWithHashes:(NSArray*)hashes scroll:(BOOL)shouldscroll {
    if (hashes.count) {
        NSMutableArray *newSelection = [NSMutableArray arrayWithCapacity:hashes.count];
        for (NSString *hashTag in hashes) {
            Game *game = [Fetches fetchGameForHash:hashTag inContext:self.managedObjectContext];
            if (game) {
                [newSelection addObject:game];
            } else {
                NSLog(@"No game with hash %@ in library, cannot restore selection", hashTag);
            }
        }
        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

        for (Game *game in newSelection) {
            if ([_gameTableModel containsObject:game]) {
                [indexSet addIndex:[_gameTableModel indexOfObject:game]];
            }
        }
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        _selectedGames = [_gameTableModel objectsAtIndexes:indexSet];
        if (shouldscroll && indexSet.count && !_currentlyAddingGames) {
            [_gameTableView scrollRowToVisible:(NSInteger)indexSet.firstIndex];
        }
    }
}

- (void)selectGames:(NSSet*)games
{
    if (games.count) {
        NSIndexSet *indexSet = [_gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
            return [games containsObject:obj];
        }];
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        _selectedGames = [_gameTableModel objectsAtIndexes:indexSet];
    }
}

+ (NSInteger)stringcompare:(NSString *)a with:(NSString *)b {
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedStandardCompare: b];
}

+ (NSInteger)compareGame:(Metadata *)a with:(Metadata *)b key:(id)key ascending:(BOOL)ascending {
    NSString * ael = [a valueForKey:key];
    NSString * bel = [b valueForKey:key];
    return [TableViewController compareString:ael withString:bel ascending:ascending];
}

+ (NSInteger)compareString:(NSString *)ael withString:(NSString *)bel ascending:(BOOL)ascending {
    if ((!ael || ael.length == 0) && (!bel || bel.length == 0))
        return NSOrderedSame;
    if (!ael || ael.length == 0) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!bel || bel.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;

    return [TableViewController stringcompare:ael with:bel];
}

+ (NSInteger)compareDate:(NSDate *)ael withDate:(NSDate *)bel ascending:(BOOL)ascending {
    if ((!ael) && (!bel))
        return NSOrderedSame;
    if (!ael) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!bel) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [ael compare:bel];
}

+ (NSInteger)compareIntegers:(NSInteger)a and:(NSInteger)b ascending:(BOOL)ascending {
    if (a == b)
        return NSOrderedSame;
    if (a == NSNotFound) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (b == NSNotFound) return ascending ? NSOrderedAscending : NSOrderedDescending;
    if (a < b)
        return NSOrderedAscending;
    else
        return NSOrderedDescending;
}

+ (NSPredicate *)searchPredicateForWord:(NSString *)word {
    return [NSPredicate predicateWithFormat: @"(detectedFormat contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.languageAsWord contains [c] %@) OR (metadata.firstpublished contains %@)", word, word, word, word, word, word, word, word, word, word, word];
}

- (void)updateTableViews {
    if (!_gameTableDirty)
        return;

    NSError *error = nil;
    NSArray<Game *> *searchResult = nil;

    NSFetchRequest *fetchRequest = [Game fetchRequest];
    NSMutableArray<NSPredicate *> *predicateArray = [NSMutableArray new];

    [predicateArray addObject:[NSPredicate predicateWithFormat:@"hidden == NO"]];

    NSCompoundPredicate *comp;

    BOOL alreadySearched = NO;

    if (searchString.length)
    {
        // First we search using the entire search string as a phrase
        // such as "beyond zork"

        [predicateArray addObject:[TableViewController searchPredicateForWord:searchString]];

        comp = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];

        fetchRequest.predicate = comp;
        error = nil;
        searchResult = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
        if (error)
            NSLog(@"executeFetchRequest for searchString: %@", error);

        alreadySearched = YES;

        // If this gives zero results, search for each word
        // separately, all matches for "beyond" AND "zork"
        if (searchResult.count == 0) {
            NSArray *searchStrings = [searchString componentsSeparatedByString:@" "];
            if (searchStrings.count > 1) {
                alreadySearched = NO;
                [predicateArray removeLastObject];

                for (NSString *word in searchStrings) {
                    if (word.length)
                        [predicateArray addObject:
                         [TableViewController searchPredicateForWord:word]];
                }
            }
        }
    }

    if (!searchResult.count && !alreadySearched) {
        comp = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];
        fetchRequest.predicate = comp;
        error = nil;
        searchResult = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    }

    _gameTableModel = searchResult.mutableCopy;
    if (_gameTableModel == nil)
        NSLog(@"Problem! %@",error);

    TableViewController * __weak weakSelf = self;

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:_sortAscending comparator:^(Game *aid, Game *bid) {

        NSInteger cmp = 0;

        TableViewController *strongSelf = weakSelf;

        if (!strongSelf)
            return cmp;

        Metadata *a = aid.metadata;
        Metadata *b = bid.metadata;
        if ([strongSelf.gameSortColumn isEqual:@"firstpublishedDate"]) {
            cmp = [TableViewController compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"added"] || [strongSelf.gameSortColumn isEqual:@"lastPlayed"]) {
            cmp = [TableViewController compareDate:[aid valueForKey:strongSelf.gameSortColumn] withDate:[bid valueForKey:strongSelf.gameSortColumn] ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"lastModified"]) {
            cmp = [TableViewController compareDate:[a valueForKey:strongSelf.gameSortColumn] withDate:[b valueForKey:strongSelf.gameSortColumn] ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"found"]) {
            NSString *string1, *string2;
            [self imageNameForGame:aid description:&string1];
            [self imageNameForGame:bid description:&string2];
            cmp = [TableViewController compareString:string1 withString:string2 ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"like"]) {
            NSInteger numA = aid.like;
            NSInteger numB = bid.like;
            if (numA == 1) {
                numA = 2;
            } else if (numA == 2) {
                numA = 1;
            }
            if (numB == 1) {
                numB = 2;
            } else if (numA == 2) {
                numB = 1;
            }
            cmp = [TableViewController compareIntegers:numA and:numB ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"forgivenessNumeric"]) {
            NSString *string1 = a.forgivenessNumeric ? [NSString stringWithFormat:@"%@", a.forgivenessNumeric] : nil;
            NSString *string2 = b.forgivenessNumeric ? [NSString stringWithFormat:@"%@", b.forgivenessNumeric] : nil;
            cmp = [TableViewController compareString:string1 withString:string2 ascending:strongSelf.sortAscending];

            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"seriesnumber"]) {
            NSInteger numA = a.seriesnumber ? a.seriesnumber.integerValue : NSNotFound;
            NSInteger numB = b.seriesnumber ? b.seriesnumber.integerValue : NSNotFound;
            cmp = [TableViewController compareIntegers:numA and:numB ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"series"]) {
            cmp = [TableViewController compareGame:a with:b key:strongSelf.gameSortColumn ascending:strongSelf.sortAscending];
            if (cmp == NSOrderedSame) {
                NSInteger numA = a.seriesnumber ? a.seriesnumber.integerValue : NSNotFound;
                NSInteger numB = b.seriesnumber ? b.seriesnumber.integerValue : NSNotFound;
                cmp = [TableViewController compareIntegers:numA and:numB ascending:strongSelf.sortAscending];
                if (cmp) return cmp;
            } else return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"format"]) {
            cmp = [TableViewController compareString:aid.detectedFormat withString:bid.detectedFormat ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if (strongSelf.gameSortColumn) {
            cmp = [TableViewController compareGame:a with:b key:strongSelf.gameSortColumn ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        cmp = [TableViewController compareGame:a with:b key:@"title" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [TableViewController compareGame:a with:b key:@"author" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [TableViewController compareGame:a with:b key:@"seriesnumber" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [TableViewController compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        return [TableViewController compareString:aid.hashTag withString:bid.hashTag ascending:strongSelf.sortAscending];
    }];

    NSSortDescriptor *fallback = [NSSortDescriptor sortDescriptorWithKey:@"hashTag" ascending:_sortAscending comparator:^(Game *x, Game *y) {
        TableViewController *strongSelf = weakSelf;

        BOOL sortAscending;
        if (!strongSelf) {
            sortAscending = strongSelf.sortAscending;
        } else {
            sortAscending = YES;
        }

        return [TableViewController compareString:x.hashTag withString:y.hashTag ascending:sortAscending];
    }];


    [_gameTableModel sortUsingDescriptors:@[sort, fallback]];

    dispatch_async(dispatch_get_main_queue(), ^{
        self.gameTableDirty = NO;
        [self.gameTableView reloadData];
        [self selectGames:[NSSet setWithArray:self->_selectedGames]];
        [self invalidateRestorableState];
    });
}

- (void)tableView:(NSTableView *)tableView
sortDescriptorsDidChange:(NSArray *)oldDescriptors {
    if (tableView == _gameTableView) {
        NSArray *sortDescriptors = tableView.sortDescriptors;
        if (!sortDescriptors.count)
            return;
        NSSortDescriptor *sortDescriptor = sortDescriptors[0];
        if (!sortDescriptor)
            return;
        _gameSortColumn = sortDescriptor.key;
        _sortAscending = sortDescriptor.ascending;
        _gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    if (tableView == _gameTableView)
        return (NSInteger)_gameTableModel.count;
    return 0;
}

- (NSString *)imageNameForGame:(Game *)game description:(NSString * __autoreleasing *)description {
    NSString *name;
    if (!game.found) {
        *description = NSLocalizedString(@"File not found", nil);
        name = @"exclamationmark.circle";
    } else {
        BOOL playing = NO;
        if (_gameSessions.count < 100) {
            for (GlkController *session in _gameSessions.allValues) {
                if ([session.game.hashTag isEqual:game.hashTag]) {
                    if (session.alive || session.showingCoverImage) {
                        *description = NSLocalizedString(@"In progress", nil);
                        if (@available(macOS 11.0, *)) {
                            name = @"play.fill";
                        } else {
                            name = @"play";
                        }
                    } else {
                        *description = NSLocalizedString(@"Stopped", nil);
                        if (@available(macOS 11.0, *)) {
                            name = @"stop.fill";
                        } else {
                            name = @"stop";
                        }
                    }
                    playing = YES;
                    break;
                }
            }
        }
        if (!playing) {
            if (game.autosaved) {
                *description = NSLocalizedString(@"Autosaved", nil);
                name = @"pause.fill";
            } else {
                *description = nil;
                name = nil;
            }
        }
    }
    return name;
}

- (nullable NSView *)tableView:(NSTableView*)tableView viewForTableColumn:(nullable NSTableColumn *)column row:(NSInteger)row {

    NSTableCellView *cellView = [tableView makeViewWithIdentifier:column.identifier owner:self];

    if (tableView == _gameTableView) {
        NSString *string = nil;
        if ((NSUInteger)row >= _gameTableModel.count) return nil;

        NSMutableAttributedString *headerAttrStr = column.headerCell.attributedStringValue.mutableCopy;

        if (headerAttrStr.length) {
            NSMutableDictionary *attributes = [headerAttrStr attributesAtIndex:0 effectiveRange:nil].mutableCopy;
            NSFont *font = nil;
            if ([_gameSortColumn isEqualToString:column.identifier]) {
                font = [NSFont systemFontOfSize:12 weight:NSFontWeightBold];
            } else {
                font = [NSFont systemFontOfSize:12];
            }
            
            if ([column.identifier isEqual:@"like"])
                font = [NSFont systemFontOfSize:10];

            attributes[NSFontAttributeName] = font;
            column.headerCell.attributedStringValue = [[NSAttributedString alloc] initWithString:headerAttrStr.string attributes:attributes];
        }

        Game *game = _gameTableModel[(NSUInteger)row];
        Metadata *meta = game.metadata;

        NSString *identifier = column.identifier;

        if ([identifier isEqual:@"found"]) {
            NSString *description;
            NSString *name = [self imageNameForGame:game description:&description];
            if (@available(macOS 11.0, *)) {
                cellView.imageView.image = [NSImage imageWithSystemSymbolName:name accessibilityDescription:description];
            } else {
                cellView.imageView.image = [NSImage imageNamed:name];
                cellView.imageView.image.accessibilityDescription = description;
            }
            cellView.imageView.accessibilityLabel = description;
        } else if ([identifier isEqual:@"like"]) {
            LikeCellView *likeCellView = (LikeCellView *)cellView;
            switch (game.like) {
                case 2:
                    if (@available(macOS 11.0, *)) {
                        likeCellView.likeButton.image = [NSImage imageWithSystemSymbolName:@"heart.slash.fill" accessibilityDescription:NSLocalizedString(@"Disliked", nil)];
                    } else {
                        likeCellView.likeButton.image = [NSImage imageNamed:@"heart.slash.fill"];
                        likeCellView.likeButton.image.accessibilityDescription = NSLocalizedString(@"Disliked", nil);
                    }
                    likeCellView.toolTip = NSLocalizedString(@"Disliked", nil);
                    likeCellView.accessibilityLabel = NSLocalizedString(@"Disliked", nil);
                    break;
                case 1:
                    if (@available(macOS 11.0, *)) {
                        likeCellView.likeButton.image = [NSImage imageWithSystemSymbolName:@"heart.fill" accessibilityDescription:NSLocalizedString(@"Liked", nil)];
                    } else {
                        likeCellView.likeButton.image = [NSImage imageNamed:@"heart.fill"];
                        likeCellView.likeButton.image.accessibilityDescription = NSLocalizedString(@"Liked", nil);
                    }
                    likeCellView.toolTip = NSLocalizedString(@"Liked", nil);
                    likeCellView.accessibilityLabel = NSLocalizedString(@"Liked", nil);
                    break;
                default:
                    if (row == _gameTableView.mouseOverRow) {
                        if (@available(macOS 11.0, *)) {
                            likeCellView.likeButton.image = [NSImage imageWithSystemSymbolName:@"heart" accessibilityDescription:NSLocalizedString(@"Like", nil)];
                        } else {
                            likeCellView.likeButton.image =  [NSImage imageNamed:@"heart"];
                            likeCellView.likeButton.image.accessibilityDescription = NSLocalizedString(@"Like", nil);
                        }
                    } else {
                        likeCellView.likeButton.image = nil;
                    }
                    likeCellView.toolTip = NSLocalizedString(@"Like", nil);
                    likeCellView.accessibilityLabel = NSLocalizedString(@"Not liked", nil);
                    break;
            }
            return likeCellView;
        } else if ([identifier isEqual:@"format"]) {
            if (!game.detectedFormat)
                game.detectedFormat = meta.format;
            string = game.detectedFormat;
        } else if ([identifier isEqual:@"added"] || [identifier isEqual:@"lastPlayed"]) {
            string = [[game valueForKey: identifier] formattedRelativeString];
        } else if ([identifier isEqual:@"lastModified"]) {
            string = [meta.lastModified formattedRelativeString];
        } else if ([identifier isEqual:@"firstpublishedDate"]) {
            NSDate *date = meta.firstpublishedDate;
            if (date) {
                NSDateComponents *components = [[NSCalendar currentCalendar] components:NSCalendarUnitDay | NSCalendarUnitMonth | NSCalendarUnitYear fromDate:date];
                string = [NSString stringWithFormat:@"%ld", components.year];
            }
        } else if ([identifier isEqual:@"forgivenessNumeric"]) {
            ForgivenessCellView *forgivenessCell = (ForgivenessCellView *)cellView;
            NSInteger value = meta.forgivenessNumeric.integerValue;
            if (value < 0 || value > 5)
                value = 0;
            NSPopUpButton *popUp = forgivenessCell.popUpButton;
            NSMenuItem *none = popUp.menu.itemArray[0];
            if (value == 0) {
                [popUp selectItem:none];
                none.state = NSOnState;
                if (@available(macOS 10.14, *)) {
                    popUp.title = @"";
                } else {
                    // 10.13 has a problem with showing the "empty" pop up button value, and draws a horizontal line instead.
                    none.title = @"  ";
                    popUp.title = @"  ";
                }
            } else {
                [popUp selectItem:[popUp.menu itemWithTag:value]];
                popUp.title = meta.forgiveness;
            }
            popUp.cell.bordered = NO;
            return forgivenessCell;
        } else if ([identifier isEqual:@"starRating"] ||
                   [identifier isEqual:@"myRating"]) {
            NSInteger value;
            BOOL isMy = [identifier isEqual:@"myRating"];

            RatingsCellView *indicatorCell = (RatingsCellView *)cellView;
            NSLevelIndicator *indicator = indicatorCell.rating;

            if (!isMy) {
                value = meta.starRating.integerValue;
                ((MyIndicator *)indicator).tableView = _gameTableView;
            } else {
                value = meta.myRating.integerValue;
            }

            if (value > 0 || (isMy && row == _gameTableView.mouseOverRow)) {
                indicator.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityAlways;
            } else {
                indicator.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityWhileEditing;
            }
            indicator.objectValue = @(value);
            return indicatorCell;
        } else {
            string = [meta valueForKey: identifier];
        }
        if (string == nil)
            string = @"";

        cellView.textField.stringValue = string;
    }
    return cellView;
}

- (IBAction)endedEditing:(id)sender {
    NSTextField *textField = (NSTextField *)sender;
    if (textField) {
        canEdit = NO;
        NSInteger row = [_gameTableView rowForView:sender];
        if ((NSUInteger)row >= _gameTableModel.count)
            return;
        NSInteger col = [_gameTableView columnForView:sender];
        if ((NSUInteger)col >= _gameTableView.tableColumns.count)
            return;

        Game *game = _gameTableModel[(NSUInteger)row];
        Metadata *meta = game.metadata;
        NSString *key = _gameTableView.tableColumns[(NSUInteger)col].identifier;
        NSString *oldval = [meta valueForKey:key];

        NSString *value = [textField.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (value.length == 0)
            value = nil;
        if ([value isEqual: oldval] || (value == oldval))
            return;

        if ([key isEqualToString:@"title"]) {
            if (value.length == 0) {
                textField.stringValue = meta.title;
                return;
            }
        }

        meta.userEdited = @(YES);
        meta.source = @(kUser);
        game.metadata.lastModified = [NSDate date];

        if ([key isEqualToString:@"firstpublishedDate"]) {
            NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
            dateFormatter.dateFormat = @"yyyy";
            meta.firstpublishedDate = [dateFormatter dateFromString:value];
            meta.firstpublished = value;
            return;
        }

        [meta setValue: value forKey: key];

        if ([key isEqualToString:@"languageAsWord"]) {
            // The player may have entered a language code such as "en"
            // but more likely a full word ("English".)
            // so we try to deal with both cases here to give proper
            // values to language as well as languageAsWord
            NSString *languageAsWord = value;
            NSString *languageCode;
            if (languageAsWord.length > 1) {
                // First we try to match the entered string to a language (in words)
                languageCode = languageCodes[languageAsWord.lowercaseString];
                if (!languageCode) {
                    languageCode = value;
                    // If this does not work, we try to use it as a language code
                    if (languageCode.length > 3) {
                        languageCode = [languageAsWord substringToIndex:2];
                    }
                    languageCode = languageCode.lowercaseString;
                    languageAsWord = [englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
                    if (!languageAsWord) {
                        // If that doesn't work either, we just use the raw string for both values
                        languageCode = value;
                        languageAsWord = value;
                    }
                }
                meta.language = languageCode;
                meta.languageAsWord = languageAsWord.capitalizedString;
            }
        }
    }
}


- (IBAction)editedStarValue:(id)sender {
    NSLevelIndicator *indicator = (NSLevelIndicator *)sender;
    NSNumber *value = indicator.objectValue;
    NSInteger integerValue;
    if (value == nil)
        integerValue = 0;
    else
        integerValue = value.integerValue;
    NSInteger row = [_gameTableView rowForView:sender];
    if ((NSUInteger)row >= _gameTableModel.count)
        return;
    Game *game = _gameTableModel[(NSUInteger)row];
    if (integerValue != game.metadata.myRating.integerValue && !(integerValue == 0 && game.metadata.myRating == nil)) {
        if (integerValue == 0)
            game.metadata.myRating = nil;
        else
            game.metadata.myRating = [NSString stringWithFormat:@"%@", value];
    }
}

- (IBAction)changedForgivenessValue:(id)sender {
    NSPopUpButton *popUp = (NSPopUpButton *)sender;
    NSInteger selInteger = popUp.selectedItem.tag;
    if (@available(macOS 10.14, *)) {
        if (selInteger == 0) {
            popUp.title = @"";
        }
    } else {
        if (selInteger == 0) {
            popUp.menu.itemArray[0].title = @"  ";
            popUp.title = @"  ";
        }
    }

    NSInteger row = [_gameTableView rowForView:sender];
    if ((NSUInteger)row >= _gameTableModel.count)
        return;
    Game *game = _gameTableModel[(NSUInteger)row];
    if (selInteger != game.metadata.forgivenessNumeric.integerValue) {
        if (selInteger == 0) {
            if (game.metadata.forgivenessNumeric == nil)
                return;
            game.metadata.forgivenessNumeric = nil;
            game.metadata.forgiveness = nil;
        } else {
            game.metadata.forgivenessNumeric = @(selInteger);
            game.metadata.forgiveness = forgiveness[game.metadata.forgivenessNumeric];
        }
    }
}
- (IBAction)pushedContextualButton:(id)sender {
    NSInteger row = [_gameTableView rowForView:sender];
    if ((NSUInteger)row >= _gameTableModel.count)
        return;
    if (![_gameTableView.selectedRowIndexes containsIndex:(NSUInteger)row])
        [_gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row] byExtendingSelection:NO];
    [NSMenu popUpContextMenu:_gameTableView.menu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)liked:(id)sender {
    NSInteger row = [_gameTableView rowForView:sender];
    if ((NSUInteger)row >= _gameTableModel.count)
        return;
    Game *game = _gameTableModel[(NSUInteger)row];
    if (game.like == 1) {
        game.like = 0;
    } else {
        game.like = 1;
    }
}

- (void)mouseOverChangedFromRow:(NSInteger)lastRow toRow:(NSInteger)currentRow {
    NSInteger likeColumnIdx = -1;
    NSInteger myRatingColumnIdx = -1;

    NSInteger index = 0;

    for (NSTableColumn *column in _gameTableView.tableColumns) {
        if (myRatingColumnIdx == -1 && [column.identifier isEqual:@"myRating"]) {
            myRatingColumnIdx = index;
        } else if (likeColumnIdx == -1 && [column.identifier isEqual:@"like"]) {
            likeColumnIdx = index;
        }

        index++;
    }

    RatingsCellView *rating;
    Game *game;

    if (lastRow > -1 && lastRow < (NSInteger)_gameTableModel.count) {
        game = _gameTableModel[(NSUInteger)lastRow];
        if (myRatingColumnIdx > -1 && game.metadata.myRating == 0) {
            rating = [_gameTableView viewAtColumn:myRatingColumnIdx row:lastRow makeIfNecessary:NO];
            rating.rating.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityWhileEditing;
            rating.needsDisplay = YES;
        }

        if (likeColumnIdx > -1 && game.like == 0) {
            LikeCellView *like = [_gameTableView viewAtColumn:likeColumnIdx row:lastRow makeIfNecessary:NO];
            like.likeButton.image = nil;
            like.needsDisplay = YES;
        }
    }

    if (currentRow > -1 && currentRow < (NSInteger)_gameTableModel.count) {
        game = _gameTableModel[(NSUInteger)currentRow];

        if (myRatingColumnIdx > -1 && game.metadata.myRating == 0) {
            rating = [_gameTableView viewAtColumn:myRatingColumnIdx row:currentRow makeIfNecessary:NO];
            rating.rating.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityAlways;
            rating.needsDisplay = YES;
        }


        if (likeColumnIdx > -1 && game.like == 0) {
            LikeCellView *like = [_gameTableView viewAtColumn:likeColumnIdx row:currentRow makeIfNecessary:NO];
            like.likeButton.image = [NSImage imageNamed:@"heart"];
            like.needsDisplay = YES;
        }
    }
}

- (CGFloat)tableView:(NSTableView *)tableView sizeToFitWidthOfColumn:(NSInteger)columnIndex {
    CGFloat longestWidth = 0.0;
    if (tableView == _gameTableView) {
        NSTableColumn *column = tableView.tableColumns[(NSUInteger)columnIndex];
        NSString *identifier = column.identifier;

        if ([identifier isEqual:@"found"] || [identifier isEqual:@"like"]) {
            return column.maxWidth;
        }

        BOOL isTitle = [identifier isEqual:@"title"];
        BOOL isFormat = [identifier isEqual:@"format"];
        BOOL isDate = ([identifier isEqual:@"added"] || [identifier isEqual:@"lastPlayed"]);
        BOOL isYear = [identifier isEqual:@"firstpublishedDate"];
        BOOL isModified = [identifier isEqual:@"lastModified"];

        BOOL isSortColumn = [_gameSortColumn isEqual:column.identifier];

        NSSize headerSize;
        headerSize = [column.headerCell.stringValue sizeWithAttributes:[column.headerCell.attributedStringValue attributesAtIndex:0 effectiveRange:nil]];

        longestWidth = headerSize.width + 9;

        // Add width of sort direction indicator
        if (isSortColumn) {
            longestWidth += 24;
        }

        if (@available(macOS 12.0, *)) {
        } else {
            if ([identifier isEqual:@"seriesnumber"] && longestWidth < 111) {
                return 111;
            }
        }

        if ([identifier isEqual:@"starRating"] || [identifier isEqual:@"myRating"] || [identifier isEqual:@"forgivenessNumeric"]) {
            return longestWidth;
        }

        for (Game *game in _gameTableModel) {
            NSString *string;
            if (isFormat) {
                string = game.detectedFormat;
            } else if (isDate) {
                string = [[game valueForKey: identifier] formattedRelativeString];
            } else if (isModified) {
                string = [game.metadata.lastModified formattedRelativeString];
            } else if (isYear) {
                string = game.metadata.firstpublished;
            } else {
                string = [game.metadata valueForKey:identifier];
            }
            if (string.length) {
                CGFloat viewWidth =  [string sizeWithAttributes:@{NSFontAttributeName: [NSFont systemFontOfSize:12]}].width + 5;
                // Add width of context menu button
                if (isTitle)
                    viewWidth += 25;
                if (viewWidth > longestWidth) {
                    longestWidth = viewWidth;
                    if (longestWidth >= column.maxWidth)
                        return column.maxWidth;
                }
            }
        }
    }

    return longestWidth;
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == _gameTableView) {
        NSIndexSet *rows = tableView.selectedRowIndexes;
        if (_gameTableModel.count && rows.count) {
            _selectedGames = [_gameTableModel objectsAtIndexes:rows];
            Game *game = _selectedGames[0];
            if (!game.theme) {
                game.theme = [Preferences currentTheme];
            } else {
                Preferences *prefs = Preferences.instance;
                if (prefs && prefs.currentGame == nil && !prefs.lightOverrideActive && !prefs.darkOverrideActive && self.view.window.keyWindow) {
                    [prefs restoreThemeSelection:game.theme];
                }
            }
        } else _selectedGames = @[];

        NSArray<Game *> *selected = _selectedGames;
        if (selected.count > 2)
            selected = @[_selectedGames[0], _selectedGames[1]];
        
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"UpdateSideView" object:selected]];
        [self invalidateRestorableState];
    }
}

- (NSString *)tableView:(NSTableView *)tableView typeSelectStringForTableColumn:(NSTableColumn *)tableColumn
                    row:(NSInteger)row {
    if ([tableColumn.identifier isEqualToString:@"title"]) {
        return _gameTableModel[(NSUInteger)row].metadata.title;
    }
    return nil;
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    _gameTableDirty = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateTableViews];
    });
    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSSet *insertedObjects = (notification.userInfo)[NSInsertedObjectsKey];

    for (id obj in updatedObjects) {
        if (countingMetadataChanges && [obj isKindOfClass:[Metadata class]]) {
            updatedMetadataCount++;
        }

        if ([obj isKindOfClass:[Theme class]]) {
            [self rebuildThemesSubmenu];
            break;
        }
    }

    for (id obj in insertedObjects) {
        if (countingMetadataChanges &&
            [obj isKindOfClass:[Metadata class]]) {
            insertedMetadataCount++;
        }
    }

    if (@available(macOS 11.0, *)) {
        NSArray *games = [Fetches fetchObjects:@"Game" predicate:@"hidden == NO" inContext:self.managedObjectContext];
        if (games.count == 0)
            self.view.window.subtitle = @"No games";
        else if (games.count == 1)
            self.view.window.subtitle = @"1 game";
        else
            self.view.window.subtitle = [NSString stringWithFormat:@"%ld games", games.count];
    }
}

#pragma mark -
#pragma mark Open game on double click, edit on click and wait

- (void)doClick:(id)sender {
    if (canEdit) {
        NSInteger row = _gameTableView.clickedRow;
        if ([_gameTableView.selectedRowIndexes containsIndex:(NSUInteger)row]) {
            TableViewController * __weak weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
                TableViewController *strongSelf = weakSelf;
                if (strongSelf && strongSelf->canEdit) {
                    NSInteger selectedRow = strongSelf.gameTableView.selectedRow;
                    NSInteger column = strongSelf.gameTableView.selectedColumn;
                    if (selectedRow != -1 && column != -1) {
                        [strongSelf.gameTableView editColumn:column row:selectedRow withEvent:nil select:YES];
                    }
                }
            });
        }
    }
}

// DoubleAction
- (void)doDoubleClick:(id)sender {
    if (_gameTableView.clickedRow == -1) {
        return;
    }
    [self enableClickToRenameAfterDelay];
    [self play:sender];
}

- (void)enableClickToRenameAfterDelay {
    canEdit = NO;
    TableViewController * __weak weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        TableViewController *strongSelf = weakSelf;
        if (strongSelf) {
            strongSelf->canEdit = YES;
        }
    });
}

@end
