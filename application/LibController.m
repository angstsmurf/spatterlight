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
    [(LibController *)self.delegate updateSideView];
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
        gameTableModel = [[self fetchObjects:@"Game"] mutableCopy];

//        [_coreDataManager saveChanges];
    }
    return self;
}

- (void)windowDidLoad {
//    NSLog(@"libctl: windowDidLoad");

    _coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;
    _managedObjectContext = _coreDataManager.mainManagedObjectContext;

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

    gameTableDirty = YES;
    [self updateTableViews];

    // Add metadata and games from plists to Core Data store if we have just created a new one
    gameTableModel = [[self fetchObjects:@"Game"] mutableCopy];
    if ([self fetchObjects:@"Metadata"].count == 0 || gameTableModel.count == 0)
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
        NSError *saveError = nil;
        [_managedObjectContext save:&saveError];
        //more error handling here
        gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (IBAction)purgeLibrary:(id)sender {
    NSInteger choice =
        NSRunAlertPanel(@"Do you really want to purge the library?",
                        @"Purging will delete the information about games that "
                        @"are not in the library at the moment.",
                        @"Purge", NULL, @"Cancel");
    if (choice != NSAlertOtherReturn) {

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
            NSError *saveError = nil;
            [_managedObjectContext save:&saveError];
            //more error handling here
        }
    }
}

- (void)beginImporting {
    [importProgressPanel makeKeyAndOrderFront:self];
}

- (void)endImporting {
    [importProgressPanel orderOut:self];
}

- (IBAction)importMetadata:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.prompt = @"Import";

    panel.allowedFileTypes = @[ @"iFiction" ];

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSFileHandlingPanelOKButton) {
                          NSURL *url = panel.URL;

                          [self beginImporting];
                          [self importMetadataFromFile:url.path];
                          [self updateTableViews];
                          [self endImporting];
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

- (IBAction)addGamesToLibrary:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseDirectories:YES];
    panel.prompt = @"Add";

    panel.allowedFileTypes = gGameFileTypes;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSFileHandlingPanelOKButton) {
                          NSArray *urls = panel.URLs;

                          [self beginImporting];
                          [self addFiles:urls];
                          [self endImporting];
                      }
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
        NSString *ifid = [gameTableModel objectAtIndex:i];
        Metadata *meta = [self fetchMetadataForIFID:ifid];

        GlkController *gctl = [_gameSessions objectForKey:ifid];

        if (gctl) {
            [gctl reset:sender];
        } else {
            gctl = [[GlkController alloc] init];
            [gctl deleteAutosaveFilesForGame:meta.game];
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
            if ([game.found isEqual:@(YES)]) {
                game.found = @(NO);
                [self updateTableViews];
            }
            NSRunAlertPanel(
                @"Cannot find the file.",
                @"The file could not be found at its original location. Maybe "
                @"it has been moved since it was added to the library.",
                @"Okay", NULL, NULL);
            return;
        } else if ([game.found isEqual:@(NO)]) {
            game.found = @(YES);
            [self updateTableViews];
        }
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
            [self.managedObjectContext deleteObject:game];
        }

        gameTableDirty = YES;
        [self updateTableViews];
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
        Game *game;
        NSInteger i;
        NSString *ifid;
        NSArray *selectedGames = [gameTableModel objectsAtIndexes:rows];


        for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
        {
            game = [gameTableModel objectAtIndex:i];
            ifid = game.metadata.ifid;
            [self downloadMetadataForIFID: ifid];
			[self extractMetadataFromFile:game];
        }
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
				[self addImage: img toMetadata:game.metadata];
				[self updateSideView];
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

    if (action == @selector(delete:))
        return count > 0;

    if (action == @selector(deleteGame:))
        return count > 0;

    if (action == @selector(revealGameInFinder:))
        return count > 0;

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
        [self beginImporting];
        [self addFiles:urls];
        [self endImporting];
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

- (void) addMetadata:(NSMutableDictionary*)dict forIFIDs:(NSArray*)list {
    [self addMetadata:dict forIFIDs:list inContext:_managedObjectContext];
}

