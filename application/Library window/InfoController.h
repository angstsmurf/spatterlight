/*
 * Pop up a sheet/panel with more detailed info about a game.
 * This will show the cover art, headline and description.
 * Eventually, this might become the metadata editor.
 */

#import <Cocoa/Cocoa.h>

@class Game, Metadata, TableViewController, CoreDataManager;

void showInfoForFile(NSString *path, NSDictionary *info);

@interface InfoController : NSWindowController <NSWindowDelegate, NSTextFieldDelegate, NSTextViewDelegate>

@property (weak) Game *game;
@property (strong) TableViewController *libcontroller;

@property NSString *path;
@property Metadata *meta;

@property BOOL inAnimation;
@property BOOL downArrowWhileInAnimation;
@property BOOL upArrowWhileInAnimation;


@property IBOutlet NSTextField *titleField;

- (instancetype)initWithGame:(Game *)game;

// Used for window restoration
- (instancetype)initWithpath:(NSString *)path;

- (void)animateIn:(NSRect)frame;
- (void)hideWindow;

- (void)updateImage;

+ (void)closeStrayInfoWindows;

@end
