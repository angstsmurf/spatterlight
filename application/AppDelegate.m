/*
 * Application -- the main application controller
 */

#import "main.h"
#import "Compatibility.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation AppDelegate

NSArray *gGameFileTypes;
NSDictionary *gExtMap;
NSDictionary *gFormatMap;

- (void) awakeFromNib
{
    //NSLog(@"appdel: awakeFromNib");

    gGameFileTypes = @[@"d$$", @"dat", @"sna",
                      @"advsys", @"quill",
                      @"l9", @"mag", @"a3c", @"acd", @"agx", @"gam", @"t3", @"hex", @"taf",
                      @"z3", @"z4", @"z5", @"z7", @"z8", @"ulx",
                      @"blb", @"blorb", @"glb", @"gblorb", @"zlb", @"zblorb"];

    gExtMap = @{@"acd": @"alan2",
               @"a3c": @"alan3",
               @"d$$": @"agility"};

    gFormatMap = @{@"adrift": @"scare",
                  @"advsys": @"advsys",
                  @"agt": @"agility",
                  @"glulx": @"glulxe",
                  @"hugo": @"hugo",
                  @"level9": @"level9",
                  @"magscrolls": @"magnetic",
                  @"quill": @"unquill",
                  @"tads2": @"tadsr",
                  @"tads3": @"tadsr",
                  @"zcode": @"frotz"};

    addToRecents = YES;

    prefctl = [[Preferences alloc] initWithWindowNibName: @"PrefsWindow"];
    libctl = [[LibController alloc] initWithWindowNibName: @"LibraryWindow"];
    [libctl loadLibrary];
    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12)
    {
        [libctl.window setValue:@2 forKey:@"tabbingMode"];
    }
}

- (IBAction) showPrefs: (id)sender
{
    NSLog(@"appdel: showPrefs");
    [prefctl showWindow: nil];
}

- (IBAction) showLibrary: (id)sender
{
    NSLog(@"appdel: showLibrary");
    [libctl showWindow: nil];
}

- (IBAction) showHelpFile: (id)sender
{
//    NSLog(@"appdel: showHelpFile('%@')", [sender title]);
    id title = [sender title];
    id pathname = [NSBundle mainBundle].resourcePath;
    id filename = [NSString stringWithFormat: @"%@/docs/%@.rtf", pathname, title];

    NSURL *url = [NSURL fileURLWithPath:filename];
    NSError *error;

    if (!helpLicenseWindow)
    {
        helpLicenseWindow = [[HelpPanelController alloc] initWithWindowNibName:@"HelpPanelController"];
        helpLicenseWindow.window.minSize = NSMakeSize(290, 200);
    }

    NSAttributedString *content = [[NSAttributedString alloc] initWithURL:url
                                                                  options: @{NSDocumentTypeDocumentOption: NSRTFTextDocumentType}
                                                       documentAttributes:nil
                                                                    error:&error];

    [helpLicenseWindow showHelpFile:content withTitle:title];
}

/*
 * Forward some menu commands to libctl even if its window is closed.
 */

- (IBAction) addGamesToLibrary: (id)sender
{
    [libctl showWindow: sender];
    [libctl addGamesToLibrary: sender];
}

- (IBAction) importMetadata: (id)sender
{
    [libctl showWindow: sender];
    [libctl importMetadata: sender];
}

- (IBAction) exportMetadata: (id)sender
{
    [libctl showWindow: sender];
    [libctl exportMetadata: sender];
}

- (IBAction) deleteLibrary: (id)sender
{
    [libctl deleteLibrary: sender];
}


/*
 * Open and play game directly.
 */

- (IBAction) openDocument: (id)sender
{
    NSLog(@"appdel: openDocument");

    NSURL *directory = [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] objectForKey: @"GameDirectory"] isDirectory:YES];
    NSOpenPanel *panel;

    if (filePanel)
    {
        [filePanel makeKeyAndOrderFront: nil];
    }
    else
    {
        panel = [NSOpenPanel openPanel];

        panel.allowedFileTypes = gGameFileTypes;
        panel.directoryURL = directory;
        NSLog(@"directory = %@", directory);
        [panel beginWithCompletionHandler:^(NSInteger result){
            if (result == NSFileHandlingPanelOKButton) {
                NSURL*  theDoc = panel.URLs[0];
                {
                    NSString *pathString = theDoc.path.stringByDeletingLastPathComponent;
                    NSLog(@"directory = %@", directory);
                    if ([theDoc.path.pathExtension isEqualToString: @"sav"])
                        [[NSUserDefaults standardUserDefaults] setObject: pathString forKey: @"SaveDirectory"];
                    else
                        [[NSUserDefaults standardUserDefaults] setObject: pathString forKey: @"GameDirectory"];

                    [self application: NSApp openFile: theDoc.path];
                }
            }
        }];
    }
    filePanel = nil;
}

- (BOOL) application: (NSApplication*)theApp openFile: (NSString *)path
{
    NSLog(@"appdel: openFile '%@'", path);

    if ([path.pathExtension.lowercaseString isEqualToString: @"ifiction"])
    {
        [libctl importMetadataFromFile: path];
    }
    else
    {
        [libctl importAndPlayGame: path];
    }

    return YES;
}

- (BOOL) applicationOpenUntitledFile: (NSApplication*)theApp
{
    NSLog(@"appdel: applicationOpenUntitledFile");
    [self showLibrary: nil];
    return YES;
}

- (NSWindow *) preferencePanel
{
	return prefctl.window;
}

-(void)addToRecents:(NSArray*)URLs
{
    if (!addToRecents) {
        addToRecents = YES;
        return;
    }

    if (!theDocCont) {
        theDocCont = [NSDocumentController sharedDocumentController];
    }
	
	NSDocumentController *localDocCont = theDocCont;

    [URLs enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [localDocCont noteNewRecentDocumentURL:obj];
    }];
}


-(void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    /* This is called when we select a file from the Open Recent menu,
     so we don't add them again */

    addToRecents = NO;
    for (NSString *path in filenames)
    {
        [self application: NSApp openFile: path];
    }
    addToRecents = YES;

}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *)app
{
    NSArray *windows = app.windows;
    NSInteger count = windows.count;
    NSInteger alive = 0;

    NSLog(@"appdel: applicationShouldTerminate");

    while (count--)
    {
        NSWindow *window = windows[count];
        id glkctl = window.delegate;
        if ([glkctl isKindOfClass: [GlkController class]] && [glkctl isAlive])
            alive ++;
    }

    NSLog(@"appdel: windows=%lu alive=%ld", (unsigned long)[windows count], (long)alive);

    if (alive > 0)
    {
        NSString *msg = @"You still have one game running.\nAny unsaved progress will be lost.";
        if (alive > 1)
            msg = [NSString stringWithFormat: @"You have %ld games running.\nAny unsaved progress will be lost.", (long)alive];
        NSInteger choice = NSRunAlertPanel(@"Do you really want to quit?", @"%@", @"Quit", NULL, @"Cancel", msg);
        if (choice == NSAlertOtherReturn)
            return NSTerminateCancel;
    }

    return NSTerminateNow;
}

- (void) applicationWillTerminate: (NSNotification*)notification
{
    [libctl saveLibrary:self];
    if ([NSFontPanel.sharedFontPanel isVisible])
		[NSFontPanel.sharedFontPanel orderOut:self];
}

@end
