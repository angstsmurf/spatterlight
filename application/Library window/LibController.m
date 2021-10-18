/*
 *
 */

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

#import "CoreDataManager.h"
#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Theme.h"
#import "Image.h"

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

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

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

@implementation LibHelperTableView

// NSResponder (super)
-(BOOL)becomeFirstResponder {
    BOOL flag = [super becomeFirstResponder];
    if (flag) {
        [(LibController *)self.delegate enableClickToRenameAfterDelay];
    }
    return flag;
}

- (void)keyDown:(NSEvent *)event {
    NSString *pressed = event.characters;
    if ([pressed isEqualToString:@" "])
        [(LibController *)self.delegate showGameInfo:nil];
    else
        [super keyDown:event];
}

// NSTableView delegate
-(BOOL)tableView:(NSTableView *)tableView
shouldEditTableColumn:(NSTableColumn *)tableColumn row:(int)rowIndex {
    return NO;
}

- (void)textDidEndEditing:(NSNotification *)notification {
    NSDictionary *userInfo;
    NSDictionary *newUserInfo;
    NSNumber *textMovement;
    int movementCode;

    userInfo = notification.userInfo;
    textMovement = userInfo[@"NSTextMovement"];
    movementCode = textMovement.intValue;

    // see if this a 'pressed-return' instance
    if (movementCode == NSReturnTextMovement) {
        // hijack the notification and pass a different textMovement value
        textMovement = @(NSIllegalTextMovement);
        newUserInfo = @{@"NSTextMovement" : textMovement};
        notification = [NSNotification notificationWithName:notification.name
                                                     object:notification.object
                                                   userInfo:newUserInfo];
    }

    [super textDidEndEditing:notification];
}

@end

@interface LibController () <NSDraggingDestination, NSWindowDelegate, NSSplitViewDelegate> {

    IBOutlet NSButton *infoButton;
    IBOutlet NSButton *playButton;
    IBOutlet NSPanel *importProgressPanel;
    IBOutlet NSView *exportTypeView;
    IBOutlet NSPopUpButton *exportTypeControl;

    IBOutlet NSMenu *headerMenu;

    BOOL gameTableDirty;

    BOOL canEdit;
    NSTimer *timer;

    NSString *searchString;
    CGFloat lastSideviewWidth;
    CGFloat lastSideviewPercentage;

    Game *currentSideView;

    /* for the importing */
    NSInteger cursrc;
    NSString *currentIfid;
    NSMutableArray *ifidbuf;
    NSMutableDictionary *metabuf;
    NSInteger errorflag;

    NSLocale *englishUSLocale;
    NSDictionary *languageCodes;

    NSManagedObjectContext *importContext;
    BOOL sideViewUpdatePending;
    NSMenuItem *enabledThemeItem;

    BOOL countingMetadataChanges;
    NSUInteger insertedMetadataCount, updatedMetadataCount;

    BOOL verifyIsCancelled;
    NSTimer *verifyTimer;
}

@end

@implementation LibController

#pragma mark Window controller, menus and file dialogs.

- (NSMutableDictionary *)load_mutable_plist:(NSString *)path {
    NSMutableDictionary *dict = nil;
    NSError *error;

    NSData *data = [NSData dataWithContentsOfFile:path];
    if (data) {
    dict = [NSPropertyListSerialization propertyListWithData:[NSData dataWithContentsOfFile:path] options:NSPropertyListMutableContainersAndLeaves format:nil error:&error];
    }
    if (!dict) {
        NSLog(@"libctl: cannot load plist: %@", error);
        dict = [[NSMutableDictionary alloc] init];
    }

    return dict;
}

- (instancetype)init {
    self = [super initWithWindowNibName:@"LibraryWindow"];
    if (self) {
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

        _coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;
        _managedObjectContext = _coreDataManager.mainManagedObjectContext;
    }
    return self;
}

- (void)windowDidLoad {
//    NSLog(@"libctl: windowDidLoad");

    self.windowFrameAutosaveName = @"LibraryWindow";
    _gameTableView.autosaveName = @"GameTable";
    _gameTableView.autosaveTableColumns = YES;

    _gameTableView.action = @selector(doClick:);
    _gameTableView.doubleAction = @selector(doDoubleClick:);
    _gameTableView.target = self;

    self.window.excludedFromWindowsMenu = YES;
    [self.window registerForDraggedTypes:@[ NSFilenamesPboardType ]];

    [infoButton setEnabled:NO];
    [playButton setEnabled:NO];

    _infoWindows = [[NSMutableDictionary alloc] init];
    _gameSessions = [[NSMutableDictionary alloc] init];
    _gameTableModel = [[NSMutableArray alloc] init];

    NSString *key;
    NSSortDescriptor *sortDescriptor;

    for (NSTableColumn *tableColumn in _gameTableView.tableColumns) {

        key = tableColumn.identifier;
        sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:key
                                                       ascending:YES];
        tableColumn.sortDescriptorPrototype = sortDescriptor;

        for (NSMenuItem *menuitem in headerMenu.itemArray) {
            if ([[menuitem valueForKey:@"identifier"] isEqualToString:key]) {
                menuitem.state = ![tableColumn isHidden];
                break;
            }
        }
    }

    NSArray *sortDescriptors = _gameTableView.sortDescriptors;

    if (!sortDescriptors.count)
        _gameTableView.sortDescriptors = @[ [NSSortDescriptor sortDescriptorWithKey:@"title" ascending:YES] ];

    // The "found" column seems to move about sometimes,
    // so we move it back to the front here.
    NSInteger foundColumnIndex = [_gameTableView columnWithIdentifier:@"found"];
    if (foundColumnIndex != 0)
        [_gameTableView moveColumn:foundColumnIndex toColumn:0];
 
    if (!_managedObjectContext) {
        NSLog(@"LibController windowDidLoad: no _managedObjectContext!");
        return;
    }

    _leftView.hidden = ![[NSUserDefaults standardUserDefaults] boolForKey:@"ShowSidebar"];

    if (_leftView.isHidden)
        [self collapseLeftView];
    else
        [self uncollapseLeftView];

    // Set up a dictionary to match language codes to languages
    // To be used when a user enters a language for a game
    englishUSLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    NSArray *ISOLanguageCodes = [NSLocale ISOLanguageCodes];
    NSMutableDictionary *mutablelanguageCodes = [NSMutableDictionary dictionaryWithCapacity:ISOLanguageCodes.count];
    NSString *languageWord;
    for (NSString *languageCode in ISOLanguageCodes) {
        languageWord = [englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
        if (languageWord) {
            [mutablelanguageCodes setObject:languageCode
                                     forKey:[englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode].lowercaseString];
        }
    }

    languageCodes = mutablelanguageCodes;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];


    _gameTableModel = [[self fetchObjects:@"Game" predicate:nil inContext:_managedObjectContext] mutableCopy];

    // Add metadata and games from plists to Core Data store if we have just created a new one
    if (_gameTableModel.count == 0 && [[NSUserDefaults standardUserDefaults] boolForKey:@"HasConvertedLibrary"] == NO) {
        [self convertLibraryToCoreData];
    }

    [self rebuildThemesSubmenu];
    [self performSelector:@selector(restoreSideViewSelection:) withObject:nil afterDelay:0.1];

    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12) {
        [self.window setValue:@2 forKey:@"tabbingMode"];
    }

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"RecheckForMissing"]) {
        [self startVerifyTimer];
        [self verifyInBackground:nil];
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"HasAskedToDownload"] == NO) {
        [self askToDownload];
    }
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
//    NSLog(@"libctl: windowWillReturnUndoManager!")
    return _managedObjectContext.undoManager;
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

        BOOL forceQuit = _forceQuitCheckBox.state == NSOnState;

        CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
        [index deleteSearchableItemsWithDomainIdentifiers:@[@"net.ccxvii.spatterlight"] completionHandler:^(NSError *blockerror){
            if (blockerror) {
                NSLog(@"Deleting all searchable items failed: %@", blockerror);
            }
        }];

        NSArray *entitiesToDelete = @[@"Metadata", @"Game", @"Ifid", @"Image"];

        NSMutableSet *gamesToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet *metadataToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet *imagesToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSSet *ifidsToKeep = [[NSSet alloc] init];

        for (GlkController *ctl in [_gameSessions allValues]) {
            if (forceQuit || !ctl.alive) {
                [ctl.window close];
            } else {
                Game *game = ctl.game;
                [gamesToKeep addObject:game];
                [metadataToKeep addObject:game.metadata];
                if (game.metadata.cover)
                    [imagesToKeep addObject:game.metadata.cover];
                ifidsToKeep  = [ifidsToKeep setByAddingObjectsFromSet:game.metadata.ifids];
            }
        }

        if (forceQuit) {
            _gameSessions = [NSMutableDictionary new];
        }

        for (NSString *entity in entitiesToDelete) {
            NSFetchRequest *fetchEntities = [[NSFetchRequest alloc] init];
            [fetchEntities setEntity:[NSEntityDescription entityForName:entity inManagedObjectContext:_managedObjectContext]];
            [fetchEntities setIncludesPropertyValues:NO]; //only fetch the managedObjectID

            NSError *error = nil;
            NSArray *objectsToDelete = [_managedObjectContext executeFetchRequest:fetchEntities error:&error];
            if (error)
                NSLog(@"deleteLibrary: %@", error);
            //error handling goes here

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

            objectsToDelete = [set allObjects];

            for (NSManagedObject *object in objectsToDelete) {
                [_managedObjectContext deleteObject:object];
            }
        }

        [self saveMainContext];
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

        NSArray *metadataEntriesToDelete =
        [self fetchObjects:@"Metadata" predicate:@"ANY games == NIL" inContext:_managedObjectContext];
        NSLog(@"Pruning %ld metadata entities", metadataEntriesToDelete.count);
        NSUInteger counter = metadataEntriesToDelete.count;

        for (Metadata *meta in metadataEntriesToDelete) {
            NSLog(@"Pruning metadata for %@", meta.title);
            [_managedObjectContext deleteObject:meta];
        }

        // Now we removed any orphaned images
        NSArray *imageEntriesToDelete = [self fetchObjects:@"Image" predicate:@"ANY metadata == NIL" inContext:_managedObjectContext];

        NSLog(@"Pruning %ld image entities", imageEntriesToDelete.count);
        counter += imageEntriesToDelete.count;
        for (Image *img in imageEntriesToDelete) {
            [_managedObjectContext deleteObject:img];
        }

        // And then any orphaned ifids
        NSArray *ifidEntriesToDelete = [self fetchObjects:@"Ifid" predicate:@"metadata == NIL" inContext:_managedObjectContext];

        counter += ifidEntriesToDelete.count;
        for (Ifid *ifid in ifidEntriesToDelete) {
            NSLog(@"Pruning ifid %@", ifid.ifidString);
            [_managedObjectContext deleteObject:ifid];
        }

        NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.window.screen];
        [notification showStandardWithText:[NSString stringWithFormat:@"%ld entit%@ pruned", counter, counter == 1 ? @"y" : @"ies"]];

        [self saveMainContext];
    }
}

