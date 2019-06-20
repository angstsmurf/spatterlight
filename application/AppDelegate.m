/*
 * Application -- the main application controller
 */

#import "Compatibility.h"
#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation AppDelegate

NSArray *gGameFileTypes;
NSDictionary *gExtMap;
NSDictionary *gFormatMap;

- (void)awakeFromNib {
    // NSLog(@"appdel: awakeFromNib");

    gGameFileTypes = @[
        @"d$$",   @"dat", @"sna",    @"advsys", @"quill", @"l9",  @"mag",
        @"a3c",   @"acd", @"agx",    @"gam",    @"t3",    @"hex", @"taf",
        @"z3",    @"z4",  @"z5",     @"z7",     @"z8",    @"ulx", @"blb",
        @"blorb", @"glb", @"gblorb", @"zlb",    @"zblorb"
    ];

    gExtMap = @{@"acd" : @"alan2", @"a3c" : @"alan3", @"d$$" : @"agility"};

    gFormatMap = @{
        @"adrift" : @"scare",
        @"advsys" : @"advsys",
        @"agt" : @"agility",
        @"glulx" : @"glulxe",
        @"hugo" : @"hugo",
        @"level9" : @"level9",
        @"magscrolls" : @"magnetic",
        @"quill" : @"unquill",
        @"tads2" : @"tadsr",
        @"tads3" : @"tadsr",
        @"zcode" : @"frotz"
    };
    //  @"zcode": @"fizmo"};

    addToRecents = YES;

    _prefctl = [[Preferences alloc] initWithWindowNibName:@"PrefsWindow"];
    _prefctl.window.restorable = YES;
    _prefctl.window.restorationClass = [self class];
    _prefctl.window.identifier = @"preferences";

    _libctl = [[LibController alloc] init];
    _libctl.window.restorable = YES;
    _libctl.window.restorationClass = [self class];
    _libctl.window.identifier = @"library";

    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12) {
        [_libctl.window setValue:@2 forKey:@"tabbingMode"];
    }
}

#pragma mark -
#pragma mark License window

- (void)setHelpLicenseWindow:(HelpPanelController *)newValue {
    _helpLicenseWindow = newValue;
}

- (HelpPanelController *)helpLicenseWindow {
    if (!_helpLicenseWindow) {
        _helpLicenseWindow = [[HelpPanelController alloc]
            initWithWindowNibName:@"HelpPanelController"];
        _helpLicenseWindow.window.restorable = YES;
        _helpLicenseWindow.window.restorationClass = [AppDelegate class];
        _helpLicenseWindow.window.identifier = @"licenseWin";
        _helpLicenseWindow.window.minSize = NSMakeSize(290, 200);
    }
    return _helpLicenseWindow;
}

- (IBAction)showHelpFile:(id)sender {
    //    NSLog(@"appdel: showHelpFile('%@')", [sender title]);
    id title = [sender title];
    id pathname = [NSBundle mainBundle].resourcePath;
    id filename =
        [NSString stringWithFormat:@"%@/docs/%@.rtf", pathname, title];

    NSURL *url = [NSURL fileURLWithPath:filename];
    NSError *error;

    if (!_helpLicenseWindow) {
        _helpLicenseWindow = [self helpLicenseWindow];
    }

    NSAttributedString *content = [[NSAttributedString alloc]
               initWithURL:url
                   options:@{
                       NSDocumentTypeDocumentOption : NSRTFTextDocumentType
                   }
        documentAttributes:nil
                     error:&error];

    [_helpLicenseWindow showHelpFile:content withTitle:title];
}

#pragma mark -
#pragma mark Windows restoration

+ (void)restoreWindowWithIdentifier:(NSString *)identifier
                              state:(NSCoder *)state
                  completionHandler:
                      (void (^)(NSWindow *, NSError *))completionHandler {
//    NSLog(@"restoreWindowWithIdentifier called with identifier %@", identifier);
    NSWindow *window = nil;
    AppDelegate *appDelegate = (AppDelegate *)[NSApp delegate];
    if ([identifier isEqualToString:@"library"]) {
        window = appDelegate.libctl.window;
    } else if ([identifier isEqualToString:@"licenseWin"]) {
        window = appDelegate.helpLicenseWindow.window;
    } else if ([identifier isEqualToString:@"preferences"]) {
        window = appDelegate.prefctl.window;
    } else {
        NSString *firstLetters =
            [identifier substringWithRange:NSMakeRange(0, 7)];

        if ([firstLetters isEqualToString:@"infoWin"]) {
            NSString *path = [identifier substringFromIndex:7];
            if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
                InfoController *infoctl =
                    [[InfoController alloc] initWithpath:path];
                NSWindow *infoWindow = infoctl.window;
                infoWindow.restorable = YES;
                infoWindow.restorationClass = [AppDelegate class];
                infoWindow.identifier =
                    [NSString stringWithFormat:@"infoWin%@", path];
                [appDelegate.libctl.infoWindows setObject:infoctl forKey:path];
                window = infoctl.window;
            }
        } else if ([firstLetters isEqualToString:@"gameWin"]) {
            NSString *ifid = [identifier substringFromIndex:7];
            window = [appDelegate.libctl playGameWithIFID:ifid];
        }
    }
    completionHandler(window, nil);
}

