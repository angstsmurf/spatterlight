/*
 *
 */

#import "Compatibility.h"
#import "NSString+Categories.h"
#import "NSDate+relative.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "Image.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Theme.h"
#import "SideInfoView.h"
#import "InfoController.h"
#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFIdentification.h"
#import "IFDBDownloader.h"
#import "main.h"

#import "Blorb.h"
#import "BlorbResource.h"
#include "iff.h"

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

@implementation LibHelperWindow
- (NSDragOperation)draggingEntered:sender {
    return [(LibController *)self.delegate draggingEntered:sender];
}
- (void)draggingExited:sender {
    [(LibController *)self.delegate draggingEntered:sender];
}
- (BOOL)prepareForDragOperation:sender {
    return [(LibController *)self.delegate prepareForDragOperation:sender];
}
- (BOOL)performDragOperation:sender {
    return [(LibController *)self.delegate performDragOperation:sender];
}
@end

@implementation LibHelperTableView

// NSResponder (super)
-(BOOL)becomeFirstResponder {
    BOOL flag = [super becomeFirstResponder];
    if (flag) {
        [(LibController *)self.delegate enableClickToRenameAfterDelay];
    }
    return flag;
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

    NSArray *searchStrings;
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
            NSLog(@"libctl: Could not find Application Support directory! "
                  @"Error: %@",
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

    [[Preferences instance] createDefaultThemes];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];

    // Add metadata and games from plists to Core Data store if we have just created a new one
    _gameTableModel = [[self fetchObjects:@"Game" predicate:nil inContext:_managedObjectContext] mutableCopy];

    [self rebuildThemesSubmenu];
    [self performSelector:@selector(restoreSideViewSelection:) withObject:nil afterDelay:0.1];

    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"HasConvertedLibrary"]) {
        [self convertLibraryToCoreData];
    }
}

- (void)restoreSideViewSelection:(id)sender {
    [self updateSideViewForce:YES];
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
//    NSLog(@"libctl: windowWillReturnUndoManager!")
    return _managedObjectContext.undoManager;
}


- (IBAction)deleteLibrary:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:NSLocalizedString(@"Do you really want to delete the library?", nil)];
    [alert setInformativeText:[NSString stringWithFormat:NSLocalizedString(@"This will empty your game list and delete all metadata. %@\nThe original game files will not be affected.", nil),
                               (_gameSessions.count) ? NSLocalizedString(@"Currently running games will be skipped. ", nil) : @""]];
    [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {

        NSArray *entitiesToDelete = @[@"Metadata", @"Game", @"Ifid", @"Image"];

        NSMutableSet *gamesToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet *metadataToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSMutableSet *imagesToKeep = [[NSMutableSet alloc] initWithCapacity:_gameSessions.count];
        NSSet *ifidsToKeep = [[NSSet alloc] init];

        for (GlkController *ctl in [_gameSessions allValues]) {
            Game *game = ctl.game;
            [gamesToKeep addObject:game];
            [metadataToKeep addObject:game.metadata];
            [imagesToKeep addObject:game.metadata.cover];
            ifidsToKeep  = [ifidsToKeep setByAddingObjectsFromSet:game.metadata.ifids];
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

        [_coreDataManager saveChanges];
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
        NSFetchRequest *orphanedMetadata = [[NSFetchRequest alloc] init];
        [orphanedMetadata setEntity:[NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:_managedObjectContext]];

        orphanedMetadata.predicate = [NSPredicate predicateWithFormat: @"ANY games == NIL"];

        NSError *error = nil;
        NSArray *metadataEntriesToDelete = [_managedObjectContext executeFetchRequest:orphanedMetadata error:&error];
        //error handling goes here
        NSLog(@"Pruning %ld metadata entities", metadataEntriesToDelete.count);
        for (Metadata *meta in metadataEntriesToDelete) {
            NSLog(@"Pruning metadata for %@", meta.title)
            [_managedObjectContext deleteObject:meta];
        }
        [_coreDataManager saveChanges];
    }
}

- (BOOL)lookForMissingFile:(Game *)game {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(@"Cannot find the file.", nil);
    alert.informativeText = [NSString stringWithFormat: NSLocalizedString(@"The game file for \"%@\" could not be found at its original location. Do you want to look for it?", nil), game.metadata.title];
    [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        panel.prompt = NSLocalizedString(@"Open", nil);

        NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                                           fromBookmarkData:(NSData *)game.fileLocation];

        NSString *path = [values objectForKey:NSURLPathKey];

        if (!path)
            path = game.path;

        NSString *extension = path.pathExtension;
        if (extension)
            panel.allowedFileTypes = @[extension];

        LibController * __unsafe_unretained weakSelf = self;

        [panel beginSheetModalForWindow:self.window
                      completionHandler:^(NSInteger result) {
                          if (result == NSModalResponseOK) {
                              NSString *newPath = ((NSURL *)panel.URLs[0]).path;
                              NSString *ifid = [weakSelf ifidFromFile:newPath];
                              if (ifid && [ifid isEqualToString:game.ifid]) {
                                  [game bookmarkForPath:newPath];
                                  game.found = YES;
                                  [self lookForMoreMissingFilesInFolder:newPath.stringByDeletingLastPathComponent];
                              } else {
                                  if (ifid) {
                                      NSAlert *anAlert = [[NSAlert alloc] init];
                                      anAlert.alertStyle = NSInformationalAlertStyle;
                                      anAlert.messageText = NSLocalizedString(@"Not a match.", nil);
                                      anAlert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"This file does not match the game \"%@.\"\nDo you want to replace it anyway?", nil), game.metadata.title];
                                      [anAlert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
                                      [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
                                      NSInteger response = [anAlert runModal];
                                      if (response == NSAlertFirstButtonReturn) {
                                          if ([weakSelf importGame:newPath inContext:weakSelf.managedObjectContext reportFailure:YES]) {
                                              [weakSelf.managedObjectContext deleteObject:game];
                                          }
                                      }

                                  } else {
                                      NSAlert *anAlert = [[NSAlert alloc] init];
                                      anAlert.messageText = NSLocalizedString(@"Not a match.", nil);
                                      anAlert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"This file does not match the game \"%@.", nil), game.metadata.title];
                                      [anAlert runModal];
                                  }
                              }
                          }
                      }];
    }
    return NO;
}