#pragma mark Check library for missing files
- (IBAction)verifyLibrary:(id)sender{
    if ([self verifyAlert] == NSAlertFirstButtonReturn) {
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
    verifyIsCancelled = NO;

    NSManagedObjectContext *childContext = [_coreDataManager privateChildManagedObjectContext];
    childContext.undoManager = nil;

    LibController * __unsafe_unretained weakSelf = self;

    [childContext performBlock:^{
        LibController *strongSelf = weakSelf;
        [[NSNotificationCenter defaultCenter] addObserver:strongSelf
                                                 selector:@selector(noteBackgroundManagedObjectContextDidChange:)
                                                     name:NSManagedObjectContextDidSaveNotification
                                                   object:childContext];
        BOOL deleteMissing = [[NSUserDefaults standardUserDefaults] boolForKey:@"DeleteMissing"];
        for (Game *gameInMain in strongSelf.gameTableModel) {
            if (strongSelf->verifyIsCancelled) {
                strongSelf->verifyIsCancelled = NO;
                return;
            }
            Game *game = [childContext objectWithID:[gameInMain objectID]];
            if (game) {
                if (![[NSFileManager defaultManager] fileExistsAtPath:game.path] &&
                    ![[NSFileManager defaultManager] fileExistsAtPath:[game urlForBookmark].path]) {
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
            NSError *error = nil;
            if (childContext.hasChanges) {
                [childContext save:&error];
                if (error) {
                    NSLog(@"verifyInBackground: childContext save: %@", error);
                }
            }
        }
        [[NSNotificationCenter defaultCenter] removeObserver:strongSelf
                                                     name:NSManagedObjectContextDidSaveNotification
                                                   object:childContext];
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
    LibController * __unsafe_unretained weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!weakSelf.spinnerSpinning) {
            [weakSelf.progressCircle startAnimation:weakSelf];
            weakSelf.spinnerSpinning = YES;
        }
    });
}

- (void)endImporting {
    LibController * __unsafe_unretained weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (weakSelf.spinnerSpinning) {
            [weakSelf.progressCircle stopAnimation:weakSelf];
            weakSelf.spinnerSpinning = NO;
        }
        
        NSIndexSet *rows = weakSelf.gameTableView.selectedRowIndexes;

        if ((weakSelf.gameTableView.clickedRow != -1) && ![weakSelf.gameTableView isRowSelected:weakSelf.gameTableView.clickedRow])
            rows = [NSIndexSet indexSetWithIndex:(NSUInteger)weakSelf.gameTableView.clickedRow];
        if (rows.count)
            [weakSelf.gameTableView scrollRowToVisible:(NSInteger)rows.firstIndex];
    });
}

- (IBAction)importMetadata:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.prompt = NSLocalizedString(@"Import", nil);

    panel.allowedFileTypes = @[ @"iFiction", @"ifiction" ];

    LibController * __unsafe_unretained weakSelf = self; 

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;
            [weakSelf.managedObjectContext performBlock:^{
                LibController *strongSelf = weakSelf;
                [strongSelf waitToReportMetadataImport];
                [strongSelf beginImporting];
                [strongSelf importMetadataFromFile:url.path inContext:strongSelf.managedObjectContext];
                [strongSelf saveMainContext];
                [strongSelf endImporting];
            }];
        }
    }];
}

- (IBAction)exportMetadata:(id)sender {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.accessoryView = exportTypeView;
    panel.allowedFileTypes = @[ @"iFiction" ];
    panel.prompt = NSLocalizedString(@"Export", nil);
    panel.nameFieldStringValue = NSLocalizedString(@"Interactive Fiction Metadata.iFiction", nil);

    NSPopUpButton *localExportTypeControl = exportTypeControl;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
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

    LibController * __unsafe_unretained weakSelf = self;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
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
    NSLog(@"addInBackground %ld files lookForImages:%@ downloadInfo:%@", files.count, lookForImages ? @"YES" : @"NO", downloadInfo ? @"YES" : @"NO");

    if (_currentlyAddingGames)
        return;

    [self saveMainContext];

    NSManagedObjectContext *childContext = [_coreDataManager privateChildManagedObjectContext];
    childContext.undoManager = nil;

    [self beginImporting];
    
    _currentlyAddingGames = YES;
    _addButton.enabled = NO;

    NSDictionary *options = @{ @"context":childContext,
                               @"lookForImages":@(lookForImages),
                               @"downloadInfo":@(downloadInfo)
    };

    LibController * __unsafe_unretained weakSelf = self;

    [childContext performBlock:^{
        LibController *strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf addFiles:files options:options];
            [strongSelf endImporting];
            [FolderAccess releaseBookmark:[FolderAccess suitableDirectoryForURL:files.firstObject]];
            dispatch_async(dispatch_get_main_queue(), ^{
                strongSelf.addButton.enabled = YES;
                strongSelf.currentlyAddingGames = NO;
                [[NSNotificationCenter defaultCenter]
                 removeObserver:self
                 name:NSManagedObjectContextDidSaveNotification
                 object:weakSelf.managedObjectContext];
            });
        }
    }];
}

