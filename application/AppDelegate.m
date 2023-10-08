/*
 * Application -- the main application controller
 */

#import <CoreSpotlight/CoreSpotlight.h>

#import "main.h"
#import "Constants.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface AppDelegate () <NSWindowDelegate> {
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

    addToRecents = YES;

    NSStoryboard *storyboard = [NSStoryboard storyboardWithName:@"SplitView" bundle:nil];
}

#pragma mark -
#pragma mark License window

- (IBAction)showHelpFile:(id)sender {
    NSString *title = [sender title];
    NSURL *url = [[NSBundle mainBundle] URLForResource:title
                                         withExtension:@"rtf"
                                          subdirectory:@"docs"];
    NSError *error;


    NSAttributedString *content = [[NSAttributedString alloc]
                                   initWithURL:url
                                   options:@{ NSDocumentTypeDocumentOption :
                                                  NSRTFTextDocumentType }
                                   documentAttributes:nil
                                   error:&error];


}

/*
 * Forward some menu commands to libctl even if its window is closed.
 */

/*
 * Open and play game directly.
 */

- (IBAction)openDocument:(id)sender {
    NSLog(@"appdel: openDocument");

    NSArray *windows = [NSApplication sharedApplication].windows;
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
    panel.allowedFileTypes = allowedTypes;
    panel.directoryURL = directory;
    panel.message = NSLocalizedString(@"Please select a game", nil);

    NSButton *checkbox = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    checkbox.buttonType = NSSwitchButton;
    checkbox.title = NSLocalizedString(@"Add to library", @"");
    checkbox.state = [[NSUserDefaults standardUserDefaults]
                      boolForKey:@"AddToLibrary"];
    panel.accessoryView = checkbox;

    [panel beginWithCompletionHandler:^(NSInteger result) {
        if (result == NSModalResponseOK) {
            NSButton *finalButton = (NSButton*)panel.accessoryView;
            BOOL addToLibrary = (finalButton.state == NSOnState); ;
            [[NSUserDefaults standardUserDefaults]
             setBool:addToLibrary forKey:@"AddToLibrary"];

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

    return YES;
}

- (BOOL)applicationOpenUntitledFile:(NSApplication *)theApp {
    NSLog(@"appdel: applicationOpenUntitledFile");

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ShowLibrary"]) {
        return YES;
    } else {
        [self openDocument:nil];
    }
    return NO;
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
            if (((GlkController *)glkctl).supportsAutorestore) {
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

    return NSTerminateNow;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    NSWindow.allowsAutomaticWindowTabbing = NO;
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

@end
