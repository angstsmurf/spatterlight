/*
 * Launcher -- the main application controller
 */

@interface AppDelegate : NSObject
{
    Preferences *prefctl;
    LibController *libctl;
    NSPanel *filePanel;
}

- (IBAction) openDocument: (id)sender;

- (IBAction) showPrefs: (id)sender;
- (IBAction) showLibrary: (id)sender;
- (IBAction) showHelpFile: (id)sender;

- (void) openPanelDidEnd: (NSOpenPanel*)panel returnCode: (int)code contextInfo: (void*)ctx;

@end
