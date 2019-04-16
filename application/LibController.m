/*
 *
 */

#import "main.h"
#import "InfoController.h"

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

- (void) textDidEndEditing: (NSNotification *) notification
{
    NSDictionary *userInfo;
    NSDictionary *newUserInfo;
    NSNumber *textMovement;
    int movementCode;

    userInfo = notification.userInfo;
    textMovement = [userInfo objectForKey:@"NSTextMovement"];
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
}

@end

@implementation LibController

#pragma mark Window controller, menus and file dialogs.

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

static BOOL save_plist(NSString *path, NSDictionary *plist)
{
    NSData *plistData;
    NSString *error;

    plistData = [NSPropertyListSerialization dataFromPropertyList: plist
                                                           format: NSPropertyListBinaryFormat_v1_0
                                                 errorDescription: &error];
    if (plistData)
    {
        return [plistData writeToFile:path atomically:YES];
    }
    else
    {
        NSLog(@"%@", error);
        return NO;
    }
}

- (instancetype) init {
    self = [super initWithWindowNibName: @"LibraryWindow"];
    if (self) {
        NSError *error;
        homepath = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];
        if (error)
            NSLog(@"libctl: Could not find Application Support directory! Error: %@", error);

        homepath = [NSURL URLWithString: @"Spatterlight" relativeToURL:homepath];

        [[NSFileManager defaultManager] createDirectoryAtURL:homepath withIntermediateDirectories:YES attributes:NULL error:NULL];

        metadata = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Metadata.plist"]);
        games = load_mutable_plist([homepath.path stringByAppendingPathComponent: @"Games.plist"]);
    }
    return self;
}

- (IBAction) saveLibrary: (id)sender
{
    NSLog(@"libctl: saveLibrary");

    int code;

    code = save_plist([homepath.path stringByAppendingPathComponent: @"Metadata.plist"], metadata);
    if (!code)
        NSLog(@"libctl: cannot write metadata!");

    code = save_plist([homepath.path stringByAppendingPathComponent: @"Games.plist"], games);
    if (!code)
        NSLog(@"libctl: cannot write game list!");
}

- (void) windowDidLoad
{
    NSLog(@"libctl: windowDidLoad");

    self.windowFrameAutosaveName = @"LibraryWindow";
    _gameTableView.autosaveName = @"GameTable";
    _gameTableView.autosaveTableColumns = YES;

    _gameTableView.action = @selector(doClick:);
    _gameTableView.doubleAction = @selector(doDoubleClick:);
    _gameTableView.target = self;

    self.window.excludedFromWindowsMenu = YES;
    [self.window registerForDraggedTypes:
     @[NSFilenamesPboardType]];

    [infoButton setEnabled: NO];
    [playButton setEnabled: NO];

    _infoWindows = [[NSMutableDictionary alloc] init];
    _gameSessions = [[NSMutableDictionary alloc] init];
    gameTableModel = [[NSMutableArray alloc] init];

    NSString *key;
    NSSortDescriptor *sortDescriptor;

    for (NSTableColumn *tableColumn in _gameTableView.tableColumns)
    {

        key = tableColumn.identifier;
        sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:key ascending:YES];
        tableColumn.sortDescriptorPrototype = sortDescriptor;
        
        for (NSMenuItem *menuitem in headerMenu.itemArray)
        {
            if ([[menuitem valueForKey:@"identifier"] isEqualToString:key])
            {
                menuitem.state = ![tableColumn isHidden];
                break;
            }
        }
    }

    gameTableDirty = YES;
    [self updateTableViews];
}