- (NSString *)ifidFromFile:(NSString *)path {

    void *context = get_babel_ctx();
    char *format = babel_init_ctx((char*)path.UTF8String, context);
    if (!format || !babel_get_authoritative_ctx(context))
    {
        NSAlert *anAlert = [[NSAlert alloc] init];
        anAlert.messageText = NSLocalizedString(@"Unknown file format.", nil);
        anAlert.informativeText = NSLocalizedString(@"Babel can not identify the file format.", nil);
        [anAlert runModal];
        babel_release_ctx(context);
        return nil;
    }

    char buf[TREATY_MINIMUM_EXTENT];

    int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, context);
    if (rv <= 0)
    {
        NSAlert *anAlert = [[NSAlert alloc] init];
        anAlert.messageText = NSLocalizedString(@"Fatal error.", nil);
        anAlert.informativeText = NSLocalizedString(@"Can not compute IFID from the file.", nil);
        [anAlert runModal];
        babel_release_ctx(context);
        return nil;
    }

    babel_release_ctx(context);
    return @(buf);
}

- (void)lookForMoreMissingFilesInFolder:(NSString *)directory {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"found == NO"];

    fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"lookForMoreMissingFilesInFolder: %@",error);
    }

    if (fetchedObjects.count == 0) {
        return; //Found no missing files in library.
    }
    NSMutableDictionary *filenames = [[NSMutableDictionary alloc] initWithCapacity:fetchedObjects.count];
    NSDictionary *values;

    NSString *filename;
    for (Game *game in fetchedObjects) {
        values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                             fromBookmarkData:(NSData *)game.fileLocation];
        filename = [values objectForKey:NSURLPathKey];
        if (!filename)
            filename = game.path;

        NSString *dirname = [filename stringByDeletingLastPathComponent];
        dirname = dirname.lastPathComponent;

        NSString *searchPath = [directory stringByAppendingPathComponent:filename.lastPathComponent];
        if ([[NSFileManager defaultManager] fileExistsAtPath:searchPath] && [[self ifidFromFile:searchPath] isEqualToString:game.ifid]) {
            [filenames setObject:game forKey:searchPath];
        } else {
            //Check inside folders as well, one level down, for good measure
            searchPath = [directory stringByAppendingPathComponent:dirname];
            searchPath = [searchPath stringByAppendingPathComponent:filename.lastPathComponent];
            if ([[NSFileManager defaultManager] fileExistsAtPath:searchPath] && [[self ifidFromFile:searchPath] isEqualToString:game.ifid])
                [filenames setObject:game forKey:searchPath];
        }
    }

    fetchedObjects = [filenames allValues];

    if (filenames.count > 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.alertStyle = NSInformationalAlertStyle;
        alert.messageText = [NSString stringWithFormat:NSLocalizedString(@"%@ %@ also in this folder.", nil), [NSString stringWithSummaryOf:fetchedObjects], (fetchedObjects.count > 1) ? NSLocalizedString(@"are", nil) : NSLocalizedString(@"is", nil)];
        alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Do you want to update %@ as well?", nil), (fetchedObjects.count > 1) ? @"them" : @"it"];
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"Cancel"];

        NSModalResponse choice = [alert runModal];
        if (choice == NSAlertFirstButtonReturn) {
            for (filename in filenames) {
                Game *game = [filenames objectForKey:filename];
                NSLog(@"Updating game %@ with new path %@", game.metadata.title, filename);
                [game bookmarkForPath:filename];
                game.found = YES;
            }
        }
    }
}

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
                [weakSelf beginImporting];
                [weakSelf importMetadataFromFile:url.path];
                [weakSelf.coreDataManager saveChanges];
                [weakSelf endImporting];
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

- (void)addIfictionFile:(NSString *)file {
    if (!_iFictionFiles)
        _iFictionFiles = [NSMutableArray arrayWithObject:file];
    else
        [_iFictionFiles addObject:file];
}

- (IBAction)addGamesToLibrary:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseDirectories:YES];
    panel.prompt =NSLocalizedString(@"Add", nil);

    panel.allowedFileTypes = gGameFileTypes;

    LibController * __unsafe_unretained weakSelf = self;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSModalResponseOK) {
                          NSArray *urls = panel.URLs;
                          [weakSelf addInBackground:urls];
                      }
                  }];
}

