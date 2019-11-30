//
//  Game.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import "Game.h"
#import "Interpreter.h"
#import "Metadata.h"
#import "Settings.h"


@implementation Game

@dynamic added;
@dynamic fileLocation;
@dynamic found;
@dynamic group;
@dynamic height;
@dynamic lastModified;
@dynamic lastPlayed;
@dynamic width;
@dynamic interpreter;
@dynamic metadata;
@dynamic setting;
@dynamic override;

- (NSURL *)urlForBookmark {
    BOOL bookmarkIsStale = NO;
    NSError* theError = nil;
    if (!self.fileLocation) {
        NSLog(@"Error! File location for game %@ is nil!", self.metadata.title);
        self.found = @(NO);
        return nil;
    }
    NSURL* bookmarkURL = [NSURL URLByResolvingBookmarkData:(NSData *)self.fileLocation
                                                   options:NSURLBookmarkResolutionWithoutUI
                                             relativeToURL:nil
                                       bookmarkDataIsStale:&bookmarkIsStale
                                                     error:&theError];

    if (bookmarkIsStale) {
        NSLog(@"Bookmark is stale! New location: %@", bookmarkURL.path);
        [self bookmarkForPath:bookmarkURL.path];
        // Handle any errors
        return bookmarkURL;
    }
    if (theError != nil) {

        NSLog(@"Game urlForBookmark: Error! %@", theError);
        // Handle any errors
        self.found = @(NO);
        return nil;
    }

    return bookmarkURL;
}

- (void)bookmarkForPath:(NSString *)path {


    NSError* theError = nil;
    NSURL* theURL = [NSURL fileURLWithPath:path];

    NSData* bookmark = [theURL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                        includingResourceValuesForKeys:nil
                                         relativeToURL:nil
                                                 error:&theError];

    if (theError || (bookmark == nil)) {
        // Handle any errors.
        self.found = @(NO);
        NSLog(@"Could not create bookmark from file at %@",path);
        return;
    }

    self.fileLocation=bookmark;
}


@end
