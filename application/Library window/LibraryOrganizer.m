//
//  LibraryOrganizer.m
//  Spatterlight
//

#import "LibraryOrganizer.h"

#import "Game.h"
#import "Metadata.h"
#import "Fetches.h"
#import "FolderAccess.h"

NSString * const kKeepGamesOrganisedKey = @"KeepGamesOrganised";
NSString * const kOrganiseDirBookmarkKey = @"OrganiseDirBookmark";

// Marker file dropped in every game folder, naming the owning game so we never
// move a game into, or clobber, a folder that belongs to a different game.
static NSString * const kIdentityFilename = @".spatterlightIdentity";

// How many "Title", "Title 2", "Title 3"… variants to try before giving up on
// finding a free game folder in a group.
static const NSInteger kMaxDuplicateDirs = 20;

@interface LibraryOrganizer ()
// Kept alive so a custom (security-scoped) library folder stays accessible for
// the life of the app once resolved.
@property (nonatomic) NSURL *accessedCustomURL;
@end

@implementation LibraryOrganizer

+ (instancetype)sharedOrganizer {
    static LibraryOrganizer *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[LibraryOrganizer alloc] init];
    });
    return shared;
}

+ (BOOL)keepGamesOrganised {
    return [NSUserDefaults.standardUserDefaults boolForKey:kKeepGamesOrganisedKey];
}

#pragma mark - Library location

- (NSURL *)defaultLibraryRootURL {
    NSURL *appSupport =
    [NSFileManager.defaultManager URLForDirectory:NSApplicationSupportDirectory
                                         inDomain:NSUserDomainMask
                                appropriateForURL:nil
                                           create:YES
                                            error:NULL];
    // Mirrors TableViewController.homepath (<App Support>/Spatterlight).
    NSURL *home = [appSupport URLByAppendingPathComponent:@"Spatterlight" isDirectory:YES];
    return [home URLByAppendingPathComponent:@"Library" isDirectory:YES];
}

- (nullable NSURL *)customLibraryURL {
    NSData *bookmark = [NSUserDefaults.standardUserDefaults dataForKey:kOrganiseDirBookmarkKey];
    if (!bookmark)
        return nil;

    // Reuse the already-accessed URL if we resolved this bookmark before.
    if (self.accessedCustomURL)
        return self.accessedCustomURL;

    BOOL stale = NO;
    NSError *error = nil;
    NSURL *url = [NSURL URLByResolvingBookmarkData:bookmark
                                          options:NSURLBookmarkResolutionWithSecurityScope
                                    relativeToURL:nil
                              bookmarkDataIsStale:&stale
                                            error:&error];
    if (!url) {
        NSLog(@"LibraryOrganizer: could not resolve custom library bookmark: %@", error);
        return nil;
    }
    if (![url startAccessingSecurityScopedResource]) {
        NSLog(@"LibraryOrganizer: could not access custom library folder %@", url.path);
        return nil;
    }
    if (stale) {
        NSData *fresh = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                      includingResourceValuesForKeys:nil
                                       relativeToURL:nil
                                               error:NULL];
        if (fresh)
            [NSUserDefaults.standardUserDefaults setObject:fresh forKey:kOrganiseDirBookmarkKey];
    }
    self.accessedCustomURL = url;
    return url;
}

- (nullable NSURL *)libraryRootURLCreatingIfNeeded:(BOOL)create {
    NSURL *root = self.customLibraryURL ?: self.defaultLibraryRootURL;
    if (create) {
        NSError *error = nil;
        if (![NSFileManager.defaultManager createDirectoryAtURL:root
                                    withIntermediateDirectories:YES
                                                     attributes:nil
                                                          error:&error] &&
            ![root checkResourceIsReachableAndReturnError:NULL]) {
            NSLog(@"LibraryOrganizer: could not create library root %@: %@", root.path, error);
            return nil;
        }
    }
    return root;
}