- (void) addMetadata:(NSMutableDictionary*)dict forIFIDs:(NSArray*)list inContext:(NSManagedObjectContext *)context {
    NSInteger count;
    NSInteger i;
    NSDateFormatter *dateFormatter;
    Metadata *entry;
    NSString *keyVal;
    NSString *sub;

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

    if (cursrc == kIfdb)
        list = @[[list objectAtIndex:0]];

    count = list.count;

    for (i = 0; i < count; i++)
    {
        NSString *ifid =[list objectAtIndex:i];
        entry = [self fetchMetadataForIFID:ifid];
        if (!entry)
        {
            entry = (Metadata *) [NSEntityDescription
                                  insertNewObjectForEntityForName:@"Metadata"
                                  inManagedObjectContext:context];
            entry.ifid = ifid;
            NSLog(@"addMetaData:forIFIDs: Created new Metadata instance with ifid %@", ifid);
            //dict[kSource] = @((int)cursrc);
        }

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

                entry.firstpublished = [dateFormatter dateFromString:keyVal];
                dateFormatter.dateFormat = @"yyyy";
                entry.yearAsString = [dateFormatter stringFromDate:entry.firstpublished];
            }
            else if ([(NSString *)key isEqualToString:@"language"])
            {

                sub = [keyVal substringWithRange:NSMakeRange(0, 2)];
                sub = sub.lowercaseString;
                if ([language objectForKey:sub])
                    entry.language = [language objectForKey:sub];
                else
                    entry.language = keyVal;
            }
            else if ([(NSString *)key isEqualToString:@"format"])
            {
                if (entry.format == nil)
                    entry.format = [dict objectForKey:@"format"];
            }
            else if ([(NSString *)key isEqualToString:@"forgiveness"])
            {
                if ([forgiveness objectForKey:keyVal])
                    entry.forgiveness = [forgiveness objectForKey:keyVal];
                else
                    entry.forgiveness = @(FORGIVENESS_NONE);

            }
            else
            {
                [entry setValue:keyVal forKey:key];
                if ([(NSString *)key isEqualToString:@"coverArtURL"])
                {
                    [self downloadImage:entry.coverArtURL for:entry];
                }
                // NSLog(@"Set key %@ to value %@ in metadata %@", key, [dict valueForKey:key], entry)
            }

        }
        gameTableDirty = YES;

    }
}

- (void) setMetadataValue: (NSString*)val forKey: (NSString*)key forIFID: (NSString*)ifid
{
    Metadata *dict = [self fetchMetadataForIFID:ifid];
    if (dict)
    {
        NSLog(@"libctl: user set value %@ = '%@' for %@", key, val, ifid);
        dict.userEdited = @(YES);
        [dict setValue:val forKey:key];
        gameTableDirty = YES;
    }
}

- (Metadata *)fetchMetadataForIFID:(NSString *)ifid {
    return [self fetchMetadataForIFID:ifid inContext:_managedObjectContext];
}

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
        NSLog(@"fetchMetadataForIFID: Found no Metadata entity with ifid %@",ifid);
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

