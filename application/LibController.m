/*
 *
 */

#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

enum { X_EDITED, X_LIBRARY, X_DATABASE }; // export selections

#define kSource @".source"
#define kDefault 1
#define kInternal 2
#define kExternal 3
#define kUser 4
#define kIfdb 5

#include <ctype.h>

/* the treaty of babel headers */
#include "treaty.h"
#include "ifiction.h"
#include "babel_handler.h"

@implementation LibHelperWindow
- (NSDragOperation) draggingEntered:sender { return [(LibController *)self.delegate draggingEntered:sender]; }
- (void) draggingExited:sender { [(LibController *)self.delegate draggingEntered:sender]; }
- (BOOL) prepareForDragOperation:sender { return [(LibController *)self.delegate prepareForDragOperation:sender]; }
- (BOOL) performDragOperation:sender { return [(LibController *)self.delegate performDragOperation:sender]; }
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

- (void) textDidEndEditing: (NSNotification *) notification
{
    NSDictionary *userInfo;
    NSDictionary *newUserInfo;
    NSNumber *textMovement;
    int movementCode;
    
    userInfo = notification.userInfo;
    textMovement = userInfo[@"NSTextMovement"];
    movementCode = textMovement.intValue;
    
    // see if this a 'pressed-return' instance
    if (movementCode == NSReturnTextMovement)
    {
        // hijack the notification and pass a different textMovement value
        textMovement = @(NSIllegalTextMovement);
        newUserInfo = @{@"NSTextMovement": textMovement};
        notification = [NSNotification notificationWithName: notification.name
                                                     object: notification.object
                                                   userInfo: newUserInfo];
    }
    
    [super textDidEndEditing: notification];
    [(LibController *)self.delegate updateSideView];
}

@end

@implementation LibController

/*
 * Window controller, menus and file dialogs.
 *
 */

//- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
//{
//
//    if (editDelayer == NO)
//    {
//        if ([gameTableView isRowSelected:gameTableView.clickedRow])
//        {
//            editDelayer = YES;
//            return YES;
//        }
//    }
//    else
//    {
//        editDelayer = NO;
//        return NO;
//    }
//}

static NSMutableDictionary *load_mutable_plist(NSString *path)
{
    NSMutableDictionary *dict;
    NSString *error;
    
    dict = [NSPropertyListSerialization
             propertyListFromData: [NSData dataWithContentsOfFile: path]
             mutabilityOption: NSPropertyListMutableContainersAndLeaves
             format: NULL
             errorDescription: &error];
    
    if (!dict)
    {
        NSLog(@"libctl: cannot load plist: %@", error);
        dict = [[NSMutableDictionary alloc] init];
    }
    
    return dict;
}

- (void) loadLibrary
{
//    NSLog(@"libctl: loadLibrary");

    /* in case we are called more than once... */
    
    homepath = [NSURL fileURLWithPath:(@"~/Library/Application Support/Spatterlight").stringByExpandingTildeInPath  isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:homepath withIntermediateDirectories:YES attributes:NULL error:NULL];

    if ([[self fetchObjects:@"Metadata"] count] == 0)
    {
        NSMutableDictionary *metadata = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Metadata.plist"]);
        NSString *ifid;

        NSEnumerator *enumerator = [metadata keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            [self addMetadata:metadata[ifid] forIFIDs:@[ifid]];
        }
    }

    gameTableModel = [[self fetchObjects:@"Game"] mutableCopy];

    if ([gameTableModel count] == 0)
    {
        NSMutableDictionary *games = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Games.plist"]);
        NSString *ifid;

        NSEnumerator *enumerator = [games keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            Metadata *meta = [self fetchMetadataForIFID:ifid];
            if (meta)
            {
                Game *game = (Game *) [NSEntityDescription
                                         insertNewObjectForEntityForName:@"Game"
                                         inManagedObjectContext:self.managedObjectContext];

                game.setting = (Settings *) [NSEntityDescription
                                       insertNewObjectForEntityForName:@"Settings"
                                       inManagedObjectContext:self.managedObjectContext];

                game.setting.bufferFont = (Font *) [NSEntityDescription
                                             insertNewObjectForEntityForName:@"Font"
                                             inManagedObjectContext:self.managedObjectContext];
                game.setting.gridFont = (Font *) [NSEntityDescription
                                                    insertNewObjectForEntityForName:@"Font"
                                                    inManagedObjectContext:self.managedObjectContext];
                game.setting.bufInput = (Font *) [NSEntityDescription
                                                  insertNewObjectForEntityForName:@"Font"
                                                  inManagedObjectContext:self.managedObjectContext];


                NSLog(@"Creating new instance of Game %@ and giving it metadata of ifid %@", meta.title, ifid);
                game.metadata = meta;
                game.added = [NSDate date];
                [game bookmarkForPath:[games valueForKey:ifid]];
            }
            else
            NSLog(@"Could not find game file corresponding to %@. Skipping.", meta.title);
        }
    }

    NSError *error = nil;
    if (![self.managedObjectContext save:&error]) {
        NSLog(@"There's a problem: %@", error);
    }
}