- (BOOL)setCustomLibraryURL:(nullable NSURL *)url error:(NSError **)error {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;

    // Drop any access we hold to a previous custom folder.
    if (self.accessedCustomURL) {
        [self.accessedCustomURL stopAccessingSecurityScopedResource];
        self.accessedCustomURL = nil;
    }

    if (!url) {
        [defaults removeObjectForKey:kOrganiseDirBookmarkKey];
        return YES;
    }

    NSData *bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                     includingResourceValuesForKeys:nil
                                      relativeToURL:nil
                                              error:error];
    if (!bookmark)
        return NO;

    [defaults setObject:bookmark forKey:kOrganiseDirBookmarkKey];
    return YES;
}

#pragma mark - Name and path helpers

// Removes characters that are illegal or ugly in a folder name. '/' becomes ':'
// (which the Finder displays as '/'), ':' becomes '_', control characters
// become '?'. Ported from ZoomStoryOrganiser -sanitiseDirectoryName:.
- (NSString *)sanitiseName:(nullable NSString *)name fallback:(NSString *)fallback {
    NSString *trimmed = [name stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (!trimmed.length)
        return fallback;

    NSMutableString *result = [NSMutableString stringWithCapacity:trimmed.length];
    [trimmed enumerateSubstringsInRange:NSMakeRange(0, trimmed.length)
                               options:NSStringEnumerationByComposedCharacterSequences
                            usingBlock:^(NSString *sub, NSRange r1, NSRange r2, BOOL *stop) {
        unichar c = [sub characterAtIndex:0];
        if ([sub isEqualToString:@"/"])
            [result appendString:@":"];
        else if ([sub isEqualToString:@":"])
            [result appendString:@"_"];
        else if (c < ' ')
            [result appendString:@"?"];
        else
            [result appendString:sub];
    }];
    return result.length ? result : fallback;
}

- (NSString *)groupNameForGame:(Game *)game {
    NSString *group = game.group;
    if (!group.length)
        group = game.metadata.group;
    if (group.length)
        return [self sanitiseName:group fallback:@"Ungrouped"];
    // No group set: file the game under a folder named after its format.
    return [self formatGroupNameForGame:game];
}

// The group-folder name for a game with no group of its own: the game's format
// spelled out as a proper label with " games" appended, e.g. "Z-code games" or
// "Quest 5 games". Falls back to "Ungrouped" when the format is unknown.
- (NSString *)formatGroupNameForGame:(Game *)game {
    // Maps the babel format codes stored in Game.detectedFormat to the labels
    // players see, for the formats where the code isn't already a nice name.
    static NSDictionary<NSString *, NSString *> *labels = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        labels = @{
            @"adrift"     : @"Adrift",
            @"advsys"     : @"AdvSys",
            @"agt"        : @"AGT",
            @"alan2"      : @"Alan 2",
            @"alan3"      : @"Alan 3",
            @"archetype"  : @"Archetype",
            @"comprehend" : @"Comprehend",
            @"glulx"      : @"Glulx",
            @"hugo"       : @"Hugo",
            @"jacl"       : @"JACL",
            @"level9"     : @"Level 9",
            @"magscrolls" : @"Magnetic Scrolls",
            @"quest4"     : @"Quest 4",
            @"quest5"     : @"Quest 5",
            @"quill"      : @"Quill",
            @"sagaplus"   : @"SAGA Plus",
            @"scott"      : @"Scott Adams",
            @"tads2"      : @"TADS 2",
            @"tads3"      : @"TADS 3",
            @"taylor"     : @"Taylor",
            @"twine"      : @"Twine",
            @"zcode"      : @"Z-code",
        };
    });

    NSString *format = game.detectedFormat.length ? game.detectedFormat : game.metadata.format;
    if (!format.length)
        return @"Ungrouped";
    // Use the pretty label when we have one, otherwise the raw code capitalised.
    NSString *label = labels[format] ?: format.capitalizedString;
    return [self sanitiseName:[label stringByAppendingString:@" games"] fallback:@"Ungrouped"];
}

- (NSString *)titleNameForGame:(Game *)game {
    NSString *title = game.metadata.title;
    if (!title.length)
        title = game.path.lastPathComponent.stringByDeletingPathExtension;
    return [self sanitiseName:title fallback:@"Untitled"];
}

// The identity string written into a game folder's marker file.
- (nullable NSString *)identityForGame:(Game *)game {
    if (game.ifid.length)
        return game.ifid;
    if (game.hashTag.length)
        return game.hashTag;
    return nil;
}

