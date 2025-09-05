/*
 * Application -- the main application controller
 */

#import <CoreSpotlight/CoreSpotlight.h>

#import "main.h"
#import "Constants.h"
#import "CoreDataManager.h"
#import "NSString+Categories.h"
#import "AttributeDictionaryTransformer.h"
#import "InsecureValueTransformer.h"
#import "ColorTransformer.h"
#import "HelpPanelController.h"
#import "InfoController.h"
#import "FolderAccess.h"
#import "LibController.h"
#import "TableViewController.h"
#import "Game.h"
#import "Theme.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface AppDelegate () <NSWindowDelegate, NSWindowRestoration> {
    HelpPanelController *_helpLicenseWindow;
    NSDocumentController *theDocCont;
    BOOL addToRecents;
}
@end

@implementation AppDelegate

NSArray *gGameFileTypes;
NSArray *gDocFileTypes;
NSArray *gSaveFileTypes;

NSArray *gBufferStyleNames;
NSArray *gGridStyleNames;

NSDictionary *gExtMap;
NSDictionary *gFormatMap;
NSMutableDictionary<NSURL *, FolderAccess *> *globalBookmarks;


NSPasteboardType PasteboardFileURLPromise,
PasteboardFilePromiseContent,
PasteboardFilePasteLocation;

- (void)awakeFromNib {
    [super awakeFromNib];

    InsecureValueTransformer *insecureValueTransformer = nil;

    if (@available(macOS 10.14, *)) {
        AttributeDictionaryTransformer *dicTransformer = [AttributeDictionaryTransformer new];
        [NSValueTransformer setValueTransformer:dicTransformer
                                        forName:@"AttributeDictionaryTransformer"];

        ColorTransformer *colorTransformer = [ColorTransformer new];
        [NSValueTransformer setValueTransformer:colorTransformer
                                        forName:@"ColorTransformer"];
    } else {
        insecureValueTransformer = [InsecureValueTransformer new];
        [NSValueTransformer setValueTransformer:insecureValueTransformer
                                        forName:@"AttributeDictionaryTransformer"];
        [NSValueTransformer setValueTransformer:insecureValueTransformer
                                        forName:@"ColorTransformer"];
        [NSValueTransformer setValueTransformer:insecureValueTransformer
                                        forName:@"NSSecureUnarchiveFromData"];
    }

    gGameFileTypes = @[
        @"d$$", @"dat", @"sna",  @"z80",  @"tap",   @"tzx",   @"advsys",
        @"l9",  @"mag", @"a3c",  @"acd",  @"agx",   @"gam",   @"t3",
        @"hex", @"taf", @"z1",   @"z2",   @"z3",    @"z4",    @"z5",
        @"z6",  @"z7",  @"z8",   @"cas",  @"asl",   @"saga",  @"jacl",
        @"j2",  @"ulx", @"blb",  @"zlb",  @"blorb", @"glb",   @"gblorb",
        @"d64", @"t64", @"fiad", @"dsk",  @"quill", @"zblorb",@"atr",
        @"st",  @"msa", @"woz",  @"sag",  @"plus",  @"plu",   @"tay",
        @"taylor"
    ];

    gDocFileTypes = @[@"rtf", @"rtfd", @"html", @"doc", @"docx", @"odt", @"xml", @"webarchive", @"txt"];

    gSaveFileTypes = @[@"glksave", @"sav", @"qut", @"qzl"];

    // To map the Glk style indices onto our Core Data relation names
    gBufferStyleNames = @[
                       @"bufferNormal",
                       @"bufEmph",
                       @"bufPre",
                       @"bufHead",
                       @"bufSubH",
                       @"bufAlert",
                       @"bufNote",
                       @"bufBlock",
                       @"bufInput",
                       @"bufUsr1",
                       @"bufUsr2",
                       ];

    gGridStyleNames = @[
                       @"gridNormal",
                       @"gridEmph",
                       @"gridPre",
                       @"gridHead",
                       @"gridSubH",
                       @"gridAlert",
                       @"gridNote",
                       @"gridBlock",
                       @"gridInput",
                       @"gridUsr1",
                       @"gridUsr2",
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
                   @"zcode" : @"bocfel",
                   @"quest4" : @"geas",
                   @"jacl" : @"jacl",
                   @"sagaplus" : @"plus",
                   @"scott" : @"scott",
                   @"taylor" : @"taylor"
                   };

    PasteboardFileURLPromise = (NSPasteboardType)kPasteboardTypeFileURLPromise;
    PasteboardFilePromiseContent = (NSPasteboardType)kPasteboardTypeFilePromiseContent;
    PasteboardFilePasteLocation = (NSPasteboardType)@"com.apple.pastelocation";

    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"HasDeletedSecurityBookmarks"]) {
        [FolderAccess deleteBookmarks];
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasDeletedSecurityBookmarks"];
    }

    [FolderAccess loadBookmarks];

    addToRecents = YES;

    if (!self.coreDataManager.persistentContainer) {
        NSLog(@"No persistentContainer?");
        abort();
    }

    NSStoryboard *storyboard = [NSStoryboard storyboardWithName:@"SplitView" bundle:nil];
    _libctl = (LibController *)[storyboard instantiateControllerWithIdentifier:@"LibraryWindowController"];
    _libctl.tableViewController = _tableViewController;
    _libctl.window.restorable = YES;
    _libctl.window.restorationClass = [self class];
    _libctl.window.identifier = @"library";

    _prefctl = [[Preferences alloc] initWithWindowNibName:@"PrefsWindow"];
    _prefctl.window.restorable = YES;
    _prefctl.window.restorationClass = [self class];
    _prefctl.window.identifier = @"preferences";
    _prefctl.libcontroller = _tableViewController;
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
    NSString *title = [sender title];
    NSURL *url = [[NSBundle mainBundle] URLForResource:title
                                         withExtension:@"rtf"
                                          subdirectory:@"docs"];
    NSError *error;

    if (!_helpLicenseWindow) {
        _helpLicenseWindow = self.helpLicenseWindow;
    }

    NSAttributedString *content = [[NSAttributedString alloc]
                                   initWithURL:url
                                   options:@{ NSDocumentTypeDocumentOption :
                                                  NSRTFTextDocumentType }
                                   documentAttributes:nil
                                   error:&error];

    [_helpLicenseWindow showHelpFile:content withTitle:title];
    _helpLicenseWindow.window.representedFilename = url.path;
}