- (NSArray *) fetchObjects:(NSString *)entityName
{
    self.persistentContainer = ((AppDelegate*)[[NSApplication sharedApplication] delegate]).persistentContainer;
    self.managedObjectContext = self.persistentContainer.viewContext;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:self.managedObjectContext];
    [fetchRequest setEntity:entity];

    NSError *error = nil;
    NSArray *fetchedObjects = [[self managedObjectContext] executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    return fetchedObjects;
}

- (IBAction) saveLibrary: (id)sender
{
    NSLog(@"libctl: saveLibrary");
    
    // Save the Managed Object Context
    NSError *error = nil;
    if (![self.managedObjectContext save:&error]) {
        NSLog(@"There's a problem: %@", error);
    }
}

- (void) windowDidLoad
{
    NSString *key;
    NSSortDescriptor *sortDescriptor;

//    NSLog(@"libctl: windowDidLoad");

    defaultSession = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
    
    self.persistentContainer = ((AppDelegate*)[[NSApplication sharedApplication] delegate]).persistentContainer;
    self.managedObjectContext = self.persistentContainer.viewContext;

    self.windowFrameAutosaveName = @"LibraryWindow";
    _splitView.autosaveName = @"SplitView";
    gameTableView.autosaveName = @"GameTable";
    [gameTableView setAutosaveTableColumns: YES];

    [gameTableView setAction:@selector(doClick:)];
    [gameTableView setDoubleAction:@selector(doDoubleClick:)];
    [gameTableView setTarget:self];

    [self.window setExcludedFromWindowsMenu: YES];
    [self.window registerForDraggedTypes: @[NSFilenamesPboardType]];
    
    [infoButton setEnabled: NO];
    [playButton setEnabled: NO];
        
    gameTableModel = [[NSMutableArray alloc] init];

    for (NSTableColumn *tableColumn in gameTableView.tableColumns ) {

        key = tableColumn.identifier;
        sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:key ascending:YES];
        [tableColumn setSortDescriptorPrototype:sortDescriptor];

        for (NSMenuItem *menuitem in _headerMenu.itemArray)
        {
            if ([menuitem.identifier isEqualToString:key])
            {
                menuitem.state = !(tableColumn.hidden);
                break;
            }
        }
    }

    gameTableDirty = YES;
    [self updateTableViews];
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    NSLog(@"libctl: windowWillReturnUndoManager!")
    return [self.managedObjectContext undoManager];
}

- (IBAction) deleteLibrary: (id)sender
{
    NSInteger choice =
    NSRunAlertPanel(@"Do you really want to delete the library?",
                    @"All the information about your games will be lost. The original game files will not be harmed.",
                    @"Delete", NULL, @"Cancel");
    if (choice != NSAlertOtherReturn)
    {
        NSFetchRequest *request = [[NSFetchRequest alloc] initWithEntityName:@"Metadata"];
        NSBatchDeleteRequest *delete = [[NSBatchDeleteRequest alloc] initWithFetchRequest:request];

        NSError *deleteError = nil;
        [self.persistentContainer.persistentStoreCoordinator
 executeRequest:delete withContext:self.managedObjectContext error:&deleteError];
        request = [[NSFetchRequest alloc] initWithEntityName:@"Game"];
        delete = [[NSBatchDeleteRequest alloc] initWithFetchRequest:request];
        [self.persistentContainer.persistentStoreCoordinator
         executeRequest:delete withContext:self.managedObjectContext error:&deleteError];

        gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (IBAction) purgeLibrary: (id)sender
{
    NSInteger choice =
    NSRunAlertPanel(@"Do you really want to purge the library?",
                    @"Purging will delete the information about games that are not in the library at the moment.",
                    @"Purge", NULL, @"Cancel");
    if (choice != NSAlertOtherReturn)
    {

        if (choice != NSAlertOtherReturn)
        {
            NSFetchRequest *request = [[NSFetchRequest alloc] initWithEntityName:@"Metadata"];

            NSPredicate *predicate = [NSPredicate predicateWithFormat:@"game != NIL"];
            [request setPredicate:predicate];

            NSBatchDeleteRequest *delete = [[NSBatchDeleteRequest alloc] initWithFetchRequest:request];

            NSError *deleteError = nil;
            [self.persistentContainer.persistentStoreCoordinator
             executeRequest:delete withContext:self.managedObjectContext error:&deleteError];

        }
    }
}

- (void) beginImporting
{
    [importProgressPanel makeKeyAndOrderFront: self];
}

- (void) endImporting
{
    [importProgressPanel orderOut: self];
}


- (IBAction) importMetadata: (id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.prompt = @"Import";
    
    panel.allowedFileTypes = @[@"iFiction"];
    
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        if (result == NSFileHandlingPanelOKButton) {
            NSURL* url = panel.URL;
            
            [self beginImporting];
            [self importMetadataFromFile: url.path];
            [self updateTableViews];
            [self endImporting];
        }
        
    }];
}

- (IBAction) exportMetadata: (id)sender
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.accessoryView = exportTypeView;
    panel.allowedFileTypes=@[@"iFiction"];
    panel.prompt = @"Export";
    panel.nameFieldStringValue = @"Interactive Fiction Metadata.iFiction";
    
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        if (result == NSFileHandlingPanelOKButton)
        {
            NSURL* url = panel.URL;
            
            [self exportMetadataToFile: url.path what: exportTypeControl.indexOfSelectedItem];
        }
    }];
}