// YES if `dir` is free to use for `game`: it either does not exist yet, or it
// exists and its identity marker names this same game.
- (BOOL)directory:(NSURL *)dir isAvailableForGame:(Game *)game {
    NSFileManager *fm = NSFileManager.defaultManager;
    BOOL isDir = NO;
    if (![fm fileExistsAtPath:dir.path isDirectory:&isDir])
        return YES;               // does not exist: free to create
    if (!isDir)
        return NO;                // a file is in the way

    NSURL *idFile = [dir URLByAppendingPathComponent:kIdentityFilename];
    NSString *stored = [NSString stringWithContentsOfURL:idFile
                                               encoding:NSUTF8StringEncoding
                                                  error:NULL];
    if (!stored.length)
        return NO;                // someone else's (or an unmarked) folder
    NSString *mine = [self identityForGame:game];
    return mine.length && [stored isEqualToString:mine];
}

- (void)writeIdentityForGame:(Game *)game inDirectory:(NSURL *)dir {
    NSString *identity = [self identityForGame:game];
    if (!identity.length)
        return;
    NSURL *idFile = [dir URLByAppendingPathComponent:kIdentityFilename];
    [identity writeToURL:idFile atomically:YES encoding:NSUTF8StringEncoding error:NULL];
}

// Finds (and, if `create`, creates) the game folder for `game` under `root`,
// i.e. <root>/<group>/<title>, disambiguating with "Title 2" etc. when a
// different game already owns the plain name.
- (nullable NSURL *)gameDirectoryForGame:(Game *)game
                               underRoot:(NSURL *)root
                                  create:(BOOL)create {
    NSFileManager *fm = NSFileManager.defaultManager;

    NSString *group = [self groupNameForGame:game];
    NSString *title = [self titleNameForGame:game];

    NSURL *groupDir = [root URLByAppendingPathComponent:group isDirectory:YES];
    if (create && ![fm createDirectoryAtURL:groupDir
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:NULL] &&
        ![groupDir checkResourceIsReachableAndReturnError:NULL]) {
        return nil;
    }

    for (NSInteger n = 0; n < kMaxDuplicateDirs; n++) {
        NSString *name = n == 0 ? title : [NSString stringWithFormat:@"%@ %ld", title, (long)n];
        NSURL *candidate = [groupDir URLByAppendingPathComponent:name isDirectory:YES];
        if ([self directory:candidate isAvailableForGame:game]) {
            if (create && ![candidate checkResourceIsReachableAndReturnError:NULL]) {
                if (![fm createDirectoryAtURL:candidate
                  withIntermediateDirectories:YES
                                   attributes:nil
                                        error:NULL] &&
                    ![candidate checkResourceIsReachableAndReturnError:NULL]) {
                    return nil;
                }
            }
            return candidate;
        }
    }
    return nil;
}

- (BOOL)url:(NSURL *)url isInsideRoot:(NSURL *)root {
    NSString *rootPath = root.URLByStandardizingPath.path;
    if (![rootPath hasSuffix:@"/"])
        rootPath = [rootPath stringByAppendingString:@"/"];
    NSString *path = url.URLByStandardizingPath.path;
    return [path.lowercaseString hasPrefix:rootPath.lowercaseString];
}

#pragma mark - Organising a single game

