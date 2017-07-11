/*
 * Launcher -- the main application controller
 */
#import <CoreData/CoreData.h>
#import "HelpPanelController.h"

@interface AppDelegate : NSObject < NSWindowDelegate >
{
    Preferences *prefctl;
    LibController *libctl;
    NSPanel *filePanel;
    HelpPanelController *helpLicenseWindow;
    NSDocumentController *theDocCont;
    BOOL addToRecents;

}

- (IBAction) openDocument: (id)sender;

- (IBAction) showPrefs: (id)sender;
- (IBAction) showLibrary: (id)sender;
- (IBAction) showHelpFile: (id)sender;

- (void) addToRecents:(NSArray*)URLs;
- (void) updatePreferencePanel;


@property (readonly, strong) NSPersistentContainer *persistentContainer;

@end