- (void)addInBackground:(NSArray *)files {

    if (_currentlyAddingGames)
        return;

    LibController * __unsafe_unretained weakSelf = self;
    NSManagedObjectContext *childContext = [_coreDataManager privateChildManagedObjectContext];
    childContext.undoManager = nil;
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(backgroundManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:childContext];

    [self beginImporting];
    
    _currentlyAddingGames = YES;
    _addButton.enabled = NO;

    [childContext performBlock:^{
        [weakSelf addFiles:files inContext:childContext];
        [weakSelf endImporting];
        dispatch_async(dispatch_get_main_queue(), ^{
            weakSelf.addButton.enabled = YES;
            weakSelf.currentlyAddingGames = NO;
        });

    }];
}

- (void)performFindPanelAction:(id<NSValidatedUserInterfaceItem>)sender {
    [_searchField selectText:self];
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

- (IBAction)showGameInfo:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    NSUInteger i;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = _gameTableModel[i];
        [self showInfoForGame:game];
    }
}

- (void)showInfoForGame:(Game *)game {
    InfoController *infoctl;

    NSString *path = [game urlForBookmark].path;
    if (!path)
        path = game.path;
    // First, we check if we have created this info window already
    infoctl = _infoWindows[[game urlForBookmark].path];

    if (!infoctl) {
        infoctl = [[InfoController alloc] initWithGame:game];
        NSWindow *infoWindow = infoctl.window;
        infoWindow.restorable = YES;
        infoWindow.restorationClass = [AppDelegate class];
        if (path) {
            infoWindow.identifier = [NSString stringWithFormat:@"infoWin%@", path];
            _infoWindows[path] = infoctl;
        }
    }

    [infoctl showWindow:nil];
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

    __block Game *game;

    LibController * __unsafe_unretained weakSelf = self;

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
         game = weakSelf.gameTableModel[idx];
         if (weakSelf.gameSessions[game.ifid] == nil)
             [weakSelf.managedObjectContext deleteObject:game];
     }];
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

- (IBAction) download:(id)sender
{
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow != -1) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)_gameTableView.clickedRow];

    if (rows.count > 0)
    {
        LibController * __unsafe_unretained weakSelf = self;
        NSManagedObjectContext *childContext = [_coreDataManager privateChildManagedObjectContext];
        childContext.undoManager = nil;

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(backgroundManagedObjectContextDidChange:)
                                                     name:NSManagedObjectContextObjectsDidChangeNotification
                                                   object:childContext];
        _currentlyAddingGames = YES;
        _addButton.enabled = NO;

        [childContext performBlock:^{
            IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithContext:childContext];
            BOOL result = NO;
            for (Game *game in [weakSelf.gameTableModel objectsAtIndexes:rows]) {
                [weakSelf beginImporting];
                
                //It makes some kind of sense to also check if the game file still exists while downloading metadata
                if (![[NSFileManager defaultManager] isReadableFileAtPath:[game urlForBookmark].path])
                    game.found = NO;

                if (game.metadata.tuid) {
                    result = [downloader downloadMetadataForTUID:game.metadata.tuid];
                } else {
                    for (Ifid *ifidObj in game.metadata.ifids) {
                        result = [downloader downloadMetadataForIFID:ifidObj.ifidString];
                        if (result)
                            break;
                    }
                }
                if (result) {
                    NSError *error = nil;
                    if (childContext.hasChanges) {
                        if (![childContext save:&error]) {
                            if (error) {
                                [[NSApplication sharedApplication] presentError:error];
                            }
                            game.metadata.source = @(kIfdb);
                        }
                    }
                }
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                weakSelf.currentlyAddingGames = NO;
                weakSelf.addButton.enabled = YES;
                if (NSAppKitVersionNumber < NSAppKitVersionNumber10_9) {

                    [weakSelf.coreDataManager saveChanges];
                    for (Game *aGame in [weakSelf.gameTableModel objectsAtIndexes:rows]) {
                        [weakSelf.managedObjectContext refreshObject:aGame.metadata
                                                mergeChanges:YES];
                    }
                    [weakSelf updateSideViewForce:YES];
                }
            });

            [weakSelf endImporting];
        }];
    }
}

- (void)rebuildThemesSubmenu {

    NSMenu *themesMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Apply Theme", nil)];
    
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
    
    NSIndexSet *matchingIndexes = [_gameTableModel indexesOfObjectsPassingTest:^BOOL(NSString *obj, NSUInteger idx, BOOL *stop) {
        return [gamesWithTheme containsObject:obj];
    }];

    [_gameTableView selectRowIndexes:matchingIndexes byExtendingSelection:NO];
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
        if (count == 0)
            return NO;
        NSArray *selection = [_gameTableModel objectsAtIndexes:rows];
        for (Game *game in selection) {
            if (_gameSessions[game.ifid] == nil)
                return YES;
        }
        return NO;
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

    if (action == @selector(toggleSidebar:))
    {
        NSString* title = [_leftView isHidden] ? NSLocalizedString(@"Show Sidebar", nil) : NSLocalizedString(@"Hide Sidebar", nil);
        ((NSMenuItem*)menuItem).title = title;
    }

    return YES;
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
        [self addInBackground:urls];
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

- (void) addMetadata:(NSDictionary*)dict forIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
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
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:context];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
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

