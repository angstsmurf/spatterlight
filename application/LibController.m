/*
 *
 */

#import "InfoController.h"
#import "main.h"

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

#define kSource @".source"
#define kDefault 1
#define kInternal 2
#define kExternal 3
#define kUser 4
#define kIfdb 5

#define RIGHT_VIEW_MIN_WIDTH 340.0
#define PREFERRED_LEFT_VIEW_MIN_WIDTH 200.0

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
    //    NSLog(@"becomeFirstResponder");
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
    textMovement = [userInfo objectForKey:@"NSTextMovement"];
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

@implementation LibController

#pragma mark Window controller, menus and file dialogs.

static NSMutableDictionary *load_mutable_plist(NSString *path) {
    NSMutableDictionary *dict;
    NSString *error;

    dict = [NSPropertyListSerialization
        propertyListFromData:[NSData dataWithContentsOfFile:path]
            mutabilityOption:NSPropertyListMutableContainersAndLeaves
                      format:NULL
            errorDescription:&error];

    if (!dict) {
        NSLog(@"libctl: cannot load plist: %@", error);
        dict = [[NSMutableDictionary alloc] init];
    }

    return dict;
}

static BOOL save_plist(NSString *path, NSDictionary *plist) {
    NSData *plistData;
    NSString *error;

    plistData = [NSPropertyListSerialization
        dataFromPropertyList:plist
                      format:NSPropertyListBinaryFormat_v1_0
            errorDescription:&error];
    if (plistData) {
        return [plistData writeToFile:path atomically:YES];
    } else {
        NSLog(@"%@", error);
        return NO;
    }
}

- (instancetype)init {
    NSLog(@"LibController: init");
    self = [super initWithWindowNibName:@"LibraryWindow"];
    if (self) {
        NSError *error;
        homepath = [[NSFileManager defaultManager]
              URLForDirectory:NSApplicationSupportDirectory
                     inDomain:NSUserDomainMask
            appropriateForURL:nil
                       create:YES
                        error:&error];
        if (error)
            NSLog(@"libctl: Could not find Application Support directory! "
                  @"Error: %@",
                  error);

        homepath = [NSURL URLWithString:@"Spatterlight" relativeToURL:homepath];

        [[NSFileManager defaultManager] createDirectoryAtURL:homepath
                                 withIntermediateDirectories:YES
                                                  attributes:NULL
                                                       error:NULL];

        _coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;
        _managedObjectContext = _coreDataManager.mainManagedObjectContext;
    }
    return self;
}

- (void)windowDidLoad {
    NSLog(@"libctl: windowDidLoad");

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
    gameTableModel = [[NSMutableArray alloc] init];

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
 
    if (!_managedObjectContext) {
        NSLog(@"LibController windowDidLoad: no _managedObjectContext!");
        return;
    }

    [self updateSideViewForce:YES];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];

    // Add metadata and games from plists to Core Data store if we have just created a new one
    gameTableModel = [[self fetchObjects:@"Game" inContext:_managedObjectContext] mutableCopy];
    NSArray *allMetadata = [self fetchObjects:@"Metadata" inContext:_managedObjectContext];

    if (allMetadata.count == 0 || gameTableModel.count == 0)
    {
        [self convertLibraryToCoreData];
    }
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    NSLog(@"libctl: windowWillReturnUndoManager!")
    return _managedObjectContext.undoManager;
}


- (IBAction)deleteLibrary:(id)sender {
    NSInteger choice =
        NSRunAlertPanel(@"Do you really want to delete the library?",
                        @"All the information about your games will be lost. "
                        @"The original game files will not be harmed.",
                        @"Delete", NULL, @"Cancel");
    
    if (choice != NSAlertOtherReturn) {
        NSFetchRequest *allMetadata = [[NSFetchRequest alloc] init];
        [allMetadata setEntity:[NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:_managedObjectContext]];
        [allMetadata setIncludesPropertyValues:NO]; //only fetch the managedObjectID

        NSError *error = nil;
        NSArray *metadataEntries = [_managedObjectContext executeFetchRequest:allMetadata error:&error];
        //error handling goes here
        for (NSManagedObject *meta in metadataEntries) {
            [_managedObjectContext deleteObject:meta];
        }

        NSFetchRequest *allGames = [[NSFetchRequest alloc] init];
        [allGames setEntity:[NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext]];
        [allGames setIncludesPropertyValues:NO]; //only fetch the managedObjectID

        error = nil;
        NSArray *gameEntries = [_managedObjectContext executeFetchRequest:allGames error:&error];
        //error handling goes here
        for (NSManagedObject *game in gameEntries) {
            [_managedObjectContext deleteObject:game];
        }

        [_coreDataManager saveChanges];
    }
}

- (IBAction)pruneLibrary:(id)sender {
    NSInteger choice =
        NSRunAlertPanel(@"Do you really want to prune the library?",
                        @"Pruning will delete the information about games that "
                        @"are not in the library at the moment.",
                        @"Prune", NULL, @"Cancel");
    if (choice != NSAlertOtherReturn) {

        if (choice != NSAlertOtherReturn) {
            NSFetchRequest *orphanedMetadata = [[NSFetchRequest alloc] init];
            [orphanedMetadata setEntity:[NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:_managedObjectContext]];

            orphanedMetadata.predicate = [NSPredicate predicateWithFormat: @"(game == nil)"];

            NSError *error = nil;
            NSArray *metadataEntriesToDelete = [_managedObjectContext executeFetchRequest:orphanedMetadata error:&error];
            //error handling goes here
            NSLog(@"Pruning %ld metadata entities", metadataEntriesToDelete.count);
            for (Metadata *meta in metadataEntriesToDelete) {
                NSLog(@"Pruning metadata for %@ with ifid %@", meta.title, meta.ifid)
                [_managedObjectContext deleteObject:meta];
            }
            [_coreDataManager saveChanges];
        }
    }
}

- (void)beginImporting {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!spinnerSpinning) {
            [_progressCircle startAnimation:self];
            spinnerSpinning = YES;
        }
    });
}

- (void)endImporting {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (spinnerSpinning) {
            [_progressCircle stopAnimation:self];
            spinnerSpinning = NO;
        }
    });
}

- (IBAction)importMetadata:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.prompt = @"Import";

    panel.allowedFileTypes = @[ @"iFiction" ];

    LibController * __unsafe_unretained weakSelf = self; 

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSFileHandlingPanelOKButton) {
                          NSURL *url = panel.URL;
//                          [_managedObjectContext performBlock:^{
                              [weakSelf beginImporting];
                              [weakSelf importMetadataFromFile:url.path];
                              [_coreDataManager saveChanges];
                              [weakSelf endImporting];
//                          }];
                      }
                  }];
}

- (IBAction)exportMetadata:(id)sender {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.accessoryView = exportTypeView;
    panel.allowedFileTypes = @[ @"iFiction" ];
    panel.prompt = @"Export";
    panel.nameFieldStringValue = @"Interactive Fiction Metadata.iFiction";

    NSPopUpButton *localExportTypeControl = exportTypeControl;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSFileHandlingPanelOKButton) {
                          NSURL *url = panel.URL;

                          [self exportMetadataToFile:url.path
                                                what:localExportTypeControl
                                                         .indexOfSelectedItem];
                      }
                  }];
}

- (void)addIfidFile:(NSString *)file {
    if (!iFictionFiles)
        iFictionFiles = [NSMutableArray arrayWithObject:file];
    else
        [iFictionFiles addObject:file];
}

- (IBAction)addGamesToLibrary:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseDirectories:YES];
    panel.prompt = @"Add";

    panel.allowedFileTypes = gGameFileTypes;

    LibController * __unsafe_unretained weakSelf = self;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSFileHandlingPanelOKButton) {
                          NSArray *urls = panel.URLs;
                          [weakSelf addInBackground:urls];
                      }
                  }];
}

