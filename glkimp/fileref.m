#import <Foundation/Foundation.h>
#import "NSString+Categories.h"

#include "fileref.h"
#include "glkimp.h"

char *autosavedir = NULL;
char tempdir[BUFLEN] = "";

static void setdefaultworkdir(char **string)
{
    size_t length;
    *string = NULL;
    @autoreleasepool {

        /* Keyed by the lower-cased FIRST WORD of the terp's program name
         * (garglk_set_program_name), e.g. "ScottFree 1.14" -> "scottfree".
         * Keys must match that first word, not the format or binary name,
         * or the lookup falls through to a "(null) Files" folder. */
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
                                     @"geas": @"Quest",
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
        NSString *firstWord = [terpName componentsSeparatedByString:@" "][0].lowercaseString;

        if ([firstWord isEqualToString:@"alan"] && [[terpName componentsSeparatedByString:@"."][0] isEqualToString:@"Alan 3"])
            firstWord = @"alan 3";

        NSString *dirstr = [NSString stringWithFormat: @"Spatterlight/%@", gFolderMap[firstWord]];

        dirstr = [dirstr stringByAppendingString:@" Files"];
        dirstr = [dirstr stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];
        appSupportDir = [NSURL URLWithString:dirstr relativeToURL:appSupportDir];

        length = appSupportDir.path.length;
        *string = malloc(length + 1);
        strncpy(*string, appSupportDir.fileSystemRepresentation, length);
    }

    (*string)[length] = 0;
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

        NSString *signature = gamepath.signatureFromFile;

        if (signature.length == 0) {
            NSLog(@"getautosavedir: Could not create file signature from file at \"%@\"", gamepath);
            return;
        }

        NSString *finalDir = [dirname stringByAppendingPathComponent:signature];
        NSUInteger length = finalDir.length + 1;

        autosavedir = malloc(length);
        [finalDir getFileSystemRepresentation:autosavedir maxLength:length];
        autosavedir[finalDir.length] = 0;
    }
}

void gettempdir(void)
{
    /* if we have already set a temp dir path, we return right away */
    if (strcmp(tempdir, "")) {
        return;
    }

    @autoreleasepool {
        getworkdir();

        NSString *string = @(gli_workdir);
        NSURL *desktopURL = [NSURL fileURLWithPath:string
                                       isDirectory:YES];
        NSError *error = nil;

        NSURL *temporaryDirectoryURL = [[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory
                   inDomain:NSUserDomainMask
          appropriateForURL:desktopURL
                     create:YES
                      error:&error];

        if (error) {
            NSLog(@"gettempdir error: %@", error);
        }

        strncpy(tempdir, temporaryDirectoryURL.fileSystemRepresentation, sizeof tempdir);

    }
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
