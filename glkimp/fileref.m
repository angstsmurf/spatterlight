#import <Foundation/Foundation.h>
#import "NSString+Categories.h"

#include "fileref.h"
#include "glkimp.h"

char *autosavedir = NULL;
char tempdir[BUFLEN] = "";

void setdefaultworkdir(char **string)
{
    size_t length;
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
                                     @"bocfel": @"Bocfel"
                                     };
        NSError *error;
        NSURL *appSupportDir = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];

        if (error)
            NSLog(@"Could not find Application Support folder. Error: %@", error);

        NSString *terpName = [NSString stringWithUTF8String:gli_program_name];
        NSString *firstWord = [[[terpName componentsSeparatedByString:@" "] objectAtIndex:0] lowercaseString];

        if ([firstWord isEqualToString:@"alan"] && [[[terpName componentsSeparatedByString:@"."] objectAtIndex:0] isEqualToString:@"Alan 3"])
            firstWord = @"alan 3";

        NSString *dirstr = [NSString stringWithFormat: @"Spatterlight/%@", [gFolderMap objectForKey:firstWord]];

        dirstr = [dirstr stringByAppendingString:@" Files"];
        dirstr = [dirstr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        appSupportDir = [NSURL URLWithString:dirstr relativeToURL:appSupportDir];

        [[NSFileManager defaultManager] createDirectoryAtURL:appSupportDir withIntermediateDirectories:YES attributes:nil error:NULL];

        length = appSupportDir.path.length;
        *string = malloc(length + 1);
        strncpy(*string, appSupportDir.fileSystemRepresentation, length);
    }

    (*string)[length] = 0;
}

void getworkdir()
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

    @autoreleasepool {

        NSError *error = nil;

        setdefaultworkdir(&autosavedir);
        NSString *gamepath = [NSString stringWithUTF8String:file];
        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        dirname = [dirname stringByAppendingPathComponent:@"Autosaves"];
        dirname = [dirname stringByAppendingPathComponent:[gamepath signatureFromFile]];

        [[NSFileManager defaultManager] createDirectoryAtURL:[NSURL fileURLWithPath:dirname] withIntermediateDirectories:YES attributes:nil error:&error];
        if (error)
            NSLog(@"Could not create autosave directory at %@. Error:%@",dirname,error);

        NSUInteger length = dirname.length;
        autosavedir = malloc(length + 1);
        strncpy(autosavedir, dirname.fileSystemRepresentation, length);
        autosavedir[length] = 0;
    }
}

void gettempdir()
{
    /* if we have already set an autosave dir path, we return right away */
    if (strcmp(tempdir, "")) {
        return;
    }

    @autoreleasepool {
        getworkdir();

        NSString *string = [NSString stringWithUTF8String:gli_workdir];
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