- (void)addInBackground:(NSArray *)files {

    if (currentlyAddingGames)
        return;

    LibController * __unsafe_unretained weakSelf = self;
    NSManagedObjectContext *childContext = [_coreDataManager privateChildManagedObjectContext];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(backgroundManagedObjectContextDidSave:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:childContext];

    [self beginImporting];
    
    currentlyAddingGames = YES;
    _addButton.enabled = NO;

    [childContext performBlock:^{
        [weakSelf addFiles:files inContext:childContext];
        [weakSelf endImporting];
        dispatch_async(dispatch_get_main_queue(), ^{
            _addButton.enabled = YES;
            currentlyAddingGames = NO;
        });

    }];
}

- (void)performFindPanelAction:(id<NSValidatedUserInterfaceItem>)sender {
    [_searchField selectText:self];
}

- (IBAction)reset:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = [gameTableModel objectAtIndex:i];
        GlkController *gctl = [_gameSessions objectForKey:game.metadata.ifid];

        if (gctl) {
            [gctl reset:sender];
        } else {
            gctl = [[GlkController alloc] init];
            [gctl deleteAutosaveFilesForGame:game];
        }
    }
}

#pragma mark Contextual menu

- (IBAction)play:(id)sender {
    NSInteger rowidx;

    if (_gameTableView.clickedRow != -1)
        rowidx = _gameTableView.clickedRow;
    else
        rowidx = _gameTableView.selectedRow;

    if (rowidx >= 0) {
         [self playGame:[gameTableModel objectAtIndex:rowidx]];
    }
}

- (IBAction)showGameInfo:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = [gameTableModel objectAtIndex:i];
        [self showInfoForGame:game];
    }
}

- (void)showInfoForGame:(Game *)game {
    InfoController *infoctl;

    // First, we check if we have created this info window already
    infoctl = [_infoWindows objectForKey:[game urlForBookmark].path];

    if (!infoctl) {
        infoctl = [[InfoController alloc] initWithGame:game];
        NSWindow *infoWindow = infoctl.window;
        infoWindow.restorable = YES;
        infoWindow.restorationClass = [AppDelegate class];
        NSString *path = [game urlForBookmark].path;
        if (path) {
            infoWindow.identifier = [NSString stringWithFormat:@"infoWin%@", path];
            [_infoWindows setObject:infoctl forKey:path];
        } else return;
    }

    [infoctl showWindow:nil];
}

- (IBAction)revealGameInFinder:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only reveal game in clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = [gameTableModel objectAtIndex:i];
        NSURL *url = [game urlForBookmark];
        if (![[NSFileManager defaultManager] fileExistsAtPath:url.path]) {
            game.found = @(NO);
            NSRunAlertPanel(
                @"Cannot find the file.",
                @"The file could not be found at its original location. Maybe "
                @"it has been moved since it was added to the library.",
                @"Okay", NULL, NULL);
            return;
        } else game.found = @(YES);
        [[NSWorkspace sharedWorkspace] selectFile:url.path
                         inFileViewerRootedAtPath:@""];
    }
}

- (IBAction)deleteGame:(id)sender {
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only delete game in clicked row
    if (_gameTableView.clickedRow != -1 &&
        ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    if (rows.count > 0) {
        Game *game;
        NSInteger i;

        for (i = rows.firstIndex; i != NSNotFound;
             i = [rows indexGreaterThanIndex:i]) {
            game = [gameTableModel objectAtIndex:i];
            NSLog(@"libctl: delete game %@", game.metadata.title);
            [_managedObjectContext deleteObject:game];
        }
    }
}

- (IBAction)delete:(id)sender {
    [self deleteGame:sender];
}

- (IBAction)openIfdb:(id)sender {

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow >= 0) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    if (rows.count > 0)
    {
        Game *game;
        NSInteger i;
        NSString *urlString;

        for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
        {
            game = [gameTableModel objectAtIndex:i];
            urlString = [@"https://ifdb.tads.org/viewgame?id=" stringByAppendingString:game.metadata.tuid];
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: urlString]];
        }
    }
}

- (IBAction) download:(id)sender
{
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if ((_gameTableView.clickedRow >= 0) && ![_gameTableView isRowSelected:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    if (rows.count > 0)
    {
        LibController * __unsafe_unretained weakSelf = self;
        NSManagedObjectContext *childContext = [_coreDataManager privateChildManagedObjectContext];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(backgroundManagedObjectContextDidSave:)
                                                     name:NSManagedObjectContextObjectsDidChangeNotification
                                                   object:childContext];

        currentlyAddingGames = YES;
        _addButton.enabled = NO;

        [childContext performBlock:^{

            for (Game *game in selectedGames) {
                [weakSelf beginImporting];
                if ([weakSelf downloadMetadataForIFID: game.metadata.ifid inContext:childContext]) {

                    NSError *error = nil;
                    if (childContext.hasChanges) {
                        if (![childContext save:&error]) {
                            if (error) {
                                [[NSApplication sharedApplication] presentError:error];
                            }
                        }
                        
                        NSManagedObjectID *objID = game.objectID;

                        [_managedObjectContext performBlock:^{
                            Game *updatedGame = (Game *)[_managedObjectContext objectWithID:objID];
                            [_managedObjectContext refreshObject:updatedGame
                                                    mergeChanges:YES];
                        }];

                    }

                }
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                currentlyAddingGames = NO;
                _addButton.enabled = YES;
            });

            [weakSelf endImporting];
        }];
    }
}

- (void) extractMetadataFromFile:(Game *)game
{
	int mdlen;

	BOOL report = YES;
	currentIfid = nil;
	cursrc = kInternal;

	NSString *path = game.urlForBookmark.path;

	char *format = babel_init((char*)path.UTF8String);
	if (format)
	{
		char buf[TREATY_MINIMUM_EXTENT];
		char *s;
		NSString *ifid;
		int rv;

		rv = babel_treaty(GET_STORY_FILE_IFID_SEL, buf, sizeof buf);
		if (rv > 0)
		{
			s = strchr(buf, ',');
			if (s) *s = 0;
			ifid = @(buf);

			if (!ifid || [ifid isEqualToString:@""])
				ifid = game.metadata.ifid;

			mdlen = babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL, NULL, 0);
			if (mdlen > 0)
			{
				char *mdbuf = malloc(mdlen);
				if (!mdbuf)
				{
					if (report)
						NSRunAlertPanel(@"Out of memory.",
										@"Can not allocate memory for the metadata text.",
										@"Okay", NULL, NULL);
					babel_release();
					return;
				}

				rv = babel_treaty(GET_STORY_FILE_METADATA_SEL, mdbuf, mdlen);
				if (rv > 0)
				{
					cursrc = kInternal;
                    importContext = _managedObjectContext;
					[self importMetadataFromXML: mdbuf];
					cursrc = 0;
				}

				free(mdbuf);
			}
			NSString *dirpath = (@"~/Library/Application Support/Spatterlight/Cover Art").stringByStandardizingPath;
			NSString *imgpath = [[dirpath stringByAppendingPathComponent: ifid] stringByAppendingPathExtension: @"tiff"];


			NSData *img = [[NSData alloc] initWithContentsOfFile: imgpath];
			if (!img)
			{
				int imglen = babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL, NULL, 0);
				if (imglen > 0)
				{
					char *imgbuf = malloc(imglen);
					if (imgbuf)
					{
						babel_treaty(GET_STORY_FILE_COVER_SEL, imgbuf, imglen);
						NSData *imgdata = [[NSData alloc] initWithBytesNoCopy: imgbuf length: imglen freeWhenDone: YES];
						img = [[NSData alloc] initWithData: imgdata];
					}
				}

			}
			if (img)
			{
				[self addImage: img toMetadata:game.metadata inContext:_managedObjectContext];
			}
            
		}
	}
	babel_release();
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;
    NSInteger count = _gameTableView.numberOfSelectedRows;

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if (_gameTableView.clickedRow != -1 &&
        (![rows containsIndex:_gameTableView.clickedRow]))
        count = 1;

    if (action == @selector(performFindPanelAction:)) {
        if (menuItem.tag == NSTextFinderActionShowFindInterface)
            return YES;
        else
            return NO;
    }

    if (action == @selector(addGamesToLibrary:))
        return !(currentlyAddingGames);

    if (action == @selector(importMetadata:))
         return !(currentlyAddingGames);

    if (action == @selector(download:))
        return !(currentlyAddingGames);

    if (action == @selector(delete:))
        return count > 0;

    if (action == @selector(deleteGame:))
        return count > 0;

    if (action == @selector(revealGameInFinder:))
        return count > 0 && count < 10;

    if (action == @selector(showGameInfo:))
        return count > 0;

    if (action == @selector(reset:))
        return count > 0;

    if (action == @selector(playGame:))
        return count == 1;

    if (action == @selector(openIfdb:))
        return (((Game *)[gameTableModel objectAtIndex:_gameTableView.clickedRow]).metadata.tuid != nil);

    if (action == @selector(toggleSidebar:))
    {
        NSString* title = [_leftView isHidden] ? @"Show Sidebar" : @"Hide Sidebar";
        ((NSMenuItem*)menuItem).title = title;
    }

    return YES;
}