#pragma mark -
#pragma mark Windows restoration

+ (void)restoreWindowWithIdentifier:(NSString *)identifier
                              state:(NSCoder *)state
                  completionHandler:
(void (^)(NSWindow *, NSError *))completionHandler {
    NSWindow *window = nil;
    AppDelegate *appDelegate = (AppDelegate *)NSApp.delegate;
    if ([identifier isEqualToString:@"library"]) {
        window = appDelegate.libctl.window;
    } else if ([identifier isEqualToString:@"licenseWin"]) {
        window = appDelegate.helpLicenseWindow.window;
    } else if ([identifier isEqualToString:@"preferences"]) {
        window = appDelegate.prefctl.window;
    } else {
        NSString *firstLetters = [identifier substringToIndex:7];
        NSString *hashTag = [identifier substringFromIndex:7];

        if ([firstLetters isEqualToString:@"infoWin"]) {
            InfoController *infoctl =
            [[InfoController alloc] initWithHash:hashTag];
            infoctl.libcontroller = appDelegate.libctl.tableViewController;
            NSWindow *infoWindow = infoctl.window;
            infoWindow.restorable = YES;
            infoWindow.restorationClass = [AppDelegate class];
            infoWindow.identifier =
            [NSString stringWithFormat:@"infoWin%@", hashTag];
            (appDelegate.libctl.tableViewController.infoWindows)[hashTag] = infoctl;
            window = infoctl.window;
        } else if ([firstLetters isEqualToString:@"gameWin"]) {
            window = [appDelegate.libctl.tableViewController playGameWithHash:hashTag restorationHandler:[completionHandler copy]];
            if (window)
                return;
        }
    }
    completionHandler(window, nil);
}

#pragma mark -
#pragma mark Library

- (IBAction)showLibrary:(id)sender {
    [_libctl showWindow:nil];
}

/*
 * Forward some menu commands to libctl even if its window is closed.
 */

- (IBAction)addGamesToLibrary:(id)sender {
    [_libctl showWindow:sender];
    [_tableViewController addGamesToLibrary:sender];
}

