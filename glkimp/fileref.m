#import <Foundation/Foundation.h>
#import "NSString+Categories.h"

#include "fileref.h"
#include "glkimp.h"

#include <errno.h>
#include <signal.h>
#include <unistd.h>

char *autosavedir = NULL;
char tempdir[BUFLEN] = "";

/* Holds the process id of the owner of a temp directory */
static NSString * const kOwnerFileName = @".glkimp-owner";

static void sweep_abandoned_tempdirs(NSURL *parent, NSString *ours);
static NSURL *default_tempdir_parent(void);

static void setdefaultworkdir(char **string)
{
    *string = NULL;
    if (gli_program_name == NULL)
        return;

    @autoreleasepool {

        /* Keyed by the lower-cased FIRST WORD of the terp's program name
         * (garglk_set_program_name), e.g. "ScottFree 1.15" -> "scottfree".
         * Keys must match that first word, not the format or binary name,
         * or the lookup falls back to a folder named after that word. */
        NSDictionary *gFolderMap = @{@"scarier": @"SCARE",
                                     @"advsys": @"AdvSys",
                                     @"agility": @"AGiliTy",
                                     @"alan": @"Alan 2",
                                     @"alan 3": @"Alan 3",
                                     @"archetype": @"Archetype",
                                     @"glulxe": @"Glulxe",
                                     @"hugo": @"Hugo",
                                     @"level": @"Level 9",
                                     @"magnetic": @"Magnetic",
                                     @"unquill": @"UnQuill",
                                     @"tads": @"TADS",
                                     @"frotz": @"Frotz",
                                     @"fizmo": @"Fizmo",
                                     @"bocfel": @"Bocfel",
                                     @"comprehend": @"Comprehend",
                                     @"question": @"Quest",
                                     @"jacl": @"JACL",
                                     @"plus": @"Plus",
                                     @"scottfree": @"ScottFree",
                                     @"taylormade": @"TaylorMade"
                                     };
        NSError *error;
        NSURL *appSupportDir = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:&error];

        if (error)
            NSLog(@"Could not find Application Support folder. Error: %@", error);

        if (appSupportDir == nil) {
            return;
        }

        NSString *terpName = @(gli_program_name);
        NSString *firstWord = [terpName componentsSeparatedByString:@" "][0];
        NSString *key = firstWord.lowercaseString;

        if ([key isEqualToString:@"alan"] && [[terpName componentsSeparatedByString:@"."][0] isEqualToString:@"Alan 3"])
            key = @"alan 3";

        NSString *folder = gFolderMap[key];
        if (folder == nil)
            folder = firstWord.length ? firstWord : @"Glk";

        NSURL *dir = [appSupportDir URLByAppendingPathComponent:@"Spatterlight" isDirectory:YES];
        dir = [dir URLByAppendingPathComponent:[folder stringByAppendingString:@" Files"] isDirectory:YES];

        /* fileSystemRepresentation is UTF-8 and may be longer than the
           UTF-16 path, e.g. for a home folder with non-ASCII characters. */
        const char *path = dir.fileSystemRepresentation;
        if (path)
            *string = strdup(path);
    }
}

void getworkdir(void)
{
    /* if we have already set a work dir path, we return right away */
    if (gli_workdir != NULL) {
        return;
    }

    setdefaultworkdir(&gli_workdir);
}

void getautosavedir(char *file)
{
    /* if we have already set an autosave dir path, we return right away */
    if (autosavedir != NULL) {
        return;
    }

    char *tempstring = NULL;
    setdefaultworkdir(&tempstring);

    if (tempstring == NULL)
        return;

    @autoreleasepool {
        NSString *gamepath = @(file);
        NSString *dirname = @(tempstring);
        free(tempstring);

        if (dirname.length == 0)
            return;

        dirname = [dirname stringByAppendingPathComponent:@"Autosaves"];

        /* Prefer the signature handed to us by the host app, which can fall
         * back to the library's stored hash when the game file itself is
         * unreadable (e.g. an evicted iCloud file). This also guarantees the
         * GUI and interpreter agree on the autosave directory. We only
         * recompute it from the file when running without the app, as in the
         * headless test harnesses. */
        NSString *signature = nil;
        const char *envsignature = getenv("SPATTERLIGHT_AUTOSAVE_SIGNATURE");
        if (envsignature != NULL && envsignature[0] != '\0')
            signature = @(envsignature);

        if (signature.length == 0)
            signature = gamepath.signatureFromFile;

        if (signature.length == 0) {
            NSLog(@"getautosavedir: Could not create file signature from file at \"%@\"", gamepath);
            return;
        }

        NSString *finalDir = [dirname stringByAppendingPathComponent:signature];

        /* Not finalDir.length: the UTF-8 form can be longer. */
        const char *path = finalDir.fileSystemRepresentation;
        if (path)
            autosavedir = strdup(path);
    }
}

