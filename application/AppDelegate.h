/*
 * Launcher -- the main application controller
 */

@interface AppDelegate : NSObject < NSWindowDelegate >
{
    Preferences *prefctl;
    LibController *libctl;
    NSPanel *filePanel;
    NSDocumentController *theDocCont;
    BOOL addToRecents;
}

- (IBAction) openDocument: (id)sender;

- (IBAction) showPrefs: (id)sender;
- (IBAction) showLibrary: (id)sender;
- (IBAction) showHelpFile: (id)sender;

- (void) addToRecents:(NSArray*)URLs;

@end
