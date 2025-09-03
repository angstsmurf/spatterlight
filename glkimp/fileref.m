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

        NSDictionary *gFolderMap = @{@"scare": @"SCARE",
                                     @"advsys": @"AdvSys",
                                     @"agility": @"AGiliTy",
                                     @"alan": @"Alan 2",
                                     @"alan 3": @"Alan 3",
                                     @"glulxe": @"Glulxe",
                                     @"hugo": @"Hugo",
                                     @"level9": @"Level 9",
                                     @"magnetic": @"Magnetic",
                                     @"unquill": @"UnQuill",
                                     @"tads": @"TADS",
                                     @"frotz": @"Frotz",
                                     @"fizmo": @"Fizmo",
                                     @"bocfel": @"Bocfel",
                                     @"quest4": @"Geas",
                                     @"jacl": @"JACL",
                                     @"sagaplus": @"Plus",
                                     @"scott": @"ScottFree",
                                     @"taylor": @"TaylorMade"
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

        dirname = [dirname stringByAppendingPathComponent:signature];

        NSUInteger length = dirname.length;
        autosavedir = malloc(length + 1);
        strncpy(autosavedir, dirname.fileSystemRepresentation, length);
        autosavedir[length] = 0;
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