#pragma mark Drag-n-drop destination handler

- (NSDragOperation)draggingEntered:sender {
    extern NSString *terp_for_filename(NSString * path);

    NSFileManager *mgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSInteger i;

    if (currentlyAddingGames)
        return NSDragOperationNone;

    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([pboard.types containsObject:NSFilenamesPboardType]) {
        NSArray *paths = [pboard propertyListForType:NSFilenamesPboardType];
        NSInteger count = paths.count;
        for (i = 0; i < count; i++) {
            NSString *path = [paths objectAtIndex:i];
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

- (void) addMetadata:(NSDictionary*)metadataDictionary forIFIDs:(NSArray*)ifidArray inContext:(NSManagedObjectContext *)context {
    NSInteger count;
    NSInteger i;
    NSDateFormatter *dateFormatter;
    Metadata *entry;
    NSString *keyVal;
    NSString *sub;

    // Make things slightly less thread-unsafe
    NSDictionary *dict = [metadataDictionary copy];
    NSArray *list = [ifidArray copy];

    NSDictionary *language = @{
                               @"en" : @"English",
                               @"fr" : @"French",
                               @"es" : @"Spanish",
                               @"ru" : @"Russian",
                               @"se" : @"Swedish",
                               @"de" : @"German",
                               @"zh" : @"Chinesese"
                               };

    NSDictionary *forgiveness = @{
                                  @"" : @(FORGIVENESS_NONE),
                                  @"Cruel" : @(FORGIVENESS_CRUEL),
                                  @"Nasty" : @(FORGIVENESS_NASTY),
                                  @"Tough" : @(FORGIVENESS_TOUGH),
                                  @"Polite" : @(FORGIVENESS_POLITE),
                                  @"Merciful" : @(FORGIVENESS_MERCIFUL)
                                  };

    if (cursrc == 0)
        NSLog(@"libctl: current metadata source failed...");

    /* prune the ifid list if it comes from ifdb */
    /* until we have a proper fix for downloading many images at once */

    if (cursrc == kIfdb) {
        if (currentIfid) {
            for (NSString *str in list)
                if ([str isEqualToString:currentIfid]) {
                    list = @[str];
                    break;
                }
        }
        list = @[[list objectAtIndex:0]];
    }

    count = list.count;

    for (i = 0; i < count; i++)
    {
        NSString *ifid =[list objectAtIndex:i];
        entry = [self fetchMetadataForIFID:ifid inContext:context];
        if (!entry)
        {
            entry = (Metadata *) [NSEntityDescription
                                  insertNewObjectForEntityForName:@"Metadata"
                                  inManagedObjectContext:context];
            entry.ifid = ifid;
            NSLog(@"addMetaData:forIFIDs: Created new Metadata instance with ifid %@", ifid);
            //dict[kSource] = @((int)cursrc);
        } // else NSLog(@"addMetaData:forIFIDs: Found existing Metadata instance with ifid %@ and game %@", entry.ifid, entry.title);
        for(id key in dict)
        {
            keyVal = [dict objectForKey:key];

            if (cursrc == kIfdb)
			{
                keyVal = [keyVal stringByDecodingXMLEntities];
				keyVal = [keyVal stringByReplacingOccurrencesOfString:@"'" withString:@"’"];
			}
            if ([(NSString *)key isEqualToString:@"description"])
			{
                [entry setValue:keyVal forKey:@"blurb"];
			}
            else if ([(NSString *)key isEqualToString:kSource])
            {
                if ((int)[dict objectForKey:kSource] == kUser)
                    entry.userEdited=@(YES);
                else
                    entry.userEdited=@(NO);
            }
            else if ([(NSString *)key isEqualToString:@"ifid"])
            {
                if (entry.ifid == nil)
                    entry.ifid = [dict objectForKey:@"ifid"];
            }
            else if ([(NSString *)key isEqualToString:@"firstpublished"])
            {
                if (dateFormatter == nil)
                    dateFormatter = [[NSDateFormatter alloc] init];
                if (keyVal.length == 4)
                    dateFormatter.dateFormat = @"yyyy";
                else if (keyVal.length == 10)
                    dateFormatter.dateFormat = @"yyyy-MM-dd";
                else NSLog(@"Illegal date format!");

                entry.firstpublishedDate = [dateFormatter dateFromString:keyVal];
                dateFormatter.dateFormat = @"yyyy";
                entry.firstpublished = [dateFormatter stringFromDate:entry.firstpublishedDate];
            }
            else if ([(NSString *)key isEqualToString:@"language"])
            {
                sub = [keyVal substringWithRange:NSMakeRange(0, 2)];
                sub = sub.lowercaseString;
                if ([language objectForKey:sub])
                    entry.languageAsWord = [language objectForKey:sub];
                else
                    entry.languageAsWord = sub;
                entry.language = sub;
            }
            else if ([(NSString *)key isEqualToString:@"format"])
            {
                if (entry.format == nil)
                    entry.format = [dict objectForKey:@"format"];
            }
            else if ([(NSString *)key isEqualToString:@"forgiveness"])
            {
                entry.forgiveness = [dict objectForKey:@"forgiveness"];
                entry.forgivenessNumeric = [forgiveness objectForKey:keyVal];
            }
            else
            {
                [entry setValue:keyVal forKey:key];
                if ([(NSString *)key isEqualToString:@"coverArtURL"])
                {
                    [self downloadImage:entry.coverArtURL for:entry inContext:context];
                }
//              NSLog(@"Set key %@ to value %@ in metadata with ifid %@ title %@", key, [dict valueForKey:key], entry.ifid, entry.title)
            }
        }
    }
}

//- (void) setMetadataValue: (NSString*)val forKey: (NSString*)key forIFID: (NSString*)ifid
//{
//    Metadata *dict = [self fetchMetadataForIFID:ifid inContext:_managedObjectContext];
//    if (dict)
//    {
//        NSLog(@"libctl: user set value %@ = '%@' for %@", key, val, ifid);
//        dict.userEdited = @(YES);
//        [dict setValue:val forKey:key];
////        gameTableDirty = YES;
//    }
//}

- (Metadata *)fetchMetadataForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;
    NSPredicate *predicate;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:context];

    fetchRequest.entity = entity;

    predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@",ifid];
    fetchRequest.predicate = predicate;

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
        NSLog(@"fetchMetadataForIFID: Found no Metadata entity with ifid %@ in %@", ifid, (context == _managedObjectContext)?@"_managedObjectContext":@"childContext");
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

- (NSArray *) fetchObjects:(NSString *)entityName inContext:(NSManagedObjectContext *)context {

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:context];
    fetchRequest.entity = entity;

    NSError *error = nil;
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    return fetchedObjects;
}

- (void) convertLibraryToCoreData {

    // Add games from plist to Core Data store if we have just created a new one

    LibController * __unsafe_unretained weakSelf = self;

    NSManagedObjectContext *private = [_coreDataManager privateChildManagedObjectContext];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(backgroundManagedObjectContextDidSave:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:private];

    NSError *error;
    NSURL *appSuppDir = [[NSFileManager defaultManager]
               URLForDirectory:NSApplicationSupportDirectory
               inDomain:NSUserDomainMask
               appropriateForURL:nil
               create:YES
               error:&error];
    
    [self beginImporting];
    _addButton.enabled = NO;
    currentlyAddingGames = YES;

    [private performBlock:^{
        // First, we try to load the Metadata.plist and add all entries as Metadata entities
        NSMutableDictionary *metadata = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Metadata.plist"]);

        NSString *ifid;
        cursrc = kInternal;

        NSEnumerator *enumerator = [metadata keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [weakSelf addMetadata:[metadata objectForKey:ifid] forIFIDs:@[ifid] inContext:private];
        }

        // Second, we try to load the Games.plist and add all entries as Game entities
        NSMutableDictionary *games = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Games.plist"]);

        NSDate *timestamp = [NSDate date];
        
        enumerator = [games keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [weakSelf beginImporting];
            Metadata *meta = [weakSelf fetchMetadataForIFID:ifid inContext:private];

            Game *game;

            if (!meta) {
                // If we did not create a matching Metadata entity for this Game above, we simply
                // import it again, creating new metadata. This could happen if the user has deleted
                // the Metadata.plist but not the Games.plist file, or if the Metadata and Games plists
                // have gone out of sync somehow.
                meta = [weakSelf importGame: [games valueForKey:ifid] inContext:private reportFailure: NO];
                game = meta.game;
            } else {
                // Otherwise we simply use the Metadata entity we created
                game = (Game *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Game"
                                 inManagedObjectContext:private];
            }
            // Now we should have a Game with corresponding Metadata
            // (but we check anyway just to make sure)
            if (meta) {
                game.setting = (Settings *) [NSEntityDescription
                                             insertNewObjectForEntityForName:@"Settings"
                                             inManagedObjectContext:private];

                game.setting.bufferFont = (Font *) [NSEntityDescription
                                                    insertNewObjectForEntityForName:@"Font"
                                                    inManagedObjectContext:private];
                game.setting.gridFont = (Font *) [NSEntityDescription
                                                  insertNewObjectForEntityForName:@"Font"
                                                  inManagedObjectContext:private];
                game.setting.bufInput = (Font *) [NSEntityDescription
                                                  insertNewObjectForEntityForName:@"Font"
                                                  inManagedObjectContext:private];
                game.metadata = meta;
                game.added = [NSDate date];
                [game bookmarkForPath:[games valueForKey:ifid]];

                // First, we look for a cover image file in Spatterlight Application Support folder

                NSString *pathstring =
                [[@"Spatterlight/Cover%20Art" stringByAppendingPathComponent:ifid]
                 stringByAppendingPathExtension:@"tiff"];
                NSURL *imgpath = [NSURL URLWithString:pathstring relativeToURL:appSuppDir];
                NSData *imgdata = [[NSData alloc] initWithContentsOfURL:imgpath];

                // If that fails, we try Babel
                if (!imgdata) {
                    const char *format = babel_init((char *)((NSString *)[games valueForKey:ifid]).UTF8String);
                    if (format) {
                        int imglen = babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL, NULL, 0);
                        if (imglen > 0) {
                            char *imgbuf = malloc(imglen);
                            if (imgbuf) {
                                NSLog(@"Babel found image data in %@!", meta.title);
                                babel_treaty(GET_STORY_FILE_COVER_SEL, imgbuf, imglen);
                                imgdata = [[NSData alloc] initWithBytesNoCopy:imgbuf
                                                                               length:imglen
                                                                         freeWhenDone:YES];
                            }
                        }
                    }
                    babel_release();
                }  else NSLog(@"Found cover image file for %@ at %@!", meta.title, imgpath.path);

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
                    } else NSLog(@"Changes in private were saved");
                } else NSLog(@"No changes to save in private");
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
            _addButton.enabled = YES;
            currentlyAddingGames = NO;
        });

        cursrc = 0;
    }];
}