- (NSArray *) fetchObjects:(NSString *)entityName {
    return [self fetchObjects:entityName inContext:_managedObjectContext];
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

    LibController * __unsafe_unretained weakSelf = self;

    NSManagedObjectContext *private = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
    [private setParentContext:_managedObjectContext];

    [private performBlock:^{
        cursrc = kInternal;
        NSMutableDictionary *metadata = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Metadata.plist"]);
        NSString *ifid;

        NSEnumerator *enumerator = [metadata keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [weakSelf addMetadata:[metadata objectForKey:ifid] forIFIDs:@[ifid] inContext:private];
        }

        // Add games from plist to Core Data store if we have just created a new one
        if (gameTableModel.count == 0)
        {
            NSMutableDictionary *games = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Games.plist"]);
            NSString *ifid;

            NSEnumerator *enumerator = [games keyEnumerator];
            NSInteger counter = 0;

            while ((ifid = [enumerator nextObject]))
            {
                Metadata *meta = [weakSelf fetchMetadataForIFID:ifid inContext:private];
                if (meta)
                {
                    Game *game = (Game *) [NSEntityDescription
                                           insertNewObjectForEntityForName:@"Game"
                                           inManagedObjectContext:private];

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


                    NSLog(@"Creating new instance of Game %@ and giving it metadata of ifid %@", meta.title, ifid);
                    game.metadata = meta;
                    game.added = [NSDate date];
                    [game bookmarkForPath:[games valueForKey:ifid]];

                    //We save data and update the UI every 10 games
                    if (++counter % 10 == 0) {
                        NSError *error = nil;
                        if (![private save:&error]) {
                            NSLog(@"Error saving context: %@\n%@", [error localizedDescription], [error userInfo]);
                            abort();
                        }
                        [_managedObjectContext performBlockAndWait:^{
                            [_coreDataManager saveChanges];
                            gameTableDirty = YES;
                            [weakSelf updateTableViews];
                        }];
                    }
                    
                }
                else
                    NSLog(@"Could not find game file corresponding to %@. Skipping.", meta.title);
            }
        }
        
        NSError *error = nil;
        if (![private save:&error]) {
            NSLog(@"Error saving context: %@\n%@", [error localizedDescription], [error userInfo]);
            abort();
        }
        [_managedObjectContext performBlockAndWait:^{
            [_coreDataManager saveChanges];
            gameTableDirty = YES;
            [weakSelf updateTableViews];
        }];
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
        NSLog(@"libctl: import metadata for %@ by %@",
              [metabuf objectForKey:@"title"],
              [metabuf objectForKey:@"author"]);
        [self addMetadata:metabuf forIFIDs:ifidbuf];
        ifidbuf = nil;
        if (currentIfid == nil) /* We are not importing from ifdb */
            metabuf = nil;
    }
    
    /* parse extra ifdb tags... */
    else if (!strcmp(tag->tag, "ifdb"))
    {
        NSLog(@"libctl: import extra ifdb metadata for %@", [metabuf objectForKey:@"title"]);
        [self addMetadata: metabuf forIFIDs: @[currentIfid]];
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

    NSMutableData *data;

    data = [NSMutableData dataWithContentsOfFile:filename];
    if (!data)
        return NO;
    [data appendBytes:"\0" length:1];

    cursrc = kExternal;
    [self importMetadataFromXML:data.mutableBytes];
    cursrc = 0;

    return YES;
}

- (BOOL) downloadMetadataForIFID: (NSString*)ifid
{
    NSLog(@"libctl: downloadMetadataForIFID %@", ifid);

    if (dataTask != nil) {
        [dataTask cancel];
    }

    NSURL *url = [NSURL URLWithString:[@"https://ifdb.tads.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid]];

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];

    [NSURLConnection sendAsynchronousRequest:urlRequest queue:queue completionHandler:^(NSURLResponse *response, NSData *data, NSError *error)
     {
         if (error) {
             if (!data) {
                 [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                     NSLog(@"Error connecting: %@", [error localizedDescription]);
                 }];
                 return;
             }
         }
         else
         {
             NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
             if (httpResponse.statusCode == 200 && data) {
                 NSMutableData *mutableData = [data mutableCopy];
                 [mutableData appendBytes: "\0" length: 1];

                 [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                     cursrc = kIfdb;
                     currentIfid = ifid;
                     NSLog(@"Set currentIfid to %@", currentIfid);
                     [self importMetadataFromXML: mutableData.mutableBytes];
                     currentIfid = nil;
                     cursrc = 0;
                     [self updateTableViews];
                 }];
                 
             }
         }
     }];
    
    return YES;

}

- (BOOL) downloadImage:(NSString*)imgurl for:(Metadata *)metadata
{
    NSLog(@"libctl: download image from url %@", imgurl);

    if (dataTask != nil) {
        [dataTask cancel];
    }

    NSURL *url = [NSURL URLWithString:imgurl];

    NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];
    [NSURLConnection sendAsynchronousRequest:urlRequest queue:queue completionHandler:^(NSURLResponse *response, NSData *data, NSError *error)
     {

         if (error) {
             if (!data) {
                 [[NSOperationQueue mainQueue] addOperationWithBlock: ^{
                     NSLog(@"Error connecting: %@", [error localizedDescription]);
                 }];
                 return;
             }


         }
         else
         {

             NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
             if (httpResponse.statusCode == 200 && data) {
                 NSMutableData *mutableData = [data mutableCopy];
                 [[NSOperationQueue mainQueue] addOperationWithBlock: ^{

                     [self addImage: mutableData toMetadata:metadata];
                     [self updateSideView];
                 }];
                 
             }
         }
     }];

    return YES;

}