- (NSArray *) fetchObjects:(NSString *)entityName predicate:(nullable NSString *)predicate inContext:(NSManagedObjectContext *)context {

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

- (void) convertLibraryToCoreData {

    // Add games from plist files to Core Data store if we have just created a new one

    LibController * __unsafe_unretained weakSelf = self;

    NSManagedObjectContext *private = [_coreDataManager privateChildManagedObjectContext];
    private.undoManager = nil;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(backgroundManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:private];
    [self beginImporting];
    _addButton.enabled = NO;
    _currentlyAddingGames = YES;

    [private performBlock:^{
        // First, we try to load the Metadata.plist and add all entries as Metadata entities
        NSMutableDictionary *metadata = [weakSelf load_mutable_plist:[weakSelf.homepath.path stringByAppendingPathComponent: @"Metadata.plist"]];

        NSString *ifid;

        NSEnumerator *enumerator = [metadata keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [weakSelf addMetadata:metadata[ifid] forIFID:ifid inContext:private];
        }

        // Second, we try to load the Games.plist and add all entries as Game entities
        NSDictionary *games = [weakSelf load_mutable_plist:[weakSelf.homepath.path stringByAppendingPathComponent: @"Games.plist"]];

        NSDate *timestamp = [NSDate date];
        
        enumerator = [games keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [weakSelf beginImporting];
            Metadata *meta = [weakSelf fetchMetadataForIFID:ifid inContext:private];

            Game *game;

            if (!meta || meta.games.count) {
                // If we did not create a matching Metadata entity for this Game above, we simply
                // import it again, creating new metadata. This could happen if the user has deleted
                // the Metadata.plist but not the Games.plist file, or if the Metadata and Games plists
                // have gone out of sync somehow.
                game = [weakSelf importGame: [games valueForKey:ifid] inContext:private reportFailure: NO];
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
                NSURL *imgpath = [NSURL URLWithString:[ifid stringByAppendingPathExtension:@"tiff"] relativeToURL:weakSelf.imageDir];
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
                        }
                    }
                    babel_release_ctx(context);
                }

                if (imgdata)
                    [weakSelf addImage:imgdata toMetadata:game.metadata inContext:private];

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
            [weakSelf endImporting];
            weakSelf.addButton.enabled = YES;
            weakSelf.currentlyAddingGames = NO;
        });
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasConvertedLibrary"];
    }];
}

- (Metadata *)importMetadataFromXML:(NSData *)mdbuf inContext:(NSManagedObjectContext *)context {
    IFictionMetadata *metadata = [[IFictionMetadata alloc] initWithData:mdbuf andContext:context];
    if (!metadata || metadata.stories.count == 0)
        return nil;
    return ((IFStory *)(metadata.stories)[0]).identification.metadata;
}

- (BOOL)importMetadataFromFile:(NSString *)filename {
    NSLog(@"libctl: importMetadataFromFile %@", filename);

    NSData *data = [NSData dataWithContentsOfFile:filename];
    if (!data)
        return NO;

    Metadata *metadata = [self importMetadataFromXML:data inContext:_managedObjectContext];
    if (!metadata)
        return NO;
    metadata.source = @(kExternal);
    return YES;
}

- (void) addImage:(NSData *)rawImageData toMetadata:(Metadata *)metadata inContext:(NSManagedObjectContext *)context {

    Image *img = (Image *) [NSEntityDescription
                     insertNewObjectForEntityForName:@"Image"
                     inManagedObjectContext:context];
    img.data = rawImageData;
    img.originalURL = metadata.coverArtURL;
    metadata.cover = img;
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
//    NSDictionary *info;
//    int src;

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
            fetchRequest.predicate = [NSPredicate predicateWithFormat: @"(game != nil)"];
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

    enumerator = [metadata objectEnumerator];
    while ((meta = [enumerator nextObject]))
    {
        //src = [[info objectForKey:kSource] intValue];

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
        fprintf(fp, "</story>\n\n");
    }

    fprintf(fp, "</ifindex>\n");

    fclose(fp);
    return YES;
}

/*
 * The list of games is kept separate from metadata.
 * We save them as dictionaries mapping IFIDs to filenames.
 */

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
            winRestore:(BOOL)restoreflag {

// The winRestore flag is just to let us know whether
// this is called from restoreWindowWithIdentifier in
// AppDelegate.m. It really should be called 
// systemWindowRestoration or something like that.
// The playGameWithIFID method will pass this flag on
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

//    NSLog(@"LibController playGame: %@ winRestore: %@", game.metadata.title, restoreflag ? @"YES" : @"NO");

    if (gctl) {
        NSLog(@"A game with this ifid is already in session");
        [gctl.window makeKeyAndOrderFront:nil];
        return gctl.window;
    }

    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        game.found = NO;
        if (!restoreflag) // Everything will break if we throw up a dialog during system window restoration
            [self lookForMissingFile:game];
        return nil;
    } else game.found = YES;


    if (![[NSFileManager defaultManager] isReadableFileAtPath:path]) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Cannot read the file.", nil);
        alert.informativeText = NSLocalizedString(@"The file exists but can not be read.", nil);
        [alert runModal];
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

    _gameSessions[game.ifid] = gctl;

    [gctl runTerp:terp withGame:game reset:NO winRestore:restoreflag];

    
    game.lastPlayed = [NSDate date];
    [self addURLtoRecents: game.urlForBookmark];

    [Preferences changeCurrentGame:game];

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

- (void)importAndPlayGame:(NSString *)path {

    Game *game = [self importGame: path inContext:_managedObjectContext reportFailure: YES];
    if (game)
    {
        [self selectGames:[NSSet setWithArray:@[game]]];
        [self playGame:game];
    }
}

