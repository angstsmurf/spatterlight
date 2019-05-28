/*
 * Pop up a sheet/panel with more detailed info about a game.
 * This will show the cover art, headline and description.
 * Eventually, this might become the metadata editor.
 */

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

@property NSString *path;
@property NSDictionary *meta;

- (instancetype)initWithpath:(NSString *)path andInfo:(NSDictionary *)meta;
- (instancetype)initWithpath:(NSString *)path;

- (IBAction)saveImage:sender;

@end