- (void)performFindPanelAction:(id<NSValidatedUserInterfaceItem>)sender {
    [_searchField selectText:self];
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
    Game *game = [[Preferences instance] currentGame];
    if (!game)
        return nil;
    GlkController *gctl = _gameSessions[game.ifid];
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
        NSURL *url = [game urlForBookmark];
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
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only delete game in clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    __block NSMutableArray *running = [NSMutableArray new];
    __block Game *game;

    LibController * __unsafe_unretained weakSelf = self;

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
         game = weakSelf.gameTableModel[idx];
         if (weakSelf.gameSessions[game.ifid] == nil)
             [weakSelf.managedObjectContext deleteObject:game];
        else
            [running addObject:game];
     }];

    NSUInteger count = running.count;
    if (count) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:NSLocalizedString(@"Are you sure?", nil)];
        [alert setInformativeText:[NSString stringWithFormat:@"%@ %@ currently in use. Do you want to close and delete %@?", [NSString stringWithSummaryOf:running], (count == 1) ? @"is" : @"are", (count == 1) ? @"it" : @"them"]];
        [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSInteger choice = [alert runModal];

        if (choice == NSAlertFirstButtonReturn) {
            for (Game *toDelete in running) {
                [_gameSessions[toDelete.ifid].window close];
                [_managedObjectContext deleteObject:toDelete];
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

    __block Game *game;
    __block NSString *urlString;

    LibController * __unsafe_unretained weakSelf = self;

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

- (void)downloadMetadataForGames:(NSArray<Game *> *)games {
    [self saveMainContext];
    LibController * __unsafe_unretained weakSelf = self;

    if (games.count == _gameTableModel.count)
        [_gameTableView selectRowIndexes:[NSIndexSet new] byExtendingSelection:NO];

    _nestedDownload = _currentlyAddingGames;

    _currentlyAddingGames = YES;
    _addButton.enabled = NO;

    __block NSMutableArray<NSManagedObjectID *> *blockGames = [NSMutableArray new];

    for (Game *gameInMain in games) {
        [blockGames addObject:gameInMain.objectID];
    }

    NSManagedObjectContext *childContext = _coreDataManager.privateChildManagedObjectContext;

    [childContext performBlock:^{

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(noteBackgroundManagedObjectContextDidChange:)
                                                     name:NSManagedObjectContextDidSaveNotification
                                                   object:childContext];

        LibController *strongSelf = weakSelf;
        IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithContext:childContext];
        BOOL result = NO;
        BOOL reportFailure = (games.count == 1);
        for (NSManagedObjectID *objectID in blockGames) {
            if (!strongSelf.currentlyAddingGames)
                break;
            [strongSelf beginImporting];

            Game *game = [childContext objectWithID:objectID];

            game.hasDownloaded = YES;

            NSLog(@"downloadGames: downloading metadata for game %@", game.metadata.title);
            result = [downloader downloadMetadataFor:game reportFailure:reportFailure imageOnly:NO];

            if (result) {
                NSError *error = nil;
                if (childContext.hasChanges) {
                    if (![childContext save:&error]) {
                        if (error) {
                            NSLog(@"Error: %@", error);
                        }
                    }
                    game.metadata.source = @(kIfdb);
                }
            }
        }

        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:NSManagedObjectContextDidSaveNotification
                                                      object:childContext];

        dispatch_async(dispatch_get_main_queue(), ^{
            if (strongSelf.nestedDownload) {
                strongSelf.nestedDownload = NO;
            } else {
                strongSelf.currentlyAddingGames = NO;
                strongSelf.addButton.enabled = YES;
            }
        });

        [strongSelf endImporting];
    }];
}

- (void)rebuildThemesSubmenu {

    enabledThemeItem = nil;
    NSMenu *themesMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Apply Theme", nil)];

    themesMenu.showsStateColumn = YES;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.sortDescriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES], [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES selector:@selector(localizedStandardCompare:)]];
    NSError *error = nil;
    
    NSArray *themes = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (themes == nil) {
        NSLog(@"Problem! %@",error);
    }

    for (Theme *theme in themes) {
        [themesMenu addItemWithTitle:theme.name action:@selector(applyTheme:) keyEquivalent:@""];
    }

    _themesSubMenu.submenu = themesMenu;
}

- (IBAction) applyTheme:(id)sender {

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow != -1) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    NSString *name = ((NSMenuItem *)sender).title;

    Theme *theme = [self findTheme:name inContext:self.managedObjectContext];

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

    Game *selectedGame = _gameTableModel[[rows firstIndex]];

    NSSet *gamesWithTheme = selectedGame.theme.games;
    
    NSIndexSet *matchingIndexes = [_gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
        return [gamesWithTheme containsObject:obj];
    }];

    [_gameTableView selectRowIndexes:matchingIndexes byExtendingSelection:NO];
}

- (IBAction)deleteSaves:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;
    LibController * __unsafe_unretained weakSelf = self;

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

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;
    NSInteger count = _gameTableView.numberOfSelectedRows;

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if (_gameTableView.clickedRow != -1 &&
        (![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])) {
        count = 1;
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];
    }

    if (action == @selector(applyTheme:)) {
        if (enabledThemeItem != nil) {
            for (NSMenuItem *item in _themesSubMenu.submenu.itemArray) {
                item.state = NSOffState;
            }
            enabledThemeItem = nil;
        }
        if (count > 0 && count < 10000) {
            __block NSMutableSet *themeNamesToSelect = [NSMutableSet new];
            [rows enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
                NSString *name = _gameTableModel[idx].theme.name;
                if (name)
                    [themeNamesToSelect addObject:name];
            }];
            NSControlStateValue state = NSOnState;
            if (themeNamesToSelect.count > 1)
                state = NSMixedState;
            for (NSString *name in themeNamesToSelect) {
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

    if (action == @selector(delete:) || action == @selector(deleteGame:)) {
        return (count != 0);
    }

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

    if (action == @selector(myToggleSidebar:))
    {
        NSString* title = [_leftView isHidden] ? NSLocalizedString(@"Show Sidebar", nil) : NSLocalizedString(@"Hide Sidebar", nil);
        ((NSMenuItem*)menuItem).title = title;
    }

    return YES;
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
        [self showInfoForGame:game];
        // Don't open more than 20 info windows at once
        if (counter++ > 20)
            break;
    }
}

- (void)showInfoForGame:(Game *)game {
    InfoController *infoctl;

    NSString *path = game.path;
    if (!path)
        path = [game urlForBookmark].path;
    if (!path)
        return;
    // First, we check if we have created this info window already
    infoctl = _infoWindows[path];

    if (!infoctl) {
        infoctl = [[InfoController alloc] initWithGame:game];
        infoctl.libcontroller = self;
        _infoWindows[path] = infoctl;
        NSWindow *infoWindow = infoctl.window;
        infoWindow.restorable = YES;
        infoWindow.restorationClass = [AppDelegate class];
        infoWindow.identifier = [NSString stringWithFormat:@"infoWin%@", path];

        NSRect targetFrame = infoctl.window.frame;

        [infoctl hideWindow];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [infoctl animateIn:targetFrame];
        });
    } else {
        [infoctl showWindow:nil];
    }
}

- (void)closeAndOpenNextAbove:(InfoController *)infocontroller {
    NSUInteger index = [_gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound && index > 0) {
        [_gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index - 1] byExtendingSelection:NO];
        [infocontroller.window performClose:nil];
        [self showInfoForGame:_gameTableModel[index - 1]];
    }
}

- (void)closeAndOpenNextBelow:(InfoController *)infocontroller {
    NSUInteger index = [_gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound && index < _gameTableModel.count - 1) {
        [_gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index + 1] byExtendingSelection:NO];
        [infocontroller.window performClose:nil];
        [self showInfoForGame:_gameTableModel[index + 1]];
    }
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
    NSPasteboard *pboard = [sender draggingPasteboard];
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