-(BOOL) checkZcode:(NSString *)file {
    NSData *mem = [NSData dataWithContentsOfFile:file];

    int dictionary = word(mem, 0x08);
    int static_start = word(mem, 0x0e);

    if(dictionary != 0 && dictionary < static_start)
    {   // corrupted story: dictionary is not in static memory
        return NO;
    }
    return YES;
}

static inline uint16_t word(NSData *mem, uint32_t addr)
{
    uint8_t *memory = (uint8_t *)mem.bytes;

    return (uint16_t)((memory[addr] << 8) | memory[addr + 1]);
}


- (Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report {
    char buf[TREATY_MINIMUM_EXTENT];
    Metadata *metadata;
    Game *game;
    NSString *ifid;
    char *format;
    char *s;
    Blorb *blorb = nil;
    int rv;

    NSString *extension = path.pathExtension.lowercaseString;

    if ([extension isEqualToString: @"ifiction"])
    {
        [self addIfictionFile:path];
        return nil;
    }

    if ([extension isEqualToString: @"d$$"])
    {
        path = [self convertAGTFile: path];
        if (!path)
        {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Conversion failed.", nil);
                    alert.informativeText = NSLocalizedString(@"This old style AGT file could not be converted to the new AGX format.", nil);
                    [alert runModal];
                });
            }
            return nil;
        }
    }

    if (![gGameFileTypes containsObject: extension])
    {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Can not recognize the file extension \"%@.\"", nil), path.pathExtension];
                [alert runModal];
            });
        }
        return nil;
    }

    if (report && ([extension isEqualToString:@"blorb"] || [extension isEqualToString:@"blb"])) {
        blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];
        BlorbResource *executable = [blorb resourcesForUsage:ExecutableResource].firstObject;
        if (!executable) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Not a game.", nil);
                alert.informativeText = NSLocalizedString(@"No executable chunk found in Blorb file.", nil);
                [alert runModal];
            });
            return nil;
        }

        if ([executable.chunkType isEqualToString:@"ADRI"]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unsupported format.", nil);
                alert.informativeText = NSLocalizedString(@"Adrift 5 games are not supported.", nil);
                [alert runModal];
            });
            return nil;
        }
    }

    void *ctx = get_babel_ctx();
    format = babel_init_ctx((char*)path.UTF8String, ctx);
    if (!format || !babel_get_authoritative_ctx(ctx))
    {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = NSLocalizedString(@"Babel can not identify the file format.", nil);
                [alert runModal];
            });
        }
        babel_release_ctx(ctx);
        return nil;
    }

    s = strchr(format, ' ');
    if (s) format = s+1;

    rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, ctx);
    if (rv <= 0)
    {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Fatal error.", nil);
                alert.informativeText = NSLocalizedString(@"Can not compute IFID from the file.", nil);
                [alert runModal];
            });
        }
        babel_release_ctx(ctx);
        return nil;
    }

    s = strchr(buf, ',');
    if (s) *s = 0;
    ifid = @(buf);

    if ([ifid isEqualToString:@"ZCODE-5-------"] && [[path signatureFromFile] isEqualToString:@"0304000545ff60e931b802ea1e6026860000c4cacbd2c1cb022acde526d400000000000000000000000000000000000000000000000000000000000000000000"])
        ifid = @"ZCODE-5-830222";

    if (([extension isEqualToString:@"dat"] &&
        !(([@(format) isEqualToString:@"zcode"] && [self checkZcode:path]) ||
          [@(format) isEqualToString:@"level9"] ||
          [@(format) isEqualToString:@"advsys"])) ||
        ([extension isEqualToString:@"gam"] && ![@(format) isEqualToString:@"tads2"])) {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = NSLocalizedString(@"Not a supported format.", nil);
                [alert runModal];
            });
        }
        babel_release_ctx(ctx);
        return nil;
    }

    //NSLog(@"libctl: import game %@ (%s)", path, format);

    metadata = [self fetchMetadataForIFID:ifid inContext:context];

    if (!metadata)
    {
        if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path]]) {
            blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];
            NSData *mdbufData = [blorb metaData];
            if (mdbufData) {
                metadata = [self importMetadataFromXML: mdbufData inContext:context];
                metadata.source = @(kInternal);
                NSLog(@"Extracted metadata from blorb. Title: %@", metadata.title);
            }
            else NSLog(@"Found no metadata in blorb file %@", path);
        }
    }
    else
    {
        game = [self fetchGameForIFID:ifid inContext:context];
        if (game)
        {
            if ([game.detectedFormat isEqualToString:@"glulx"])
                game.hashTag = [path signatureFromFile];
            else if ([game.detectedFormat isEqualToString:@"zcode"]) {
                [self addZCodeIDfromFile:path blorb:blorb toGame:game];
            }
            if (![path isEqualToString:game.path])
            {
                NSLog(@"File location did not match. Updating library with new file location.");
                [game bookmarkForPath:path];
            }
            if (![game.detectedFormat isEqualToString:@(format)]) {
                NSLog(@"Game format did not match. Updating library with new detected format (%s).", format);
                game.detectedFormat = @(format);
            }
            game.found = YES;
            return game;
        }
    }

    if (!metadata)
    {
        metadata = (Metadata *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Metadata"
                                 inManagedObjectContext:context];
    }
    
    [metadata findOrCreateIfid:ifid];

    if (!metadata.format)
        metadata.format = @(format);
    if (!metadata.title || metadata.title.length == 0)
    {
        metadata.title = path.lastPathComponent;
    }

    if (!metadata.cover)
    {
        NSURL *imgpath = [NSURL URLWithString:[ifid stringByAppendingPathExtension:@"tiff"] relativeToURL:_imageDir];
        NSData *img = [[NSData alloc] initWithContentsOfURL:imgpath];
        if (img)
        {
            NSLog(@"Found cover image in image directory for game %@", metadata.title);
            metadata.coverArtURL = imgpath.path;
            [self addImage:img toMetadata:metadata inContext:context];
        }
        else
        {
            if (blorb) {
                NSData *imageData = blorb.coverImageData;
                if (imageData) {
                    metadata.coverArtURL = path;
                    [self addImage:imageData toMetadata:metadata inContext:context];
                    NSLog(@"Extracted cover image from blorb for game %@", metadata.title);
                }
                else NSLog(@"Found no image in blorb file %@", path);
            }
        }
    }

    babel_release_ctx(ctx);

    game = (Game *) [NSEntityDescription
                           insertNewObjectForEntityForName:@"Game"
                           inManagedObjectContext:context];

    [game bookmarkForPath:path];

    game.added = [NSDate date];
    game.metadata = metadata;
    game.ifid = ifid;
    game.detectedFormat = @(format);
    if ([game.detectedFormat isEqualToString:@"glulx"]) {
        game.hashTag = [path signatureFromFile];
    }
    if ([game.detectedFormat isEqualToString:@"zcode"]) {
        [self addZCodeIDfromFile:path blorb:blorb toGame:game];
    }

    return game;
}

