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

#import <CoreData/CoreData.h>

#import "Game.h"
#import "Image.h"
#import "Metadata.h"
#import "Settings.h"
#import "SideInfoView.h"
#import "NSString+Categories.h"
#import "NSDate+relative.h"
#import "CoreDataManager.h"


@interface LibHelperWindow : NSWindow <NSDraggingDestination>
@end

@interface LibHelperTableView : NSTableView
@end

@class InfoController;

@interface LibController
    : NSWindowController <NSDraggingDestination, NSWindowDelegate, NSURLConnectionDelegate, NSSplitViewDelegate> {
    NSURL *homepath;

    IBOutlet NSButton *infoButton;
    IBOutlet NSButton *playButton;
    IBOutlet NSPanel *importProgressPanel;
    IBOutlet NSView *exportTypeView;
    IBOutlet NSPopUpButton *exportTypeControl;

    IBOutlet NSMenu *headerMenu;

    NSMutableArray *gameTableModel;
    NSArray *selectedGames;
    NSString *gameSortColumn;
    BOOL gameTableDirty;
    BOOL sortAscending;

    BOOL canEdit;
    NSTimer *timer;

    NSArray *searchStrings;
    CGFloat lastSideviewWidth;

    Metadata *noneSelected;
    Metadata *manySelected;
    Metadata *currentSideView;

    BOOL currentlyAddingGames;
    BOOL currentlyImportingMetadata;
    BOOL currentlyDownloadingMedata;

    /* for the importing */
    NSInteger cursrc;
    NSString *currentIfid;
    NSMutableArray *ifidbuf;
    NSMutableDictionary *metabuf;
    NSInteger errorflag;

    NSManagedObjectContext *importContext;
}

@property (strong) CoreDataManager *coreDataManager;
@property (strong) NSManagedObjectContext *managedObjectContext;

@property NSMutableDictionary *infoWindows;
@property NSMutableDictionary *gameSessions;

@property IBOutlet NSTableView *gameTableView;
@property IBOutlet NSSearchField *searchField;

@property (strong) IBOutlet NSButton *addButton;

- (void)beginImporting;
- (void)endImporting;

- (NSWindow *)playGame:(Game *)game;
- (NSWindow *)playGame:(Game *)game winRestore:(BOOL)restoreflag;
- (NSWindow *)playGameWithIFID:(NSString *)ifid;

- (void)importAndPlayGame:(NSString *)path;

- (IBAction)addGamesToLibrary:(id)sender;
- (IBAction)deleteLibrary:(id)sender;

- (IBAction)importMetadata:(id)sender;
- (IBAction)exportMetadata:(id)sender;
- (BOOL)importMetadataFromFile:(NSString *)filename;
- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what;

- (IBAction)searchForGames:(id)sender;
- (IBAction)play:(id)sender;
- (IBAction)showGameInfo:(id)sender;
- (IBAction)revealGameInFinder:(id)sender;
- (IBAction)deleteGame:(id)sender;

- (void)showInfoForGame:(Game *)game;

- (IBAction)toggleColumn:(id)sender;
- (void)deselectGames;
- (void)updateTableViews; /* must call this after -importGame: */
- (void)updateSideViewForce:(BOOL)force;

- (void)enableClickToRenameAfterDelay;

@property (strong) IBOutlet NSView *leftView;
@property (strong) IBOutlet NSView *rightView;

@property (strong) IBOutlet NSSplitView *splitView;

- (IBAction) toggleSidebar:(id)sender;

@property (strong) IBOutlet NSTextField *sideIfid;
@property (strong) IBOutlet NSScrollView *leftScrollView;

@property (strong) IBOutlet NSProgressIndicator *progressCircle;

@property (strong) IBOutlet NSTextFieldCell *foundIndicatorCell;

- (NSString *)convertAGTFile:(NSString *)origpath;

@end