void gettempdir(void)
{
    /* if we have already set a temp dir path, we return right away */
    if (strcmp(tempdir, "")) {
        return;
    }

    getworkdir();
    if (gli_workdir == NULL)
        return;

    @autoreleasepool {
        NSString *string = @(gli_workdir);
        NSURL *desktopURL = [NSURL fileURLWithPath:string
                                       isDirectory:YES];
        NSError *error = nil;

        /* The host app only creates the work directory when it has an
           autosave to put there. NSItemReplacementDirectory needs it to
           exist, and without a temp directory no image can be handed over,
           so a V6 game in a fresh home with autosave off drew nothing. */
        [[NSFileManager defaultManager] createDirectoryAtURL:desktopURL
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:nil];

        NSURL *temporaryDirectoryURL = [[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory
                   inDomain:NSUserDomainMask
          appropriateForURL:desktopURL
                     create:YES
                      error:&error];

        if (error) {
            NSLog(@"gettempdir error: %@", error);
        }

        const char *path = temporaryDirectoryURL.fileSystemRepresentation;
        if (!path || strlen(path) >= sizeof tempdir)
            return;

        strcpy(tempdir, path);

        /* Sign the directory with our process id, so that a later process
           can tell when we have died without cleaning up after ourselves. */
        NSString *owner = [NSString stringWithFormat:@"%d\n", getpid()];
        [owner writeToFile:[@(tempdir) stringByAppendingPathComponent:kOwnerFileName]
                atomically:NO encoding:NSUTF8StringEncoding error:NULL];

        /* gli_sweep_abandoned_tempdirs() has already looked in the usual
           place at startup */
        NSURL *parent = temporaryDirectoryURL.URLByDeletingLastPathComponent.URLByStandardizingPath;
        if (![parent.path isEqualToString:default_tempdir_parent().path])
            sweep_abandoned_tempdirs(parent, temporaryDirectoryURL.lastPathComponent);
    }
}

/* Where gettempdir() normally gets its directory */
static NSURL *default_tempdir_parent(void)
{
    return [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"TemporaryItems"]
                      isDirectory:YES].URLByStandardizingPath;
}

/* Called at startup, as a session that is restored from an autosave may
   never create a temp directory of its own. */
void gli_sweep_abandoned_tempdirs(void)
{
    @autoreleasepool {
        sweep_abandoned_tempdirs(default_tempdir_parent(), nil);
    }
}

/* Remove the temp directories in parent, except ours, of interpreter
   processes that were killed or crashed: gli_cleanup_tempdir() never ran
   for them. Only directories signed by gettempdir() are touched. Nothing
   else needs their files, as an autosave carries the contents of the temp
   files it uses (see gli_is_tempfile). */
static void sweep_abandoned_tempdirs(NSURL *parent, NSString *ours)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSArray<NSURL *> *siblings = [fileManager contentsOfDirectoryAtURL:parent
                                            includingPropertiesForKeys:nil
                                                               options:0
                                                                 error:NULL];
    for (NSURL *dir in siblings) {
        if (![dir.lastPathComponent hasPrefix:@"NSIRD_"]
            || [dir.lastPathComponent isEqualToString:ours])
            continue;
        NSString *owner = [NSString stringWithContentsOfURL:[dir URLByAppendingPathComponent:kOwnerFileName]
                                                   encoding:NSUTF8StringEncoding
                                                      error:NULL];
        if (!owner)
            continue;
        pid_t pid = (pid_t)owner.intValue;
        if (pid <= 0 || kill(pid, 0) == 0 || errno != ESRCH)
            continue;
        [fileManager removeItemAtURL:dir error:NULL];
    }
}

/* Whether path is a temp file in our temp directory. The temp directory is
   deleted when the process ends, so an autosave stores the contents of such
   a file in place of its path, and a restored session recreates it in its
   own temp directory (see gli_restore_tempfile). */
BOOL gli_is_tempfile(NSString *path)
{
    if (!path || tempdir[0] == '\0')
        return NO;
    return [path hasPrefix:[@(tempdir) stringByAppendingString:@"/"]];
}

/* The new path, in our own temp directory, of a temp file of an autosaved
   session, under its old name. The file is recreated from contents, if the
   autosave has any (the file may not have been written yet). A stream and
   its fileref both carry the contents, but only the stream flushed its
   buffer before they were stored, so a stream replaces the file (with
   overwrite) and a fileref only writes it when it is missing. No stream is
   reopened before the whole autosave is read. */
