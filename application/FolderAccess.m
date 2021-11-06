//
//  FolderAccess.m
//  Spatterlight
//
//  Created by Administrator on 2021-09-15.
//

#import "FolderAccess.h"
#import "Constants.h"


@interface FolderAccess () {
    NSInteger accessCount;
    NSURL *controlURL;
}

@property NSData *bookmark;
@property BOOL active;

@end

@implementation FolderAccess

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

+ (void)askForAccessToURL:(NSURL *)url andThenRunBlock:(void (^)(void))block {

    NSURL *bookmarkURL = [FolderAccess suitableDirectoryForURL:url];
    if (bookmarkURL) {
        if ([[NSFileManager defaultManager] isReadableFileAtPath:bookmarkURL.path]) {
            [FolderAccess storeBookmark:bookmarkURL];
            [FolderAccess saveBookmarks];
        } else {

            [FolderAccess grantAccessToFolder:bookmarkURL];
            if (![[NSFileManager defaultManager] isReadableFileAtPath:bookmarkURL.path]) {

                NSOpenPanel *openPanel = [NSOpenPanel openPanel];
                openPanel.message = NSLocalizedString(@"Spatterlight would like to access files in this folder", nil);
                openPanel.prompt = NSLocalizedString(@"Authorize", nil);
                openPanel.canChooseFiles = NO;
                openPanel.canChooseDirectories = YES;
                openPanel.canCreateDirectories = NO;
                openPanel.directoryURL = bookmarkURL;

                [openPanel beginWithCompletionHandler:^(NSInteger result) {
                    if (result == NSModalResponseOK) {
                        NSURL *blockURL = openPanel.URL;
                        [FolderAccess storeBookmark:blockURL];
                        [FolderAccess saveBookmarks];
                        block();
                    }
                }];
                return;
            }
        }
    }

    block();
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

    NSString *homeString = NSHomeDirectory();
    NSArray *pathComponents = homeString.pathComponents;
    pathComponents = [pathComponents subarrayWithRange:NSMakeRange(0, 3)];
    homeString = [NSString pathWithComponents:pathComponents];

    NSURL *homeURL = [NSURL fileURLWithPath:homeString]; // To get user home root

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

    return [NSURL fileURLWithPath:parentDir];
}

+ (NSString *)getVolumeNameForURL:(NSURL *)url {
    NSError *error = nil;

    id value;
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
    NSUInteger shortest = url.path.length;
    for (NSURL *key in globalBookmarks.allKeys) {
        if (key.path.length <= shortest) {
            NSString *urlString = [url.path substringToIndex:key.path.length];
            if ([urlString isEqualToString:key.path]) {
                best = key;
                shortest = key.path.length;
            }
        }
    }
    return best;
}

+ (NSURL *)grantAccessToFolder:(NSURL *)folderURL {
    NSURL *secureURL = [FolderAccess restoreURL:folderURL];
    if (secureURL)
        folderURL = secureURL;
    if ([[NSFileManager defaultManager] isReadableFileAtPath:folderURL.path])
        return folderURL;
    return nil;
}

+ (NSString *)bookmarkPath {
    // bookmarks saved in group directory
    NSString *groupIdentifier =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

    NSURL *url = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];

    url = [url URLByAppendingPathComponent:@"Bookmarks.dict"];
    return url.path;
}

+ (void)loadBookmarks {
    NSString *path = [FolderAccess bookmarkPath];
    globalBookmarks = [NSKeyedUnarchiver unarchiveObjectWithFile:path];
}

+ (void)saveBookmarks {
    NSString *path = [FolderAccess bookmarkPath];
    [NSKeyedArchiver archiveRootObject:globalBookmarks toFile:path];
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

+ (NSURL *)restoreURL:(NSURL *)url {
    FolderAccess *storedAccess = globalBookmarks[url];
    if (!storedAccess) {
        NSLog(@"No stored data for URL %@", url.path);
        return nil;
    }
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

- (void)resetCountIfNotReadable:(NSURL *)url {
    if (accessCount != 0 && ![[NSFileManager defaultManager] isReadableFileAtPath:url.path]) {
        NSLog(@"Secure url %@: Access count was %ld but path was not readable!", url.path, accessCount);
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
        if (![[NSFileManager defaultManager] isReadableFileAtPath:url.path]) {
            NSLog(@"askToAccess: already accessing %@ (accessCount = %ld) but the file there is NOT readable!", url.path, accessCount);
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
        _active = YES;
        if (isStale) {
            [FolderAccess storeBookmark:restoredUrl];
            [FolderAccess saveBookmarks];
        }
        if (![restoredUrl startAccessingSecurityScopedResource]) {
            NSLog(@"Couldn't access: %@", restoredUrl.path);
        } else {
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

+ (void)listActiveSecurityBookmarks {
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
}

@end