- (IBAction) addGamesToLibrary: (id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection: YES];
    [panel setCanChooseDirectories: YES];
    panel.prompt = @"Add";
    
    panel.allowedFileTypes=gGameFileTypes;
    
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        if (result == NSFileHandlingPanelOKButton)
        {
            NSArray* urls = panel.URLs;
            
            [self beginImporting];
            [self addFiles: urls];
            [self endImporting];
        }
    }];
}


- (IBAction) play: (id)sender
{
    NSInteger rowidx = gameTableView.selectedRow;
    if (rowidx >= 0)
    {
        [self playGame: gameTableModel[rowidx]];
        //[(LibHelperTableView *)gameTableView enableClickToRenameAfterDelay];
    }
}

- (IBAction) showGameInfo: (id)sender
{
    NSIndexSet *rows = gameTableView.selectedRowIndexes;

    if ((gameTableView.clickedRow >= 0) && ![gameTableView isRowSelected:gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:gameTableView.clickedRow];

    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
    {
        Game *game = gameTableModel[i];
        [game showInfoWindow];
    }
}



- (IBAction) revealGameInFinder: (id)sender
{
    NSIndexSet *rows = gameTableView.selectedRowIndexes;

    if ((gameTableView.clickedRow >= 0 ) && ![gameTableView isRowSelected:gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:gameTableView.clickedRow];

    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
    {
        Game *game = gameTableModel[i];
        NSURL *url = [game urlForBookmark];
        [[NSWorkspace sharedWorkspace] selectFile: url.path inFileViewerRootedAtPath: @""];
    }
}

- (IBAction) deleteGame: (id)sender
{
    NSIndexSet *rows = gameTableView.selectedRowIndexes;

    if ((gameTableView.clickedRow >= 0) && ![gameTableView isRowSelected:gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:gameTableView.clickedRow];

    if (rows.count > 0)
    {
        Game *game;
        NSInteger i;

        for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
        {
            game = gameTableModel[i];
            //NSLog(@"libctl: delete game %@", game.metadata.title);
            [self.managedObjectContext deleteObject:game];
        }
        
        gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (IBAction)openIfdb:(id)sender {

    NSIndexSet *rows = gameTableView.selectedRowIndexes;

    if ((gameTableView.clickedRow >= 0) && ![gameTableView isRowSelected:gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:gameTableView.clickedRow];

    if (rows.count > 0)
    {
        Game *game;
        NSInteger i;
        NSString *urlString;

        for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
        {
            game = gameTableModel[i];
            urlString = [@"http://ifdb.tads.org/viewgame?id=" stringByAppendingString:game.metadata.tuid];
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: urlString]];
        }
    }
}

- (IBAction) delete: (id)sender
{
    [self deleteGame: sender];
}

- (IBAction) download:(id)sender
{
    NSIndexSet *rows = gameTableView.selectedRowIndexes;

    if ((gameTableView.clickedRow >= 0) && ![gameTableView isRowSelected:gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:gameTableView.clickedRow];

    if (rows.count > 0)
    {
        Game *game;
        NSInteger i;
        NSString *ifid;

        for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
        {
            game = gameTableModel[i];
            ifid = game.metadata.ifid;
            [self downloadMetadataForIFID: ifid];
        }
    }
}

- (BOOL) validateMenuItem: (NSMenuItem *)menuItem
{
    SEL action = menuItem.action;
    NSInteger count = gameTableView.numberOfSelectedRows;

     if (gameTableView.clickedRow >= 0)
         count = 1;

    if (action == @selector(delete:))
        return count > 0;
    
    if (action == @selector(deleteGame:))
        return count > 0;
    
    if (action == @selector(revealGameInFinder:))
        return count > 0;
    
    if (action == @selector(showGameInfo:))
        return count > 0;
    
    if (action == @selector(play:))
        return count == 1;

    if (action == @selector(openIfdb:))
        return (((Game *)gameTableModel[gameTableView.clickedRow]).metadata.tuid != nil);

    if (action == @selector(toggleSidebar:))
    {
        NSString* title = _leftView.hidden ? @"Show Sidebar" : @"Hide Sidebar";
        [(NSMenuItem*)menuItem setTitle:title];
    }

    return YES;
}


/*
 * Drag-n-drop destination handler
 */

- (NSDragOperation) draggingEntered:sender
{
    extern NSString *terp_for_filename(NSString *path);
    
    NSFileManager *mgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSInteger i;
    
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([pboard.types containsObject: NSFilenamesPboardType])
    {
        NSArray *paths = [pboard propertyListForType: NSFilenamesPboardType];
        NSInteger count = paths.count;
        for (i = 0; i < count; i++)
        {
            NSString *path = paths[i];
            if ([gGameFileTypes containsObject: path.pathExtension.lowercaseString])
                return NSDragOperationCopy;
            if ([path.pathExtension isEqualToString: @"iFiction"])
                return NSDragOperationCopy;
            if ([mgr fileExistsAtPath: path isDirectory: &isdir])
                if (isdir)
                    return NSDragOperationCopy;
        }
    }
    
    return NSDragOperationNone;
}

- (void) draggingExited:sender
{
    // unhighlight window
}

- (BOOL) prepareForDragOperation:sender
{
    return YES;
}

- (BOOL) performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([pboard.types containsObject: NSFilenamesPboardType])
    {
        NSArray *files = [pboard propertyListForType: NSFilenamesPboardType];
        NSMutableArray *urls = [[NSMutableArray alloc] initWithCapacity:files.count];
        for (id tempObject in files) {
            [urls addObject:[NSURL fileURLWithPath:tempObject]];
        }
        [self beginImporting];
        [self addFiles: urls];
        [self endImporting];
        return YES;
    }
    return NO;
}


/*
 * Metadata is not tied to games in the library.
 * We keep it around even if the games come and go.
 * Metadata that has been edited by the user is flagged,
 * and can be exported.
 */

- (void) addMetadata: (NSMutableDictionary*)dict forIFIDs: (NSArray*)list
{
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
    
    if (cursrc == 0)
        NSLog(@"libctl: current metadata source failed...");

    /* prune the ifid list if it comes from ifdb */
    /* until we have a proper fix for downloading many images at once */

    if (cursrc == kIfdb)
        list = @[list[0]];

    count = list.count;

    for (i = 0; i < count; i++)
    {
        NSString *ifid = list[i];
        entry = [self fetchMetadataForIFID:ifid];
        if (!entry)
        {
            entry = (Metadata *) [NSEntityDescription
                                          insertNewObjectForEntityForName:@"Metadata"
                                          inManagedObjectContext:self.managedObjectContext];
            entry.ifid = ifid;
            NSLog(@"addMetaData:forIFIDs: Created new Metadata instance with ifid %@", ifid);
            //dict[kSource] = @((int)cursrc);
        }

        for(id key in dict)
        {
            keyVal = dict[key];

            if (cursrc == kIfdb)
                keyVal = [keyVal stringByDecodingXMLEntities];

            if ([(NSString *)key isEqualToString:@"description"])
                [entry setValue:keyVal forKey:@"blurb"];
            else if ([(NSString *)key isEqualToString:kSource])
            {
                if ((int)dict[kSource] == kUser)
                    entry.userEdited=YES;
                else
                    entry.userEdited=NO;
            }
            else if ([(NSString *)key isEqualToString:@"ifid"])
            {
                if (entry.ifid == nil)
                    entry.ifid = dict[@"ifid"];
            }
            else if ([(NSString *)key isEqualToString:@"firstpublished"])
            {
                if (dateFormatter == nil)
                    dateFormatter = [[NSDateFormatter alloc] init];
                if (keyVal.length == 4)
                    [dateFormatter setDateFormat:@"yyyy"];
                else if (keyVal.length == 10)
                    [dateFormatter setDateFormat:@"yyyy-MM-dd"];
                else NSLog(@"Illegal date format!");

                entry.firstpublished = [dateFormatter dateFromString:keyVal];
                [dateFormatter setDateFormat:@"yyyy"];
                entry.yearAsString = [dateFormatter stringFromDate:entry.firstpublished];
            }
            else if ([(NSString *)key isEqualToString:@"language"])
            {

                sub = [keyVal substringWithRange:NSMakeRange(0, 2)];
                sub = [sub lowercaseString];
                if (language[sub])
                    entry.language = language[sub];
                else
                    entry.language = keyVal;
            }
            else if ([(NSString *)key isEqualToString:@"format"])
            {
                if (entry.format == nil)
                    entry.format = dict[@"format"];
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
    // Save the Managed Object Context
    NSError *error = nil;

    if (![self.managedObjectContext save:&error])
        NSLog(@"There's a problem: %@", error);

}


- (void) setMetadataValue: (NSString*)val forKey: (NSString*)key forIFID: (NSString*)ifid
{
    Metadata *dict = [self fetchMetadataForIFID:ifid];
    if (dict)
    {
        NSLog(@"libctl: user set value %@ = '%@' for %@", key, val, ifid);
        dict.userEdited = YES;
        [dict setValue:val forKey:key];
        gameTableDirty = YES;

//        NSError *error = nil;
//
//        if (![[self managedObjectContext] save:&error])
//            NSLog(@"There's a problem: %@", error);
    }
}

static void read_xml_text(const char *rp, char *wp)
{
    /* kill initial whitespace */
    while (*rp && isspace(*rp))
        rp++;
    
    while (*rp)
    {
        if (*rp == '<')
        {
            if (strstr(rp, "<br/>") == rp || strstr(rp, "<br />") == rp)
            {
                *wp++ = '\n';
                *wp++ = '\n';
                rp += 5;
                if (*rp == '>')
                    rp ++;
                
                /* kill trailing whitespace */
                while (*rp && isspace(*rp))
                    rp++;
            }
            else
            {
                *wp++ = *rp++;
            }
        }
        else if (*rp == '&')
        {
            if (strstr(rp, "&lt;") == rp)
            {
                *wp++ = '<';
                rp += 4;
            }
            else if (strstr(rp, "&gt;") == rp)
            {
                *wp++ = '>';
                rp += 4;
            }
            else if (strstr(rp, "&amp;") == rp)
            {
                *wp++ = '&';
                rp += 5;
            }
            else
            {
                *wp++ = *rp++;
            }
        }
        else if (isspace(*rp))
        {
            *wp++ = ' ';
            while (*rp && isspace(*rp))
                rp++;
        }
        else
        {
            *wp++ = *rp++;
        }
    }
    
    *wp = 0;
}

- (void) handleXMLCloseTag: (struct XMLTag *)tag
{
    char bigbuf[4096];

    /* we don't parse tags after bibliographic until the next story begins... */
    if (!strcmp(tag->tag, "bibliographic"))
    {
        NSLog(@"libctl: import metadata for %@ by %@", metabuf[@"title"], metabuf[@"author"]);
        [self addMetadata: metabuf forIFIDs: ifidbuf];
        ifidbuf = nil;
        if (currentIfid == nil) /* We are not importing from ifdb */
            metabuf = nil;
    }

    /* parse extra ifdb tags... */
    else if (!strcmp(tag->tag, "ifdb"))
    {
        NSLog(@"libctl: import extra ifdb metadata for %@", metabuf[@"title"]);
        [self addMetadata: metabuf forIFIDs: @[currentIfid]];
        metabuf = nil;
    }

    /* we should only find ifid:s inside the <identification> tag, */
    /* which should be the first tag in a <story> */
    else if (!strcmp(tag->tag, "ifid"))
    {
        if (!ifidbuf)
        {
            ifidbuf = [[NSMutableArray alloc] initWithCapacity: 1];
            metabuf = [[NSMutableDictionary alloc] initWithCapacity: 12];
        }

        char save = *tag->end;
        *tag->end = 0;
        [ifidbuf addObject: @(tag->begin)];
        *tag->end = save;
    }
    
    /* only extract text from within the indentification and bibliographic tags */
    else if (metabuf)
    {
        if (tag->end - tag->begin >= sizeof(bigbuf) - 1)
            return;
        
        char save = *tag->end;
        *tag->end = 0;
        read_xml_text(tag->begin, bigbuf);
        *tag->end = save;
        
        NSString *val = @(bigbuf);

        if (!strcmp(tag->tag, "format"))
            metabuf[@"format"] = val;
        if (!strcmp(tag->tag, "bafn"))
            metabuf[@"bafn"] = val;
        if (!strcmp(tag->tag, "title"))
            metabuf[@"title"] = val;
        if (!strcmp(tag->tag, "author"))
            metabuf[@"author"] = val;
        if (!strcmp(tag->tag, "language"))
            metabuf[@"language"] = val;
        if (!strcmp(tag->tag, "headline"))
            metabuf[@"headline"] = val;
        if (!strcmp(tag->tag, "firstpublished"))
            metabuf[@"firstpublished"] = val;
        if (!strcmp(tag->tag, "genre"))
            metabuf[@"genre"] = val;
        if (!strcmp(tag->tag, "group"))
            metabuf[@"group"] = val;
        if (!strcmp(tag->tag, "description"))
            metabuf[@"description"] = val;
        if (!strcmp(tag->tag, "series"))
            metabuf[@"series"] = val;
        if (!strcmp(tag->tag, "seriesnumber"))
            metabuf[@"seriesnumber"] = val;
        if (!strcmp(tag->tag, "forgiveness"))
            metabuf[@"forgiveness"] = val;
        if (!strcmp(tag->tag, "tuid"))
            metabuf[@"tuid"] = val;
        if (!strcmp(tag->tag, "averageRating"))
            metabuf[@"averageRating"] = val;
        if (!strcmp(tag->tag, "starRating"))
            metabuf[@"starRating"] = val;
        if (!strcmp(tag->tag, "url"))
            metabuf[@"coverArtURL"] = val;
    }
}

- (void) handleXMLError: (char*)msg
{
    if (strstr(msg, "Error:"))
        NSLog(@"libctl: xml: %s", msg);
}

static void handleXMLCloseTag(struct XMLTag *tag, void *ctx)
{
    [(__bridge LibController*)ctx handleXMLCloseTag: tag];
}

static void handleXMLError(char *msg, void *ctx)
{
    [(__bridge LibController*)ctx handleXMLError: msg];
}

- (void) importMetadataFromXML: (char*)mdbuf
{
    ifiction_parse(mdbuf, handleXMLCloseTag, (__bridge void *)(self), handleXMLError, (__bridge void *)(self));
    ifidbuf = nil;
    metabuf = nil;
}

- (BOOL) importMetadataFromFile: (NSString*)filename
{
    NSLog(@"libctl: importMetadataFromFile %@", filename);
    
    NSMutableData *data;
    
    data = [NSMutableData dataWithContentsOfFile: filename];
    if (!data)
        return NO;
    [data appendBytes: "\0" length: 1];
    
    cursrc = kExternal;
    [self importMetadataFromXML: data.mutableBytes];
    cursrc = 0;

    [self addURLtoRecents: [NSURL fileURLWithPath:filename]];

    return YES;
}

- (BOOL) downloadMetadataForIFID: (NSString*)ifid
{
    NSLog(@"libctl: downloadMetadataForIFID %@", ifid);

    if (dataTask != nil) {
        [dataTask cancel];
    }

    NSString *string = [@"http://ifdb.tads.org/viewgame?ifiction&ifid=" stringByAppendingString:ifid];
    NSURL *url = [NSURL URLWithString:string];
    dataTask = [defaultSession dataTaskWithURL:url completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {


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
    
    [dataTask resume];
    return YES;
    
}

- (BOOL) downloadImage: (NSString*)imgurl for: (Metadata *)metadata
{
    NSLog(@"libctl: download image from url %@", imgurl);

    if (dataTask != nil) {
        [dataTask cancel];
    }

    NSURL *url = [NSURL URLWithString:imgurl];
    dataTask = [defaultSession dataTaskWithURL:url completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {


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
    
    [dataTask resume];
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


static void write_xml_text(FILE *fp, NSDictionary *info, NSString *key)
{
    NSString *val;
    const char *tagname;
    const char *s;
    
    val = info[key];
    if (!val)
        return;
    
    tagname = key.UTF8String;
    s = val.UTF8String;
    
    fprintf(fp, "<%s>", tagname);
    while (*s)
    {
        if (*s == '&')
            fprintf(fp, "&amp;");
        else if (*s == '<')
            fprintf(fp, "&lt;");
        else if (*s == '>')
            fprintf(fp, "&gt;");
        else if (*s == '\n')
        {
            fprintf(fp, "<br/>");
            while (s[1] == '\n')
                s++;
        }
        else
        {
            putc(*s, fp);
        }
        s++;
    }
    fprintf(fp, "</%s>\n", tagname);
}

/*
 * Export user-edited metadata (ie, with kSource == kUser)
 */
- (BOOL) exportMetadataToFile: (NSString*)filename what: (NSInteger)what
{
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
        entity = [ifid entity];
        info = [entity attributesByName];
        src = [info[kSource] intValue];
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


- (void) playGame: (Game*)game
{
    GlkController *gctl;

    Metadata *meta = game.metadata;
    NSURL *url = [game urlForBookmark];
    NSString *path = url.path;
    NSString *terp;

    // NSLog(@"playgame %@ %@", ifid, info);

    if (![[NSFileManager defaultManager] fileExistsAtPath: path])
    {
        NSRunAlertPanel(@"Cannot find the file.",
                        @"The file could not be found at its original location. Maybe it has been moved since it was added to the library.",
                        @"Okay", NULL, NULL);
        return;
    }

    if (![[NSFileManager defaultManager] isReadableFileAtPath: path])
    {
        NSRunAlertPanel(@"Cannot read the file.",
                        @"The file exists but can not be read.",
                        @"Okay", NULL, NULL);
        return;
    }

    terp = gExtMap[path.pathExtension];
    if (!terp)
        terp = gFormatMap[meta.format];

    if (!terp)
    {
        NSRunAlertPanel(@"Cannot play the file.",
                        @"The game is not in a recognized file format; cannot find a suitable interpreter.",
                        @"Okay", NULL, NULL);
        return;
    }

    gctl = [[GlkController alloc] initWithWindowNibName: @"GameWindow"];
    [gctl runTerp:terp withGame:game];
    [gctl showWindow: self];
    game.lastPlayed = [NSDate date];
    [self addURLtoRecents: game.urlForBookmark];
}


- (void) importAndPlayGame: (NSString*)path
{
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

- (Game *) importGame: (NSString*)path reportFailure: (BOOL)report
{
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

- (Metadata *) fetchMetadataForIFID: (NSString *)ifid
{
    NSError *error = nil;
    NSArray *fetchedObjects;
    NSPredicate *predicate;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:self.managedObjectContext];

    [fetchRequest setEntity:entity];

    predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@",ifid];
    [fetchRequest setPredicate:predicate];

    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if ([fetchedObjects count] > 1)
    {
        NSLog(@"Found more than one entry with metadata %@",ifid);
    }
    else if ([fetchedObjects count] == 0)
    {
        NSLog(@"fetchMetadataForIFID: Found no Metadata entity with ifid %@",ifid);
        return nil;
    }

    return fetchedObjects[0];
}


- (void) addFile: (NSURL*)url select: (NSMutableArray*)select
{
    Game *game = [self importGame: url.path reportFailure: NO];
    if (game)
        [select addObject: game];
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
        NSString *path = [urls[i] path];
        
        if (![filemgr fileExistsAtPath: path isDirectory: &isdir])
            continue;
        
        if (isdir)
        {
            NSArray *contents = [filemgr contentsOfDirectoryAtURL:urls[i]
                                       includingPropertiesForKeys:@[NSURLNameKey]
                                                          options:NSDirectoryEnumerationSkipsHiddenFiles
                                                            error:nil];
            [self addFiles: contents select: select];
        }
        else
        {
            [self addFile: urls[i] select: select];
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
        [self selectGame: select[i]];
}

- (void) addFile: (NSURL*)url
{
    [self addFiles: @[url]];
}


#pragma mark -
#pragma mark Table magic

- (IBAction) searchForGames: (id)sender
{
    NSString *value = [sender stringValue];
    searchStrings = nil;
    
    if (value.length)
        searchStrings = [value componentsSeparatedByString: @" "];
    
    gameTableDirty = YES;
    [self updateTableViews];
}

- (IBAction)toggleColumn:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSTableColumn * column;
    for (NSTableColumn *tableColumn in gameTableView.tableColumns ) {

        if ([tableColumn.identifier isEqualToString:item.identifier])
        {
            column = tableColumn;
            break;
        }
    }
    if (item.state == YES)
    {
        column.hidden = YES;
        item.state = NO;
    }
    else
    {
        column.hidden = NO;
        item.state = YES;
    }

}

- (void) deselectGames
{
    [gameTableView deselectAll: self];
}

- (void) selectGame: (Game*)game
{
    NSInteger i, count;
    count = gameTableModel.count;
    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

    for (i = 0; i < count; i++)
        if (gameTableModel[i] == game)
            [indexSet addIndex:i];

    [gameTableView selectRowIndexes:indexSet byExtendingSelection:YES];
    [gameTableView scrollRowToVisible:[indexSet firstIndex]];

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
    selrow = gameTableView.selectedRow;
    if (selrow >= 0)
        selection = gameTableModel[selrow];
    

    fetchRequest = [[NSFetchRequest alloc] init];
    entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
    [fetchRequest setEntity:entity];

    if (searchStrings)
    {
        searchcount = searchStrings.count;

        NSPredicate *predicate;
        NSMutableArray *predicateArr = [NSMutableArray arrayWithCapacity:searchcount];

        for (NSString *word in searchStrings) {
            predicate = [NSPredicate predicateWithFormat: @"(metadata.format contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.language contains [c] %@) OR (metadata.yearAsString contains %@)", word, word, word, word, word, word, word, word, word, word, word];
            [predicateArr addObject:predicate];
        }


        NSCompoundPredicate *comp = [NSCompoundPredicate orPredicateWithSubpredicates: predicateArr];

        [fetchRequest setPredicate:comp];
    }

    NSError *error = nil;
    gameTableModel = [[[self managedObjectContext] executeFetchRequest:fetchRequest error:&error] mutableCopy];
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
    [gameTableView reloadData];
    
    [gameTableView deselectAll: self];
    count = gameTableModel.count;

    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

    for (i = 0; i < count; i++)
        if (gameTableModel[i] == selection)
            [indexSet addIndex:i];

    [gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
    [gameTableView scrollRowToVisible:[indexSet firstIndex]];

    gameTableDirty = NO;
}


- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray *)oldDescriptors
{
    if (tableView == gameTableView)
    {
        NSSortDescriptor *sortDescriptor = tableView.sortDescriptors.firstObject;
        if (!sortDescriptor)
            return;
        gameSortColumn = sortDescriptor.key;
        sortAscending = sortDescriptor.ascending;
        gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (NSInteger) numberOfRowsInTableView: (NSTableView*)tableView
{
    if (tableView == gameTableView)
    {
        return gameTableModel.count;
    }
    return 0;
}

- (id) tableView: (NSTableView*)tableView
objectValueForTableColumn: (NSTableColumn*)column
             row: (int)row
{
    if (tableView == gameTableView)
    {
        Game *game = gameTableModel[row];
        Metadata *meta = game.metadata;
        if ([column.identifier  isEqual: @"added"] || [column.identifier  isEqual: @"lastPlayed"] || [column.identifier  isEqual: @"lastModified"])
        {
            return [[game valueForKey: column.identifier] formattedRelativeString];
        }
        else
            return [meta valueForKey: column.identifier];
    }
    return nil;
}

- (void)tableView:(NSTableView *)tableView
   setObjectValue:(id)value
   forTableColumn:(NSTableColumn *)tableColumn
              row:(int)row
{
    if (tableView == gameTableView)
    {
        Game *game = gameTableModel[row];
        Metadata *meta = game.metadata;
        NSString *key = tableColumn.identifier;
        NSString *oldval = [meta valueForKey:key];
		if ([value isKindOfClass:[NSNumber class]])
			value = [[NSString alloc] initWithFormat:@"%@", value];
        if (oldval == nil && (value == nil || ((NSString*)value).length == 0))
            return;
        if ([value isEqualTo: oldval])
            return;
        [meta setValue: value forKey: key];
    }
}

- (void) tableViewSelectionDidChange: (id)notification
{
    NSTableView *tableView = [notification object];
    NSIndexSet *rows;
    if (tableView == gameTableView)
    {
        [self enableClickToRenameAfterDelay];

        rows = tableView.selectedRowIndexes;

        infoButton.enabled = rows.count > 0;
        playButton.enabled = rows.count == 1;
        if (rows.count == 1)
        {
            [self updateSideView];
        }
    }
}

- (void) updateSideView
{

	NSIndexSet *rows = gameTableView.selectedRowIndexes;

	if (!rows.count)
	{
		NSLog(@"No game selected in table view, returning without updating side view");
		return;
	} else NSLog(@"%lu rows selected.", (unsigned long)rows.count);

	Game *game = gameTableModel[rows.firstIndex];

	NSLog(@"\nUpdating info pane for %@", game.metadata.title);

	_infoView = [[MySideInfoView alloc] initWithFrame:[_leftScrollView frame]];

	_leftScrollView.documentView = _infoView;

	[_infoView updateSideViewForGame:game];

	if (game.metadata.ifid)
		_sideIfid.stringValue = game.metadata.ifid;
	else
		_sideIfid.stringValue = @"";

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
	return splitView.frame.size.width / 2;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
	return (CGFloat)200;
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
	NSView *left  = [[_splitView subviews] objectAtIndex:0];
	NSView *right = [[_splitView subviews] objectAtIndex:1];
	NSRect rightFrame = [right frame];
	NSRect overallFrame = [_splitView frame];
	[left setHidden:YES];
	[right setFrameSize:NSMakeSize(overallFrame.size.width,rightFrame.size.height)];
	[_splitView display];
}

-(void)uncollapseLeftView
{
	NSView *left  = [[_splitView subviews] objectAtIndex:0];
	NSView *right = [[_splitView subviews] objectAtIndex:1];
	[left setHidden:NO];

	CGFloat dividerThickness = [_splitView dividerThickness];

	NSLog(@"dividerThickness = %f", dividerThickness);

	// get the different frames
	NSRect leftFrame = [left frame];
	NSRect rightFrame = [right frame];

	rightFrame.size.width = (rightFrame.size.width-leftFrame.size.width-dividerThickness);
	leftFrame.origin.x = 0;
	[left setFrameSize:leftFrame.size];
	[right setFrame:rightFrame];
	[_splitView display];
}

CGFloat lastsplitViewWidth = 0;

//- (void)splitViewDidResizeSubviews:(NSNotification *)notification
//{
//	// TODO: This should call a faster update method rather than rebuilding the view from scratch every time, but everything I've tried makes word wrap wonky
//	[self updateSideView];
//}

#pragma mark -
#pragma mark Add to Open Recent menu stuff


- (void) addURLtoRecents: (NSURL *) url
{
    [((AppDelegate*)[[NSApplication sharedApplication] delegate]) addToRecents:@[url]];

}


#pragma mark -
#pragma mark Open game on double click, edit on click and wait

-(void)doClick:(id)sender {
//    NSLog(@"doClick:");
    if (canEdit) {
        NSInteger row = [gameTableView clickedRow];
        if (row >= 0) {
            [self startTimerWithTimeInterval:0.5 selector:@selector(renameByTimer:)];
        }
    }
}

// DoubleAction
-(void)doDoubleClick:(id)sender {
//    NSLog(@"doDoubleClick:");
    [self enableClickToRenameAfterDelay];
    [self play:sender];
}

-(void)enableClickToRenameAfterDelay {
    canEdit = NO;
    [self startTimerWithTimeInterval:0.2
                            selector:@selector(enableClickToRenameByTimer:)];
}

-(void)enableClickToRenameByTimer:(id)sender {
    //NSLog(@"enableClickToRenameByTimer:");
    canEdit = YES;
}

-(void)renameByTimer:(id)sender {
    if (canEdit) {
        NSInteger row = [gameTableView selectedRow];
        NSInteger column = [gameTableView selectedColumn];

        if (row != -1 && column != -1) {
            [gameTableView editColumn:column row:row withEvent:nil select:YES];
        }
    }
}

-(void)startTimerWithTimeInterval:(NSTimeInterval)seconds selector:(SEL)selector {
    [self stopTimer];
    timer = [NSTimer scheduledTimerWithTimeInterval:seconds
                                             target:self
                                           selector:selector
                                           userInfo:nil
                                            repeats:NO];
}

-(void)stopTimer {
    if (timer != nil) {
        if ([timer isValid]) {
            [timer invalidate];
        }
    }
}



#pragma mark -
#pragma mark Some stuff that doesn't really fit anywhere else

- (NSString*) convertAGTFile: (NSString*)origpath
{
    NSLog(@"libctl: converting agt to agx");
    char rapeme[] = "/tmp/cugelcvtout.agx"; /* babel will clobber this, so we can't send a const string */
    
    NSString *exepath = [[NSBundle mainBundle] pathForAuxiliaryExecutable: @"agt2agx"];
    NSString *dirpath;
    NSString *cvtpath;
    NSTask *task;
    char *format;
    int status;
    
    task = [[NSTask alloc] init];
    task.launchPath = exepath;
    task.arguments = @[@"-o", @"/tmp/cugelcvtout.agx", origpath];
    [task launch];
    [task waitUntilExit];
    status = task.terminationStatus;
    
    if (status != 0)
    {
        NSLog(@"libctl: agt2agx failed");
        return nil;
    }
    
    format = babel_init(rapeme);
    if (!strcmp(format, "agt"))
    {
        char buf[TREATY_MINIMUM_EXTENT];
        int rv = babel_treaty(GET_STORY_FILE_IFID_SEL, buf, sizeof buf);
        if (rv == 1)
        {
            dirpath = [homepath.path stringByAppendingPathComponent: @"Converted"];
            
            [[NSFileManager defaultManager] createDirectoryAtURL:[NSURL fileURLWithPath:dirpath isDirectory:YES] withIntermediateDirectories:YES attributes:nil error:NULL];
            
            cvtpath =
            [dirpath stringByAppendingPathComponent:
             [@(buf)
              stringByAppendingPathExtension: @"agx"]];
            
            babel_release();
            
            NSURL *url = [NSURL fileURLWithPath:cvtpath];
            
            [[NSFileManager defaultManager]  removeItemAtURL: url error: nil];
            
            status = [[NSFileManager defaultManager] moveItemAtPath: @"/tmp/cugelcvtout.agx"
                                                             toPath: cvtpath
                                                              error: nil];
            
            if (!status)
            {
                NSLog(@"libctl: could not move converted file");
                return nil;
            }
            return cvtpath;
        }
    }
    else
    {
        NSLog(@"libctl: babel did not like the converted file");
    }
    babel_release();
    
    return nil;
}

@end
