//
//  Game+CoreDataClass.m
//  Spatterlight
//
//  Created by Administrator on 2017-06-16.
//
//

#import "Game.h"
#import "Interpreter+CoreDataClass.h"

#import "Metadata+CoreDataClass.h"

#import "Settings.h"

@implementation Game

- (NSURL *)urlForBookmark {
    BOOL bookmarkIsStale = NO;
    NSError* theError = nil;
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

    NSLog(@"Error! %@", theError);
    // Handle any errors
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
        NSLog(@"Could not create bookmark from file at %@",path);
        return;
    }

    self.fileLocation=bookmark;
}

- (void) showInfoWindow {
    if (self.infoWindow == nil)
        self.infoWindow = [[InfoController alloc]initWithWindowNibName: @"InfoPanel"];

    InfoController *infoWindow = (InfoController *)self.infoWindow;
    infoWindow.game = self;

    [infoWindow showWindow: nil];
    infoWindow.window.delegate = self;

}

- (void) windowWillClose:(NSNotification *)notification
{
//    NSLog(@"glkctl: windowWillClose");
    InfoController * infoWindow = (InfoController *)self.infoWindow;
	[infoWindow updateBlurb];
    infoWindow.window.delegate = nil;
    self.infoWindow = nil;
}


@end