- (BOOL)organiseGame:(Game *)game error:(NSError **)outError {
    NSFileManager *fm = NSFileManager.defaultManager;

    NSString *sourcePath = game.path;
    if (!sourcePath.length || ![fm fileExistsAtPath:sourcePath]) {
        // Fall back to resolving the stored bookmark.
        sourcePath = game.urlForBookmark.path;
    }
    if (!sourcePath.length || ![fm fileExistsAtPath:sourcePath]) {
        if (outError)
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain
                                            code:NSFileNoSuchFileError
                                        userInfo:@{ NSLocalizedDescriptionKey:
                                                        @"The game file could not be found." }];
        return NO;
    }

    NSURL *sourceURL = [NSURL fileURLWithPath:sourcePath isDirectory:NO].URLByStandardizingPath;

    NSURL *root = [self libraryRootURLCreatingIfNeeded:YES];
    if (!root) {
        if (outError)
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain
                                            code:NSFileWriteUnknownError
                                        userInfo:@{ NSLocalizedDescriptionKey:
                                                        @"The library folder could not be created." }];
        return NO;
    }

    NSURL *destDir = [self gameDirectoryForGame:game underRoot:root create:YES];
    if (!destDir) {
        if (outError)
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain
                                            code:NSFileWriteUnknownError
                                        userInfo:@{ NSLocalizedDescriptionKey:
                                                        @"No free folder for this game in the library." }];
        return NO;
    }

    NSURL *destFile = [destDir URLByAppendingPathComponent:sourceURL.lastPathComponent].URLByStandardizingPath;

    // Record the named group folder the game landed in, so a format-derived
    // group like "Z-code games" becomes a real group on the game itself. A game
    // filed under "Ungrouped" (unknown format / no usable name) keeps no group.
    NSString *groupName = [self groupNameForGame:game];
    if (![groupName isEqualToString:@"Ungrouped"] &&
        ![game.group isEqualToString:groupName])
        game.group = groupName;

    [self writeIdentityForGame:game inDirectory:destDir];

    // Already where it belongs.
    if ([destFile.path.lowercaseString isEqualToString:sourceURL.path.lowercaseString])
        return YES;

    NSURL *sourceDir = sourceURL.URLByDeletingLastPathComponent;
    BOOL sourceInsideLibrary = [self url:sourceURL isInsideRoot:root];

    if (sourceInsideLibrary && ![sourceDir.path.lowercaseString isEqualToString:destDir.path.lowercaseString]) {
        // Reorganisation within the library: move the game and its companion
        // files out of the old game folder into the new one.
        NSArray<NSURL *> *contents =
        [fm contentsOfDirectoryAtURL:sourceDir
          includingPropertiesForKeys:nil
                             options:0
                               error:NULL];
        for (NSURL *item in contents) {
            NSURL *target = [destDir URLByAppendingPathComponent:item.lastPathComponent];
            if ([target checkResourceIsReachableAndReturnError:NULL])
                [fm removeItemAtURL:target error:NULL];
            [fm moveItemAtURL:item toURL:target error:NULL];
        }
        // Remove the now-empty old folder.
        [fm removeItemAtURL:sourceDir error:NULL];
    } else {
        // Import from outside the library: copy the game file and any sibling
        // files that share its name (companion data files, cover art, etc.).
        if (![self copyGameFileAndSiblings:sourceURL intoDirectory:destDir error:outError])
            return NO;
    }

    [game bookmarkForPath:destFile.path];
    return YES;
}

// The "disk set" key for a multi-disk game file, or nil if the name does not
// look like one disk of a numbered set. Multi-disk disk-image games (e.g. the
// Apple II Infocom titles) are split across files that share a name but for a
// disk number following a "side"/"disk"/… word, such as "Zork Zero side 1.woz"
// … "Zork Zero side 4.woz". The interpreters find the other sides by swapping
// that digit (see terps/bocfel/z6/extract_apple_2.cpp), so we must keep the
// whole set together. Replacing the digits with '#' yields a key shared by
// every disk in the set. The keyword guard keeps genuinely different games
// that merely carry a number (e.g. "Zork 1" vs "Zork 2") from being grouped.
- (nullable NSString *)diskSetKeyForFilename:(NSString *)filename {
    static NSRegularExpression *regex = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        regex = [NSRegularExpression
                 regularExpressionWithPattern:@"^(.*(?:side|disk|disc|part)[ _]*)[0-9]+(.*)$"
                                      options:NSRegularExpressionCaseInsensitive
                                        error:NULL];
    });
    NSString *lower = filename.lowercaseString;
    NSTextCheckingResult *match =
    [regex firstMatchInString:lower options:0 range:NSMakeRange(0, lower.length)];
    if (!match)
        return nil;
    NSString *prefix = [lower substringWithRange:[match rangeAtIndex:1]];
    NSString *suffix = [lower substringWithRange:[match rangeAtIndex:2]];
    return [NSString stringWithFormat:@"%@#%@", prefix, suffix];
}