static void read_xml_text(const char *rp, char *wp) {
    /* kill initial whitespace */
    while (*rp && isspace(*rp))
        rp++;

    while (*rp) {
        if (*rp == '<') {
            if (strstr(rp, "<br/>") == rp || strstr(rp, "<br />") == rp) {
                *wp++ = '\n';
                *wp++ = '\n';
                rp += 5;
                if (*rp == '>')
                    rp++;

                /* kill trailing whitespace */
                while (*rp && isspace(*rp))
                    rp++;
            } else {
                *wp++ = *rp++;
            }
        } else if (*rp == '&') {
            if (strstr(rp, "&lt;") == rp) {
                *wp++ = '<';
                rp += 4;
            } else if (strstr(rp, "&gt;") == rp) {
                *wp++ = '>';
                rp += 4;
            } else if (strstr(rp, "&amp;") == rp) {
                *wp++ = '&';
                rp += 5;
            } else {
                *wp++ = *rp++;
            }
        } else if (isspace(*rp)) {
            *wp++ = ' ';
            while (*rp && isspace(*rp))
                rp++;
        } else {
            *wp++ = *rp++;
        }
    }

    *wp = 0;
}

- (void)handleXMLCloseTag:(struct XMLTag *)tag {
    char bigbuf[4096];

    /* we don't parse tags after bibliographic until the next story begins... */
    if (!strcmp(tag->tag, "bibliographic")) {
        NSLog(@"libctl: import metadata for %@ by %@ in %@",
              [metabuf objectForKey:@"title"],
              [metabuf objectForKey:@"author"],
            (importContext == _managedObjectContext)?@"_managedObjectContext":@"childContext");
        [self addMetadata:metabuf forIFIDs:ifidbuf inContext:importContext];
        ifidbuf = nil;
        if (currentIfid == nil) /* We are not importing from ifdb */
            metabuf = nil;
    }
    /* parse extra ifdb tags... */
    else if (!strcmp(tag->tag, "ifdb"))
    {
        NSLog(@"libctl: import extra ifdb metadata for %@", [metabuf objectForKey:@"title"]);
        [self addMetadata: metabuf forIFIDs: @[currentIfid] inContext:importContext];
        metabuf = nil;
    }
    /* we should only find ifid:s inside the <identification> tag, */
    /* which should be the first tag in a <story> */
    else if (!strcmp(tag->tag, "ifid")) {
        if (!ifidbuf) {
            ifidbuf = [[NSMutableArray alloc] initWithCapacity:1];
            metabuf = [[NSMutableDictionary alloc] initWithCapacity:10];
        }
        char save = *tag->end;
        *tag->end = 0;
        [ifidbuf addObject:@(tag->begin)];
        *tag->end = save;
    }

    /* only extract text from within the indentification and bibliographic tags
     */
    else if (metabuf) {
        if (tag->end - tag->begin >= (long)sizeof(bigbuf) - 1)
            return;

        char save = *tag->end;
        *tag->end = 0;
        read_xml_text(tag->begin, bigbuf);
        *tag->end = save;

        NSString *val = @(bigbuf);

        if (!strcmp(tag->tag, "format"))
            [metabuf setObject:val forKey:@"format"];
        if (!strcmp(tag->tag, "bafn"))
            [metabuf setObject:val forKey:@"bafn"];
        if (!strcmp(tag->tag, "title"))
            [metabuf setObject:val forKey:@"title"];
        if (!strcmp(tag->tag, "author"))
            [metabuf setObject:val forKey:@"author"];
        if (!strcmp(tag->tag, "language"))
            [metabuf setObject:val forKey:@"language"];
        if (!strcmp(tag->tag, "headline"))
            [metabuf setObject:val forKey:@"headline"];
        if (!strcmp(tag->tag, "firstpublished"))
            [metabuf setObject:val forKey:@"firstpublished"];
        if (!strcmp(tag->tag, "genre"))
            [metabuf setObject:val forKey:@"genre"];
        if (!strcmp(tag->tag, "group"))
            [metabuf setObject:val forKey:@"group"];
        if (!strcmp(tag->tag, "description"))
            [metabuf setObject:val forKey:@"description"];
        if (!strcmp(tag->tag, "series"))
            [metabuf setObject:val forKey:@"series"];
        if (!strcmp(tag->tag, "seriesnumber"))
            [metabuf setObject:val forKey:@"seriesnumber"];
        if (!strcmp(tag->tag, "forgiveness"))
            [metabuf setObject:val forKey:@"forgiveness"];
        if (!strcmp(tag->tag, "tuid"))
            [metabuf setObject:val forKey:@"tuid"];
        if (!strcmp(tag->tag, "averageRating"))
            [metabuf setObject:val forKey:@"averageRating"];
        if (!strcmp(tag->tag, "starRating"))
            [metabuf setObject:val forKey:@"starRating"];
        if (!strcmp(tag->tag, "url"))
            [metabuf setObject:val forKey:@"coverArtURL"];
    }
}

