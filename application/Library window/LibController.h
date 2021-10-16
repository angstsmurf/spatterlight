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

#import <Cocoa/Cocoa.h>

@class CoreDataManager, Metadata, GlkController, InfoController, Game;

@interface LibHelperTableView : NSTableView
@end

@interface LibController
    : NSWindowController <NSDraggingDestination, NSWindowDelegate, NSSplitViewDelegate>

@property NSMutableArray *iFictionFiles;

@property NSURL *homepath;
@property NSURL *imageDir;

@property NSMutableArray<Game *> *gameTableModel;

@property BOOL currentlyAddingGames;
@property BOOL nestedDownload;
@property BOOL spinnerSpinning;
@property BOOL sortAscending;
@property NSString *gameSortColumn;

@property (strong) CoreDataManager *coreDataManager;
@property (strong) NSManagedObjectContext *managedObjectContext;

@property NSMutableDictionary <NSString *, InfoController *> *infoWindows;
@property NSMutableDictionary <NSString *, GlkController *> *gameSessions;

@property IBOutlet NSTableView *gameTableView;
@property IBOutlet NSSearchField *searchField;

@property (strong) IBOutlet NSButton *addButton;

@property (strong) IBOutlet NSView *leftView;
@property (strong) IBOutlet NSView *rightView;

@property (strong) IBOutlet NSSplitView *splitView;

@property (strong) IBOutlet NSTextField *sideIfid;
@property (strong) IBOutlet NSScrollView *leftScrollView;

@property (strong) IBOutlet NSTextFieldCell *foundIndicatorCell;
@property (strong) IBOutlet NSProgressIndicator *progressCircle;

@property (strong) IBOutlet NSMenuItem *themesSubMenu;

@property (strong) IBOutlet NSLayoutConstraint *leftViewConstraint;
@property NSArray<Game *> *selectedGames;

- (void)beginImporting;
- (void)endImporting;

- (NSWindow *)playGame:(Game *)game;
- (NSWindow *)playGame:(Game *)game winRestore:(BOOL)systemWindowRestoration;
- (NSWindow *)playGameWithIFID:(NSString *)ifid;
- (void)releaseGlkControllerSoon:(GlkController *)glkctl;

- (NSWindow *)importAndPlayGame:(NSString *)path;
- (Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (BOOL)hasActiveGames;
- (void)runCommandsFromFile:(NSString *)filename;
- (void)restoreFromSaveFile:(NSString *)filename;

- (IBAction)myToggleSidebar:(id)sender;
- (IBAction)toggleColumn:(id)sender;

- (IBAction)addGamesToLibrary:(id)sender;
- (IBAction)deleteLibrary:(id)sender;
- (IBAction)pruneLibrary:(id)sender;
- (IBAction)verifyLibrary:(id)sender;

- (IBAction)importMetadata:(id)sender;
- (IBAction)exportMetadata:(id)sender;
- (BOOL)importMetadataFromFile:(NSString *)filename inContext:(NSManagedObjectContext *)context;
- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what;

- (IBAction)searchForGames:(id)sender;
- (IBAction)play:(id)sender;
- (IBAction)download:(id)sender;
- (IBAction)showGameInfo:(id)sender;
- (IBAction)revealGameInFinder:(id)sender;
- (IBAction)deleteGame:(id)sender;
- (IBAction)selectSameTheme:(id)sender;
- (IBAction)deleteSaves:(id)sender;

- (void)showInfoForGame:(Game *)game;

- (void)selectGames:(NSSet*)games;

- (void)updateTableViews; /* must call this after -importGame: */
- (void)updateSideViewForce:(BOOL)force;

- (void)enableClickToRenameAfterDelay;

- (NSRect)rectForLineWithIfid:(NSString*)ifid;
- (void)closeAndOpenNextAbove:(InfoController *)infocontroller;
- (void)closeAndOpenNextBelow:(InfoController *)infocontroller;

- (Game *)fetchGameForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
- (Metadata *)fetchMetadataForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
- (Metadata *)importMetadataFromXML:(NSData *)mdbuf inContext:(NSManagedObjectContext *)context;

- (void)waitToReportMetadataImport;

- (void)handleSpotlightSearchResult:(id)object;

@property (strong) IBOutlet NSView *forceQuitView;
@property (weak) IBOutlet NSButton *forceQuitCheckBox;

@property (strong) IBOutlet NSView *downloadCheckboxView;
@property (weak) IBOutlet NSButton *lookForCoverImagesCheckBox;
@property (weak) IBOutlet NSButton *downloadGameInfoCheckBox;

@property (strong) IBOutlet NSView *verifyView;
@property (weak) IBOutlet NSButton *verifyReCheckbox;
@property (weak) IBOutlet NSTextField *verifyFrequencyTextField;
@property (weak) IBOutlet NSButton *verifyDeleteMissingCheckbox;

- (void)startVerifyTimer;
- (void)stopVerifyTimer;

@end
