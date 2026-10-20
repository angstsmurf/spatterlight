//
//  StoreLocation.m
//  Spatterlight
//

#include <pwd.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>

#import "StoreLocation.h"
#import "CoreDataManager.h"
#import "FolderAccess.h"

static NSString * const StoreFileName = @"Spatterlight.storedata";

@implementation StoreLocation

- (instancetype)initWithStoreURL:(NSURL *)storeURL destinationURL:(NSURL *)destinationURL {
    self = [super init];
    if (self) {
        _storeURL = storeURL;
        _destinationURL = destinationURL;
        _needsMove = ![storeURL isEqual:destinationURL];
    }
    return self;
}

+ (nullable instancetype)currentLocation {
    NSURL *testStoreURL = [CoreDataManager testStoreURL];
    if (testStoreURL)
        return [[self alloc] initWithStoreURL:testStoreURL destinationURL:testStoreURL];

    NSURL *folder = [self storeFolder];
    if (!folder)
        return nil;

    NSURL *destinationURL = [folder URLByAppendingPathComponent:StoreFileName isDirectory:NO];
    NSURL *legacyURL = [[self legacyStoreFolder] URLByAppendingPathComponent:StoreFileName isDirectory:NO];
    NSFileManager *fileManager = [NSFileManager defaultManager];

    // No store exists where we want it, but there is one in the non-sandboxed
    // Application Support folder, so we try to move it.
    if (![fileManager fileExistsAtPath:destinationURL.path] &&
        [fileManager fileExistsAtPath:legacyURL.path]) {
        [FolderAccess askForAccessToURL:legacyURL andThenRunBlock:^{}];
        if ([fileManager isReadableFileAtPath:legacyURL.path])
            return [[self alloc] initWithStoreURL:legacyURL destinationURL:destinationURL];
    }

    return [[self alloc] initWithStoreURL:destinationURL destinationURL:destinationURL];
}

// The group container in a signed build, Application Support/Spatterlight
// otherwise. Created if missing; nil (error presented) if that fails or a file
// is in the way.
+ (nullable NSURL *)storeFolder {
    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSString *teamPrefix =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"TeamPrefix"];
    NSString *groupIdentifier =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

    NSURL *folder;
    if (teamPrefix.length) {
        folder = [fileManager containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];
    } else {
        folder = [[fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil] URLByAppendingPathComponent:@"Spatterlight" isDirectory:YES];
    }

    NSError *error = nil;

    NSDictionary *properties = [folder resourceValuesForKeys:@[NSURLIsDirectoryKey] error:&error];

    if (!properties) {
        BOOL ok = NO;
        if (error.code == NSFileReadNoSuchFileError) {
            ok = [fileManager createDirectoryAtPath:folder.path withIntermediateDirectories:YES attributes:nil error:&error];
        }
        if (!ok) {
            [NSApp presentError:error];
            NSLog(@"Error: %@", error);
            return nil;
        }
    } else if (![properties[NSURLIsDirectoryKey] boolValue]) {
        NSString *failureDescription = [NSString stringWithFormat:@"Expected a folder to store application data, found a file (%@).", folder.path];
        error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteFileExistsError userInfo:@{ NSLocalizedDescriptionKey: failureDescription }];
        [NSApp presentError:error];
        return nil;
    }

    return folder;
}

// The folder of the store from before Spatterlight moved its library into the
// group container: Application Support/Spatterlight in the user's real home.
// NSHomeDirectory() is the sandbox container in a sandboxed app, so ask the
// password database instead.
+ (NSURL *)legacyStoreFolder {
    const struct passwd *pw = getpwuid(getuid());
    NSString *home = pw && pw->pw_dir ? @(pw->pw_dir) : NSHomeDirectory();
    NSString *path = [home stringByAppendingPathComponent:@"Library/Application Support/Spatterlight"];
    return [NSURL fileURLWithPath:path isDirectory:YES];
}

@end