- (void)handleXMLError:(char *)msg {
    if (strstr(msg, "Error:"))
        NSLog(@"libctl: xml: %s", msg);
}

static void handleXMLCloseTag(struct XMLTag *tag, void *ctx)
{
    [(__bridge LibController *)ctx handleXMLCloseTag:tag];
}

static void handleXMLError(char *msg, void *ctx)
{
    [(__bridge LibController *)ctx handleXMLError:msg];
}

- (void)importMetadataFromXML:(char *)mdbuf {
    ifiction_parse(mdbuf, handleXMLCloseTag, (__bridge void *)(self),
                   handleXMLError, (__bridge void *)(self));
    ifidbuf = nil;
    metabuf = nil;
}

- (BOOL)importMetadataFromFile:(NSString *)filename {
    NSLog(@"libctl: importMetadataFromFile %@", filename);

    if (currentlyAddingGames)
        return NO;

    NSMutableData *data;

    data = [NSMutableData dataWithContentsOfFile:filename];
    if (!data)
        return NO;
    [data appendBytes:"\0" length:1];

    cursrc = kExternal;
    importContext = _managedObjectContext;
    [self importMetadataFromXML:data.mutableBytes];
    importContext = nil;
    cursrc = 0;

    [_coreDataManager saveChanges];

    for (Game *game in gameTableModel) {
        [_managedObjectContext refreshObject:game.metadata
                                mergeChanges:YES];
    }
    return YES;
}

- (BOOL)downloadMetadataForIFID:(NSString*)ifid inContext:context {
    NSLog(@"libctl: downloadMetadataForIFID %@", ifid);

    if (!ifid)
        return NO;

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.tads.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid]];

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSURLResponse *response = nil;
    NSError *error = nil;

    NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest returningResponse:&response
                                                     error:&error];
    if (error) {
        if (!data) {
            NSLog(@"Error connecting: %@", [error localizedDescription]);
            return NO;
        }
    }
    else
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode == 200 && data) {
            NSMutableData *mutableData = [data mutableCopy];
            [mutableData appendBytes: "\0" length: 1];

            cursrc = kIfdb;
            currentIfid = ifid;
            NSLog(@"Set currentIfid to %@", currentIfid);
            importContext = context;
            [self importMetadataFromXML: mutableData.mutableBytes];
            importContext = nil;
            currentIfid = nil;
            cursrc = 0;
        } else return NO;
    }
    return YES;
}

- (BOOL) downloadImage:(NSString*)imgurl for:(Metadata *)metadata inContext:(NSManagedObjectContext *)context
{
    NSLog(@"libctl: download image from url %@", imgurl);

    NSURL *url = [NSURL URLWithString:imgurl];
    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSURLResponse *response = nil;
    NSError *error = nil;

    NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest returningResponse:&response
                                                     error:&error];
    if (error) {
        if (!data) {
            NSLog(@"Error connecting: %@", [error localizedDescription]);
            return NO;
        }
    }
    else
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        if (httpResponse.statusCode == 200 && data) {
            NSMutableData *mutableData = [data mutableCopy];
            [self addImage: mutableData toMetadata:metadata inContext:context];
        } else return NO;
    }
    return YES;
}

- (void) addImage:(NSData *)rawImageData toMetadata:(Metadata *)metadata inContext:(NSManagedObjectContext *)context {

    Image *img = (Image *) [NSEntityDescription
                     insertNewObjectForEntityForName:@"Image"
                     inManagedObjectContext:context];
    img.data = rawImageData;
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
    NSDictionary *info;
    int src;

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
        src = [[info objectForKey:kSource] intValue];

        
            fprintf(fp, "<story>\n");

            fprintf(fp, "<identification>\n");
            fprintf(fp, "<ifid>%s</ifid>\n", meta.ifid.UTF8String);
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
    Metadata *meta = [self fetchMetadataForIFID:ifid inContext:_managedObjectContext];
    if (!meta) return nil;
    return [self playGame:meta.game winRestore:YES];
}


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

- (NSWindow *)playGame:(Game *)game 
                 winRestore:(BOOL)restoreflag {
    Metadata *meta = game.metadata;
    NSURL *url = [game urlForBookmark];
    NSString *path = url.path;
    NSString *terp;
    GlkController *gctl = [_gameSessions objectForKey:game.metadata.ifid];

    NSLog(@"playgame %@", game);

    if (gctl) {
        NSLog(@"A game with this ifid is already in session");
        [gctl.window makeKeyAndOrderFront:nil];
        return gctl.window;
    }

    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        game.found = @(NO);
        NSRunAlertPanel(
                        @"Cannot find the file.",
                        @"The file could not be found at its original location. Maybe "
                        @"it has been moved since it was added to the library.",
                        @"Okay", NULL, NULL);
        return nil;
    } else game.found = @(YES);


    if (![[NSFileManager defaultManager] isReadableFileAtPath:path]) {
        NSRunAlertPanel(@"Cannot read the file.",
                        @"The file exists but can not be read.", @"Okay", NULL,
                        NULL);
        return nil;
    }

    terp = [gExtMap objectForKey:path.pathExtension];
    if (!terp)
        terp = [gFormatMap objectForKey:meta.format];


    if (!terp) {
        NSRunAlertPanel(@"Cannot play the file.",
                        @"The game is not in a recognized file format; cannot "
                        @"find a suitable interpreter.",
                        @"Okay", NULL, NULL);
        return nil;
    }

    gctl = [[GlkController alloc] initWithWindowNibName:@"GameWindow"];
    if ([terp isEqualToString:@"glulxe"] || [terp isEqualToString:@"fizmo"]) {
        gctl.window.restorable = YES;
        gctl.window.restorationClass = [AppDelegate class];
        gctl.window.identifier = [NSString stringWithFormat:@"gameWin%@", meta.ifid];
    } else
        gctl.window.restorable = NO;

    [_gameSessions setObject:gctl forKey:meta.ifid];

    [gctl runTerp:terp withGame:game reset:NO winRestore:restoreflag];
    game.lastPlayed = [NSDate date];
    [self addURLtoRecents: game.urlForBookmark];

    return gctl.window;
}

- (void)importAndPlayGame:(NSString *)path {

    Metadata *meta = [self importGame: path inContext:_managedObjectContext reportFailure: YES];
    if (meta)
    {
        [self selectGames:@[meta.game]];
        [self playGame: meta.game];
    }
}