- (IBAction) deleteLibrary: (id)sender
{
    NSInteger choice =
    NSRunAlertPanel(@"Do you really want to delete the library?",
                    @"All the information about your games will be lost. The original game files will not be harmed.",
                    @"Delete", NULL, @"Cancel");
    if (choice != NSAlertOtherReturn)
    {
        [metadata removeAllObjects];
        [games removeAllObjects];
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
        NSEnumerator *enumerator;
        NSString *ifid;
        enumerator = [metadata keyEnumerator];
        while ((ifid = [enumerator nextObject]))
        {
            if (![games objectForKey:ifid])
            {
                [metadata removeObjectForKey: ifid];
            }
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

	NSPopUpButton *localExportTypeControl = exportTypeControl;

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        if (result == NSFileHandlingPanelOKButton)
        {
            NSURL* url = panel.URL;

			[self exportMetadataToFile: url.path what: localExportTypeControl.indexOfSelectedItem];
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

- (void) performFindPanelAction:(id<NSValidatedUserInterfaceItem>)sender
{
    [_searchField selectText:self];
}

#pragma mark Contextual menu

- (IBAction) playGame: (id)sender
{
    NSInteger rowidx;
    
    if (_gameTableView.clickedRow != -1)
        rowidx = _gameTableView.clickedRow;
    else
        rowidx = _gameTableView.selectedRow;

    if (rowidx >= 0)
    {
        NSString *ifid = [gameTableModel objectAtIndex:rowidx];
        [self playGameWithIFID: ifid];
    }
}

- (IBAction) showGameInfo: (id)sender
{
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;
    
    // If we clicked outside selected rows, only show info for clicked row
    if (_gameTableView.clickedRow != -1 && ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];

    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
    {
        NSString *ifid = [gameTableModel objectAtIndex:i];
        NSString *path = [games objectForKey:ifid];
        NSDictionary *info = [metadata objectForKey:ifid];

        if (![[NSFileManager defaultManager] fileExistsAtPath: path])
        {
            NSRunAlertPanel(@"Cannot find the file.",
                            @"The file could not be found at its original location. Maybe it has been moved since it was added to the library.",
                            @"Okay", NULL, NULL);
            return;
        }

        [self showInfo:info forFile:path];
    }
}

- (void) showInfo:(NSDictionary *)info forFile:(NSString *)path
{
    InfoController *infoctl;

    //First, we check if we have created this info window already
    infoctl = [_infoWindows objectForKey:path];
    
    if (!infoctl) {
        infoctl = [[InfoController alloc] initWithpath:path andInfo:info];
        NSWindow *infoWindow = infoctl.window;
        infoWindow.restorable = YES;
        infoWindow.restorationClass = [AppDelegate class];
        infoWindow.identifier = [NSString stringWithFormat: @"infoWin%@", path];
        [_infoWindows setObject:infoctl forKey:path];
    }
    
    [infoctl showWindow:nil];
}

- (IBAction) revealGameInFinder: (id)sender
{
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only reveal game in clicked row
    if (_gameTableView.clickedRow != -1 && ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];
    
    NSInteger i;
    for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
    {
        NSString *ifid = [gameTableModel objectAtIndex:i];
        NSString *path = [games objectForKey:ifid];
        if (![[NSFileManager defaultManager] fileExistsAtPath: path])
        {
            NSRunAlertPanel(@"Cannot find the file.",
                            @"The file could not be found at its original location. Maybe it has been moved since it was added to the library.",
                            @"Okay", NULL, NULL);
            return;
        }
        [[NSWorkspace sharedWorkspace] selectFile: path inFileViewerRootedAtPath: @""];
    }
}

- (IBAction) deleteGame: (id)sender
{
    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only delete game in clicked row
    if (_gameTableView.clickedRow != -1 && ![rows containsIndex:_gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:_gameTableView.clickedRow];
    
    if (rows.count > 0)
    {
        NSString *ifid;
        NSInteger i;

        for (i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex: i])
        {
            ifid = [gameTableModel objectAtIndex:i];
            NSLog(@"libctl: delete game %@", ifid);
            [games removeObjectForKey: ifid];
        }

        gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (IBAction) delete: (id)sender
{
    [self deleteGame: sender];
}

- (BOOL) validateMenuItem: (NSMenuItem *)menuItem
{
    SEL action = menuItem.action;
    NSInteger count = _gameTableView.numberOfSelectedRows;

    NSIndexSet *rows = _gameTableView.selectedRowIndexes;

    if (_gameTableView.clickedRow != -1 && (![rows containsIndex:_gameTableView.clickedRow]))
        count = 1;

    if (action == @selector(performFindPanelAction:))
    {
        if (menuItem.tag == NSTextFinderActionShowFindInterface)
            return YES;
        else return NO;
    }

    if (action == @selector(delete:))
        return count > 0;

    if (action == @selector(deleteGame:))
        return count > 0;

    if (action == @selector(revealGameInFinder:))
        return count > 0;

    if (action == @selector(showGameInfo:))
        return count > 0;

    if (action == @selector(playGame:))
        return count == 1;

    return YES;
}

#pragma mark Drag-n-drop destination handler

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
            NSString *path = [paths objectAtIndex:i];
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

#pragma mark Metadata

/*
 * Metadata is not tied to games in the library.
 * We keep it around even if the games come and go.
 * Metadata that has been edited by the user is flagged,
 * and can be exported.
 */

- (void) addMetadata: (NSMutableDictionary*)dict forIFIDs: (NSArray*)list
{
    NSInteger count = list.count;
    NSInteger i;

    if (cursrc == 0)
        NSLog(@"libctl: current metadata source failed...");

    for (i = 0; i < count; i++)
    {
        NSString *ifid = [list objectAtIndex:i];
        NSDictionary *entry = [metadata objectForKey:ifid];
        if (entry)
        {
            NSNumber *oldsrcv = [entry objectForKey:kSource];
            int oldsrc = oldsrcv.intValue;
            if (cursrc >= oldsrc)
            {
                [dict setObject: @((int)cursrc) forKey:kSource];
                [metadata setObject:dict forKey:ifid];
                gameTableDirty = YES;
            }
        }
        else
        {
            [dict setObject:@((int)cursrc) forKey:kSource];
            [metadata setObject:dict forKey:ifid];
            gameTableDirty = YES;
        }
    }
}

- (void) setMetadataValue: (NSString*)val forKey: (NSString*)key forIFID: (NSString*)ifid
{
    NSMutableDictionary *dict = [metadata objectForKey:ifid];
    if (dict)
    {
        NSLog(@"libctl: user set value %@ = '%@' for %@", key, val, ifid);
        [dict setObject:@kUser forKey:kSource];
        [dict setObject:val forKey:key];
        gameTableDirty = YES;
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
        NSLog(@"libctl: import metadata for %@ by %@", [metabuf objectForKey:@"title"], [metabuf objectForKey:@"author"]);
        [self addMetadata: metabuf forIFIDs: ifidbuf];
        ifidbuf = nil;
        metabuf = nil;
    }

    /* we should only find ifid:s inside the <identification> tag, */
    /* which should be the first tag in a <story> */
    else if (!strcmp(tag->tag, "ifid"))
    {
        if (!ifidbuf)
        {
            ifidbuf = [[NSMutableArray alloc] initWithCapacity: 1];
            metabuf = [[NSMutableDictionary alloc] initWithCapacity: 10];
        }
        char save = *tag->end;
        *tag->end = 0;
        [ifidbuf addObject: @(tag->begin)];
        *tag->end = save;
    }

    /* only extract text from within the indentification and bibliographic tags */
    else if (metabuf)
    {
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

    return YES;
}

static void write_xml_text(FILE *fp, NSDictionary *info, NSString *key)
{
    NSString *val;
    const char *tagname;
    const char *s;

    val = [info objectForKey:key];
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
    NSString *ifid;
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

    if (what == X_EDITED || what == X_DATABASE)
        enumerator = [metadata keyEnumerator];
    else
        enumerator = [games keyEnumerator];
    while ((ifid = [enumerator nextObject]))
    {
        info = [metadata objectForKey:ifid];
        src = [[info objectForKey:kSource] intValue];
        if ((what == X_EDITED && src >= kUser) || what != X_EDITED)
        {
            fprintf(fp, "<story>\n");

            fprintf(fp, "<identification>\n");
            fprintf(fp, "<ifid>%s</ifid>\n", ifid.UTF8String);
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
- (NSWindow *) playGameWithIFID: (NSString*)ifid
{
    NSDictionary *info = [metadata objectForKey:ifid];
    NSString *path = [games objectForKey:ifid];
    NSString *terp;
    GlkController *gctl = [_gameSessions objectForKey:ifid];

    NSLog(@"playgame %@ %@", ifid, info);

    if (gctl)
    {
        NSLog(@"A game with this ifid is already in session");
        [gctl.window makeKeyAndOrderFront:nil];
        return gctl.window;
    }

    if (![[NSFileManager defaultManager] fileExistsAtPath: path])
    {
        NSRunAlertPanel(@"Cannot find the file.",
                        @"The file could not be found at its original location. Maybe it has been moved since it was added to the library.",
                        @"Okay", NULL, NULL);
        return nil;
    }

    if (![[NSFileManager defaultManager] isReadableFileAtPath: path])
    {
        NSRunAlertPanel(@"Cannot read the file.",
                        @"The file exists but can not be read.",
                        @"Okay", NULL, NULL);
        return nil;
    }

    terp = [gExtMap objectForKey:path.pathExtension];
    if (!terp)
        terp = [gFormatMap objectForKey:[info objectForKey: @"format"]];

    if (!terp)
    {
        NSRunAlertPanel(@"Cannot play the file.",
                        @"The game is not in a recognized file format; cannot find a suitable interpreter.",
                        @"Okay", NULL, NULL);
        return nil;
    }

    gctl = [[GlkController alloc] initWithWindowNibName: @"GameWindow"];
    if ([terp isEqualToString:@"glulxe"] || [terp isEqualToString:@"fizmo"]) {
        gctl.window.restorable = YES;
        gctl.window.restorationClass = [AppDelegate class];
        gctl.window.identifier = [NSString stringWithFormat: @"gameWin%@", ifid];
    }
    [_gameSessions setObject:gctl forKey:ifid];
    [gctl runTerp:terp withGameFile:path IFID:ifid info:info];
    [self addURLtoRecents: [NSURL fileURLWithPath:path]];

    if (!gctl.hasAutorestored)
        [gctl showWindow:self];
    return gctl.window;
}

- (void) importAndPlayGame: (NSString*)path
{
    NSEnumerator *enumerator;
    NSString *ifid;

    enumerator = [games keyEnumerator];
    while ((ifid = [enumerator nextObject]))
    {
        if ([[games objectForKey:ifid] isEqualToString: path])
        {
            [self playGameWithIFID: ifid];
            return;
        }
    }

    ifid = [self importGame: path reportFailure: YES];
    if (ifid)
    {
        [self updateTableViews];
        [self deselectGames];
        [self selectGameWithIFID: ifid];
        [self playGameWithIFID: ifid];
    }
}

- (NSString*) importGame: (NSString*)path reportFailure: (BOOL)report
{
    char buf[TREATY_MINIMUM_EXTENT];
    NSMutableDictionary *dict;
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

    dict = [metadata objectForKey:ifid];
    if (!dict)
    {
        dict = [NSMutableDictionary dictionary];
        cursrc = kDefault;
        [self addMetadata: dict forIFIDs: @[ifid]];
        cursrc = 0;
    }

    if (![dict objectForKey:@"format"])
        [dict setObject:@(format) forKey:@"format"];
    if (![dict objectForKey:@"title"])
        [dict setObject:path.lastPathComponent forKey:@"title"];

    babel_release();

    [games setObject:path forKey:ifid];

    [self addURLtoRecents: [NSURL fileURLWithPath:path]];

    gameTableDirty = YES;

    return ifid;
}

- (void) addFile: (NSURL*)url select: (NSMutableArray*)select
{
    NSString *ifid = [self importGame: url.path reportFailure: NO];
    if (ifid)
        [select addObject: ifid];
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
            [self addFile: [urls objectAtIndex:i] select: select];
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
        [self selectGameWithIFID: [select objectAtIndex:i]];
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

- (IBAction)toggleColumn:(id)sender
{
    NSMenuItem *item = (NSMenuItem *)sender;
    NSTableColumn * column;
    for (NSTableColumn *tableColumn in _gameTableView.tableColumns)
    {
        if ([tableColumn.identifier isEqualToString:[item valueForKey:@"identifier"]])
        {
            column = tableColumn;
            break;
        }
    }
    if (item.state == YES)
    {
        [column setHidden:YES];
        item.state = NO;
    }
    else
    {
        [column setHidden:NO];
        item.state = YES;
    }
}

- (void) deselectGames
{
    [_gameTableView deselectAll: self];
}

- (void) selectGameWithIFID: (NSString*)ifid
{
    NSInteger i, count;
    count = gameTableModel.count;
    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

    for (i = 0; i < count; i++)
        if ([[gameTableModel objectAtIndex:i] isEqualToString: ifid])
            [indexSet addIndex:i];

    [_gameTableView selectRowIndexes:indexSet byExtendingSelection:YES];
}

static NSInteger Strstr(NSString *haystack, NSString *needle)
{
    if (haystack)
        return [haystack rangeOfString: needle options: NSCaseInsensitiveSearch].length;
    return 0;
}

static NSInteger Strcmp(NSString *a, NSString *b)
{
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedCaseInsensitiveCompare: b];
}

static NSInteger compareDicts(NSDictionary * a, NSDictionary * b, id key, BOOL ascending)
{
    NSString * ael = [[a objectForKey:key] description];
    NSString * bel = [[b objectForKey:key] description];
    if ((!ael || ael.length == 0) && (!bel || bel.length == 0))
        return NSOrderedSame;
    if (!ael || ael.length == 0) return ascending ? NSOrderedDescending :  NSOrderedAscending;;
    if (!bel || bel.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;
;
    return Strcmp(ael, bel);
}

- (void) updateTableViews
{
    NSEnumerator *enumerator;
    NSMutableDictionary *meta;
    NSArray *selectedGames;
    NSString *needle;
    NSString *ifid;
    NSInteger searchcount;
    NSInteger count;
    NSInteger found;
    NSInteger i;

    if (!gameTableDirty)
        return;

    selectedGames = [gameTableModel objectsAtIndexes:_gameTableView.selectedRowIndexes];

    [gameTableModel removeAllObjects];

    if (searchStrings)
        searchcount = searchStrings.count;
    else
        searchcount = 0;

    enumerator = [games keyEnumerator];
    while ((ifid = [enumerator nextObject]))
    {
        meta = [metadata objectForKey:ifid];
        if (searchcount > 0)
        {
            count = 0;
            for (i = 0; i < searchcount; i++)
            {
                found = NO;
                needle = [searchStrings objectAtIndex:i];
                if (Strstr([meta objectForKey:@"format"], needle)) found++;
                if (Strstr([meta objectForKey:@"title"], needle)) found++;
                if (Strstr([meta objectForKey:@"author"], needle)) found++;
                if (Strstr([meta objectForKey:@"firstpublished"], needle)) found++;
                if (Strstr([meta objectForKey:@"group"], needle)) found++;
                if (Strstr([meta objectForKey:@"genre"], needle)) found++;
                if (Strstr([meta objectForKey:@"series"], needle)) found++;
                if (Strstr([meta objectForKey:@"forgiveness"], needle)) found++;
                if (Strstr([meta objectForKey:@"language"], needle)) found++;
                if (found)
                    count ++;
            }
            if (count == searchcount)
                [gameTableModel addObject: ifid];
        }
        else
        {
            [gameTableModel addObject: ifid];
        }
    }

	BOOL localSortAscending = sortAscending;
	NSString *localGameSortColumn = gameSortColumn;
	NSDictionary *localMetadata = metadata;

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:sortAscending comparator:^(NSString *aid, NSString *bid) {
        
        NSDictionary *a = [localMetadata objectForKey:aid];
        NSDictionary *b = [localMetadata objectForKey:bid];
        NSInteger cmp;
       
        if (localGameSortColumn)
        {
            cmp = compareDicts(a, b, localGameSortColumn, localSortAscending);
            if (cmp) return cmp;
        }
        cmp = compareDicts(a, b, @"title", localSortAscending);
        if (cmp) return cmp;
        cmp = compareDicts(a, b, @"author", localSortAscending);
        if (cmp) return cmp;
        cmp = compareDicts(a, b, @"seriesnumber",localSortAscending);
        if (cmp) return cmp;
        cmp = compareDicts(a, b, @"firstpublished",localSortAscending);
        if (cmp) return cmp;
        return compareDicts(a, b, @"format", localSortAscending);
    }];

    [gameTableModel sortUsingDescriptors:@[sort]];

    [_gameTableView reloadData];
    [_gameTableView deselectAll: self];

    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

    for (NSString *row in gameTableModel)
        if ([selectedGames containsObject:row])
            [indexSet addIndex:[gameTableModel indexOfObject:row]];

    [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];

    gameTableDirty = NO;
    [self invalidateRestorableState];
}

- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray *)oldDescriptors
{
    if (tableView == _gameTableView)
    {
        NSSortDescriptor *sortDescriptor = [tableView.sortDescriptors objectAtIndex:0];
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
    if (tableView == _gameTableView)
        return gameTableModel.count;
    return 0;
}

- (id) tableView: (NSTableView*)tableView
objectValueForTableColumn: (NSTableColumn*)column
             row: (int)row
{
    if (tableView == _gameTableView)
    {
        NSString *gameifid = [gameTableModel objectAtIndex:row];
        NSDictionary *gamemeta = [metadata objectForKey:gameifid];
        return [gamemeta objectForKey:column.identifier];
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
        NSString *ifid = [gameTableModel objectAtIndex:row];
        NSDictionary *info = [metadata objectForKey:ifid];
        NSString *key = tableColumn.identifier;
        NSString *oldval = [info objectForKey:key];
        if (oldval == nil && ((NSString*)value).length == 0)
            return;
        if ([value isEqualTo: oldval])
            return;
        [self setMetadataValue: value forKey: key forIFID: ifid];
    }
}

- (void) tableViewSelectionDidChange: (id)notification
{
    NSTableView *tableView = [notification object];
    NSIndexSet *rows = tableView.selectedRowIndexes;
    if (tableView == _gameTableView)
    {
        infoButton.enabled = rows.count > 0;
        playButton.enabled = rows.count == 1;
        [self invalidateRestorableState];
    }
}


#pragma mark -
#pragma mark Windows restoration

- (void) window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state
{
    [state encodeObject:_searchField.stringValue forKey:@"searchText"];
    NSIndexSet *selrow = _gameTableView.selectedRowIndexes;
    NSArray *selectedGames = [gameTableModel objectsAtIndexes:selrow];
    [state encodeObject:selectedGames forKey:@"selectedGames"];
}

- (void) window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state
{
    NSString *searchText = [state decodeObjectForKey:@"searchText"];
    gameTableDirty = YES;
    if (searchText.length) {
        NSLog(@"Restored searchbar text %@", searchText);
        _searchField.stringValue = searchText;
        [self searchForGames:_searchField];
    }
    NSArray *selectedGames = [state decodeObjectForKey:@"selectedGames"];
    if (selectedGames.count) {
        [self updateTableViews];
        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

        for (NSString *row in selectedGames)
            if ([gameTableModel containsObject:row])
                [indexSet addIndex:[gameTableModel indexOfObject:row]];

        [_gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
    }
}

#pragma mark -
#pragma mark Add to Open Recent menu stuff


- (void) addURLtoRecents: (NSURL *) url
{
    [((AppDelegate*)[NSApplication sharedApplication].delegate) addToRecents:@[url]];
}

#pragma mark -
#pragma mark Open game on double click, edit on click and wait

-(void)doClick:(id)sender {
//    NSLog(@"doClick:");
    if (canEdit) {
        NSInteger row = _gameTableView.clickedRow;
        if (row >= 0) {
            [self startTimerWithTimeInterval:0.5 selector:@selector(renameByTimer:)];
        }
    }
}

// DoubleAction
-(void)doDoubleClick:(id)sender {
//    NSLog(@"doDoubleClick:");
    [self enableClickToRenameAfterDelay];
    [self playGame:sender];
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
        NSInteger row = _gameTableView.selectedRow;
        NSInteger column = _gameTableView.selectedColumn;

        if (row != -1 && column != -1) {
            [_gameTableView editColumn:column row:row withEvent:nil select:YES];
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
#pragma mark Some stuff that doesn't really fit anywhere else.

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