- (void)addZCodeIDfromFile:(NSString *)path blorb:(nullable Blorb *)blorb toGame:(Game *)game {
    BOOL found = NO;
    NSData *data = nil;
    if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path]]) {
        if (!blorb)
            blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];
        if (blorb.checkSum && blorb.serialNumber && blorb.releaseNumber) {
            found = YES;
            game.checksum = blorb.checkSum;
            game.serialString = blorb.serialNumber;
            game.releaseNumber = blorb.releaseNumber;
        }
        if (!found) {
            BlorbResource *zcode = [blorb resourcesForUsage:ExecutableResource].firstObject;
            if (zcode)
                data = [blorb dataForResource:zcode];
        }
    }
    if (!found) {
        if (!data)
            data = [NSData dataWithContentsOfFile:path];
        const unsigned char *ptr = data.bytes;
        game.releaseNumber = (int32_t)unpackShort(ptr + 2);
        NSData *serialData = [NSData dataWithBytes:ptr + 18 length:6];
        game.serialString = [[NSString alloc] initWithData:serialData encoding:NSASCIIStringEncoding];
        game.checksum = (int32_t)unpackShort(ptr + 28);
    }
}

- (void)addFile: (NSURL*)url select: (NSMutableArray*)select inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)reportFailure
{
    Game *game = [self importGame:url.path inContext:context reportFailure:reportFailure];
    if (game) {
        [self beginImporting];
        [select addObject:game.ifid];
    } else {
        //NSLog(@"libctl: addFile: File %@ not added!", url.path);
    }
}

- (void) addFiles:(NSArray*)urls select:(NSMutableArray*)select inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)reportFailure {
    NSFileManager *filemgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSUInteger count;
    NSUInteger i;

    NSDate *timestamp = [NSDate date];
    
    count = urls.count;
    for (i = 0; i < count; i++)
    {
        NSString *path = [urls[i] path];

        if (![filemgr fileExistsAtPath: path isDirectory: &isdir])
            continue;

        if (isdir)
        {
            NSArray *contents = [filemgr contentsOfDirectoryAtURL:urls[i]
                                       includingPropertiesForKeys:@[NSURLNameKey]
                                                          options:NSDirectoryEnumerationSkipsHiddenFiles
                                                            error:nil];
            [self addFiles: contents select: select inContext:context reportFailure:reportFailure];
        }
        else
        {
            [self addFile:urls[i] select:select inContext:context reportFailure:reportFailure];
        }

        if ([timestamp timeIntervalSinceNow] < -0.5) {

            NSError *error = nil;
            if (context.hasChanges) {
                if (![context save:&error]) {
                    if (error) {
                        [[NSApplication sharedApplication] presentError:error];
                    }
                }
            }

            timestamp = [NSDate date];
        }

    }
}

- (void)addFiles: (NSArray*)urls inContext:(NSManagedObjectContext *)context {

    NSMutableArray *select = [NSMutableArray arrayWithCapacity: urls.count];

    LibController * __unsafe_unretained weakSelf = self;

    BOOL reportFailure = NO;

    if (urls.count == 1) {
        // If the user only selects one file, and it is invalid
        // (and not a directory) we should report the failure.
        BOOL isDir;
        [[NSFileManager defaultManager] fileExistsAtPath:((NSURL *)urls[0]).path isDirectory: &isDir];
        if (!isDir)
            reportFailure = YES;
    }
    
    [self beginImporting];
    [self addFiles: urls select: select inContext:context reportFailure:reportFailure];
    [self endImporting];

    NSError *error = nil;
    if (context.hasChanges) {
        if (![context save:&error]) {
            if (error) {
                [[NSApplication sharedApplication] presentError:error];
            }
        }
    }

    [_managedObjectContext performBlock:^{
        weakSelf.currentlyAddingGames = NO;
        [weakSelf selectGamesWithIfids:select scroll:YES];
        for (NSString *path in weakSelf.iFictionFiles) {
            [weakSelf importMetadataFromFile:path];
        }
        weakSelf.iFictionFiles = nil;
        // Fix metadata entities with no ifids
        NSArray *noIfids = [self fetchObjects:@"Metadata" predicate:@"ifids.@count == 0" inContext:weakSelf.managedObjectContext];
        for (Metadata *meta in noIfids) {
            [weakSelf.managedObjectContext deleteObject:meta];
        }
        // Fix metadata entities with empty titles
        NSArray *noTitles = [self fetchObjects:@"Metadata" predicate:@"title = nil OR title = ''" inContext:weakSelf.managedObjectContext];
        for (Metadata *meta in noTitles) {
            Game *game = meta.games.anyObject;
            if (game) {
                meta.title = [game urlForBookmark].path.lastPathComponent;
            }
        }
        [weakSelf.coreDataManager saveChanges];
    }];
}

