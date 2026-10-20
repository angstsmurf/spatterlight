//
//  FolderAccess.m
//  Spatterlight
//
//  Created by Administrator on 2021-09-15.
//

#import <Cocoa/Cocoa.h>

#import "FolderAccess.h"
#import "DebugLog.h"


@interface FolderAccess () <NSSecureCoding> {
    NSInteger accessCount;
    NSURL *controlURL;
}

@property NSData *bookmark;
@property BOOL active;

@end

@implementation FolderAccess

static NSMutableDictionary<NSURL *, FolderAccess *> *globalBookmarks;

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithURL:(NSURL *)url {
    self = [super init];
    if (self) {
        accessCount = 0;
        _active = NO;
        NSError *error = nil;
        _bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
        if (error)
            NSLog(@"FolderAccess create bookmark error: %@", error);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        _bookmark = [decoder decodeObjectOfClass:[NSData class] forKey:@"bookmark"];
        accessCount = 0;
        _active = NO;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_bookmark forKey:@"bookmark"];
}

+ (BOOL)needsPermissionForURL:(NSURL *)url
{
    if ( !url ) {
        return NO;
    }
    NSError *error = nil;

    if (@available(macOS 15.0, *)) {
        [NSData dataWithContentsOfURL:url options:NSDataReadingMappedAlways error:&error];
        // Error 257: "The file couldn’t be opened because you don’t have permission to view it."
        if (error.domain == NSCocoaErrorDomain && error.code == 257) {
            return YES;
        }
        return NO;
    } else {
        BOOL reachable = [url checkResourceIsReachableAndReturnError:&error];
        if ( !reachable ) {
            //Could not reach path (may not be an error; if file does not exist this is expected behavior
            return NO;
        }

        return ![[NSFileManager defaultManager] isReadableFileAtPath:url.path];
    }
}

+ (void)askForAccessToURL:(NSURL *)url andThenRunBlock:(void (^)(void))block {

    NSURL *bookmarkURL = [FolderAccess suitableDirectoryForURL:url];
    if (bookmarkURL) {
        if (![FolderAccess needsPermissionForURL:bookmarkURL]) {
            [FolderAccess storeBookmark:bookmarkURL];
            [FolderAccess saveBookmarks];
        } else {
            [FolderAccess grantAccessToFolder:bookmarkURL];
            if ([FolderAccess needsPermissionForURL:bookmarkURL]) {
                // Runs the block only if the user authorizes the folder
                [FolderAccess forceAccessDialogToURL:bookmarkURL andThenRunBlock:block];
                return;
            }
        }
    }

    block();
}

+ (void)forceAccessDialogToURL:(NSURL *)url andThenRunBlock:(void (^)(void))block {
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.message = NSLocalizedString(@"Spatterlight would like to access files in this folder", nil);
    openPanel.prompt = NSLocalizedString(@"Authorize", nil);
    openPanel.canChooseFiles = NO;
    openPanel.canChooseDirectories = YES;
    openPanel.canCreateDirectories = NO;
    openPanel.directoryURL = url;
    NSModalResponse result = [openPanel runModal];
    if (result == NSModalResponseOK) {
        NSURL *blockURL = openPanel.URL;
        [FolderAccess storeBookmark:blockURL];
        [FolderAccess saveBookmarks];
        block();
    }
}

+ (NSURL *)grantAccessToFile:(NSURL *)url {

    NSURL *folderURL = [FolderAccess suitableDirectoryForURL:url];

    // Return an url to the containing folder. This must be released later.
    return [FolderAccess grantAccessToFolder:folderURL];
}

+ (NSURL *)suitableDirectoryForURL:(NSURL *)url {

    NSURL *existingAncestor = [FolderAccess existingAncestor:url];
    if (existingAncestor)
        return existingAncestor;

    if (url.hasDirectoryPath)
        return url;

    NSString *homeString = NSHomeDirectory();
    NSArray *pathComponents = homeString.pathComponents;
    // "/", "Users", name: the user's home root, outside any sandbox container
    pathComponents = [pathComponents subarrayWithRange:NSMakeRange(0, MIN(3, pathComponents.count))];
    homeString = [NSString pathWithComponents:pathComponents];

    NSURL *homeURL = [NSURL fileURLWithPath:homeString isDirectory:YES]; // To get user home root

    if (![[FolderAccess getVolumeNameForURL:url] isEqualToString:[FolderAccess getVolumeNameForURL:homeURL]]) {
        pathComponents = url.path.pathComponents;
        // It is the actual volume
        if (pathComponents.count < 4)
            return url;
        // It is in volume root
        if (pathComponents.count < 5)
            return url.URLByDeletingLastPathComponent;
        return url.URLByDeletingLastPathComponent.URLByDeletingLastPathComponent;
    }

    NSString *parentDir = url.path.stringByDeletingLastPathComponent.stringByDeletingLastPathComponent;

    if (parentDir.length < homeString.length)
        parentDir = homeString;

    return [NSURL fileURLWithPath:parentDir isDirectory:YES];
}

