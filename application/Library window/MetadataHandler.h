//
//  MetadataHandler.h
//  Spatterlight
//
//  Extracted from TableViewController.m
//  Handles metadata import, export, XML parsing, and IFDB downloads.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class Game, Metadata, IFStory, TableViewController, CoreDataManager;

@interface MetadataHandler : NSObject

@property (weak) TableViewController *tableViewController;

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController;

// Import / Export UI actions
- (void)importMetadataInContext:(NSManagedObjectContext *)ctx window:(NSWindow *)window;
- (void)exportMetadataInContext:(NSManagedObjectContext *)ctx window:(NSWindow *)window accessoryView:(NSView *)accView popUpButton:(NSPopUpButton *)exportTypeControl;

// Import from file
- (void)importMetadataFromFile:(NSString *)filename inContext:(NSManagedObjectContext *)context;

// Export to file
- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what context:(NSManagedObjectContext *)context;

// XML parsing (class method kept for external callers)
+ (nonnull NSDictionary<NSString *, IFStory *> *)importMetadataFromXML:(NSData *)mdbuf indirectMatches:(NSDictionary<NSString *, IFStory *> * _Nullable __autoreleasing *_Nullable)indirectDict inContext:(NSManagedObjectContext *)context;

// Metadata import dialog
- (void)askAboutImportingMetadata:(NSDictionary<NSString *, IFStory *> *)storyDict indirectMatches:(NSDictionary<NSString *, IFStory *> *)indirectDict;

// Download from IFDB
- (void)askToDownloadInWindow:(NSWindow *)win context:(NSManagedObjectContext *)ctx;

- (void)downloadMetadataForGames:(NSArray<Game *> *)games;

// Spotlight
- (void)handleSpotlightSearchResult:(id)object;

@end

NS_ASSUME_NONNULL_END
