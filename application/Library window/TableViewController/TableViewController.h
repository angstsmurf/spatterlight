//
//  TableViewController.h
//  Spatterlight
//
//  Created by Administrator on 2023-01-16.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class Metadata, GlkController, InfoController, Game, Theme, SplitViewController, LibController, CoreDataManager, IFStory, MetadataHandler, GameLauncher, LibHelperTableView;

@interface TableViewController : NSViewController <NSDraggingDestination, NSSplitViewDelegate, NSTableViewDelegate, NSTableViewDataSource>

@property (nonatomic) LibController *windowController;

@property SplitViewController *splitViewController;

@property NSURL *homepath;

@property NSMutableArray<Game *> *gameTableModel;

@property BOOL currentlyAddingGames;
@property BOOL spinnerSpinning;
@property BOOL downloadWasCancelled;
@property BOOL sortAscending;
@property BOOL gameTableDirty;
@property NSString *gameSortColumn;

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *managedObjectContext;

@property NSMutableDictionary<NSString *, InfoController *> *infoWindows;
@property NSMutableDictionary<NSString *, GlkController *> *gameSessions;

@property (weak) IBOutlet NSMenuItem *themesSubMenu;
@property (nonatomic, weak) NSMenuItem *mainThemesSubMenu;

@property NSArray<Game *> *selectedGames;

@property (readonly) NSOperationQueue *downloadQueue;
@property (readonly) NSOperationQueue *alertQueue;
@property (readonly) NSOperationQueue *openGameQueue;
@property (nullable) NSData *lastImageComparisonData;
@property (weak) NSOperation *lastImageDownloadOperation;

@property NSInteger undoGroupingCount;

@property NSMutableDictionary<NSString *, IFStory *> *ifictionMatches;
@property NSMutableDictionary<NSString *, IFStory *> *ifictionPartialMatches;

// New helper objects
@property (strong) MetadataHandler *metadataHandler;
@property (strong) GameLauncher *gameLauncher;

// Methods that remain on the main class for backward compat / forwarding
- (void)beginImporting;
- (void)endImporting;

// Forwarding methods to GameLauncher
- (void)releaseGlkControllerSoon:(GlkController *)glkctl;

// Forwarding methods to MetadataHandler
- (void)askAboutImportingMetadata:(NSDictionary<NSString *, IFStory *> *)storyDict indirectMatches:(NSDictionary<NSString *, IFStory *> *)indirectDict;
- (void)downloadMetadataForGames:(NSArray<Game *> *)games;

// Forwarding IBActions to MetadataHandler
- (IBAction)importMetadata:(id)sender;
- (IBAction)exportMetadata:(id)sender;

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
