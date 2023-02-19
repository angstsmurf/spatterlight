/*
 * Pop up a sheet/panel with more detailed info about a game.
 * This will show the cover art, headline and description.
 * Eventually, this might become the metadata editor.
 */

#import <Cocoa/Cocoa.h>

@class Game, Metadata, TableViewController, CoreDataManager;

@interface InfoPanel : NSWindow

@property BOOL disableConstrainedWindow;

@end

@interface InfoController : NSWindowController <NSWindowDelegate, NSTextFieldDelegate, NSTextViewDelegate>

@property (weak) Game *game;
@property (strong) TableViewController *libcontroller;
@property (nonatomic, weak) NSManagedObjectContext *managedObjectContext;
@property (nonatomic, weak) CoreDataManager *coreDataManager;
@property NSString *ifid;

@property Metadata *meta;

@property BOOL inAnimation;
@property BOOL downArrowWhileInAnimation;
@property BOOL upArrowWhileInAnimation;

@property BOOL inDeletion;

@property IBOutlet NSTextField *titleField;

- (instancetype)initWithGame:(Game *)game;
- (instancetype)initWithIfid:(NSString *)ifid;

- (void)animateIn:(NSRect)frame;
- (void)hideWindow;

- (void)updateImage;

+ (void)closeStrayInfoWindows;

@end