- (void)addMetadata:(NSDictionary*)dict forIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSDateFormatter *dateFormatter;
    Metadata *entry;
    NSString *key;
    NSString *keyVal;

    NSDictionary *forgiveness = @{
                                  @"" : @(FORGIVENESS_NONE),
                                  @"Cruel" : @(FORGIVENESS_CRUEL),
                                  @"Nasty" : @(FORGIVENESS_NASTY),
                                  @"Tough" : @(FORGIVENESS_TOUGH),
                                  @"Polite" : @(FORGIVENESS_POLITE),
                                  @"Merciful" : @(FORGIVENESS_MERCIFUL)
                                  };


    entry = [self fetchMetadataForIFID:ifid inContext:context]; // this should always return nil
    if (!entry)
    {
        entry = (Metadata *) [NSEntityDescription
                              insertNewObjectForEntityForName:@"Metadata"
                              inManagedObjectContext:context];
        Ifid *ifidObj = (Ifid *) [NSEntityDescription
                                              insertNewObjectForEntityForName:@"Ifid"
                                              inManagedObjectContext:context];
        ifidObj.ifidString = ifid;
        ifidObj.metadata = entry;
    } else {
        NSLog(@"addMetaData:forIFIDs: Error! Found existing Metadata object with game %@", entry.title);
        return;
    }

    for(key in dict) {
        keyVal = dict[key];
        if ([key isEqualToString:@"description"]) {
            [entry setValue:keyVal forKey:@"blurb"];
        } else if ([key isEqualToString:kSource]) {
            NSInteger intVal = (NSInteger)dict[kSource];
            if (intVal == kUser)
                entry.userEdited=@(YES);
            else
                entry.userEdited=@(NO);
            entry.source = @(intVal);
        } else if ([key isEqualToString:@"firstpublished"]) {
            if (dateFormatter == nil)
                dateFormatter = [[NSDateFormatter alloc] init];
            if (keyVal.length == 4)
                dateFormatter.dateFormat = @"yyyy";
            else if (keyVal.length == 10)
                dateFormatter.dateFormat = @"yyyy-MM-dd";
            else NSLog(@"Illegal date format in plist!");

            entry.firstpublishedDate = [dateFormatter dateFromString:keyVal];
            dateFormatter.dateFormat = @"yyyy";
            entry.firstpublished = [dateFormatter stringFromDate:entry.firstpublishedDate];
        } else if ([key isEqualToString:@"language"]) {
            // In IFDB xml data, "language" is usually a language code such as "en"
            // but may also be a locale code such as "en-US" or adescriptive string
            // like "English, French (en, fr)." We try to deal with all of them here.
            NSString *languageCode = keyVal;
            if (languageCode.length > 1) {
                if (languageCode.length > 3) {
                    // If it is longer than three characters, we use the first two
                    // as language code. This seems to cover all known cases.
                    languageCode = [languageCode substringToIndex:2];
                }
                NSString *language = [englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
                if (language) {
                    entry.languageAsWord = language;
                } else {
                    // Otherwise we use the full string for both language and languageAsWord.
                    entry.languageAsWord = keyVal;
                    languageCode = keyVal;
                }
            }
            entry.language = languageCode;
        } else if ([key isEqualToString:@"format"]) {
            if (entry.format == nil)
                entry.format = dict[@"format"];
        } else if ([key isEqualToString:@"forgiveness"]) {
            entry.forgiveness = dict[@"forgiveness"];
            entry.forgivenessNumeric = forgiveness[keyVal];
        }
        else
        {
            [entry setValue:keyVal forKey:key];
        }
    }
}

- (Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context {

    NSError *error = nil;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:context];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];

    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"findTheme: inContext: %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"findTheme: inContext: Found more than one Theme object with name %@ (total %ld)",name, fetchedObjects.count);
     } else if (fetchedObjects.count == 0) {
        NSLog(@"findTheme: inContext: Found no Ifid object with with name %@", name);
        return nil;
    }

    return fetchedObjects[0];
}


- (Metadata *)fetchMetadataForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Ifid" inManagedObjectContext:context];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifidString like[c] %@",ifid];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchMetadataForIFID: %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"fetchMetadataForIFID: Found more than one Ifid object with ifidString %@",ifid);
    }
    else if (fetchedObjects.count == 0)
    {
        return nil;
    }

    return ((Ifid *)fetchedObjects[0]).metadata;
}

- (Game *)fetchGameForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@",ifid];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one entry with metadata %@",ifid);
    }
    else if (fetchedObjects.count == 0)
    {
        return nil;
    }

    return fetchedObjects[0];
}

- (NSArray *)fetchObjects:(NSString *)entityName predicate:(nullable NSString *)predicate inContext:(NSManagedObjectContext *)context {

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:context];
    fetchRequest.entity = entity;
    fetchRequest.predicate = [NSPredicate predicateWithFormat:predicate];

    NSError *error = nil;
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    return fetchedObjects;
}

- (void)convertLibraryToCoreData {

    // Add games from plist files to Core Data store if we have just created a new one

    LibController * __unsafe_unretained weakSelf = self;

    NSManagedObjectContext *private = [_coreDataManager privateChildManagedObjectContext];
    private.undoManager = nil;

    [self beginImporting];
    _addButton.enabled = NO;
    _currentlyAddingGames = YES;

    [private performBlock:^{

        LibController *strongSelf = weakSelf;

        // First, we try to load the Metadata.plist and add all entries as Metadata entities
        NSMutableDictionary *metadata = [strongSelf load_mutable_plist:[strongSelf.homepath.path stringByAppendingPathComponent: @"Metadata.plist"]];

        NSString *ifid;

        NSEnumerator *enumerator = [metadata keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [strongSelf addMetadata:metadata[ifid] forIFID:ifid inContext:private];
        }

        // Second, we try to load the Games.plist and add all entries as Game entities
        NSDictionary *games = [strongSelf load_mutable_plist:[strongSelf.homepath.path stringByAppendingPathComponent: @"Games.plist"]];

        NSDate *timestamp = [NSDate date];
        
        enumerator = [games keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [strongSelf beginImporting];
            Metadata *meta = [strongSelf fetchMetadataForIFID:ifid inContext:private];

            Game *game;

            if (!meta || meta.games.count) {
                // If we did not create a matching Metadata entity for this Game above, we just
                // import it again, creating new metadata. This could happen if the user has deleted
                // the Metadata.plist but not the Games.plist file, or if the Metadata and Games plists
                // have gone out of sync somehow.
                game = [strongSelf importGame:[games valueForKey:ifid] inContext:private reportFailure:NO hide:NO];
                if (game)
                    meta = game.metadata;
            } else {
                // Otherwise we simply use the Metadata entity we created
                game = (Game *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Game"
                                 inManagedObjectContext:private];
            }
            // Now we should have a Game with corresponding Metadata
            // (but we check anyway just to make sure)
            if (meta) {
                if (!game) {
                    NSLog(@"No game?");
                    continue;
                }

                game.ifid = ifid;
                game.metadata = meta;
                game.added = [NSDate date];
                [game bookmarkForPath:[games valueForKey:ifid]];
                game.path = [games valueForKey:ifid];

                // First, we look for a cover image file in Spatterlight Application Support folder
                NSURL *imgpath = [NSURL URLWithString:[ifid stringByAppendingPathExtension:@"tiff"] relativeToURL:strongSelf.imageDir];
                NSData *imgdata;

                if ([[NSFileManager defaultManager] fileExistsAtPath:imgpath.path]) {
                    imgdata = [[NSData alloc] initWithContentsOfURL:imgpath];
                }

                if (imgdata) {
                    game.metadata.coverArtURL = imgpath.path;

                    // If that fails, we try Babel
                } else {
                    NSString *path = (NSString *)[games valueForKey:ifid];
                    void *context = get_babel_ctx();
                    const char *format = babel_init_ctx((char *)path.UTF8String, context);
                    if (format) {
                        if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path]]) {
                            Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];
                            imgdata = [blorb coverImageData];
                            game.metadata.coverArtURL = path;
                        }
                    }
                    babel_release_ctx(context);
                }

                if (imgdata) {
                    IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithContext:private];
                    [downloader insertImageData:imgdata inMetadata:meta];
                }

            } else NSLog (@"Error! Could not create Game entity for game with ifid %@ and path %@", ifid, [games valueForKey:ifid]);

            if ([timestamp timeIntervalSinceNow] < -0.5) {
                NSError *error = nil;
                if (private.hasChanges) {
                    if (![private save:&error]) {
                        NSLog(@"Unable to Save Changes of private managed object context!");
                        if (error) {
                            [[NSApplication sharedApplication] presentError:error];
                        }
                    } //else NSLog(@"Changes in private were saved");
                } //else NSLog(@"No changes to save in private");
                timestamp = [NSDate date];
            }
        }
        
        NSError *error = nil;
        if (private.hasChanges) {
            if (![private save:&error]) {
                NSLog(@"Unable to Save Changes of private managed object context!");
                if (error) {
                    [[NSApplication sharedApplication] presentError:error];
                }
            } else NSLog(@"Changes in private were saved");
        } else NSLog(@"No changes to save in private");

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf saveMainContext];
            [strongSelf endImporting];
            strongSelf.addButton.enabled = YES;
            strongSelf.currentlyAddingGames = NO;
            [strongSelf askToDownload];
        });
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasConvertedLibrary"];
    }];
}

- (void)askToDownload {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"HasAskedToDownload"])
        return;

    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hasDownloaded = YES"];

    fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"askToDownload: Could not fetch Game entities: %@",error);
        return;
    }

    if (fetchedObjects.count == 0)
    {
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];
        fetchRequest.predicate = nil;
        fetchRequest.includesPropertyValues = NO;
        fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects.count < 5)
            return;

        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasAskedToDownload"];

        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Do you want to download game info from IFDB in the background?", nil);
        alert.informativeText = NSLocalizedString(@"This will only be done once. You can do this later by selecting games in the library and choosing Download Info from the contextual menu.", nil);
        [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];


        [alert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {

            if (result == NSAlertFirstButtonReturn) {
                [self downloadMetadataForGames:fetchedObjects];
            }
        }];
    }
}