- (Metadata *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report {
    char buf[TREATY_MINIMUM_EXTENT];
    Metadata *metadata;
    NSString *ifid;
    char *format;
    char *s;
    int mdlen;
    int rv;

    if ([path.pathExtension.lowercaseString isEqualToString: @"ifiction"])
    {
        [self addIfidFile:path];
        return nil;
    }

    if ([path.pathExtension.lowercaseString isEqualToString: @"d$$"])
    {
        path = [self convertAGTFile: path];
        if (!path)
        {
            if (report)
                NSRunAlertPanel(@"Conversion failed.",
                                @"This old style AGT file could not be converted to the new AGX format.",
                                @"Okay", NULL, NULL);
            return nil;
        }
    }

    if (![gGameFileTypes containsObject: path.pathExtension.lowercaseString])
    {
        if (report)
            NSRunAlertPanel(@"Unknown file format.",
                            @"Can not recognize the file extension.",
                            @"Okay", NULL, NULL);
        return nil;
    }

    format = babel_init((char*)path.UTF8String);
    if (!format || !babel_get_authoritative())
    {
        if (report)
            NSRunAlertPanel(@"Unknown file format.",
                            @"Babel can not identify the file format.",
                            @"Okay", NULL, NULL);
        babel_release();
        return nil;
    }

    s = strchr(format, ' ');
    if (s) format = s+1;

    rv = babel_treaty(GET_STORY_FILE_IFID_SEL, buf, sizeof buf);
    if (rv <= 0)
    {
        if (report)
            NSRunAlertPanel(@"Fatal error.",
                            @"Can not compute IFID from the file.",
                            @"Okay", NULL, NULL);
        babel_release();
        return nil;
    }

    s = strchr(buf, ',');
    if (s) *s = 0;
    ifid = @(buf);

    //NSLog(@"libctl: import game %@ (%s)", path, format);

    mdlen = babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL, NULL, 0);
    if (mdlen > 0)
    {
        char *mdbuf = malloc(mdlen);
        if (!mdbuf)
        {
            if (report)
                NSRunAlertPanel(@"Out of memory.",
                                @"Can not allocate memory for the metadata text.",
                                @"Okay", NULL, NULL);
            babel_release();
            return nil;
        }

        rv = babel_treaty(GET_STORY_FILE_METADATA_SEL, mdbuf, mdlen);
        if (rv > 0)
        {
            cursrc = kInternal;
            importContext = context;
            [self importMetadataFromXML: mdbuf];
            importContext = nil;
            cursrc = 0;
        }

        free(mdbuf);
    }

    metadata = [self fetchMetadataForIFID:ifid inContext:context];
    if (!metadata)
    {
        metadata = (Metadata *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Metadata"
                                 inManagedObjectContext:context];
    }
	else
	{
		if (metadata.game)
		{
			NSLog(@"Game %@ already exists in library!", metadata.title);
			if (![path isEqualToString:[metadata.game urlForBookmark].path])
			{
				NSLog(@"File location did not match. Updating library with new file location.");
				[metadata.game bookmarkForPath:path];
			}
            metadata.game.found = @(YES);
			return metadata;
		}
	}

    if (!metadata.format)
        metadata.format = @(format);
    if (!metadata.ifid)
        metadata.ifid = ifid;
    if (!metadata.title)
    {
        metadata.title = [path.lastPathComponent stringByDeletingPathExtension];
//      [self downloadMetadataForIFID:ifid inContext:context];
    }

    if (!metadata.cover)
    {
        NSString *dirpath = (@"~/Library/Application Support/Spatterlight/Cover Art").stringByStandardizingPath;
        NSString *imgpath = [[dirpath stringByAppendingPathComponent: ifid] stringByAppendingPathExtension: @"tiff"];
        NSData *img = [[NSData alloc] initWithContentsOfFile: imgpath];
        if (img)
        {
            [self addImage:img toMetadata:metadata inContext:context];
        }
        else
        {
            int imglen = babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL, NULL, 0);
            if (imglen > 0)
            {
                char *imgbuf = malloc(imglen);
                if (imgbuf)
                {
                    //                    rv =
					babel_treaty(GET_STORY_FILE_COVER_SEL, imgbuf, imglen);
                    [self addImage:[[NSData alloc] initWithBytesNoCopy: imgbuf length: imglen freeWhenDone: YES] toMetadata:metadata inContext:context];

                }
            }
        }
    }

    babel_release();

    Game *game = (Game *) [NSEntityDescription
                           insertNewObjectForEntityForName:@"Game"
                           inManagedObjectContext:context];

    [game bookmarkForPath:path];

    game.added = [NSDate date];
    game.metadata = metadata;

    return metadata;
}

- (void) addFile: (NSURL*)url select: (NSMutableArray*)select inContext:(NSManagedObjectContext *)context
{
    Metadata *meta = [self importGame: url.path inContext:context reportFailure: NO];
    if (meta) {
        [self beginImporting];
        [select addObject: meta.ifid];

//        NSError *error = nil;
//        if (context.hasChanges) {
//            if (![context save:&error]) {
//                if (error) {
//                    [[NSApplication sharedApplication] presentError:error];
//                }
//            }
//        }
//
//        [_managedObjectContext performBlock:^{
//            [_coreDataManager saveChanges];
//        }];

    } else {
		NSLog(@"libctl: addFile: File %@ not added!", url.path);
	}
}

- (void) addFiles:(NSArray*)urls select:(NSMutableArray*)select inContext:(NSManagedObjectContext *)context {
    NSFileManager *filemgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSInteger count;
    NSInteger i;

    NSDate *timestamp = [NSDate date];
    
    count = urls.count;
    for (i = 0; i < count; i++)
    {
        NSString *path = [[urls objectAtIndex:i] path];

        if (![filemgr fileExistsAtPath: path isDirectory: &isdir])
            continue;

        if (isdir)
        {
            NSArray *contents = [filemgr contentsOfDirectoryAtURL:[urls objectAtIndex:i]
                                       includingPropertiesForKeys:@[NSURLNameKey]
                                                          options:NSDirectoryEnumerationSkipsHiddenFiles
                                                            error:nil];
            [self addFiles: contents select: select inContext:context];
        }
        else
        {
            [self addFile:[urls objectAtIndex:i] select: select inContext:context];
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
//            dispatch_async(dispatch_get_main_queue(), ^{
//                [self selectGamesWithIfids:select scroll:NO];
//            });
            timestamp = [NSDate date];
        }

    }
}

- (void) addFiles: (NSArray*)urls inContext:(NSManagedObjectContext *)context {

    NSLog(@"libctl: adding %lu files. First: %@", (unsigned long)urls.count, ((NSURL *)[urls objectAtIndex:0]).path);

    
    NSMutableArray *select = [NSMutableArray arrayWithCapacity: urls.count];

    LibController * __unsafe_unretained weakSelf = self;

    [self beginImporting];
    [self addFiles: urls select: select inContext:context];
    [self endImporting];

    NSError *error = nil;
    if (context.hasChanges) {
        if (![context save:&error]) {
            if (error) {
                [[NSApplication sharedApplication] presentError:error];
            }
        }
    }

//    dispatch_async(dispatch_get_main_queue(), ^{
//        for (NSString *path in iFictionFiles) {
//            [weakSelf importMetadataFromFile:path];
//        }
//        iFictionFiles = nil;
//        [_coreDataManager saveChanges];
//        [weakSelf selectGamesWithIfids:select scroll:NO];
//    });

    [_managedObjectContext performBlock:^{
        [_coreDataManager saveChanges];
        [weakSelf selectGamesWithIfids:select scroll:NO];
        for (NSString *path in iFictionFiles) {
            [weakSelf importMetadataFromFile:path];
        }
        iFictionFiles = nil;
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
//        [self updateTableViews];
        for (NSString *ifid in ifids) {
            Metadata *meta = [self fetchMetadataForIFID:ifid inContext:_managedObjectContext];
            if (meta && meta.game) {
                [newSelection addObject:meta.game];
                NSLog(@"Selecting game with ifid %@", ifid);
            } else NSLog(@"No game with ifid %@ in library, cannot restore selection", ifid);
        }
        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

        for (Game *game in newSelection) {
            if ([gameTableModel containsObject:game]) {
                [indexSet addIndex:[gameTableModel indexOfObject:game]];
                NSLog(@"Selecting game %@ in table.", game.metadata.title);

            }
        }
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:YES];
        if (shouldscroll && indexSet.count && !currentlyAddingGames)
            [_gameTableView scrollRowToVisible:indexSet.firstIndex];
    }
}

- (void) selectGames:(NSArray*)games
{
    NSLog(@"selectGames called with %ld games", games.count);

    if (games.count) {
        
        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

        for (Game *game in games) {
            if ([gameTableModel containsObject:game]) {
                [indexSet addIndex:[gameTableModel indexOfObject:game]];
                NSLog(@"selecting game %@",game.metadata.title);
            } else NSLog(@"Game %@ not found in gameTableModel",game.metadata.title);

        }
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        if (indexSet.count == 1 && !currentlyAddingGames)
            [_gameTableView scrollRowToVisible:indexSet.firstIndex];
    }
}

