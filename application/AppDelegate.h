/*
 * Launcher -- the main application controller
 */

#import <Cocoa/Cocoa.h>

@class HelpPanelController, LibController, Preferences, MyCoreDataCoreSpotlightDelegate;
;

@interface AppDelegate : NSObject <NSWindowDelegate, NSWindowRestoration> 

@property Preferences *prefctl;
@property LibController *libctl;
@property HelpPanelController *helpLicenseWindow;

@property (readonly) NSPersistentContainer *persistentContainer;
@property MyCoreDataCoreSpotlightDelegate *spotlightDelegate;

- (void)startIndexing;
- (void)stopIndexing;

- (IBAction)openDocument:(id)sender;

- (IBAction)showPrefs:(id)sender;
- (IBAction)showLibrary:(id)sender;
- (IBAction)showHelpFile:(id)sender;
- (IBAction)pruneLibrary:(id)sender;

- (void)addToRecents:(NSArray *)URLs;

@property (weak) IBOutlet NSMenuItem *themesMenuItem;

@end
