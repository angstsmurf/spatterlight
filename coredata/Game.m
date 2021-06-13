//
//  Game.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-31.
//
//

#import "Game.h"
#import "Metadata.h"
#import "Theme.h"


@implementation Game

@dynamic added;
@dynamic autosaved;
@dynamic blorbFile;
@dynamic checksum;
@dynamic compiler;
@dynamic detectedFormat;
@dynamic fileLocation;
@dynamic found;
@dynamic group;
@dynamic hasDownloaded;
@dynamic hashTag;
@dynamic ifid;
@dynamic lastPlayed;
@dynamic path;
@dynamic releaseNumber;
@dynamic serialString;
@dynamic version;
@dynamic metadata;
@dynamic override;
@dynamic theme;

- (nullable NSURL *)urlForBookmark {
    BOOL bookmarkIsStale = NO;
    NSError* theError = nil;
    if (!self.fileLocation) {
        NSLog(@"Game urlForBookmark: Error! File location for game %@ is nil!", self.metadata.title);
        self.found = NO;
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
    }
    if (theError != nil) {

        NSLog(@"Game urlForBookmark: Error! %@", theError);
        self.found = NO;
        return nil;
    }
    self.path = bookmarkURL.path;
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
        self.found = NO;
        NSLog(@"Could not create bookmark from file at %@",path);
        return;
    }

    self.path = path;
    self.fileLocation = bookmark;
}

@end
