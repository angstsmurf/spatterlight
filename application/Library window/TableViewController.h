//
//  TableViewController.h
//  Spatterlight
//
//  Created by Administrator on 2023-01-16.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class Metadata, GlkController, InfoController, Game, Theme, SplitViewController, LibController, CoreDataManager;

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
@property (weak) LibHelperTableView *tableView;
@end

@interface CellViewWithConstraint : NSTableCellView
@property (strong) IBOutlet NSLayoutConstraint *topConstraint;
@end


@interface TableViewController : NSViewController <NSDraggingDestination, NSSplitViewDelegate, NSTableViewDelegate, NSTableViewDataSource>

@property (nonatomic) LibController *windowController;

@property SplitViewController *splitViewController;

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

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *managedObjectContext;

@property NSMutableDictionary <NSString *, InfoController *> *infoWindows;
@property NSMutableDictionary <NSString *, GlkController *> *gameSessions;

@property (weak) IBOutlet NSMenuItem *themesSubMenu;
@property (nonatomic, weak) NSMenuItem *mainThemesSubMenu;

@property NSArray<Game *> *selectedGames;

@property (readonly) NSOperationQueue *downloadQueue;
@property (readonly) NSOperationQueue *alertQueue;
@property (readonly) NSOperationQueue *openGameQueue;
@property (nullable) NSData *lastImageComparisonData;

@property NSInteger undoGroupingCount;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL hasActiveGames;

- (void)askToDownload;

- (void)beginImporting;
- (void)endImporting;

- (nullable NSWindow *)playGame:(Game *)game;
- (nullable NSWindow *)playGame:(Game *)game winRestore:(BOOL)systemWindowRestoration;
- (nullable NSWindow *)playGameWithHash:(NSString *)ifid;
- (void)releaseGlkControllerSoon:(GlkController *)glkctl;
- (void)releaseInfoController:(InfoController *)infoctl;

- (nullable NSWindow *)importAndPlayGame:(NSString *)path;
- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (void)runCommandsFromFile:(NSString *)filename;
- (void)restoreFromSaveFile:(NSString *)filename;

- (IBAction)toggleColumn:(id)sender;

- (IBAction)addGamesToLibrary:(id)sender;
- (IBAction)deleteLibrary:(id)sender;
- (IBAction)pruneLibrary:(id)sender;
- (IBAction)verifyLibrary:(id)sender;

- (IBAction)importMetadata:(id)sender;
- (IBAction)exportMetadata:(id)sender;
- (BOOL)importMetadataFromFile:(NSString *)filename inContext:(NSManagedObjectContext *)context;
- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what;

- (IBAction)searchForGames:(nullable id)sender;
- (IBAction)play:(id)sender;
- (IBAction)download:(id)sender;
- (IBAction)showGameInfo:(nullable id)sender;
- (IBAction)revealGameInFinder:(id)sender;
- (IBAction)deleteGame:(id)sender;
- (IBAction)selectSameTheme:(id)sender;
- (IBAction)deleteSaves:(id)sender;
- (IBAction)openIfdb:(id)sender;
- (IBAction)applyTheme:(id)sender;

- (void)downloadMetadataForGames:(NSArray<Game *> *)games;

- (void)showInfoForGame:(Game *)game toggle:(BOOL)toggle;

- (void)selectGames:(NSSet*)games;
- (void)selectGamesWithHashes:(NSArray*)ifids scroll:(BOOL)shouldscroll;

- (void)updateTableViews; /* must call this after -importGame: */

- (void)enableClickToRenameAfterDelay;

- (NSRect)rectForLineWithHash:(NSString*)ifid;
- (void)closeAndOpenNextAbove:(InfoController *)infocontroller;
- (void)closeAndOpenNextBelow:(InfoController *)infocontroller;

+ (nullable Game *)fetchGameForHash:(NSString *)ifid inContext:(NSManagedObjectContext *)context;
+ (nullable Metadata *)fetchMetadataForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context;
- (nullable Metadata *)importMetadataFromXML:(NSData *)mdbuf inContext:(NSManagedObjectContext *)context;
- (void)waitToReportMetadataImport;

+ (nullable Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context;

- (void)handleSpotlightSearchResult:(id)object;

- (void)mouseOverChangedFromRow:(NSInteger)lastRow toRow:(NSInteger)currentRow;


- (IBAction)like:(id)sender;
- (IBAction)dislike:(id)sender;

- (void)startVerifyTimer;
- (void)stopVerifyTimer;

@property (strong) IBOutlet NSView *forceQuitView;
@property (strong) IBOutlet NSButton *forceQuitCheckBox;
@property (strong) IBOutlet NSView *addToLibraryView;
@property (strong) IBOutlet NSButton *addToLibraryCheckBox;

@property (strong) IBOutlet NSView *downloadCheckboxView;
@property (weak) IBOutlet NSButton *lookForCoverImagesCheckBox;
@property (weak) IBOutlet NSButton *downloadGameInfoCheckBox;

@property (strong) IBOutlet NSView *verifyView;
@property (weak) IBOutlet NSButton *verifyReCheckbox;
@property (weak) IBOutlet NSTextField *verifyFrequencyTextField;
@property (weak) IBOutlet NSButton *verifyDeleteMissingCheckbox;


@property (weak) IBOutlet LibHelperTableView *gameTableView;
@property (weak) IBOutlet NSMenu *headerMenu;

@property (strong) IBOutlet NSView *exportTypeView;
@property (weak) IBOutlet NSPopUpButton *exportTypeControl;

@end

NS_ASSUME_NONNULL_END