static NSInteger Strcmp(NSString *a, NSString *b)
{
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedCaseInsensitiveCompare: b];
}

static NSInteger compareGames(Metadata *a, Metadata *b, id key, bool ascending)
{
    NSString * ael = [a valueForKey:key];
    NSString * bel = [b valueForKey:key];
    return compareStrings(ael, bel, ascending);
}

static NSInteger compareStrings(NSString *ael, NSString *bel, bool ascending)
{
    if ((!ael || ael.length == 0) && (!bel || bel.length == 0))
        return NSOrderedSame;
    if (!ael || ael.length == 0) return ascending ? NSOrderedDescending :  NSOrderedAscending;
    if (!bel || bel.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return Strcmp(ael, bel);
}

static NSInteger compareDates(NSDate *ael, NSDate *bel,bool ascending)
{
    if ((!ael) && (!bel))
        return NSOrderedSame;
    if (!ael) return ascending ? NSOrderedDescending :  NSOrderedAscending;
    if (!bel) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [ael compare:bel];
}

- (void) updateTableViews
{

    NSFetchRequest *fetchRequest;
    NSEntityDescription *entity;

    NSInteger searchcount;

    if (!gameTableDirty)
        return;

    NSLog(@"Updating table view");
    
    fetchRequest = [[NSFetchRequest alloc] init];
    entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];
    fetchRequest.entity = entity;

    if (searchStrings)
    {
        searchcount = searchStrings.count;

        NSPredicate *predicate;
        NSMutableArray *predicateArr = [NSMutableArray arrayWithCapacity:searchcount];

        for (NSString *word in searchStrings) {
            predicate = [NSPredicate predicateWithFormat: @"(metadata.format contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.language contains [c] %@) OR (metadata.firstpublished contains %@)", word, word, word, word, word, word, word, word, word, word, word];
            [predicateArr addObject:predicate];
        }


        NSCompoundPredicate *comp = (NSCompoundPredicate *)[NSCompoundPredicate orPredicateWithSubpredicates: predicateArr];

        fetchRequest.predicate = comp;
    }

    NSError *error = nil;
    gameTableModel = [[_managedObjectContext executeFetchRequest:fetchRequest error:&error] mutableCopy];
    if (gameTableModel == nil)
        NSLog(@"Problem! %@",error);

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:sortAscending comparator:^(Game *aid, Game *bid) {

        Metadata *a = aid.metadata;
        Metadata *b = bid.metadata;
        NSInteger cmp;
        if ([gameSortColumn isEqual:@"firstpublishedDate"])
        {
            cmp = compareDates(a.firstpublishedDate, b.firstpublishedDate, sortAscending);
            if (cmp) return cmp;
        }
        else if ([gameSortColumn isEqual:@"added"] || [gameSortColumn isEqual:@"lastPlayed"] || [gameSortColumn isEqual:@"lastModified"])
        {
            cmp = compareDates([a.game valueForKey:gameSortColumn], [b.game valueForKey:gameSortColumn], sortAscending);
            if (cmp) return cmp;
        }
        else if ([gameSortColumn isEqual:@"found"]) {
            NSString *string1 = [a.game.found isEqual:@(YES)]?nil:@"A";
            NSString *string2 = [b.game.found isEqual:@(YES)]?nil:@"A";
            cmp = compareStrings(string1, string2, sortAscending);
            if (cmp) return cmp;
        }
        else if ([gameSortColumn isEqual:@"forgivenessNumeric"]) {
            NSString *string1 = a.forgivenessNumeric ? [NSString stringWithFormat:@"%@", a.forgivenessNumeric] : nil;
            NSString *string2 = b.forgivenessNumeric ? [NSString stringWithFormat:@"%@", b.forgivenessNumeric] : nil;
            cmp = compareStrings(string1, string2, sortAscending);
            if (cmp) return cmp;
        }
        else if (gameSortColumn)
        {
            cmp = compareGames(a, b, gameSortColumn, sortAscending);
            if (cmp) return cmp;
        }
        cmp = compareGames(a, b, @"title", sortAscending);
        if (cmp) return cmp;
        cmp = compareGames(a, b, @"author", sortAscending);
        if (cmp) return cmp;
        cmp = compareGames(a, b, @"seriesnumber", sortAscending);
        if (cmp) return cmp;
        cmp = compareDates(a.firstpublishedDate, b.firstpublishedDate, sortAscending);
        if (cmp) return cmp;
        return compareGames(a, b, @"format", sortAscending);
    }];

    [gameTableModel sortUsingDescriptors:@[sort]];
    [_gameTableView reloadData];

    [self selectGames:selectedGames];

    gameTableDirty = NO;
}

