#import <Foundation/Foundation.h>
#import "NSString+Categories.h"

#include "fileref.h"
#include "glkimp.h"

char autosavedir[1024] = "";

void getworkdir()
{
    /* if we have already set a working dir path, we return right away */
    if (strcmp(workingdir, ""))
        return;

    @autoreleasepool {

        NSDictionary *gFolderMap = @{@"scare": @"SCARE",
                                     @"advsys": @"AdvSys",
                                     @"agility": @"AGiliTy",
                                     @"glulxe": @"Glulxe",
                                     @"hugo": @"Hugo",
                                     @"level9": @"Level 9",
                                     @"magnetic": @"Magnetic",
                                     @"unquill": @"UnQuill",
                                     @"tads": @"TADS",
                                     @"frotz": @"Frotz",
                                     @"fizmo": @"Fizmo"};
        NSError *error;
        NSURL *appSupportDir = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];

        if (error)
            NSLog(@"Could not find Application Support folder. Error: %@", error);

        NSString *firstWord = [[[[NSString stringWithUTF8String:gli_program_name] componentsSeparatedByString:@" "] objectAtIndex:0] lowercaseString];
        
        NSString *dirstr = [NSString stringWithFormat: @"Spatterlight/%@", [gFolderMap objectForKey:firstWord]];

        dirstr = [dirstr stringByAppendingString:@" Files"];
        dirstr = [dirstr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        appSupportDir = [NSURL URLWithString: dirstr relativeToURL:appSupportDir];

        [[NSFileManager defaultManager] createDirectoryAtURL:appSupportDir withIntermediateDirectories:YES attributes:nil error:NULL];
        
        strncpy(workingdir, [appSupportDir.path UTF8String], sizeof workingdir);
    }
    
	workingdir[sizeof workingdir-1] = 0;
}

void getautosavedir(char *file)
{
    /* if we have already set an autosave dir path, we return right away */
    if (strcmp(autosavedir, ""))
        return;

    @autoreleasepool {

        NSError *error = nil;

        getworkdir();
        NSString *gamepath = [NSString stringWithUTF8String:file];
        NSString *dirname = [NSString stringWithUTF8String:workingdir];
        dirname = [dirname stringByAppendingPathComponent:@"Autosaves"];
        dirname = [dirname stringByAppendingPathComponent:[gamepath signatureFromFile]];

        [[NSFileManager defaultManager] createDirectoryAtURL:[NSURL fileURLWithPath:dirname] withIntermediateDirectories:YES attributes:nil error:&error];
        if (error)
            NSLog(@"Could not create autosave directory at %@. Error:%@",dirname,error);

        strncpy(autosavedir, [dirname UTF8String], sizeof autosavedir);
    }

	autosavedir[sizeof autosavedir-1] = 0;
}