- (Metadata *)importMetadataFromXML:(NSData *)mdbuf inContext:(NSManagedObjectContext *)context {
    IFictionMetadata *metadata = [[IFictionMetadata alloc] initWithData:mdbuf andContext:context];
    if (!metadata || metadata.stories.count == 0)
        return nil;
    return ((IFStory *)(metadata.stories)[0]).identification.metadata;
}

- (BOOL)importMetadataFromFile:(NSString *)filename inContext:(NSManagedObjectContext *)context{
    NSLog(@"libctl: importMetadataFromFile %@", filename);

    NSData *data = [NSData dataWithContentsOfFile:filename];
    if (!data)
        return NO;

    Metadata *metadata = [self importMetadataFromXML:data inContext:context];
    if (!metadata)
        return NO;
    metadata.source = @(kExternal);
    return YES;
}

- (void)waitToReportMetadataImport {
    insertedMetadataCount = 0;
    updatedMetadataCount = 0;
    countingMetadataChanges = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self performSelector:@selector(reportChangedMetadata:) withObject:nil afterDelay:1];
    });
}

- (void)reportChangedMetadata:(id)sender {
    countingMetadataChanges = NO;
    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.window.screen];
    if (insertedMetadataCount || updatedMetadataCount == 0) {
        NSLog(@"reportChangedMetadata: %ld inserted", insertedMetadataCount);
        [notification showStandardWithText:[NSString stringWithFormat:@"%ld entr%@ imported", insertedMetadataCount, insertedMetadataCount == 1 ? @"y" : @"ies"]];
    }
    if (updatedMetadataCount) {
        NSLog(@"reportChangedMetadata: %ld updated", updatedMetadataCount);
        double delayInSeconds = 0;
        if (insertedMetadataCount)
            delayInSeconds = 2;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            [notification showStandardWithText:[NSString stringWithFormat:@"%ld entr%@ updated", self->updatedMetadataCount, self->updatedMetadataCount == 1 ? @"y" : @"ies"]];
        });
    }
}

static void write_xml_text(FILE *fp, Metadata *info, NSString *key) {
    NSString *val;
    const char *tagname;
    const char *s;

    val = [info valueForKey:key];
    if (!val)
        return;

    //"description" is a reserved word in core data
    if ([key isEqualToString:@"blurb"])
        key = @"description";

    NSLog(@"write_xml_text: key:%@ val:%@", key, val);

    tagname = key.UTF8String;
    s = [[NSString stringWithFormat:@"%@", val] UTF8String];

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


    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    
    fetchRequest.entity = [NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:_managedObjectContext];
    
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
    NSArray *metadata = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (!metadata) {
        NSLog(@"exportMetadataToFile: Could not fetch metadata list. Error: %@", error);
    }

    if (metadata.count == 0) {
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
            if ([data isJPEG])
                type = @"jpg";
            else if (![data isPNG])
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
        game = meta.games.anyObject;
    } else if ([object isKindOfClass:[Game class]]) {
        game = (Game *)object;
        meta = game.metadata;
    } else if ([object isKindOfClass:[Image class]]) {
        Image *image = (Image *)object;
        meta = image.metadata.anyObject;
        game = meta.games.anyObject;
    } else {
        NSLog(@"Object: %@", [object class]);
    }

    if (game && [_gameTableModel indexOfObject:game] == NSNotFound) {
        _searchField.stringValue = @"";
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

    double delayInSeconds = 0.3;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [self selectGames:[NSSet setWithObject:game]];

        if ([self.splitView isSubviewCollapsed:self.leftView]) {
            [self uncollapseLeftView];
        }
    });
}

- (void)saveMainContext {
    if (!_managedObjectContext.hasChanges)
        return;
    if (@available(macOS 10.13, *)) {
        NSError *error = nil;
        [_managedObjectContext save:&error];
        if (error)
            NSLog(@"LibController saveMainContext error: %@", error);
    } else {
        [self.coreDataManager saveChanges];
    }
}

#pragma mark Actually starting the game

- (NSWindow *) playGame:(Game *)game {
    return [self playGame:game winRestore:NO];
}

- (NSWindow *)playGameWithIFID:(NSString *)ifid {
    Game *game = [self fetchGameForIFID:ifid inContext:_managedObjectContext];
    if (!game) return nil;
    return [self playGame:game winRestore:YES];
}

- (NSWindow *)playGame:(Game *)game
            winRestore:(BOOL)systemWindowRestoration {

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

    NSURL *url = [game urlForBookmark];
    NSString *path = url.path;
    NSString *terp;
    GlkController *gctl = _gameSessions[game.ifid];

    NSFileManager *fileManager = [NSFileManager defaultManager];

    if (gctl) {
        NSLog(@"A game with this ifid is already in session");
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
    NSURL *trashUrl = [fileManager URLForDirectory:NSTrashDirectory inDomain:NSUserDomainMask appropriateForURL:[NSURL fileURLWithPath:path] create:NO error:&error];
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
        gctl.window.identifier = [NSString stringWithFormat:@"gameWin%@", game.ifid];
    } else
        gctl.window.restorable = NO;

    gctl.window.title = game.metadata.title;

    __unsafe_unretained LibController *weakSelf = self;

    [gctl askForAccessToURL:url showDialog:!systemWindowRestoration andThenRunBlock:^{
        [weakSelf addURLtoRecents: url];
        weakSelf.gameSessions[game.ifid] = gctl;
        game.lastPlayed = [NSDate date];
        [Preferences changeCurrentGame:game];
        [gctl runTerp:terp withGame:game reset:NO winRestore:systemWindowRestoration];
    }];

    return gctl.window;
}

- (void)releaseGlkControllerSoon:(GlkController *)glkctl {
    NSString *key = nil;
    for (GlkController *controller in _gameSessions.allValues)
        if (controller == glkctl) {
            NSArray *temp = [_gameSessions allKeysForObject:controller];
            key = [temp objectAtIndex:0];
            break;
        }
    if (key) {
        glkctl.window.delegate = nil;
        [_gameSessions removeObjectForKey:key];
        [self performSelector:@selector(releaseGlkControllerNow:) withObject:glkctl afterDelay:1];
    }
}

- (void)releaseGlkControllerNow:(GlkController *)glkctl {
}

- (NSWindow *)importAndPlayGame:(NSString *)path {

    BOOL hide = ![[NSUserDefaults standardUserDefaults] boolForKey:@"AddToLibrary"];

    Game *game = [self importGame:path inContext:_managedObjectContext reportFailure:YES hide:hide];
    if (game)
    {
        [self selectAndPlayGame:game];
    }
    return nil;
}

- (NSWindow *)selectAndPlayGame:(Game *)game {
    [self selectGames:[NSSet setWithArray:@[game]]];
    return [self playGame:game];
}

- (Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide {
    GameImporter *importer = [GameImporter new];
    importer.libController = self;
    return [importer importGame:path inContext:context reportFailure:report hide:hide];
}

- (IBAction)cancel:(id)sender {
    _currentlyAddingGames = NO;
    _nestedDownload = NO;
    verifyIsCancelled = YES;
}

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options {

    NSManagedObjectContext *context = options[@"context"];
    NSMutableArray *select = nil;

    //Don't select every game after import if we start with no games
    if (_gameTableModel.count)
        select = [NSMutableArray arrayWithCapacity: urls.count];

    BOOL reportFailure = NO;

    if (urls.count == 1) {
        // If the user only selects one file, and it is
        // not a directory, we should report any failures.
        BOOL isDir;
        [[NSFileManager defaultManager] fileExistsAtPath:((NSURL *)urls[0]).path isDirectory: &isDir];
        if (!isDir)
            reportFailure = YES;
    }

    NSMutableDictionary *newOptions = options.mutableCopy;
    newOptions[@"reportFailure"] = @(reportFailure);
    newOptions[@"select"] = select;
    
    [self beginImporting];
    GameImporter *importer = [GameImporter new];
    importer.libController = self;
    [importer addFiles:urls options:newOptions];
    [self endImporting];

    NSError *error = nil;
    if (context.hasChanges) {
        if (![context save:&error]) {
            if (error) {
                NSLog(@"Error: %@", error);
            }
        }
    }

    if (!_currentlyAddingGames)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        [self selectGamesWithIfids:select scroll:YES];
    });

    LibController * __unsafe_unretained weakSelf = self;

    NSManagedObjectContext *private = [_coreDataManager privateChildManagedObjectContext];
    private.undoManager = nil;

    [private performBlock:^{
        LibController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        strongSelf.currentlyAddingGames = NO;

        if (strongSelf.iFictionFiles.count) {
            [self waitToReportMetadataImport];
            for (NSString *path in strongSelf.iFictionFiles) {
                [strongSelf importMetadataFromFile:path inContext:private];
            }
        }
        strongSelf.iFictionFiles = nil;
        // Fix metadata entities with no ifids
        NSArray *noIfids = [self fetchObjects:@"Metadata" predicate:@"ifids.@count == 0" inContext:private];
        for (Metadata *meta in noIfids) {
            [private deleteObject:meta];
        }
        // Fix metadata entities with empty titles
        NSArray *noTitles = [self fetchObjects:@"Metadata" predicate:@"title = nil OR title = ''" inContext:private];
        for (Metadata *meta in noTitles) {
            Game *game = meta.games.anyObject;
            if (game) {
                meta.title = [game urlForBookmark].lastPathComponent;
            }
        }
        if (private.hasChanges) {
            NSError *blockError = nil;
            [private save:&blockError];
            if (blockError)
                NSLog(@"%@", blockError);
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf saveMainContext];
            });
        }
    }];
}