- (void)tableView:(NSTableView *)tableView
    sortDescriptorsDidChange:(NSArray *)oldDescriptors {
    if (tableView == _gameTableView) {
        NSSortDescriptor *sortDescriptor =
            [tableView.sortDescriptors objectAtIndex:0];
        if (!sortDescriptor)
            return;
        gameSortColumn = sortDescriptor.key;
        sortAscending = sortDescriptor.ascending;
        gameTableDirty = YES;
        [self updateTableViews];
        NSIndexSet *rows = tableView.selectedRowIndexes;
        [_gameTableView scrollRowToVisible:rows.firstIndex];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    if (tableView == _gameTableView)
        return gameTableModel.count;
    return 0;
}

- (id) tableView: (NSTableView*)tableView
objectValueForTableColumn: (NSTableColumn*)column
             row: (int)row
{
    if (tableView == _gameTableView) {
        Game *game = [gameTableModel objectAtIndex:row];
        Metadata *meta = game.metadata;
        if ([column.identifier isEqual: @"found"])
            return [game.found isEqual: @(YES)]?nil:@"!";
        if ([column.identifier isEqual: @"added"] || [column.identifier  isEqual: @"lastPlayed"] ||
            [column.identifier  isEqual: @"lastModified"]) {
            return [[game valueForKey: column.identifier] formattedRelativeString];
        } else
            return [meta valueForKey: column.identifier];
    }
    return nil;
}

- (void)tableView:(NSTableView *)tableView
  willDisplayCell:(id)cell
   forTableColumn:(NSTableColumn *)tableColumn
              row:(NSInteger)row {
    if (cell == _foundIndicatorCell) {
        NSMutableAttributedString *attstr = [((NSTextFieldCell *)cell).attributedStringValue mutableCopy];

        NSFont *font = [NSFont fontWithName:@"ExclamationCircleNew-Regular" size:14];

        [attstr addAttribute:NSFontAttributeName
                       value:font
                       range:NSMakeRange(0, attstr.length)];

        if (NSAppKitVersionNumber < NSAppKitVersionNumber10_9) {

            [attstr addAttribute:NSBaselineOffsetAttributeName
                       value:[NSNumber numberWithFloat:2.0]
                       range:NSMakeRange(0, attstr.length)];
        }

        [(NSTextFieldCell *)cell setAttributedStringValue:attstr];
    }
}

- (void)tableView:(NSTableView *)tableView
   setObjectValue:(id)value
   forTableColumn:(NSTableColumn *)tableColumn
              row:(int)row {
    if (tableView == _gameTableView)
    {
        Game *game = [gameTableModel objectAtIndex:row];
        Metadata *meta = game.metadata;
        NSString *key = tableColumn.identifier;
        NSString *oldval = [meta valueForKey:key];

        if ([key isEqualToString:@"forgivenessNumeric"] && [value isEqual:@(FORGIVENESS_NONE)])
            value = nil;

        if (([key isEqualToString:@"starRating"] || [key isEqualToString:@"myRating"]) && [value isEqual:@(0)])
            value = nil;

		if ([value isKindOfClass:[NSNumber class]] && ![key isEqualToString:@"forgivenessNumeric"]) {
			value = [NSString stringWithFormat:@"%@", value];
        }

        if ([value isKindOfClass:[NSString class]] && ((NSString*)value).length == 0)
            value = nil;
        
        if ([value isEqualTo: oldval])
            return;

        [meta setValue: value forKey: key];
        meta.userEdited = @(YES);
        game.lastModified = [NSDate date];
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
        if (gameTableModel.count && rows.count) {
            selectedGames = [gameTableModel objectsAtIndexes:rows];
        } else selectedGames = nil;
        [self updateSideViewForce:NO];
    }
}

- (void)backgroundManagedObjectContextDidSave:(id)sender {
    NSLog(@"backgroundManagedObjectContextDidSave");
    [_coreDataManager saveChanges];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    NSLog(@"noteManagedObjectContextDidChange");
    gameTableDirty = YES;
    [self updateTableViews];
    NSArray *updatedObjects = [notification.userInfo objectForKey:NSUpdatedObjectsKey];
    if ([updatedObjects containsObject:currentSideView])
    {
        NSLog(@"Game currently on display in side view (%@) did change", currentSideView.title);

        [self updateSideViewForce:YES];
    }
}

- (void) updateSideViewForce:(BOOL)force
{
    Metadata *meta = nil;
    NSString *string = nil;

	if ([_splitView isSubviewCollapsed:_leftView])
	{
		NSLog(@"Side view collapsed, returning without updating side view");
		return;
	}

    if (!selectedGames || !selectedGames.count || selectedGames.count > 1) {

        string = (selectedGames.count > 1) ? @"Multiple selections" : @"No selection";

    } else meta = ((Game *)[selectedGames objectAtIndex:0]).metadata;

    if (force == NO && meta == currentSideView) {
        NSLog(@"updateSideView: %@ is already shown and force is NO", meta.title);
        return;
    }

    currentSideView = meta;

	NSLog(@"\nUpdating info pane for %@", meta.title);
    //NSLog(@"Side view width: %f", NSWidth(_leftView.frame));

    [_leftScrollView.documentView performSelector:@selector(removeFromSuperview)];

	SideInfoView *infoView = [[SideInfoView alloc] initWithFrame:_leftScrollView.frame];
    
	_leftScrollView.documentView = infoView;

    _sideIfid.stringValue = @"";

    if (meta) {

        [infoView updateSideViewWithMetadata:meta];

        if (meta.ifid)
            _sideIfid.stringValue=meta.ifid;
    } else if (string) {
        [infoView updateSideViewWithString:string];
    }

	_sideIfid.delegate = infoView;
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
    CGFloat result;
    if (NSWidth(_rightView.frame) <= RIGHT_VIEW_MIN_WIDTH)
        result = NSWidth(splitView.frame) - RIGHT_VIEW_MIN_WIDTH;
    else
        result = NSWidth(splitView.frame) / 2;

//    NSLog(@"splitView:constrainMaxCoordinate:%f ofSubviewAt:%ld (returning %f)", proposedMaximumPosition, dividerIndex, result);
    return result;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
//    NSLog(@"splitView:constrainMinCoordinate:%f ofSubviewAt:%ld (returning 200)", proposedMinimumPosition, dividerIndex);

	return (CGFloat)PREFERRED_LEFT_VIEW_MIN_WIDTH;
}

-(IBAction)toggleSidebar:(id)sender;
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
	_leftView.hidden = YES;
    [_splitView setPosition:0
           ofDividerAtIndex:0];
	[_splitView display];
}

-(void)uncollapseLeftView
{
	_leftView.hidden = NO;

    CGFloat dividerThickness = _splitView.dividerThickness;

	// make sideview at least PREFERRED_LEFT_VIEW_MIN_WIDTH
    if (lastSideviewWidth < PREFERRED_LEFT_VIEW_MIN_WIDTH)
        lastSideviewWidth = PREFERRED_LEFT_VIEW_MIN_WIDTH;
    
    if (self.window.frame.size.width < PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + dividerThickness) {
        [self.window setContentSize:NSMakeSize(PREFERRED_LEFT_VIEW_MIN_WIDTH + RIGHT_VIEW_MIN_WIDTH + dividerThickness, ((NSView *)self.window.contentView).frame.size.height)];
    }

    [_splitView setPosition:lastSideviewWidth
           ofDividerAtIndex:0];

    [_splitView display];
    [self updateSideViewForce:YES];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
    // TODO: This should call a faster update method rather than rebuilding the view from scratch every time, but everything I've tried makes word wrap wonky
    if (currentSideView)
        [self updateSideViewForce:YES];
}

#pragma mark -
#pragma mark Windows restoration

- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state {
    [state encodeObject:_searchField.stringValue forKey:@"searchText"];

    [state encodeFloat:NSWidth(_leftView.frame) forKey:@"sideviewWidth"];
//    NSLog(@"Encoded left view width as %f", NSWidth(_leftView.frame))
    [state encodeBool:[_splitView isSubviewCollapsed:_leftView] forKey:@"sideviewHidden"];
//    NSLog(@"Encoded left view collapsed as %@", [_splitView isSubviewCollapsed:_leftView]?@"YES":@"NO");

    if (selectedGames.count) {
        NSMutableArray *selectedGameIfids = [NSMutableArray arrayWithCapacity:selectedGames.count];
        NSString *str;
        for (Game *game in selectedGames) {
            str = game.metadata.ifid;
            if (str)
                [selectedGameIfids addObject:str];
        }
        [state encodeObject:selectedGameIfids forKey:@"selectedGames"];
//        NSLog(@"Encoded %ld selected games", (unsigned long)selectedGameIfids.count);
    }
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    NSString *searchText = [state decodeObjectForKey:@"searchText"];
    gameTableDirty = YES;
    if (searchText.length) {
        //        NSLog(@"Restored searchbar text %@", searchText);
        _searchField.stringValue = searchText;
        [self searchForGames:_searchField];
    }
    NSArray *selectedIfids = [state decodeObjectForKey:@"selectedGames"];
    [self updateTableViews];
    [self selectGamesWithIfids:selectedIfids scroll:NO];

    CGFloat newDividerPos = [state decodeFloatForKey:@"sideviewWidth"];
    
    if (newDividerPos < 50 && newDividerPos > 0) {
        NSLog(@"Left view width too narrow, setting to 50.");
        newDividerPos = 50;
    }
    
    [_splitView setPosition:newDividerPos
ofDividerAtIndex:0];
    
     NSLog(@"Restored left view width as %f", NSWidth(_leftView.frame));
     NSLog(@"Now _leftView.frame is %@", NSStringFromRect(_leftView.frame));

    BOOL collapsed = [state decodeBoolForKey:@"sideviewHidden"];
    NSLog(@"Decoded left view visibility as %@", collapsed?@"YES":@"NO");

    if (collapsed)
        [self collapseLeftView];
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
    //    NSLog(@"doClick:");
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
    //    NSLog(@"doDoubleClick:");
    [self enableClickToRenameAfterDelay];
    [self play:sender];
}

- (void)enableClickToRenameAfterDelay {
    canEdit = NO;
    [self startTimerWithTimeInterval:0.2
                            selector:@selector(enableClickToRenameByTimer:)];
}

- (void)enableClickToRenameByTimer:(id)sender {
    // NSLog(@"enableClickToRenameByTimer:");
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

    format = babel_init(rapeme);
    if (!strcmp(format, "agt")) {
        char buf[TREATY_MINIMUM_EXTENT];
        int rv = babel_treaty(GET_STORY_FILE_IFID_SEL, buf, sizeof buf);
        if (rv == 1) {
            dirpath =
                [homepath.path stringByAppendingPathComponent:@"Converted"];

            [[NSFileManager defaultManager]
                       createDirectoryAtURL:[NSURL fileURLWithPath:dirpath
                                                       isDirectory:YES]
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:NULL];

            cvtpath =
                [dirpath stringByAppendingPathComponent:
                             [@(buf) stringByAppendingPathExtension:@"agx"]];

            babel_release();

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
    babel_release();
    return nil;
}

@end