#pragma mark -
#pragma mark Library

- (IBAction)showLibrary:(id)sender {
//    NSLog(@"appdel: showLibrary");
    [_libctl showWindow:nil];
}

/*
 * Forward some menu commands to libctl even if its window is closed.
 */

- (IBAction)addGamesToLibrary:(id)sender {
    [_libctl showWindow:sender];
    [_libctl addGamesToLibrary:sender];
}

- (IBAction)importMetadata:(id)sender {
    [_libctl showWindow:sender];
    [_libctl importMetadata:sender];
}

- (IBAction)exportMetadata:(id)sender {
    [_libctl showWindow:sender];
    [_libctl exportMetadata:sender];
}

- (IBAction)deleteLibrary:(id)sender {
    [_libctl deleteLibrary:sender];
}

/*
 * Open and play game directly.
 */

- (IBAction)openDocument:(id)sender {
    NSLog(@"appdel: openDocument");

    NSURL *directory =
        [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults]
                                   objectForKey:@"GameDirectory"]
                   isDirectory:YES];
    NSOpenPanel *panel;

    if (filePanel) {
        [filePanel makeKeyAndOrderFront:nil];
    } else {
        panel = [NSOpenPanel openPanel];

        panel.allowedFileTypes = gGameFileTypes;
        panel.directoryURL = directory;
        NSLog(@"directory = %@", directory);
        [panel beginWithCompletionHandler:^(NSInteger result) {
            if (result == NSFileHandlingPanelOKButton) {
                NSURL *theDoc = [panel.URLs objectAtIndex:0];
                {
                    NSString *pathString =
                        theDoc.path.stringByDeletingLastPathComponent;
                    NSLog(@"directory = %@", directory);
                    if ([theDoc.path.pathExtension isEqualToString:@"sav"])
                        [[NSUserDefaults standardUserDefaults]
                            setObject:pathString
                               forKey:@"SaveDirectory"];
                    else
                        [[NSUserDefaults standardUserDefaults]
                            setObject:pathString
                               forKey:@"GameDirectory"];

                    [self application:NSApp openFile:theDoc.path];
                }
            }
        }];
    }
    filePanel = nil;
}

- (BOOL)application:(NSApplication *)theApp openFile:(NSString *)path {
    NSLog(@"appdel: openFile '%@'", path);

    if ([path.pathExtension.lowercaseString isEqualToString:@"ifiction"]) {
        [_libctl importMetadataFromFile:path];
    } else {
        [_libctl importAndPlayGame:path];
    }

    return YES;
}

- (BOOL)applicationOpenUntitledFile:(NSApplication *)theApp {
    NSLog(@"appdel: applicationOpenUntitledFile");
    [self showLibrary:nil];
    return YES;
}

- (NSWindow *)preferencePanel {
    return _prefctl.window;
}

- (IBAction)showPrefs:(id)sender {
    //    NSLog(@"appdel: showPrefs");
    [_prefctl showWindow:nil];
}

- (void)addToRecents:(NSArray *)URLs {
    if (!addToRecents) {
        addToRecents = YES;
        return;
    }

    if (!theDocCont) {
        theDocCont = [NSDocumentController sharedDocumentController];
    }

    __weak NSDocumentController *localDocCont = theDocCont;

    [URLs enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [localDocCont noteNewRecentDocumentURL:obj];
    }];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames {
    /* This is called when we select a file from the Open Recent menu,
       so we don't add them again */

    addToRecents = NO;
    for (NSString *path in filenames) {
        [self application:NSApp openFile:path];
    }
    addToRecents = YES;
}

#pragma mark - Core Data stack

@synthesize coreDataManager = _coreDataManager;