- (void) addImage:(NSData *)rawImageData toMetadata:(Metadata *)metadata
{
    Image *img;
    img = (Image *) [NSEntityDescription
                     insertNewObjectForEntityForName:@"Image"
                     inManagedObjectContext:self.managedObjectContext];
    img.data = rawImageData;
    metadata.cover = img;
    NSLog(@"added a new image to game %@", metadata.title);
    
}

static void write_xml_text(FILE *fp, NSDictionary *info, NSString *key) {
    NSString *val;
    const char *tagname;
    const char *s;

    val = [info objectForKey:key];
    if (!val)
        return;

    tagname = key.UTF8String;
    s = val.UTF8String;

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
 * Export user-edited metadata (ie, with kSource == kUser)
 */
- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what {
    NSEnumerator *enumerator;
    Metadata *ifid;
    //NSString *key, *val;
    NSDictionary *info;
    int src;

    NSLog(@"libctl: exportMetadataToFile %@", filename);

    FILE *fp;

    fp = fopen(filename.UTF8String, "w");
    if (!fp)
        return NO;

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<ifindex version=\"1.0\">\n\n");

    //FIXME
    NSArray *metadata = [self fetchObjects:@"Metadata"];
    //    NSArray *games = [self fetchObjects:@"Game"];

    NSEntityDescription *entity;

    //    if (what == X_EDITED || what == X_DATABASE)
    //    {
    enumerator = [metadata objectEnumerator];
    //    }
    //    else
    //        enumerator = [games objectEnumerator];
    while ((ifid = [enumerator nextObject]))
    {
        entity = ifid.entity;
        info = entity.attributesByName;
        src = [[info objectForKey:kSource] intValue];
        if ((what == X_EDITED && src >= kUser) || what != X_EDITED)
        {
            fprintf(fp, "<story>\n");

            fprintf(fp, "<identification>\n");
            fprintf(fp, "<ifid>%s</ifid>\n", ifid.title.UTF8String);
            write_xml_text(fp, info, @"format");
            write_xml_text(fp, info, @"bafn");
            fprintf(fp, "</identification>\n");

            fprintf(fp, "<bibliographic>\n");

            write_xml_text(fp, info, @"title");
            write_xml_text(fp, info, @"author");
            write_xml_text(fp, info, @"language");
            write_xml_text(fp, info, @"headline");
            write_xml_text(fp, info, @"firstpublished");
            write_xml_text(fp, info, @"genre");
            write_xml_text(fp, info, @"group");
            write_xml_text(fp, info, @"series");
            write_xml_text(fp, info, @"seriesnumber");
            write_xml_text(fp, info, @"forgiveness");
            write_xml_text(fp, info, @"description");

            fprintf(fp, "</bibliographic>\n");
            fprintf(fp, "</story>\n\n");
        }
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
    Metadata *meta = [self fetchMetadataForIFID:ifid];
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
        if ([game.found isEqual:@(YES)]) {
            game.found = @(NO);
            [self updateTableViews];
        }
        NSRunAlertPanel(
                        @"Cannot find the file.",
                        @"The file could not be found at its original location. Maybe "
                        @"it has been moved since it was added to the library.",
                        @"Okay", NULL, NULL);
        return nil;
    } else if ([game.found isEqual:@(NO)]) {
        game.found = @(YES);
        [self updateTableViews];
    }

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
    [self addURLtoRecents: game.urlForBookmark];

    return gctl.window;
}

- (void)importAndPlayGame:(NSString *)path {

    Game *game;
    
    game = [self importGame: path reportFailure: YES];
    if (game)
    {
        [self updateTableViews];
        [self deselectGames];
        [self selectGame: game];
        [self playGame: game];
    }
}