// Copies `sourceURL` and every file in its folder that belongs to the same
// game, into `destDir`. A file belongs to the game when it shares the main
// file's name without extension (AGT's .d$$ plus .da1, .da2 …; companion data
// files; cover art) or when it is another disk of the same numbered set
// (multi-disk .woz/.dsk games such as "Zork Zero side 1…4.woz").
- (BOOL)copyGameFileAndSiblings:(NSURL *)sourceURL
                  intoDirectory:(NSURL *)destDir
                          error:(NSError **)outError {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *sourceDir = sourceURL.URLByDeletingLastPathComponent;
    NSString *stem = sourceURL.lastPathComponent.stringByDeletingPathExtension.lowercaseString;
    NSString *diskSetKey = [self diskSetKeyForFilename:sourceURL.lastPathComponent];

    NSArray<NSURL *> *contents =
    [fm contentsOfDirectoryAtURL:sourceDir
      includingPropertiesForKeys:@[ NSURLIsDirectoryKey ]
                         options:NSDirectoryEnumerationSkipsHiddenFiles
                           error:NULL];

    BOOL copiedMain = NO;
    for (NSURL *item in contents) {
        NSNumber *isDir = nil;
        [item getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:NULL];
        if (isDir.boolValue)
            continue;
        NSString *itemStem = item.lastPathComponent.stringByDeletingPathExtension.lowercaseString;
        BOOL isMain = [item.path.lowercaseString isEqualToString:sourceURL.path.lowercaseString];
        BOOL sameStem = [itemStem isEqualToString:stem];
        BOOL sameDiskSet = diskSetKey &&
            [diskSetKey isEqualToString:[self diskSetKeyForFilename:item.lastPathComponent]];
        if (!isMain && !sameStem && !sameDiskSet)
            continue;

        NSURL *target = [destDir URLByAppendingPathComponent:item.lastPathComponent];
        if ([target checkResourceIsReachableAndReturnError:NULL])
            [fm removeItemAtURL:target error:NULL];

        NSError *copyError = nil;
        if ([fm copyItemAtURL:item toURL:target error:&copyError]) {
            if (isMain)
                copiedMain = YES;
        } else if (isMain) {
            if (outError)
                *outError = copyError;
            return NO;
        }
    }

    if (!copiedMain) {
        // The stem loop should always cover the main file, but make sure.
        NSURL *target = [destDir URLByAppendingPathComponent:sourceURL.lastPathComponent];
        if (![target checkResourceIsReachableAndReturnError:NULL] &&
            ![fm copyItemAtURL:sourceURL toURL:target error:outError])
            return NO;
    }
    return YES;
}

#pragma mark - Re-organise on rename

- (void)reorganiseGame:(Game *)game {
    if (![LibraryOrganizer keepGamesOrganised])
        return;
    NSURL *root = [self libraryRootURLCreatingIfNeeded:NO];
    if (!root || !game.path.length)
        return;
    NSURL *current = [NSURL fileURLWithPath:game.path isDirectory:NO];
    // Only touch games that already live inside the library.
    if (![self url:current isInsideRoot:root])
        return;
    NSError *error = nil;
    if (![self organiseGame:game error:&error])
        NSLog(@"LibraryOrganizer: reorganiseGame failed: %@", error.localizedDescription);
}

#pragma mark - Organise everything

- (void)organiseAllGamesInContext:(NSManagedObjectContext *)context
                       completion:(nullable void (^)(NSUInteger))completion {
    [context performBlock:^{
        NSArray<Game *> *games =
        [Fetches fetchObjects:@"Game" predicate:@"hidden == NO" inContext:context];

        NSUInteger organised = 0;
        for (Game *game in games) {
            NSURL *source = game.urlForBookmark;
            if (!source)
                continue;

            // Grant read access to the source folder (a no-op / nil for files
            // that already live in our container, which are readable anyway).
            NSURL *granted = [FolderAccess grantAccessToFile:source];

            NSError *error = nil;
            if ([self organiseGame:game error:&error])
                organised++;
            else if (error)
                NSLog(@"LibraryOrganizer: could not organise \"%@\": %@",
                      game.metadata.title, error.localizedDescription);

            if (granted)
                [FolderAccess releaseBookmark:granted];
        }

        NSError *saveError = nil;
        if (context.hasChanges && ![context save:&saveError])
            NSLog(@"LibraryOrganizer: save failed: %@", saveError);

        if (completion) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(organised);
            });
        }
    }];
}

@end