#pragma mark -
#pragma mark Table magic

- (IBAction)searchForGames:(id)sender {
    NSString *value = [sender stringValue];
    searchStrings = nil;

    if (value.length)
        searchStrings = [value componentsSeparatedByString:@" "];

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
        NSIndexSet *indexSet = [_gameTableModel indexesOfObjectsPassingTest:^BOOL(NSString *obj, NSUInteger idx, BOOL *stop) {
            return [games containsObject:obj];
        }];
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        _selectedGames = [_gameTableModel objectsAtIndexes:indexSet];
        if (indexSet.count && indexSet.count < 50 && !_currentlyAddingGames)
            [_gameTableView scrollRowToVisible:(NSInteger)indexSet.firstIndex];
    }
}

- (NSInteger)stringcompare:(NSString *)a with:(NSString *)b {
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedCaseInsensitiveCompare: b];
}

- (NSInteger)compareGame:(Metadata *)a with:(Metadata *)b key:(id)key ascending:(BOOL)ascending {
    NSString * ael = [a valueForKey:key];
    NSString * bel = [b valueForKey:key];
    return [self compareString:ael withString:bel ascending:ascending];
}

- (NSInteger)compareString:(NSString *)ael withString:(NSString *)bel ascending:(BOOL)ascending {
    if ((!ael || ael.length == 0) && (!bel || bel.length == 0))
        return NSOrderedSame;
    if (!ael || ael.length == 0) return ascending ? NSOrderedDescending :  NSOrderedAscending;
    if (!bel || bel.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [self stringcompare:ael with:bel];
}

- (NSInteger)compareDate:(NSDate *)ael withDate:(NSDate *)bel ascending:(BOOL)ascending {
    if ((!ael) && (!bel))
        return NSOrderedSame;
    if (!ael) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!bel) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [ael compare:bel];
}

