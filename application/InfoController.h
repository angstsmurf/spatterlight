/*
 * Pop up a sheet/panel with more detailed info about a game.
 * This will show the cover art, headline and description.
 * Eventually, this might become the metadata editor.
 */

void showInfoForFile(NSString *path, NSDictionary *info);

@class LibController;

@interface InfoController : NSWindowController
{
    IBOutlet NSTextField *titleField;
    IBOutlet NSTextField *authorField;
    IBOutlet NSTextField *headlineField;
    IBOutlet NSTextField *ifidField;
    IBOutlet NSTextView *descriptionText;
    IBOutlet NSImageView *imageView;
    
    NSString *path;
    NSDictionary *meta;
    NSString *ifid;
}

- (void) showForFile: (NSString*)path info: (NSDictionary*)meta;
- (IBAction) saveImage: sender;

@end
