/*
 * Pop up a sheet/panel with more detailed info about a game.
 * This will show the cover art, headline and description.
 * Eventually, this might become the metadata editor.
 */

#import <CoreData/CoreData.h>

#import "Metadata+CoreDataProperties.h"

@class LibController;

@interface InfoController : NSWindowController
{
    IBOutlet NSTextField *titleField;
    IBOutlet NSTextField *authorField;
    IBOutlet NSTextField *headlineField;
    IBOutlet NSTextField *ifidField;
    IBOutlet NSTextView *descriptionText;
    IBOutlet NSImageView *imageView;
    
    NSString *ifid;
}

@property (strong) Game *game;

- (IBAction) saveImage: sender;

@end
