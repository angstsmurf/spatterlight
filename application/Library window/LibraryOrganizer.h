//
//  LibraryOrganizer.h
//  Spatterlight
//
//  Keeps games organised in a central library folder, in the manner of
//  Zoom's ZoomStoryOrganiser (and of the "keep media organised" option in
//  Music/iTunes). When enabled, imported games are copied into
//
//      <library root>/<group>/<title>/<game file>
//
//  and the game's stored bookmark is re-pointed at the copy. Each game folder
//  carries a hidden .spatterlightIdentity marker naming the owning game so we
//  never clobber an unrelated folder.
//

#import <Foundation/Foundation.h>

@class Game;
@class NSManagedObjectContext;

NS_ASSUME_NONNULL_BEGIN

/// `BOOL`: master switch for the feature. Defaults to NO.
extern NSString * const kKeepGamesOrganisedKey;
/// `NSData`: security-scoped bookmark for a user-chosen library folder.
/// Absent means the default location inside the app container is used.
extern NSString * const kOrganiseDirBookmarkKey;

@interface LibraryOrganizer : NSObject

+ (instancetype)sharedOrganizer;

/// Convenience reader for the `KeepGamesOrganised` user default.
+ (BOOL)keepGamesOrganised;

/// The default library location: a "Library" folder inside Spatterlight's own
/// Application Support container. Always writable, never needs a sandbox grant.
@property (readonly) NSURL *defaultLibraryRootURL;

/// The resolved custom library folder, or nil when the default is in use.
- (nullable NSURL *)customLibraryURL;

/// The library root actually in effect (custom folder if configured, else the
/// default container location). Optionally creates it. Returns nil on failure.
- (nullable NSURL *)libraryRootURLCreatingIfNeeded:(BOOL)create;

/// Store (or clear, when `url` is nil) a custom library location. The location
/// is kept as a security-scoped bookmark.
- (BOOL)setCustomLibraryURL:(nullable NSURL *)url error:(NSError **)error;

/// Copy or move a single game into the library folder, re-pointing its
/// bookmark at the new location. Must be called on `game`'s context queue.
/// The caller must already hold security-scoped access to the game's source
/// folder (the importer does; `organiseAllGamesInContext:` handles it itself).
/// Returns YES if the game now lives inside the library.
- (BOOL)organiseGame:(Game *)game error:(NSError **)error;

/// Re-run organisation for a game whose title/group just changed. Does nothing
/// unless the game currently lives inside the library folder.
- (void)reorganiseGame:(Game *)game;

/// Organise every non-hidden game. Runs asynchronously on `context`'s queue,
/// granting source access per game, saving as it goes. `completion` is called
/// on the main queue with the number of games moved into the library.
- (void)organiseAllGamesInContext:(NSManagedObjectContext *)context
                       completion:(nullable void (^)(NSUInteger organisedCount))completion;

@end

NS_ASSUME_NONNULL_END
