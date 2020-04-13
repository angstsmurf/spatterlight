/*
 * Pop up a sheet/panel with more detailed info about a game.
 * This will show the cover art, headline and description.
 * Eventually, this might become the metadata editor.
 */

#import <CoreData/CoreData.h>

@class Game;
@class Metadata;

void showInfoForFile(NSString *path, NSDictionary *info);

@class LibController;

@interface InfoController : NSWindowController <NSWindowDelegate> {
    IBOutlet NSTextField *titleField;
    IBOutlet NSTextField *authorField;
    IBOutlet NSTextField *headlineField;
    IBOutlet NSTextField *ifidField;
    IBOutlet NSTextView *descriptionText;
    IBOutlet NSImageView *imageView;

    NSString *ifid;
}

@property (strong) Game *game;

@property NSString *path;
@property Metadata *meta;

- (instancetype)initWithGame:(Game *)game;
- (instancetype)initWithpath:(NSString *)path;

//- (void)updateBlurb;

- (IBAction)saveImage:sender;

@end