+ (NSString *)getVolumeNameForURL:(NSURL *)url {
    NSError *error = nil;

    id value = nil;
    BOOL result = [url getResourceValue:&value
                                 forKey:NSURLVolumeNameKey
                                  error:&error];

    if (error || !result)
        NSLog(@"getVolumeNameForURL: %@", error);

    return (NSString *)value;
}

+ (NSURL *)existingAncestor:(NSURL *)url {
    NSURL *best = nil;
    if (!url)
        return nil;
    NSString *path = url.path;
    NSUInteger shortest = path.length;
    for (NSURL *key in globalBookmarks.allKeys) {
        NSString *keyPath = key.path;
        if (keyPath.length > shortest)
            continue;
        // Match whole path components only: /a/Games is not an
        // ancestor of /a/GamesOld/x.
        NSString *prefix = [keyPath hasSuffix:@"/"] ? keyPath : [keyPath stringByAppendingString:@"/"];
        if ([path isEqualToString:keyPath] || [path hasPrefix:prefix]) {
            best = key;
            shortest = keyPath.length;
        }
    }
    return best;
}

+ (NSURL *)grantAccessToFolder:(NSURL *)folderURL {
    NSURL *secureURL = [FolderAccess restoreURL:folderURL];
    if (secureURL)
        folderURL = secureURL;
    if (![FolderAccess needsPermissionForURL:folderURL])
        return folderURL;
    return nil;
}

+ (NSURL *)sharedContainerURL {
    NSString *groupIdentifier =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];
    NSURL *container = groupIdentifier.length ?
    [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier] : nil;
    if (container)
        return container;

    NSError *error = nil;
    NSURL *appSupport = [[NSFileManager defaultManager]
                         URLForDirectory:NSApplicationSupportDirectory
                         inDomain:NSUserDomainMask
                         appropriateForURL:nil
                         create:YES
                         error:&error];
    if (!appSupport)
        NSLog(@"sharedContainerURL: Could not find Application Support directory! %@", error);
    return [appSupport URLByAppendingPathComponent:@"Spatterlight" isDirectory:YES];
}

+ (NSString *)bookmarkPath {
    // bookmarks saved in group directory
    return [[FolderAccess sharedContainerURL] URLByAppendingPathComponent:@"Bookmarks.dict" isDirectory:NO].path;
}

+ (void)loadBookmarks {
    NSString *path = [FolderAccess bookmarkPath];
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data) {
        globalBookmarks = nil;
        return;
    }
    NSError *error = nil;
    NSSet *classes = [NSSet setWithObjects:[NSMutableDictionary class], [NSDictionary class], [NSURL class], [FolderAccess class], nil];
    NSDictionary *dict = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes fromData:data error:&error];
    if (error)
        NSLog(@"loadBookmarks: error: %@", error);
    globalBookmarks = [dict isKindOfClass:[NSDictionary class]] ? [dict mutableCopy] : nil;
}

+ (void)saveBookmarks {
    if (!globalBookmarks)
        return;
    NSString *path = [FolderAccess bookmarkPath];
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:globalBookmarks requiringSecureCoding:YES error:&error];
    if (!data || ![data writeToFile:path options:NSDataWritingAtomic error:&error])
        NSLog(@"saveBookmarks: error: %@", error);
}


+ (void)deleteBookmarks {
    NSError *error = nil;
    [[NSFileManager defaultManager] removeItemAtPath:[FolderAccess bookmarkPath] error:&error];
    if (error)
        NSLog(@"deleteBookmarks: error: %@", error);
}

