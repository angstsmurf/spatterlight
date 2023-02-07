/*
 * Launcher -- the main application controller
 */

#import <Cocoa/Cocoa.h>

@class HelpPanelController, LibController, Preferences, TableViewController, CoreDataManager;

@interface AppDelegate : NSObject <NSWindowDelegate, NSWindowRestoration> 

@property Preferences *prefctl;
@property LibController *libctl;
@property TableViewController *tableViewController;

@property HelpPanelController *helpLicenseWindow;

@property (readonly) CoreDataManager *coreDataManager;

- (IBAction)openDocument:(id)sender;

- (IBAction)showPrefs:(id)sender;
- (IBAction)showLibrary:(id)sender;
- (IBAction)showHelpFile:(id)sender;
- (IBAction)pruneLibrary:(id)sender;

- (void)addToRecents:(NSArray *)URLs;

@property (weak) IBOutlet NSMenuItem *themesMenuItem;

@end