#pragma mark -
#pragma mark Table magic

- (IBAction)searchForGames:(id)sender {
    searchString = [sender stringValue];

    searchString = [searchString stringByReplacingOccurrencesOfString:@"\"" withString:@""];

    gameTableDirty = YES;
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

- (NSRect)rectForLineWithIfid:(NSString*)ifid {
    Game *game;
    NSRect frame = NSZeroRect;
    NSRect myFrame = self.window.frame;
    frame.origin.x = myFrame.origin.x + myFrame.size.width / 2;
    frame.origin.y = myFrame.origin.y + myFrame.size.height / 2;

    if (ifid.length) {
        game = [self fetchGameForIFID:ifid inContext:_managedObjectContext];
        if ([_gameTableModel containsObject:game]) {
            NSUInteger index = [_gameTableModel indexOfObject:game];
            frame = [_gameTableView rectOfRow:(NSInteger)index];
            frame = [_gameTableView convertRect:frame toView:nil];
            frame = [[self window] convertRectToScreen:frame];
        }
    }
    return frame;
}

- (void) selectGamesWithIfids:(NSArray*)ifids scroll:(BOOL)shouldscroll
{
    if (ifids.count) {
        NSMutableArray *newSelection = [NSMutableArray arrayWithCapacity:ifids.count];
        for (NSString *ifid in ifids) {
            Game *game = [self fetchGameForIFID:ifid inContext:_managedObjectContext];
            if (game) {
                [newSelection addObject:game];
                //NSLog(@"Selecting game with ifid %@", ifid);
            } else NSLog(@"No game with ifid %@ in library, cannot restore selection", ifid);
        }
        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

        for (Game *game in newSelection) {
            if ([_gameTableModel containsObject:game]) {
                [indexSet addIndex:[_gameTableModel indexOfObject:game]];
            }
        }
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        _selectedGames = [_gameTableModel objectsAtIndexes:indexSet];
        if (shouldscroll && indexSet.count && !_currentlyAddingGames)
            [_gameTableView scrollRowToVisible:(NSInteger)indexSet.firstIndex];
    }
}

- (void)selectGames:(NSSet*)games
{
    //NSLog(@"selectGames called with %ld games", games.count);

    if (games.count) {
        NSIndexSet *indexSet = [_gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
            return [games containsObject:obj];
        }];
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        _selectedGames = [_gameTableModel objectsAtIndexes:indexSet];
        if (indexSet.count && indexSet.count < 50 && !_currentlyAddingGames)
            [_gameTableView scrollRowToVisible:(NSInteger)indexSet.firstIndex];
    }
}

+ (NSInteger)stringcompare:(NSString *)a with:(NSString *)b {
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedCaseInsensitiveCompare: b];
}

+ (NSInteger)compareGame:(Metadata *)a with:(Metadata *)b key:(id)key ascending:(BOOL)ascending {
    NSString * ael = [a valueForKey:key];
    NSString * bel = [b valueForKey:key];
    return [LibController compareString:ael withString:bel ascending:ascending];
}

+ (NSInteger)compareString:(NSString *)ael withString:(NSString *)bel ascending:(BOOL)ascending {
    if ((!ael || ael.length == 0) && (!bel || bel.length == 0))
        return NSOrderedSame;
    if (!ael || ael.length == 0) return ascending ? NSOrderedDescending :  NSOrderedAscending;
    if (!bel || bel.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [LibController stringcompare:ael with:bel];
}

+ (NSInteger)compareDate:(NSDate *)ael withDate:(NSDate *)bel ascending:(BOOL)ascending {
    if ((!ael) && (!bel))
        return NSOrderedSame;
    if (!ael) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!bel) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [ael compare:bel];
}


+ (NSPredicate *)searchPredicateForWord:(NSString *)word {
    return [NSPredicate predicateWithFormat: @"(detectedFormat contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.languageAsWord contains [c] %@) OR (metadata.firstpublished contains %@)", word, word, word, word, word, word, word, word, word, word, word];
}

