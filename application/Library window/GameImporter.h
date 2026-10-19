//
//  GameImporter.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-27.
//

#import <Foundation/Foundation.h>

@class Game, TableViewController, Blorb;
@class NSManagedObjectContext;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, BabelIdentifyResult) {
    kBabelIdentified,
    kBabelUnknownFormat,
    kBabelNoIfid,
};

/// Shows a modal alert about a file that could not be imported or identified.
/// Can be called from any thread; off the main thread the alert is shown
/// asynchronously.
void ReportImportFailure(NSString *title, NSString *info);

@interface GameImporter : NSObject

/// Asks babel for the format (e.g. "zcode", "glulx") and IFID of the file at
/// `path`. A file with several IFIDs gives the first one. `format` is set
/// whenever babel recognised the file, even if no IFID could be computed.
+ (BabelIdentifyResult)identifyFile:(NSString *)path format:(NSString * _Nullable * _Nullable)format ifid:(NSString * _Nullable * _Nullable)ifid;

/// The IFID of an AGT game, computed from its .D$$ file set without
/// converting it to AGX. nil if the files are not a readable AGT game.
+ (nullable NSString *)identifyAGTFile:(NSString *)path;

- (instancetype)initWithTableViewController:(TableViewController *)libController;

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide;

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options;
- (void)updateImageFromBlorb:(Blorb *)blorb inGame:(Game *)game;
- (void)lookForImagesForGame:(Game *)game;

@property (weak) TableViewController *tableViewController;


@end

// NSUserDefaults keys marking the one-time migrations below as done
extern NSString *const kAGTConvertedToDirectMigrationDone;
extern NSString *const kAGTMigratedHashRepairDone;
extern NSString *const kOutdatedIfidMigrationDone;

@interface GameImporter (Migrations)

// One-time migration: re-point AGT games that were imported the old way (stored
// as a converted Converted/<IFID>.agx in the app container) at their original
// .D$$ file, when it can be located in a folder we still have access to. Games
// whose original .D$$ can't be found are left on the AGX path (still playable).
- (void)migrateConvertedAGTGamesInContext:(NSManagedObjectContext *)context;

// One-time repair: the migration above used to leave a re-pointed game with
// the hash of its deleted .agx. Gives every AGT game played from a .D$$ the
// signature of that file.
- (void)repairHashesOfMigratedAGTGamesInContext:(NSManagedObjectContext *)context;

// One-time migration: bring the stored IFIDs of Quest, Z-code and Glulx games
// in line with what babel now computes for them (QUEST-<MD5> where it used to
// be the bare MD5; a validated, capitalised UUID or the header-based IFID where
// the UUID:// marker used to be copied as found). Updates the game, its
// metadata's IFID record and the organised folder's identity marker.
- (void)migrateOutdatedIfidsInContext:(NSManagedObjectContext *)context;

@end

NS_ASSUME_NONNULL_END
