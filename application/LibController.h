/*
 * Library with metadata --
 *
 * Keep an archive of game metadata.
 * Import iFiction format from files or babel software.
 * Save the database in Library/Application Support/Cugel/Metadata.plist
 * Tag user-edited entries for export.
 *
 * Keep a list of games with map filename -> ifid
 * Save list of games to Games.plist
 *
 * TODO: babel-get from ifarchive
 *
 */

#import <CoreData/CoreData.h>

#import "Game.h"
#import "Image+CoreDataProperties.h"
#import "Metadata+CoreDataProperties.h"
#import "Settings.h"
#import "MySideInfoView.h"
#import "NSString+XML.h"
#import "NSDate+relative.h"

@class MySideInfoView;

@interface LibHelperWindow : NSWindow<NSDraggingDestination>
@end

@interface LibHelperTableView : NSTableView
@end

@interface LibController : NSWindowController <NSDraggingDestination, NSWindowDelegate, NSControlTextEditingDelegate, NSSplitViewDelegate>
{
    NSURL *homepath;
    
    IBOutlet NSButton *infoButton;
    IBOutlet NSButton *playButton;
    IBOutlet NSPanel *importProgressPanel;
    IBOutlet NSView *exportTypeView;
    IBOutlet NSPopUpButton *exportTypeControl;
    
    //NSMutableDictionary *metadata; /* ifid -> metadata dict */
    //NSMutableDictionary *games; /* ifid -> filename */

    IBOutlet NSTableView *gameTableView;
    NSMutableArray *gameTableModel;
    NSString *gameSortColumn;
    BOOL sortAscending;
    BOOL gameTableDirty;

    BOOL canEdit;
    NSTimer * timer;

    NSArray *searchStrings;

    /* for the importing */
    NSInteger cursrc;
    NSString *currentIfid;
    NSMutableArray *ifidbuf;
    NSMutableDictionary *metabuf;
    NSInteger errorflag;

    NSURLSession *defaultSession;
    NSURLSessionDataTask *dataTask;
}

@property (strong) NSPersistentContainer *persistentContainer;
@property (strong) NSManagedObjectContext *managedObjectContext;

- (void) loadLibrary; /* initializer */
- (IBAction) saveLibrary: sender;

- (void) beginImporting;
- (void) endImporting;

- (Game*) importGame: (NSString*)path reportFailure: (BOOL)report;
- (void) addFile: (NSString*)path select: (NSMutableArray*)select;
- (void) addFiles: (NSArray*)paths select: (NSMutableArray*)select;
- (void) addFiles: (NSArray*)paths;
- (void) addFile: (NSString*)path;

- (void) playGame: (Game *)game;
- (void) importAndPlayGame: (NSString*)path;

- (IBAction) addGamesToLibrary: (id)sender;
- (IBAction) download: (id)sender;

- (IBAction) deleteLibrary: (id)sender;

- (IBAction) importMetadata: (id)sender;
- (IBAction) exportMetadata: (id)sender;
- (BOOL) importMetadataFromFile: (NSString*)filename;
- (BOOL) exportMetadataToFile: (NSString*)filename what: (NSInteger)what;

- (IBAction) searchForGames: (id)sender;
- (IBAction) play: (id)sender;
- (IBAction) showGameInfo: (id)sender;
- (IBAction) revealGameInFinder: (id)sender;
- (IBAction) deleteGame: (id)sender;
- (IBAction) openIfdb:(id)sender;

@property (strong) IBOutlet NSMenu *headerMenu;
- (IBAction)toggleColumn:(id)sender;

- (void) deselectGames;
- (void) selectGame: (Game *)game;
- (void) updateTableViews; /* must call this after -importGame: */
- (void) updateSideView;

-(void)enableClickToRenameAfterDelay;

@property (strong) IBOutlet NSView *leftView;
@property (strong) IBOutlet NSSplitView *splitView;

- (IBAction) toggleSidebar:(id)sender;

@property (strong) IBOutlet NSTextField *sideIfid;
@property (strong) IBOutlet NSClipView *sideClipView;
@property (strong) IBOutlet NSScrollView *leftScrollView;

- (NSString*) convertAGTFile: (NSString*)origpath;



@end