NSString *gli_restore_tempfile(NSString *oldpath, NSData *contents, BOOL overwrite)
{
    gettempdir();
    if (tempdir[0] == '\0')
        return oldpath;
    NSString *path = [@(tempdir) stringByAppendingPathComponent:oldpath.lastPathComponent];
    if (contents && (overwrite || ![[NSFileManager defaultManager] fileExistsAtPath:path]))
        [contents writeToFile:path options:0 error:NULL];
    return path;
}

/* Archive the file of a stream or fileref: a bookmark, the URL as a fallback,
   and for a temp file what it holds (see gli_is_tempfile). */
void gli_encode_file_url(NSCoder *encoder, NSURL *URL)
{
    if (!URL)
        return;

    NSError *error = nil;
    NSData *bookmark = [URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                     includingResourceValuesForKeys:nil
                                      relativeToURL:nil
                                              error:&error];
    if (bookmark)
        [encoder encodeObject:bookmark forKey:@"bookmark"];
    else
        NSLog(@"Error when encoding bookmark %@: %@", URL.path, error);
    [encoder encodeObject:URL forKey:@"URL"];

    if (gli_is_tempfile(URL.path)) {
        [encoder encodeBool:YES forKey:@"tempfile"];
        NSData *contents = [NSData dataWithContentsOfURL:URL];
        if (contents)
            [encoder encodeObject:contents forKey:@"tempfiledata"];
    }
}

/* The counterpart of gli_encode_file_url. A temp file is recreated in our
   own temp directory; overwrite as for gli_restore_tempfile. */
NSURL *gli_decode_file_url(NSCoder *decoder, BOOL overwrite)
{
    NSData *bookmark = [decoder decodeObjectOfClass:[NSData class] forKey:@"bookmark"];
    NSURL *URL = nil;
    if (bookmark)
        URL = [NSURL URLByResolvingBookmarkData:bookmark options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:nil error:nil];
    if (!URL)
        URL = [decoder decodeObjectOfClass:[NSURL class] forKey:@"URL"];

    if ([decoder decodeBoolForKey:@"tempfile"] && URL) {
        NSData *contents = [decoder decodeObjectOfClass:[NSData class] forKey:@"tempfiledata"];
        URL = [NSURL fileURLWithPath:gli_restore_tempfile(URL.path, contents, overwrite) isDirectory:NO];
    }
    return URL;
}

/* Remove the per-session temp directory and everything in it. The directory
   is a dedicated NSItemReplacementDirectory created by gettempdir() and used
   only by this process, so it is safe to delete the whole tree. This sweeps
   up any temp files that outlive their owners, e.g. the last .tiff registered
   under each image resource id by the V6 image code (see z6/draw_image.cpp),
   which is never purged because nothing replaces it. */
void gli_cleanup_tempdir(void)
{
    if (tempdir[0] == '\0')   /* never created -> nothing to do */
        return;

    @autoreleasepool {
        NSError *error = nil;
        NSString *path = @(tempdir);
        if (![[NSFileManager defaultManager] removeItemAtPath:path error:&error]) {
            NSLog(@"gli_cleanup_tempdir: %@", error);
        }
    }
    tempdir[0] = '\0';
}

int create_workdir(void) {
    int retval = 0;
    getworkdir();
    if (gli_workdir == NULL)
        return retval;
    @autoreleasepool {
        NSURL *appSupportDir = [NSURL fileURLWithPath:@(gli_workdir) isDirectory:YES];
        NSError *error = nil;
        BOOL result = [[NSFileManager defaultManager] createDirectoryAtURL:appSupportDir withIntermediateDirectories:YES attributes:nil error:&error];
        if (!result || error) {
            NSLog(@"create_workdir at %@ failed: %@", appSupportDir.path, error);
        } else {
            retval = 1;
        }
    }
    return retval;
}

int create_autosavedir(char *file) {
    int retval = 0;
    getautosavedir(file);
    /* getautosavedir fails silently, e.g. when the game file cannot be read
     * to compute a signature. Boxing the NULL path below would throw
     * NSInvalidArgumentException and abort the interpreter. */
    if (autosavedir == NULL)
        return retval;
    @autoreleasepool {
        NSURL *autoSaveURL = [NSURL fileURLWithPath:@(autosavedir) isDirectory:YES];
        NSError *error = nil;
        BOOL result = [[NSFileManager defaultManager] createDirectoryAtURL:autoSaveURL withIntermediateDirectories:YES attributes:nil error:&error];
        if (!result || error) {
            NSLog(@"create_autosavedir: Could not create autosave directory at \"%@\". %@",autoSaveURL.path, error);
        } else {
            retval = 1;
        }
    }
    return retval;
}