- (IBAction)importMetadata:(id)sender {
    [_libctl showWindow:sender];
    [_tableViewController importMetadata:sender];
}

- (IBAction)exportMetadata:(id)sender {
    [_libctl showWindow:sender];
    [_tableViewController exportMetadata:sender];
}

- (IBAction)deleteLibrary:(id)sender {
    [_libctl showWindow:sender];
    [_tableViewController deleteLibrary:sender];
}

- (IBAction)pruneLibrary:(id)sender {
    [_libctl showWindow:sender];
    [_tableViewController pruneLibrary:sender];
}

/*
 * Open and play game directly.
 */

- (IBAction)openDocument:(id)sender {
    NSLog(@"appdel: openDocument");

    NSArray *windows = NSApp.windows;
    for (NSWindow *window in windows) {
        if ([window isKindOfClass:[NSOpenPanel class]]) {
            [window makeKeyAndOrderFront:nil];
            return;
        }
    }

    NSURL *directory =
    [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults]
                            objectForKey:@"GameDirectory"]
               isDirectory:YES];

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSMutableArray *allowedTypes = gGameFileTypes.mutableCopy;
    if ([_tableViewController hasActiveGames]) {
        [allowedTypes addObjectsFromArray:gDocFileTypes];
        [allowedTypes addObjectsFromArray:gSaveFileTypes];
    }
    panel.allowedFileTypes = allowedTypes;
    panel.directoryURL = directory;
    panel.message = NSLocalizedString(@"Please select a game", nil);

    panel.accessoryView = _tableViewController.addToLibraryCheckBox;
    NSButton *addToLibraryCheckBox = (NSButton *)panel.accessoryView;
    addToLibraryCheckBox.state = [[NSUserDefaults standardUserDefaults]
                      boolForKey:@"AddToLibrary"];

    [panel beginWithCompletionHandler:^(NSInteger result) {
        NSButton *blockAddToLibraryCheckBox = (NSButton*)panel.accessoryView;
        if (result == NSModalResponseOK) {
            BOOL addToLibrary = (blockAddToLibraryCheckBox.state == NSOnState); ;
            [[NSUserDefaults standardUserDefaults]
             setBool:addToLibrary forKey:@"AddToLibrary"];
            [Preferences instance].addToLibraryCheckbox.state = blockAddToLibraryCheckBox.state;

            NSURL *theDoc = panel.URLs.firstObject;
            if (theDoc) {
                NSString *pathString =
                theDoc.path.stringByDeletingLastPathComponent;
                if ([gSaveFileTypes indexOfObject:theDoc.path.pathExtension] != NSNotFound)
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

- (BOOL)application:(NSApplication *)theApp openFile:(NSString *)path {
    NSLog(@"appdel: openFile '%@'", path);

    NSString *extension = path.pathExtension.lowercaseString;

    if ([extension isEqualToString:@"ifiction"]) {
        [_tableViewController waitToReportMetadataImport];
        [_tableViewController importMetadataFromFile:path inContext:_libctl.managedObjectContext];
    } else if ([gDocFileTypes indexOfObject:extension] != NSNotFound) {
        [_tableViewController runCommandsFromFile:path];
    } else if ([gSaveFileTypes indexOfObject:extension] != NSNotFound) {
        [_tableViewController restoreFromSaveFile:path];
    } else {
        [_tableViewController importAndPlayGame:path];
    }

    return YES;
}

- (BOOL)applicationOpenUntitledFile:(NSApplication *)theApp {
    NSLog(@"appdel: applicationOpenUntitledFile");

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ShowLibrary"]) {
        [self showLibrary:nil];
        return YES;
    } else {
        [self openDocument:nil];
    }
    return NO;
}

- (BOOL)application:(NSApplication *)application
continueUserActivity:(NSUserActivity *)userActivity
 restorationHandler:(void (^)(NSArray<id<NSUserActivityRestoring>> *restorableObjects))restorationHandler {
    NSLog(@"continueUserActivity");

    // We're coming from a search result
    NSString *searchableItemIdentifier = userActivity.userInfo[CSSearchableItemActivityIdentifier];

    if (searchableItemIdentifier.length) {
        NSLog(@"searchableItemIdentifier: %@", searchableItemIdentifier);

        NSManagedObjectContext *context = _coreDataManager.mainManagedObjectContext;
        if (!context) {
            NSLog(@"Could not create new context!");
            return NO;
        }

        NSURL *uri = [NSURL URLWithString:searchableItemIdentifier];
        NSManagedObjectID *objectID = [context.persistentStoreCoordinator managedObjectIDForURIRepresentation:uri];

        id object = [context objectWithID:objectID];

        if (!object) {
            return NO;
        }

        [_tableViewController handleSpotlightSearchResult:object];
    }

    return YES;
}

- (NSWindow *)preferencePanel {
    return _prefctl.window;
}

- (IBAction)showPrefs:(id)sender {
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

    NSDocumentController __weak *localDocCont = theDocCont;

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
    NSUInteger count = windows.count;
    NSInteger alive = 0;
    NSInteger restorable = 0;

    NSLog(@"appdel: applicationShouldTerminate");

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableArray *games = [[NSMutableArray alloc] initWithCapacity:count];

    GlkController *key = NULL;

    while (count--) {
        NSWindow *window = windows[count];
        id glkctl = window.delegate;
        if ([glkctl isKindOfClass:[GlkController class]] &&
            [glkctl isAlive]) {
            if (((GlkController *)glkctl).supportsAutorestore && ((GlkController *)glkctl).theme.autosave) {
                restorable++;
                if (window.keyWindow)
                    key = glkctl;
            } else {
                alive++;
                Game *game = ((GlkController *)glkctl).game;
                if (game)
                    [games addObject:game];
            }
        }
    }

    if ([defaults boolForKey:@"TerminationAlertSuppression"]) {
        NSLog(@"Termination alert suppressed");
    } else {
        if (alive > 0) {
            NSString *msg = [NSString stringWithFormat:@"%@ %@ still running.\nAny unsaved progress will be lost.",
                             [NSString stringWithSummaryOfGames:games], (alive == 1) ? @"is" : @"are"];

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
            anAlert.messageText = NSLocalizedString(@"Do you really want to quit?", nil);

            anAlert.informativeText = msg;
            anAlert.showsSuppressionButton = YES; // Uses default checkbox title

            [anAlert addButtonWithTitle:NSLocalizedString(@"Quit", nil)];
            [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

            NSInteger choice = [anAlert runModal];

            if (anAlert.suppressionButton.state == NSOnState) {
                // Suppress this alert from now on
                [defaults setBool:YES forKey:@"TerminationAlertSuppression"];
            }
            if (choice == NSAlertSecondButtonReturn) {
                return NSTerminateCancel;
            }
        }
    }

    // We remember which window is key on termination
    // in order to make it key on restoration
    [defaults setObject:key.game.hashTag forKey:@"KeyWindowController"];

    return NSTerminateNow;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    NSWindow.allowsAutomaticWindowTabbing = NO;
}

- (void)applicationWillTerminate:(NSNotification *)notification {

    NSManagedObjectContext *mainContext = _coreDataManager.mainManagedObjectContext;

    [mainContext performBlockAndWait:^{
        NSError *error = nil;
        if (mainContext.hasChanges) {
            [mainContext save:&error];
            if (error) {
                NSLog(@"Saving managed object context on quit failed. %@", error.localizedDescription);
            }
        }
    }];

    [FolderAccess listActiveSecurityBookmarks];
}


// Returns the NSUndoManager for the application. In this case, the manager returned is that of the managed object context for the application.
- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window
{
    return (_coreDataManager.mainManagedObjectContext).undoManager;
}

- (void)applicationDidBecomeActive:(NSNotification *)notification {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        NSApplication *app = (NSApplication *)notification.object;
        BOOL visibleWindows = NO;
        for (NSWindow *win in app.windows)
            if (win.visible)
                visibleWindows = YES;
        if (!visibleWindows) {
            if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ShowLibrary"]) {
                [self showLibrary:nil];
            } else {
                [self openDocument:nil];
            }
        }
    });
}

- (void)applicationDidUpdate:(NSNotification *)notification {
    NSApplication *app = (NSApplication *)notification.object;
    for (NSWindow *win in app.windows) {
        if (([win isKindOfClass:[NSFontPanel class]] || [win isKindOfClass:[NSColorPanel class]]) && win.visible) {
            win.restorable = NO;
        }
    }
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
    return YES;
}

@end