- (void) updateTableViews
{
    NSFetchRequest *fetchRequest;
    NSEntityDescription *entity;

    NSUInteger searchcount;

    if (!gameTableDirty)
        return;

    //NSLog(@"Updating table view");
    
    fetchRequest = [[NSFetchRequest alloc] init];
    entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];
    fetchRequest.entity = entity;

    if (searchStrings)
    {
        searchcount = searchStrings.count;

        NSPredicate *predicate;
        NSMutableArray *predicateArr = [NSMutableArray arrayWithCapacity:searchcount];

        for (NSString *word in searchStrings) {
            predicate = [NSPredicate predicateWithFormat: @"(detectedFormat contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.languageAsWord contains [c] %@) OR (metadata.firstpublished contains %@)", word, word, word, word, word, word, word, word, word, word, word];
            [predicateArr addObject:predicate];
        }


        NSCompoundPredicate *comp = (NSCompoundPredicate *)[NSCompoundPredicate orPredicateWithSubpredicates: predicateArr];

        fetchRequest.predicate = comp;
    }

    NSError *error = nil;
    _gameTableModel = [[_managedObjectContext executeFetchRequest:fetchRequest error:&error] mutableCopy];
    if (_gameTableModel == nil)
        NSLog(@"Problem! %@",error);

    LibController * __unsafe_unretained weakSelf = self;

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:_sortAscending comparator:^(Game *aid, Game *bid) {

        Metadata *a = aid.metadata;
        Metadata *b = bid.metadata;
        NSInteger cmp;
        if ([weakSelf.gameSortColumn isEqual:@"firstpublishedDate"])
        {
            cmp = [weakSelf compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:weakSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([weakSelf.gameSortColumn isEqual:@"added"] || [weakSelf.gameSortColumn isEqual:@"lastPlayed"])
        {
            cmp = [weakSelf compareDate:[aid valueForKey:weakSelf.gameSortColumn] withDate:[bid valueForKey:weakSelf.gameSortColumn] ascending:weakSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([weakSelf.gameSortColumn isEqual:@"lastModified"]) {
            cmp = [weakSelf compareDate:[a valueForKey:weakSelf.gameSortColumn] withDate:[b valueForKey:weakSelf.gameSortColumn] ascending:weakSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([weakSelf.gameSortColumn isEqual:@"found"]) {
            NSString *string1 = aid.found?nil:@"A";
            NSString *string2 = bid.found?nil:@"A";
            cmp = [weakSelf compareString:string1 withString:string2 ascending:weakSelf.sortAscending];
            if (cmp) return cmp;
        }
        else if ([weakSelf.gameSortColumn isEqual:@"forgivenessNumeric"]) {
            NSString *string1 = a.forgivenessNumeric ? [NSString stringWithFormat:@"%@", a.forgivenessNumeric] : nil;
            NSString *string2 = b.forgivenessNumeric ? [NSString stringWithFormat:@"%@", b.forgivenessNumeric] : nil;
            cmp = [weakSelf compareString:string1 withString:string2 ascending:weakSelf.sortAscending];

            if (cmp) return cmp;
        }
        else if (weakSelf.gameSortColumn)
        {
            cmp = [weakSelf compareGame:a with:b key:weakSelf.gameSortColumn ascending:weakSelf.sortAscending];
            if (cmp) return cmp;
        }
        cmp = [weakSelf compareGame:a with:b key:@"title" ascending:weakSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [weakSelf compareGame:a with:b key:@"author" ascending:weakSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [weakSelf compareGame:a with:b key:@"seriesnumber" ascending:weakSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [weakSelf compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:weakSelf.sortAscending];
        if (cmp) return cmp;
        return [weakSelf compareString:aid.detectedFormat withString:bid.detectedFormat ascending:weakSelf.sortAscending];
    }];

    [_gameTableModel sortUsingDescriptors:@[sort]];
    [_gameTableView reloadData];

    [self selectGames:[NSSet setWithArray:_selectedGames]];

    gameTableDirty = NO;
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

- (void)backgroundManagedObjectContextDidChange:(id)sender {
//    NSLog(@"backgroundManagedObjectContextDidChange");
    [_coreDataManager saveChanges];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    //NSLog(@"noteManagedObjectContextDidChange");
    gameTableDirty = YES;
    [self updateTableViews];
    NSArray *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];

    for (id obj in updatedObjects) {
        if ([obj isKindOfClass:[Theme class]]) {
            [self rebuildThemesSubmenu];
            break;
        }
    }
    if ([updatedObjects containsObject:currentSideView.metadata])
    {
        [self updateSideViewForce:YES];
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
        //NSLog(@"updateSideView: %@ is already shown and force is NO", game.metadata.title);
        return;
    }

    [_leftScrollView.documentView performSelector:@selector(removeFromSuperview)];

    if (NSWidth(_leftView.frame) < ACTUAL_LEFT_VIEW_MIN_WIDTH)
        return;
    
    SideInfoView *infoView = [[SideInfoView alloc] initWithFrame:_leftScrollView.frame];
    
    _leftScrollView.documentView = infoView;
    _sideIfid.delegate = infoView;

    _sideIfid.stringValue = @"";

    if (game) {
        [infoView updateSideViewWithGame:game scroll:(game != currentSideView)];
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

-(IBAction)toggleSidebar:(id)sender
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
    // TODO: This should call a faster update method rather than rebuilding the view from scratch every time, but everything I've tried makes word wrap wonky

    if (NSWidth(_splitView.frame) < ACTUAL_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH && ![_splitView isSubviewCollapsed:_leftView]) {
        NSRect frame = self.window.frame;
        frame.size.width = PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + 10;
        [_splitView setPosition:PREFERRED_LEFT_VIEW_MIN_WIDTH - 10
               ofDividerAtIndex:0];
        [self.window setFrame:frame display:NO animate:YES];
    }
    [self updateSideViewForce:YES];
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

#pragma mark -
#pragma mark Some stuff that doesn't really fit anywhere else.

- (NSString *)convertAGTFile:(NSString *)origpath {
    NSLog(@"libctl: converting agt to agx");
    char rapeme[] = "/tmp/cugelcvtout.agx"; /* babel will clobber this, so we
                                               can't send a const string */

    NSString *exepath =
        [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"agt2agx"];
    NSString *dirpath;
    NSString *cvtpath;
    NSTask *task;
    char *format;
    int status;

    task = [[NSTask alloc] init];
    task.launchPath = exepath;
    task.arguments = @[ @"-o", @"/tmp/cugelcvtout.agx", origpath ];
    [task launch];
    [task waitUntilExit];
    status = task.terminationStatus;

    if (status != 0) {
        NSLog(@"libctl: agt2agx failed");
        return nil;
    }

    void *ctx = get_babel_ctx();

    format = babel_init_ctx(rapeme, ctx);
    if (!strcmp(format, "agt")) {
        char buf[TREATY_MINIMUM_EXTENT];
        int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, ctx);
        if (rv == 1) {
            dirpath =
                [_homepath.path stringByAppendingPathComponent:@"Converted"];

            [[NSFileManager defaultManager]
                       createDirectoryAtURL:[NSURL fileURLWithPath:dirpath
                                                       isDirectory:YES]
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:NULL];

            cvtpath =
                [dirpath stringByAppendingPathComponent:
                             [@(buf) stringByAppendingPathExtension:@"agx"]];

            babel_release_ctx(ctx);

            NSURL *url = [NSURL fileURLWithPath:cvtpath];

            [[NSFileManager defaultManager] removeItemAtURL:url error:nil];

            status = [[NSFileManager defaultManager]
                moveItemAtPath:@"/tmp/cugelcvtout.agx"
                        toPath:cvtpath
                         error:nil];

            if (!status) {
                NSLog(@"libctl: could not move converted file");
                return nil;
            }
            return cvtpath;
        }
    } else {
        NSLog(@"libctl: babel did not like the converted file");
    }
    babel_release_ctx(ctx);
    return nil;
}

@end