- (void) updateTableViews
{
    if (!gameTableDirty)
        return;

    //NSLog(@"Updating table view");

    NSError *error = nil;
    NSArray<Game *> *searchResult = nil;

    NSFetchRequest *fetchRequest = [NSFetchRequest new];
    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];

    NSMutableArray<NSPredicate *> *predicateArray = [NSMutableArray new];

    NSPredicate *notHidden = [NSPredicate predicateWithFormat:@"hidden == NO"];

    [predicateArray addObject:notHidden];

    NSCompoundPredicate *comp;

    BOOL alreadySearched = NO;

    if (searchString.length)
    {
        // First we search using the entire search string as a phrase
        // such as "beyond zork"

        [predicateArray addObject:[LibController searchPredicateForWord:searchString]];

        comp = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];

        fetchRequest.predicate = comp;
        error = nil;
        searchResult = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];
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
                         [LibController searchPredicateForWord:word]];
                }
            }
        }
    }

    if (!searchResult.count && !alreadySearched) {
        comp = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];
        fetchRequest.predicate = comp;
        error = nil;
        searchResult = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];
    }

    _gameTableModel = searchResult.mutableCopy;
    if (_gameTableModel == nil)
        NSLog(@"Problem! %@",error);

    LibController * __unsafe_unretained weakSelf = self;

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:_sortAscending comparator:^(Game *aid, Game *bid) {

        LibController *strongSelf = weakSelf;

        Metadata *a = aid.metadata;
        Metadata *b = bid.metadata;
        NSInteger cmp;
        if ([strongSelf.gameSortColumn isEqual:@"firstpublishedDate"])
        {
            cmp = [LibController compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([strongSelf.gameSortColumn isEqual:@"added"] || [strongSelf.gameSortColumn isEqual:@"lastPlayed"])
        {
            cmp = [LibController compareDate:[aid valueForKey:strongSelf.gameSortColumn] withDate:[bid valueForKey:strongSelf.gameSortColumn] ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([strongSelf.gameSortColumn isEqual:@"lastModified"]) {
            cmp = [LibController compareDate:[a valueForKey:strongSelf.gameSortColumn] withDate:[b valueForKey:strongSelf.gameSortColumn] ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([strongSelf.gameSortColumn isEqual:@"found"]) {
            NSString *string1 = aid.found?nil:@"A";
            NSString *string2 = bid.found?nil:@"A";
            cmp = [LibController compareString:string1 withString:string2 ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([strongSelf.gameSortColumn isEqual:@"forgivenessNumeric"]) {
            NSString *string1 = a.forgivenessNumeric ? [NSString stringWithFormat:@"%@", a.forgivenessNumeric] : nil;
            NSString *string2 = b.forgivenessNumeric ? [NSString stringWithFormat:@"%@", b.forgivenessNumeric] : nil;
            cmp = [LibController compareString:string1 withString:string2 ascending:strongSelf.sortAscending];

            if (cmp) return cmp;
        }
        else if (strongSelf.gameSortColumn)
        {
            cmp = [LibController compareGame:a with:b key:strongSelf.gameSortColumn ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        cmp = [LibController compareGame:a with:b key:@"title" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [LibController compareGame:a with:b key:@"author" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [LibController compareGame:a with:b key:@"seriesnumber" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [LibController compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        return [LibController compareString:aid.detectedFormat withString:bid.detectedFormat ascending:strongSelf.sortAscending];
    }];

    [_gameTableModel sortUsingDescriptors:@[sort]];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self.gameTableView reloadData];
        [self selectGames:[NSSet setWithArray:self->_selectedGames]];
        self->gameTableDirty = NO;
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
        gameTableDirty = YES;
        [self updateTableViews];
        NSIndexSet *rows = tableView.selectedRowIndexes;
        [_gameTableView scrollRowToVisible:(NSInteger)rows.firstIndex];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    if (tableView == _gameTableView)
        return (NSInteger)_gameTableModel.count;
    return 0;
}

- (id) tableView: (NSTableView*)tableView
objectValueForTableColumn: (NSTableColumn*)column
             row:(NSInteger)row
{
    if (tableView == _gameTableView) {
        if ((NSUInteger)row >= _gameTableModel.count) return nil;
        Game *game = _gameTableModel[(NSUInteger)row];
        Metadata *meta = game.metadata;
        if ([column.identifier isEqual: @"found"]) {
            return game.found?nil:@"!";
        } else if ([column.identifier isEqual: @"format"]) {
            if (!game.detectedFormat)
                game.detectedFormat = meta.format;
            return game.detectedFormat;
        } else if ([column.identifier isEqual: @"added"] || [column.identifier  isEqual: @"lastPlayed"]) {
            return [[game valueForKey: column.identifier] formattedRelativeString];
        } else if ([column.identifier  isEqual: @"lastModified"]){
            return [[meta valueForKey: column.identifier] formattedRelativeString];
        } else {
            return [meta valueForKey: column.identifier];
        }
    }
    return nil;
}

- (void)tableView:(NSTableView *)tableView
  willDisplayCell:(id)cell
   forTableColumn:(NSTableColumn *)tableColumn
              row:(NSInteger)row {

    CGFloat offset = 3; // Seems to look okay

    if (cell == _foundIndicatorCell) {
        NSMutableAttributedString *attstr = [((NSTextFieldCell *)cell).attributedStringValue mutableCopy];

        NSFont *font = [NSFont fontWithName:@"Exclamation Circle New" size:14];

        [attstr addAttribute:NSFontAttributeName
                       value:font
                       range:NSMakeRange(0, attstr.length)];

        [attstr addAttribute:NSBaselineOffsetAttributeName
                       value:@(offset)
                       range:NSMakeRange(0, attstr.length)];

        [(NSTextFieldCell *)cell setAttributedStringValue:attstr];
    }
}

- (void)tableView:(NSTableView *)tableView
   setObjectValue:(id)value
   forTableColumn:(NSTableColumn *)tableColumn
              row:(NSInteger)row {
    if (tableView == _gameTableView)
    {
        Game *game = _gameTableModel[(NSUInteger)row];
        Metadata *meta = game.metadata;
        NSString *key = tableColumn.identifier;
        NSString *oldval = [meta valueForKey:key];

        if ([key isEqualToString:@"forgivenessNumeric"] && [value isEqual:@(FORGIVENESS_NONE)]) {
            value = nil;
            if ([oldval isEqual:@(FORGIVENESS_NONE)])
                oldval = nil;
        }

        if (([key isEqualToString:@"starRating"] || [key isEqualToString:@"myRating"]) && [value isEqual:@(0)])
            value = nil;

        if ([value isKindOfClass:[NSNumber class]] && ![key isEqualToString:@"forgivenessNumeric"]) {
                value = [NSString stringWithFormat:@"%@", value];
        }

        if ([value isKindOfClass:[NSString class]] && ((NSString*)value).length == 0)
            value = nil;
        
        if ([value isEqual: oldval] || (value == oldval))
            return;

        [meta setValue: value forKey: key];
        meta.userEdited = @(YES);
        meta.source = @(kUser);
        game.metadata.lastModified = [NSDate date];

        if ([key isEqualToString:@"firstpublishedDate"]) {
            NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
            dateFormatter.dateFormat = @"yyyy";
            meta.firstpublished = [dateFormatter stringFromDate:meta.firstpublishedDate];
        }

        else if ([key isEqualToString:@"languageAsWord"]) {
            // The player may have entered a language code such as "en"
            // but more likely a full word ("English".)
            // so we try to deal with both cases here to give proper
            // values to language as well as languageAsWord
            NSString *languageAsWord = value;
            NSString *languageCode = value;
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
                NSLog(@"Set value of language to %@", languageCode);
                meta.languageAsWord = languageAsWord.capitalizedString;
            }
        }

        NSLog(@"Set value of %@ to %@", key, value);
    }
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == _gameTableView) {
        NSIndexSet *rows = tableView.selectedRowIndexes;
        infoButton.enabled = rows.count > 0;
        playButton.enabled = rows.count == 1;
        [self invalidateRestorableState];
        if (_gameTableModel.count && rows.count) {
            _selectedGames = [_gameTableModel objectsAtIndexes:rows];
            Game *game = _selectedGames[0];
            if (!game.theme)
                game.theme = [Preferences currentTheme];
            else {
                Preferences *prefs = Preferences.instance;
                if (prefs && prefs.currentGame == nil)
                    [prefs restoreThemeSelection:game.theme];
            }
        } else _selectedGames = nil;
        [self updateSideViewForce:NO];
    }
}

- (NSString *)tableView:(NSTableView *)tableView typeSelectStringForTableColumn:(NSTableColumn *)tableColumn
                    row:(NSInteger)row
{
    if ([[tableColumn identifier] isEqualToString:@"title"])
    {
        NSInteger tableColumnIndex = (NSInteger)[[tableView tableColumns] indexOfObject:tableColumn];
        return [[tableView preparedCellAtColumn:tableColumnIndex
                                            row:row] stringValue];
    }
    return nil;
}

- (void)noteBackgroundManagedObjectContextDidChange:(NSNotification *)notification {
    NSManagedObjectContext *mainContext = _managedObjectContext;
    [mainContext performBlock:^{
        [mainContext mergeChangesFromContextDidSaveNotification: notification];
        [self noteManagedObjectContextDidChange:notification];
    }];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    //NSLog(@"noteManagedObjectContextDidChange");
    gameTableDirty = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateTableViews];
    });
    NSArray *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSMutableArray *correspondingObjects = [[NSMutableArray alloc] initWithCapacity:updatedObjects.count];

    for (id obj in updatedObjects) {
        if (countingMetadataChanges && [obj isKindOfClass:[Metadata class]]) {
            updatedMetadataCount++;
        }

        if ([obj isKindOfClass:[Theme class]]) {
            [self rebuildThemesSubmenu];
            break;
        }
        [correspondingObjects addObject:[_managedObjectContext objectWithID:[obj objectID]]];
    }

    if (countingMetadataChanges) {
        NSArray *insertedObjects = (notification.userInfo)[NSInsertedObjectsKey];

        for (id obj in insertedObjects) {
            if ([obj isKindOfClass:[Metadata class]]) {
                insertedMetadataCount++;
            }
        }
    }

    if ([correspondingObjects containsObject:currentSideView.metadata] || [correspondingObjects containsObject:currentSideView.metadata.cover])
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateSideViewForce:YES];
        });
    }
}

- (void) updateSideViewForce:(BOOL)force
{
    if ([_splitView isSubviewCollapsed:_leftView])
        return;

    Game *game = nil;
    NSString *string = nil;

    if (!_selectedGames || !_selectedGames.count || _selectedGames.count > 1) {

        string = (_selectedGames.count > 1) ? @"Multiple selections" : @"No selection";

    } else {
        game = _selectedGames[0];
    }

    if (force == NO && game && game == currentSideView) {
        // If game is already shown and force is NO,
        // don't recreate the side view
        return;
    }

    [_leftScrollView.documentView performSelector:@selector(removeFromSuperview)];

    if (NSWidth(_leftView.frame) < ACTUAL_LEFT_VIEW_MIN_WIDTH)
        return;
    
    SideInfoView *infoView = [[SideInfoView alloc] initWithFrame:_leftScrollView.bounds];
    
    _leftScrollView.documentView = infoView;
//    _sideIfid.delegate = infoView;

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

    currentSideView = game;
}

#pragma mark -
#pragma mark SplitView stuff

- (void)restoreSideViewSelection:(id)sender {
    [self updateSideViewForce:YES];
}

- (void)windowDidResignKey:(NSNotification *)notification {
    if ([_leftScrollView.documentView isKindOfClass:[SideInfoView class]]) {
        SideInfoView *sideView = (SideInfoView *)_leftScrollView.documentView;
        [sideView deselectImage];
    }
}

- (BOOL)splitView:(NSSplitView *)splitView
canCollapseSubview:(NSView *)subview
{
    if (subview == _leftView) return YES;
    return NO;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMaximumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    CGFloat result = NSWidth(splitView.frame) / 2;
    if (result < RIGHT_VIEW_MIN_WIDTH)
        result = NSWidth(splitView.frame) - RIGHT_VIEW_MIN_WIDTH;

    return result;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    CGFloat result = PREFERRED_LEFT_VIEW_MIN_WIDTH;

    if (NSWidth(splitView.frame) - PREFERRED_LEFT_VIEW_MIN_WIDTH < RIGHT_VIEW_MIN_WIDTH)
        result = ACTUAL_LEFT_VIEW_MIN_WIDTH;

    return result;
}

-(IBAction)myToggleSidebar:(id)sender
{
    if ([_splitView isSubviewCollapsed:_leftView]) {
        [self uncollapseLeftView];
    } else {
        [self collapseLeftView];
    }
}

-(void)collapseLeftView
{
    lastSideviewWidth = _leftView.frame.size.width;
    lastSideviewPercentage = lastSideviewWidth / self.window.frame.size.width;

    _leftView.hidden = YES;
    [_splitView setPosition:0
           ofDividerAtIndex:0];
    NSSize minSize = self.window.minSize;
    minSize.width =  RIGHT_VIEW_MIN_WIDTH;
    self.window.minSize = minSize;
    _leftViewConstraint.priority = NSLayoutPriorityDefaultLow;
    [_splitView display];
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"ShowSidebar"];
}

- (NSSize)windowWillResize:(NSWindow *)sender
                    toSize:(NSSize)frameSize {
    if (![_splitView isSubviewCollapsed:_leftView] &&
    (_leftView.frame.size.width < PREFERRED_LEFT_VIEW_MIN_WIDTH - 1
        || _rightView.frame.size.width < RIGHT_VIEW_MIN_WIDTH - 1)) {
        [self collapseLeftView];
    }

    return frameSize;
}

-(void)uncollapseLeftView
{
     lastSideviewWidth = lastSideviewPercentage *  self.window.frame.size.width;
    _leftViewConstraint.priority = 999;

    _leftView.hidden = NO;

    CGFloat dividerThickness = _splitView.dividerThickness;

    // make sideview at least PREFERRED_LEFT_VIEW_MIN_WIDTH
    if (lastSideviewWidth < PREFERRED_LEFT_VIEW_MIN_WIDTH)
        lastSideviewWidth = PREFERRED_LEFT_VIEW_MIN_WIDTH;
    
    if (self.window.frame.size.width < PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + dividerThickness) {
        [self.window setContentSize:NSMakeSize(PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + dividerThickness, ((NSView *)self.window.contentView).frame.size.height)];
    }

    NSSize minSize = self.window.minSize;
    minSize.width = RIGHT_VIEW_MIN_WIDTH + PREFERRED_LEFT_VIEW_MIN_WIDTH + dividerThickness;
    if (minSize.width > self.window.frame.size.width)
        minSize.width  = self.window.frame.size.width;
    self.window.minSize = minSize;

    [_splitView setPosition:lastSideviewWidth
           ofDividerAtIndex:0];

    [_splitView display];
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"ShowSidebar"];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
    if (NSWidth(_splitView.frame) < ACTUAL_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH && ![_splitView isSubviewCollapsed:_leftView]) {
        NSRect frame = self.window.frame;
        frame.size.width = PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + 10;
        [_splitView setPosition:PREFERRED_LEFT_VIEW_MIN_WIDTH - 10
               ofDividerAtIndex:0];
        [self.window setFrame:frame display:NO animate:YES];
    }

    Metadata *meta = currentSideView.metadata;

    if (sideViewUpdatePending || (meta.blurb.length == 0 && meta.author.length == 0 && meta.headline.length == 0 && meta.cover == nil) ) {
        [self updateSideViewForce:NO];
        
        if ([_leftScrollView.documentView isKindOfClass:[SideInfoView class]]) {
            SideInfoView *sideView = (SideInfoView *)_leftScrollView.documentView;
            [sideView deselectImage];
            [sideView updateTitle];
        }
    } else {
        sideViewUpdatePending = YES;
        double delayInSeconds = 0.3;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            self->sideViewUpdatePending = NO;
            [self updateSideViewForce:YES];
        });

    }
        
}