+ (void)storeBookmark:(NSURL *)url {
    if (!globalBookmarks)
        globalBookmarks = [NSMutableDictionary new];
    // Already stored this URL
    if (url == nil || globalBookmarks[url] != nil)
        return;
    FolderAccess *newAccess = [[FolderAccess alloc] initWithURL:url];
    globalBookmarks[url] = newAccess;
}

+ (NSArray<NSURL *> *)bookmarkedFolders {
    return globalBookmarks.allKeys ?: @[];
}

+ (NSURL *)restoreURL:(NSURL *)url {
    return [FolderAccess restoreURL:url force:NO];
}

+ (NSURL *)forceRestoreURL:(NSURL *)url {
    return [FolderAccess restoreURL:url force:YES];
}

// force: start counting accesses from zero even if the folder is readable.
+ (NSURL *)restoreURL:(NSURL *)url force:(BOOL)force {
    FolderAccess *storedAccess = globalBookmarks[url];
    if (!storedAccess) {
        NSLog(@"No stored data for URL %@", url.path);
        return nil;
    }
    if (force)
        [storedAccess forceResetCount];
    else
        [storedAccess resetCountIfNotReadable:url];
    NSURL *storedURL = [storedAccess askToAccess];
    if (![storedURL isEqual:url]) {
        if (!storedURL)
            NSLog(@"storedURL is nil! Could not access %@!", url.path);

        NSLog(@"Bookmark is stale! File has moved from %@ to %@!", url.path, storedURL.path);
        globalBookmarks[url] = nil;
        if (storedURL)
            globalBookmarks[storedURL] = storedAccess;
    }
    return storedURL;
}

- (void)forceResetCount {
    accessCount = 0;
}

- (void)resetCountIfNotReadable:(NSURL *)url {
    if (accessCount != 0 && [FolderAccess needsPermissionForURL:url]) {
        NSLog(@"Secure url %@: Access count was %ld but path was not readable!", url.path, (long)accessCount);
        accessCount = 0;
    }
}

- (NSURL *)askToAccess {
    NSURL *url = nil;
    if (accessCount == 0) {
        url = [self startAccessing];
        if (!url)
            return nil;
    } else {
        url = controlURL;
        if ([FolderAccess needsPermissionForURL:url]) {
            NSLog(@"askToAccess: already accessing %@ (accessCount = %ld) but the file there is NOT readable!", url.path, (long)accessCount);
        }
    }
    accessCount++;
    controlURL = url;
    return url;
}

- (NSURL *)startAccessing {
    NSError *error = nil;
    BOOL isStale = NO;
    NSURL *restoredUrl = [NSURL URLByResolvingBookmarkData:_bookmark options:NSURLBookmarkResolutionWithSecurityScope relativeToURL:nil bookmarkDataIsStale:&isStale error:&error];

    if (error)
        NSLog(@"FolderAccess restoreBookmark error: %@", error);

    if  (restoredUrl) {
        if (isStale) {
            [FolderAccess storeBookmark:restoredUrl];
            [FolderAccess saveBookmarks];
        }
        if (![restoredUrl startAccessingSecurityScopedResource]) {
            NSLog(@"Couldn't access: %@", restoredUrl.path);
        } else {
            _active = YES;
            return restoredUrl;
        }
    }

    return nil;
}

- (void)askToEndAccess:(NSURL *)url {
    if (controlURL && [controlURL isNotEqualTo:url]) {
        NSLog(@"askToEndAccess: URL %@ does not match control URL %@!", url.path, controlURL.path);
    }
    if (accessCount > 0) {
        accessCount--;
    }

    if (accessCount == 0 && _active) {
        [url stopAccessingSecurityScopedResource];
        _active = NO;
    }
}

+ (void)releaseBookmark:(NSURL *)url {
    FolderAccess *storedAccess = globalBookmarks[url];
    if (!storedAccess) {
        NSLog(@"releaseBookmark: No stored data for URL %@", url.path);
        return;
    }
    [storedAccess askToEndAccess:url];
}

// Logs only, so release builds skip the loop too.
+ (void)listActiveSecurityBookmarks {
#ifdef DEBUG
    NSLog(@"Active security bookmarks:");
    BOOL found = NO;
    for (NSURL *key in globalBookmarks.allKeys) {
        FolderAccess *access = globalBookmarks[key];
        if (access.active) {
            found = YES;
            NSLog(@"%@", key.path);
        }
    }
    if (!found)
        NSLog(@"None");
#endif
}

@end
