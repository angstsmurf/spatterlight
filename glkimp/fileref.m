#import <Foundation/Foundation.h>

#include "fileref.h"
#include "glkimp.h"

extern void getworkdir()
{
    @autoreleasepool {

        NSError *error;
        NSURL *appSupportDir = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];

        NSString *firstWord = [[[NSString stringWithUTF8String:gli_program_name] componentsSeparatedByString:@" "] objectAtIndex:0];

        NSString *dirstr = [NSString stringWithFormat: @"Spatterlight/%@", firstWord];

        if ([dirstr isEqualToString:@"Level"])
            dirstr = @"Level 9";

        dirstr = [dirstr stringByAppendingString:@" Files"];
        dirstr = [dirstr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

        appSupportDir = [NSURL URLWithString: dirstr relativeToURL:appSupportDir];

        [[NSFileManager defaultManager] createDirectoryAtURL:appSupportDir withIntermediateDirectories:YES attributes:nil error:NULL];
        
        strncpy(workingdir, [appSupportDir.path UTF8String], sizeof workingdir);
    }
    
	workingdir[sizeof workingdir-1] = 0;
}