- (Game *)importGame:(NSString*)path reportFailure:(BOOL)report {
    char buf[TREATY_MINIMUM_EXTENT];
    Metadata *metadata;
    NSString *ifid;
    char *format;
    char *s;
    int mdlen;
    int rv;

    if ([path.pathExtension.lowercaseString isEqualToString: @"ifiction"])
    {
        [self importMetadataFromFile: path];
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

    NSLog(@"libctl: import game %@ (%s)", path, format);

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
            [self importMetadataFromXML: mdbuf];
            cursrc = 0;
        }

        free(mdbuf);
    }

    metadata = [self fetchMetadataForIFID:ifid];
    if (!metadata)
    {
        metadata = (Metadata *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Metadata"
                                 inManagedObjectContext:self.managedObjectContext];
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
			return metadata.game;
		}
	}

    if (!metadata.format)
        metadata.format = @(format);
    if (!metadata.ifid)
        metadata.ifid = ifid;
    if (!metadata.title)
    {
        metadata.title = path.lastPathComponent;
        [self downloadMetadataForIFID:ifid];
    }

    if (!metadata.cover)
    {
        NSString *dirpath = (@"~/Library/Application Support/Spatterlight/Cover Art").stringByStandardizingPath;
        NSString *imgpath = [[dirpath stringByAppendingPathComponent: ifid] stringByAppendingPathExtension: @"tiff"];
        NSData *img = [[NSData alloc] initWithContentsOfFile: imgpath];
        if (img)
        {
            [self addImage:img toMetadata:metadata];
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
                    [self addImage:[[NSData alloc] initWithBytesNoCopy: imgbuf length: imglen freeWhenDone: YES] toMetadata:metadata];

                }
            }
        }
    }

    babel_release();

    Game *game = (Game *) [NSEntityDescription
                           insertNewObjectForEntityForName:@"Game"
                           inManagedObjectContext:self.managedObjectContext];

    [game bookmarkForPath:path];

    game.added = [NSDate date];
    game.metadata = metadata;
    gameTableDirty = YES;

    NSLog(@"libctl: importGame: imported %@ with metadata %@", game, metadata);

    [self addURLtoRecents: game.urlForBookmark];

    return game;
}

- (void) addFile: (NSURL*)url select: (NSMutableArray*)select
{
    Game *game = [self importGame: url.path reportFailure: NO];
    if (game)
        [select addObject: game];
	else
	{
		NSLog(@"libctl: addFile: Error: File not added!");
		//[self importGame: url.path reportFailure: YES];
	}
}

- (void) addFiles: (NSArray*)urls select: (NSMutableArray*)select
{
    NSFileManager *filemgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSInteger count;
    NSInteger i;

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
            [self addFiles: contents select: select];
        }
        else
        {
            [self addFile:[urls objectAtIndex:i] select: select];
        }
    }
}

- (void) addFiles: (NSArray*)urls
{
    NSInteger count;
    NSInteger i;

    NSLog(@"libctl: adding %lu files", (unsigned long)urls.count);

    NSMutableArray *select = [NSMutableArray arrayWithCapacity: urls.count];

    [self deselectGames];
    [self addFiles: urls select: select];
    [self updateTableViews];

    count = select.count;
    for (i = 0; i < count; i++)
        [self selectGame: [select objectAtIndex:i]];
}