#pragma mark -
#pragma mark Windows restoration

- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state {
    [state encodeObject:_searchField.stringValue forKey:@"searchText"];

    [state encodeDouble:lastSideviewPercentage forKey:@"sideviewPercent"];
    [state encodeDouble:NSWidth(_leftView.frame) forKey:@"sideviewWidth"];
//    NSLog(@"Encoded left view width as %f", NSWidth(_leftView.frame))
    [state encodeBool:[_splitView isSubviewCollapsed:_leftView] forKey:@"sideviewHidden"];
//    NSLog(@"Encoded left view collapsed as %@", [_splitView isSubviewCollapsed:_leftView]?@"YES":@"NO");

    if (_selectedGames.count) {
        NSMutableArray *selectedGameIfids = [NSMutableArray arrayWithCapacity:_selectedGames.count];
        NSString *str;
        for (Game *game in _selectedGames) {
            str = game.ifid;
            if (str)
                [selectedGameIfids addObject:str];
        }
        [state encodeObject:selectedGameIfids forKey:@"selectedGames"];
//        NSLog(@"Encoded %ld selected games", (unsigned long)selectedGameIfids.count);
    }
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    NSString *searchText = [state decodeObjectOfClass:[NSString class] forKey:@"searchText"];
    gameTableDirty = YES;
    if (searchText.length) {
        //        NSLog(@"Restored searchbar text %@", searchText);
        _searchField.stringValue = searchText;
        [self searchForGames:_searchField];
    }
    NSArray *selectedIfids = [state decodeObjectOfClass:[NSArray class] forKey:@"selectedGames"];
    [self updateTableViews];
    [self selectGamesWithIfids:selectedIfids scroll:NO];

    CGFloat newDividerPos = [state decodeDoubleForKey:@"sideviewWidth"];
    lastSideviewPercentage = [state decodeDoubleForKey:@"sideviewPercent"];
    CGFloat newDividerPosbyPercentage = self.window.frame.size.width * lastSideviewPercentage;

    if (lastSideviewPercentage > 0 && newDividerPos > 0 && newDividerPosbyPercentage < newDividerPosbyPercentage)
        newDividerPos = self.window.frame.size.width * lastSideviewPercentage;

    if (newDividerPos < 50 && newDividerPos > 0) {
        NSLog(@"Left view width too narrow, setting to 50.");
        newDividerPos = 50;
    }
    
    [_splitView setPosition:newDividerPos
ofDividerAtIndex:0];
    
     //NSLog(@"Restored left view width as %f", NSWidth(_leftView.frame));
     //NSLog(@"Now _leftView.frame is %@", NSStringFromRect(_leftView.frame));

    BOOL collapsed = [state decodeBoolForKey:@"sideviewHidden"];
    //NSLog(@"Decoded left view visibility as %@", collapsed?@"NO":@"YES");

    if (collapsed) {
        [self collapseLeftView];
    } else {
        if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ShowSidebar"]) {
            [self uncollapseLeftView];
        }
    }
    double delayInSeconds = 0.2;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [self updateSideViewForce:YES];
    });
}

#pragma mark -
#pragma mark Add to Open Recent menu stuff

- (void)addURLtoRecents:(NSURL *)url {
    [((AppDelegate *)[NSApplication sharedApplication].delegate)
        addToRecents:@[ url ]];
}

#pragma mark -
#pragma mark Open game on double click, edit on click and wait

- (void)doClick:(id)sender {
    if (canEdit) {
        NSInteger row = _gameTableView.clickedRow;
        if (row >= 0) {
            [self startTimerWithTimeInterval:0.5
                                    selector:@selector(renameByTimer:)];
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
    [self startTimerWithTimeInterval:0.2
                            selector:@selector(enableClickToRenameByTimer:)];
}

- (void)enableClickToRenameByTimer:(id)sender {
    canEdit = YES;
}

- (void)renameByTimer:(id)sender {
    if (canEdit) {
        NSInteger row = _gameTableView.selectedRow;
        NSInteger column = _gameTableView.selectedColumn;

        if (row != -1 && column != -1) {
            [_gameTableView editColumn:column row:row withEvent:nil select:YES];
        }
    }
}

- (void)startTimerWithTimeInterval:(NSTimeInterval)seconds
                          selector:(SEL)selector {
    [self stopTimer];
    timer = [NSTimer scheduledTimerWithTimeInterval:seconds
                                             target:self
                                           selector:selector
                                           userInfo:nil
                                            repeats:NO];
}

- (void)stopTimer {
    if (timer != nil) {
        if ([timer isValid]) {
            [timer invalidate];
        }
    }
}

@end
