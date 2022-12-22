/*
 * Library with metadata --
 *
 * Keep an archive of game metadata.
 * Import iFiction format from files or babel software.
 * Tag user-edited entries for export.
 * 
 */

#import <Cocoa/Cocoa.h>

@class CoreDataManager, Metadata, GlkController, InfoController, Game, Theme;

@interface RatingsCellView : NSTableCellView
@property (strong) IBOutlet NSLevelIndicator *rating;
@end

@interface ForgivenessCellView : NSTableCellView
@property (strong) IBOutlet NSPopUpButton *popUpButton;
@end

@interface LikeCellView : NSTableCellView
@property (strong) IBOutlet NSButton *likeButton;
@end

@interface LibHelperTableView : NSTableView
@end

@interface MyIndicator : NSLevelIndicator
@property (weak) IBOutlet LibHelperTableView *tableView;
@end

@interface LibController
: NSWindowController <NSDraggingDestination, NSWindowDelegate, NSSplitViewDelegate, NSTableViewDelegate, NSTableViewDataSource>

@property NSURL *homepath;
@property NSURL *imageDir;

@property NSMutableArray<Game *> *gameTableModel;

@property BOOL currentlyAddingGames;
@property BOOL nestedDownload;
@property BOOL spinnerSpinning;
@property BOOL downloadWasCancelled;
@property BOOL sortAscending;
@property BOOL gameTableDirty;
@property NSString *gameSortColumn;

@property (strong) CoreDataManager *coreDataManager;
@property (strong) NSManagedObjectContext *managedObjectContext;

@property NSMutableDictionary <NSString *, InfoController *> *infoWindows;
@property NSMutableDictionary <NSString *, GlkController *> *gameSessions;

@property IBOutlet LibHelperTableView *gameTableView;
@property IBOutlet NSSearchField *searchField;

@property (strong) IBOutlet NSButton *addButton;

@property (strong) IBOutlet NSView *leftView;
@property (strong) IBOutlet NSView *rightView;

@property (strong) IBOutlet NSSplitView *splitView;

@property (strong) IBOutlet NSTextField *sideIfid;
@property (strong) IBOutlet NSScrollView *leftScrollView;

@property (strong) IBOutlet NSTextFieldCell *foundIndicatorCell;
@property (strong) IBOutlet NSProgressIndicator *progressCircle;
@property NSProgressIndicator *spinner;

@property (weak) IBOutlet NSMenuItem *themesSubMenu;
@property (weak) NSMenuItem *mainThemesSubMenu;

@property (strong) IBOutlet NSLayoutConstraint *leftViewConstraint;
@property NSArray<Game *> *selectedGames;

@property (readonly) NSOperationQueue *downloadQueue;
@property (readonly) NSOperationQueue *alertQueue;
@property NSData *lastImageComparisonData;

@property NSInteger undoGroupingCount;

@property (weak) Game *currentSideView;

- (void)clearSideView;


- (void)beginImporting;
- (void)endImporting;

- (NSWindow *)playGame:(Game *)game;
- (NSWindow *)playGame:(Game *)game winRestore:(BOOL)systemWindowRestoration;
- (NSWindow *)playGameWithIFID:(NSString *)ifid;
- (void)releaseGlkControllerSoon:(GlkController *)glkctl;

- (NSWindow *)importAndPlayGame:(NSString *)path;
- (Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL hasActiveGames;
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
- (IBAction)openIfdb:(id)sender;
- (IBAction)applyTheme:(id)sender;

- (void)downloadMetadataForGames:(NSArray<Game *> *)games;

- (void)showInfoForGame:(Game *)game toggle:(BOOL)toggle;

- (void)selectGames:(NSSet*)games;
- (void)selectGamesWithIfids:(NSArray*)ifids scroll:(BOOL)shouldscroll;

- (void)updateTableViews; /* must call this after -importGame: */
- (void)updateSideViewForce:(BOOL)force;

- (void)enableClickToRenameAfterDelay;

- (NSRect)rectForLineWithIfid:(NSString*)ifid;
- (void)closeAndOpenNextAbove:(InfoController *)infocontroller;
- (void)closeAndOpenNextBelow:(InfoController *)infocontroller;

+ (Game *)fetchGameForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
+ (Metadata *)fetchMetadataForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
- (Metadata *)importMetadataFromXML:(NSData *)mdbuf inContext:(NSManagedObjectContext *)context;
+ (void)fixMetadataWithNoIfidsInContext:(NSManagedObjectContext *)context;
- (void)waitToReportMetadataImport;

+ (Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context;

- (void)handleSpotlightSearchResult:(id)object;

- (void)mouseOverChangedFromRow:(NSInteger)lastRow toRow:(NSInteger)currentRow;

@property (strong) IBOutlet NSView *forceQuitView;
@property (weak) IBOutlet NSButton *forceQuitCheckBox;

@property (strong) IBOutlet NSView *downloadCheckboxView;
@property (weak) IBOutlet NSButton *lookForCoverImagesCheckBox;
@property (weak) IBOutlet NSButton *downloadGameInfoCheckBox;

@property (strong) IBOutlet NSView *verifyView;
@property (weak) IBOutlet NSButton *verifyReCheckbox;
@property (weak) IBOutlet NSTextField *verifyFrequencyTextField;
@property (weak) IBOutlet NSButton *verifyDeleteMissingCheckbox;

- (IBAction)like:(id)sender;
- (IBAction)dislike:(id)sender;

- (void)startVerifyTimer;
- (void)stopVerifyTimer;

@end
