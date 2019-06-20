/*
 * Launcher -- the main application controller
 */

#import <CoreData/CoreData.h>

#import "CoreDataManager.h"
#import "HelpPanelController.h"
#import "InfoController.h"

@interface AppDelegate : NSObject <NSWindowDelegate, NSWindowRestoration> {
    HelpPanelController *_helpLicenseWindow;
    NSPanel *filePanel;
    NSDocumentController *theDocCont;
    BOOL addToRecents;
}

@property Preferences *prefctl;
@property LibController *libctl;
@property HelpPanelController *helpLicenseWindow;

@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSManagedObjectContext *managedObjectContext;


- (IBAction)openDocument:(id)sender;

- (IBAction)showPrefs:(id)sender;
- (IBAction)showLibrary:(id)sender;
- (IBAction)showHelpFile:(id)sender;

- (void)addToRecents:(NSArray *)URLs;

@end