- (CoreDataManager *)coreDataManager {
    // The persistent container for the application. This implementation creates and returns a container, having loaded the store for the application to it.
    @synchronized (self) {
        if (_coreDataManager == nil) {
            _coreDataManager = [[CoreDataManager alloc] initWithModelName:@"Spatterlight"];
        }
    }

    return _coreDataManager;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)app {
    NSArray *windows = app.windows;
    NSInteger count = windows.count;
    NSInteger alive = 0;
    NSInteger restorable = 0;

    NSLog(@"appdel: applicationShouldTerminate");

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults boolForKey:@"terminationAlertSuppression"]) {
        NSLog(@"Termination alert suppressed");
    } else {
        while (count--) {
            NSWindow *window = [windows objectAtIndex:count];
            id glkctl = window.delegate;
            if ([glkctl isKindOfClass:[GlkController class]] &&
                [glkctl isAlive]) {
                if (((GlkController *)glkctl).supportsAutorestore) {
                    restorable++;
                } else {
                    alive++;
                }
            }
        }

        NSLog(@"appdel: windows=%lu alive=%ld", (unsigned long)[windows count],
              (long)alive);

        if (alive > 0) {
            NSString *msg = @"You still have one game running.\nAny unsaved "
                            @"progress will be lost.";
            if (alive > 1)
                msg = [NSString
                    stringWithFormat:@"You have %ld games running.\nAny "
                                     @"unsaved progress will be lost.",
                                     (long)alive];
            if (restorable == 1)
                msg = [msg
                    stringByAppendingString:
                        @"\n(There is also an autorestorable game running.)"];
            else if (restorable > 1)
                msg = [msg
                    stringByAppendingFormat:
                        @"\n(There are also %ld autorestorable games running.)",
                        restorable];

            NSAlert *anAlert = [[NSAlert alloc] init];
            anAlert.messageText = @"Do you really want to quit?";

            anAlert.informativeText = msg;
            anAlert.showsSuppressionButton = YES; // Uses default checkbox title

            [anAlert addButtonWithTitle:@"Quit"];
            [anAlert addButtonWithTitle:@"Cancel"];

            NSInteger choice = [anAlert runModal];

            if (anAlert.suppressionButton.state == NSOnState) {
                // Suppress this alert from now on
                [defaults setBool:YES forKey:@"terminationAlertSuppression"];
            }
            if (choice == NSAlertOtherReturn) {
                return NSTerminateCancel;
            }
        }
    }
    // Save changes in the application's managed object context before the application terminates.

    if (!_managedObjectContext) {
        return NSTerminateNow;
    }

    if (![[self managedObjectContext] commitEditing]) {
        NSLog(@"%@:%@ unable to commit editing to terminate", [self class], NSStringFromSelector(_cmd));
        return NSTerminateCancel;
    }

    if (![[self managedObjectContext] hasChanges]) {
        return NSTerminateNow;
    }

    NSError *error = nil;
    if (![[self managedObjectContext] save:&error]) {

        // Customize this code block to include application-specific recovery steps.
        BOOL result = [app presentError:error];
        if (result) {
            return NSTerminateCancel;
        }

        NSString *question = NSLocalizedString(@"Could not save changes while quitting. Quit anyway?", @"Quit without saves error question message");
        NSString *info = NSLocalizedString(@"Quitting now will lose any changes you have made since the last successful save", @"Quit without saves error question info");
        NSString *quitButton = NSLocalizedString(@"Quit anyway", @"Quit anyway button title");
        NSString *cancelButton = NSLocalizedString(@"Cancel", @"Cancel button title");
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:question];
        [alert setInformativeText:info];
        [alert addButtonWithTitle:quitButton];
        [alert addButtonWithTitle:cancelButton];

        NSInteger answer = [alert runModal];

        if (answer == NSAlertAlternateReturn) {
            return NSTerminateCancel;
        }
    }
    
    return NSTerminateNow;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    [_libctl saveLibrary:self];

    for (GlkController *glkctl in [_libctl.gameSessions allValues]) {
        [glkctl autoSaveOnExit];
    }

    if ([[NSFontPanel sharedFontPanel] isVisible]) {
        [[NSFontPanel sharedFontPanel] orderOut:self];
    }
    [_coreDataManager saveChanges];
}



// Returns the NSUndoManager for the application. In this case, the manager returned is that of the managed object context for the application.
- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window
{
    return [[self managedObjectContext] undoManager];
}

// Performs the save action for the application, which is to send the save: message to the application's managed object context. Any encountered errors are presented to the user.
//- (IBAction)saveAction:(id)sender
//{
//    NSError *error = nil;
//
//    if (![[self managedObjectContext] commitEditing]) {
//        NSLog(@"%@:%@ unable to commit editing before saving", [self class], NSStringFromSelector(_cmd));
//    }
//
//    if (![[self managedObjectContext] save:&error]) {
//        [[NSApplication sharedApplication] presentError:error];
//    }
//}

@end