- (void) addFile: (NSURL*)url
{
    [self addFiles: @[url]];
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

- (void)deselectGames {
    [_gameTableView deselectAll:self];
}

- (void) selectGame: (Game*)game
{
    NSInteger i, count;
    count = gameTableModel.count;
    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

    for (i = 0; i < count; i++)
        if ([gameTableModel objectAtIndex:i] == game)
            [indexSet addIndex:i];

    [_gameTableView selectRowIndexes:indexSet byExtendingSelection:YES];
    [_gameTableView scrollRowToVisible:indexSet.firstIndex];
    
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

    Game *selection;
    NSFetchRequest *fetchRequest;
    NSEntityDescription *entity;

    NSInteger searchcount;
    NSInteger selrow;
    NSInteger count;
    NSInteger i;

    if (!gameTableDirty)
        return;

    selection = nil;
    selrow = _gameTableView.selectedRow;
    if (gameTableModel.count && selrow >= 0)
        selection = [gameTableModel objectAtIndex:selrow];


    fetchRequest = [[NSFetchRequest alloc] init];
    entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.entity = entity;

    if (searchStrings)
    {
        searchcount = searchStrings.count;

        NSPredicate *predicate;
        NSMutableArray *predicateArr = [NSMutableArray arrayWithCapacity:searchcount];

        for (NSString *word in searchStrings) {
            predicate = [NSPredicate predicateWithFormat: @"(metadata.format contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.language contains [c] %@) OR (metadata.yearAsString contains %@)", word, word, word, word, word, word, word, word, word, word, word];
            [predicateArr addObject:predicate];
        }


        NSCompoundPredicate *comp = (NSCompoundPredicate *)[NSCompoundPredicate orPredicateWithSubpredicates: predicateArr];

        fetchRequest.predicate = comp;
    }

    NSError *error = nil;
    gameTableModel = [[self.managedObjectContext executeFetchRequest:fetchRequest error:&error] mutableCopy];
    if (gameTableModel == nil)
        NSLog(@"Problem! %@",error);

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:sortAscending comparator:^(Game *aid, Game *bid) {

        Metadata *a = aid.metadata;
        Metadata *b = bid.metadata;
        NSInteger cmp;
        if ([gameSortColumn isEqual:@"firstpublished"])
        {
            cmp = compareDates(a.firstpublished, b.firstpublished, sortAscending);
            if (cmp) return cmp;
        }
        else if ([gameSortColumn isEqual:@"added"] || [gameSortColumn isEqual:@"lastPlayed"] || [gameSortColumn isEqual:@"lastModified"])
        {
            cmp = compareDates([a.game valueForKey:gameSortColumn], [b.game valueForKey:gameSortColumn], sortAscending);
            if (cmp) return cmp;
        }
        else if ([gameSortColumn isEqual:@"found"]) {
            NSString *string1 = [a.game.found isEqual:@(YES)]?@"A":@"B";
            NSString *string2 = [b.game.found isEqual:@(YES)]?@"A":@"B";
            cmp = compareStrings(string1, string2, sortAscending);
            if (cmp) return cmp;
        }
        else if ([gameSortColumn isEqual:@"forgiveness"]) {
            NSString *string1 = [NSString stringWithFormat:@"%@", a.forgiveness];
            NSString *string2 = [NSString stringWithFormat:@"%@", b.forgiveness];
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
        cmp = compareDates(a.firstpublished, b.firstpublished, sortAscending);
        if (cmp) return cmp;
        return compareGames(a, b, @"format", sortAscending);
    }];

    [gameTableModel sortUsingDescriptors:@[sort]];
    [_gameTableView reloadData];

    [_gameTableView deselectAll: self];
    count = gameTableModel.count;

    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

    for (i = 0; i < count; i++)
        if ([gameTableModel objectAtIndex:i] == selection)
            [indexSet addIndex:i];

    [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
    [_gameTableView scrollRowToVisible:indexSet.firstIndex];
    
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
            return [game.found isEqual: @(YES)]?@"":@"!";
        if ([column.identifier isEqual: @"added"] || [column.identifier  isEqual: @"lastPlayed"] ||
            [column.identifier  isEqual: @"lastModified"]) {
            return [[game valueForKey: column.identifier] formattedRelativeString];
        } else
            return [meta valueForKey: column.identifier];
    }
    return nil;
}


- (void)tableView:(NSTableView *)tableView
   setObjectValue:(id)value
   forTableColumn:(NSTableColumn *)tableColumn
              row:(int)row
{
    if (tableView == _gameTableView)
    {
        Game *game = [gameTableModel objectAtIndex:row];
        Metadata *meta = game.metadata;
        NSString *key = tableColumn.identifier;
        NSString *oldval = [meta valueForKey:key];
		if ([value isKindOfClass:[NSNumber class]] && ![key isEqualToString:@"forgiveness"])
			value = [[NSString alloc] initWithFormat:@"%@", value];
        if (oldval == nil && (value == nil || ((NSString*)value).length == 0))
            return;
        if ([value isEqualTo: oldval])
            return;
        [meta setValue: value forKey: key];
        NSLog(@"Set value of %@ to %@", key, value);
    }
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    NSIndexSet *rows = tableView.selectedRowIndexes;
    if (tableView == _gameTableView) {
        infoButton.enabled = rows.count > 0;
        playButton.enabled = rows.count == 1;
        [self invalidateRestorableState];
        if (rows.count == 1)
            [self updateSideView];
    }
}

- (void) updateSideView
{

	NSIndexSet *rows = _gameTableView.selectedRowIndexes;

	if (!rows.count || [_splitView isSubviewCollapsed:_leftView])
	{
		NSLog(@"No game selected in table view, returning without updating side view");
		return;
	} else NSLog(@"%lu rows selected.", (unsigned long)rows.count);

	Game *game = [gameTableModel objectAtIndex:rows.firstIndex];

	NSLog(@"\nUpdating info pane for %@", game.metadata.title);
    NSLog(@"Side view width: %f", NSWidth(_leftView.frame));
//    [((NSView *)_leftScrollView.documentView).subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];

    [_leftScrollView.documentView performSelector:@selector(removeFromSuperview)];


	SideInfoView *infoView = [[SideInfoView alloc] initWithFrame:_leftScrollView.frame];
	_leftScrollView.documentView = infoView;

	[infoView updateSideViewForGame:game];

	if (game.metadata.ifid)
		_sideIfid.stringValue = game.metadata.ifid;
	else
		_sideIfid.stringValue = @"";

	_sideIfid.delegate = infoView;
	gameTableDirty = YES;
    
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

    NSLog(@"splitView:constrainMaxCoordinate:%f ofSubviewAt:%ld (returning %f)", proposedMaximumPosition, dividerIndex, result);
    return result;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    NSLog(@"splitView:constrainMinCoordinate:%f ofSubviewAt:%ld (returning 200)", proposedMinimumPosition, dividerIndex);

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
    [self updateSideView];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
    // TODO: This should call a faster update method rather than rebuilding the view from scratch every time, but everything I've tried makes word wrap wonky
    [self updateSideView];
}

#pragma mark -
#pragma mark Windows restoration

- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state {
    [state encodeObject:_searchField.stringValue forKey:@"searchText"];

    [state encodeFloat:NSWidth(_leftView.frame) forKey:@"sideviewWidth"];
    NSLog(@"Encoded left view width as %f", NSWidth(_leftView.frame))
    [state encodeBool:[_splitView isSubviewCollapsed:_leftView] forKey:@"sideviewHidden"];
    NSLog(@"Encoded left view collapsed as %@", [_splitView isSubviewCollapsed:_leftView]?@"YES":@"NO");


    NSIndexSet *selrow = _gameTableView.selectedRowIndexes;
    if (selrow) {
        NSArray *selectedGames = [gameTableModel objectsAtIndexes:selrow];
        NSMutableArray *selectedGameIfids = [NSMutableArray arrayWithCapacity:selectedGames.count];
        for (Game *game in selectedGames)
            [selectedGameIfids addObject:game.metadata.ifid];
        [state encodeObject:selectedGameIfids forKey:@"selectedGames"];
        NSLog(@"Encoded %ld selected games", (unsigned long)selectedGameIfids.count);
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
    if (selectedIfids.count) {
        NSMutableArray *selectedGames = [NSMutableArray arrayWithCapacity:selectedIfids.count];
        [self updateTableViews];
        for (NSString *ifid in selectedIfids) {
            [selectedGames addObject:[self fetchMetadataForIFID:ifid].game];
            NSLog(@"Restoring selection of game with ifid %@", ifid);
        }
        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];
        
        for (Game *game in selectedGames) {
            if ([gameTableModel containsObject:game])
                [indexSet addIndex:[gameTableModel indexOfObject:game]];
        }
        
        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
    }
    
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
