/*
 * Library with metadata --
 *
 * Keep an archive of game metadata.
 * Import iFiction format from files or babel software.
 * Save the database in Library/Application Support/Spatterlight/Metadata.plist
 * Tag user-edited entries for export.
 *
 * Keep a list of games with map filename -> ifid
 * Save list of games to Games.plist
 *
 * TODO: babel-get from ifarchive
 *
 */

#define MAX_INFO_WINDOWS 10

@interface LibHelperWindow : NSWindow<NSDraggingDestination>
{
}
@end

@interface LibHelperTableView : NSTableView
{
}
@end

@class InfoController;

@interface LibController : NSWindowController<NSDraggingDestination, NSWindowDelegate>
{
    NSURL *homepath;

    IBOutlet NSButton *infoButton;
    IBOutlet NSButton *playButton;
    IBOutlet NSPanel *importProgressPanel;
    IBOutlet NSView *exportTypeView;
    IBOutlet NSPopUpButton *exportTypeControl;

    NSMutableDictionary *metadata; /* ifid -> metadata dict */
    NSMutableDictionary *games; /* ifid -> filename */

    InfoController *infoWindows[MAX_INFO_WINDOWS];
    NSInteger infoWindowIndex;

    IBOutlet NSTableView *gameTableView;
    NSMutableArray *gameTableModel;
    NSString *gameSortColumn;
    BOOL gameTableDirty;
    BOOL sortAscending;

    BOOL canEdit;
    NSTimer * timer;

    NSArray *searchStrings;

    /* for the importing */
    NSInteger cursrc;
    NSMutableArray *ifidbuf;
    NSMutableDictionary *metabuf;
    NSInteger errorflag;
}

- (void) loadLibrary; /* initializer */
- (IBAction) saveLibrary: sender;

- (void) beginImporting;
- (void) endImporting;

- (NSString*) importGame: (NSString*)path reportFailure: (BOOL)report;
- (void) addFile: (NSString*)path select: (NSMutableArray*)select;
- (void) addFiles: (NSArray*)paths select: (NSMutableArray*)select;
- (void) addFiles: (NSArray*)paths;
- (void) addFile: (NSString*)path;

- (void) playGameWithIFID: (NSString*)ifid;
- (void) importAndPlayGame: (NSString*)path;

- (IBAction) addGamesToLibrary: (id)sender;
- (IBAction) deleteLibrary: (id)sender;

- (IBAction) importMetadata: (id)sender;
- (IBAction) exportMetadata: (id)sender;
- (BOOL) importMetadataFromFile: (NSString*)filename;
- (BOOL) exportMetadataToFile: (NSString*)filename what: (NSInteger)what;

- (IBAction) searchForGames: (id)sender;
- (IBAction) playGame: (id)sender;
- (IBAction) showGameInfo: (id)sender;
- (InfoController *) createInfoController;
- (IBAction) revealGameInFinder: (id)sender;
- (IBAction) deleteGame: (id)sender;

@property (strong) IBOutlet NSMenu *headerMenu;
- (IBAction)toggleColumn:(id)sender;
- (void) deselectGames;
- (void) selectGameWithIFID: (NSString*)ifid;
- (void) updateTableViews; /* must call this after -importGame: */

- (void)enableClickToRenameAfterDelay;

- (NSString*) convertAGTFile: (NSString*)origpath;
@property (weak) IBOutlet NSSearchField *searchField;

@end
