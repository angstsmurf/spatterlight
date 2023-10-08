/*
 * Launcher -- the main application controller
 */

#import <Cocoa/Cocoa.h>

@class HelpPanelController, LibController, Preferences, TableViewController, CoreDataManager;

@interface AppDelegate : NSObject <NSWindowDelegate> 

@property Preferences *prefctl;
@property LibController *libctl;
@property TableViewController *tableViewController;

@property (weak) HelpPanelController *helpLicenseWindow;
@property (readonly) CoreDataManager *coreDataManager;

- (IBAction)openDocument:(id)sender;

- (IBAction)showHelpFile:(id)sender;

- (void)addToRecents:(NSArray *)URLs;

@property (weak) IBOutlet NSMenuItem *themesMenuItem;

@end
