#import "PreferencesPrivate.h"

#import "Constants.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "GlkStyle.h"
#import "LibraryOrganizer.h"
#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "TableViewController+LibraryManagement.h"
#import "Theme.h"
#import "NSAlert+Spatterlight.h"

@implementation Preferences (LibraryOrganiser)

#pragma mark Keep games organised

- (void)updateLibraryOrganiserControls {
    BOOL on = [LibraryOrganizer keepGamesOrganised];
    _keepOrganisedCheckbox.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    _chooseLibraryButton.enabled = on;

    LibraryOrganizer *org = [LibraryOrganizer sharedOrganizer];
    NSURL *defaultRoot = org.defaultLibraryRootURL;
    NSURL *custom = org.customLibraryURL;
    NSURL *root = custom ?: defaultRoot;
    NSString *path = root.path.stringByAbbreviatingWithTildeInPath;
    // Also treat a custom location that points at the default folder as default.
    if ([root.path isEqualToString:defaultRoot.path])
        path = [path stringByAppendingString:NSLocalizedString(@" (default)", nil)];
    _libraryLocationField.stringValue = path ?: @"";
    _libraryLocationField.toolTip = root.path;
}

- (IBAction)changeKeepOrganised:(id)sender {
    BOOL on = ([sender state] == NSControlStateValueOn);
    [[NSUserDefaults standardUserDefaults] setBool:on forKey:kKeepGamesOrganisedKey];
    [self updateLibraryOrganiserControls];
    if (on) {
        [self organiseAllGames:nil];
    }
}

- (IBAction)chooseLibraryLocation:(id)sender {
    LibraryOrganizer *org = [LibraryOrganizer sharedOrganizer];
    NSURL *custom = org.customLibraryURL;
    // Remember where the library actually is now, so that whichever folder the
    // user ends up choosing (possibly after using "Go to default", which
    // re-opens the panel) is compared against the real current location.
    self.libraryChooserOriginalRoot = custom ?: org.defaultLibraryRootURL;
    [self presentLibraryChooserFromURL:self.libraryChooserOriginalRoot];
}

- (void)presentLibraryChooserFromURL:(NSURL *)startURL {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.canCreateDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.directoryURL = startURL;

    panel.prompt = NSLocalizedString(@"Choose", nil);
    panel.message = NSLocalizedString(@"Choose a folder to keep your game library in.", nil);

    NSButton *defaultButton =
        [NSButton buttonWithTitle:NSLocalizedString(@"Go to default", nil)
                           target:self
                           action:@selector(goToDefaultLibraryLocation:)];
    defaultButton.bezelStyle = NSBezelStyleRounded;
    [defaultButton sizeToFit];
    NSView *accessory = [[NSView alloc] initWithFrame:
        NSMakeRect(0, 0, NSWidth(defaultButton.frame) + 40, NSHeight(defaultButton.frame) + 20)];
    defaultButton.frameOrigin = NSMakePoint(20, 10);
    [accessory addSubview:defaultButton];
    panel.accessoryView = accessory;
    self.libraryLocationPanel = panel;

    Preferences * __weak weakSelf = self;
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result != NSModalResponseOK)
            return;
        BOOL locationChanged =
            ![panel.URL.path isEqualToString:weakSelf.libraryChooserOriginalRoot.path];
        NSError *error = nil;
        if (![[LibraryOrganizer sharedOrganizer] setCustomLibraryURL:panel.URL error:&error]) {
            NSAlert *alert = [NSAlert alertWithError:error];
            [alert beginSheetModalForWindow:weakSelf.window completionHandler:nil];
            return;
        }
        [weakSelf updateLibraryOrganiserControls];
        if (locationChanged &&
            [[NSUserDefaults standardUserDefaults] boolForKey:kKeepGamesOrganisedKey])
            [weakSelf organiseAllGames:nil];
    }];
}

- (void)goToDefaultLibraryLocation:(NSButton *)sender {
    NSURL *defaultURL = [LibraryOrganizer sharedOrganizer].defaultLibraryRootURL;
    // NSOpenPanel only reads directoryURL when it is first presented, so we
    // cannot navigate the panel that is already on screen. Dismiss it and
    // re-open a fresh panel rooted at the default library location instead.
    [NSFileManager.defaultManager createDirectoryAtURL:defaultURL
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    [self.window endSheet:self.libraryLocationPanel returnCode:NSModalResponseCancel];
    Preferences * __weak weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf presentLibraryChooserFromURL:defaultURL];
    });
}

- (IBAction)organiseAllGames:(id)sender {
    TableViewController *libcontroller = _libcontroller;
    if (!libcontroller)
        return;

    NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Organise all games now?", nil)
                                          info:NSLocalizedString(@"Every game in your library will be copied into the central library folder, arranged by group and title. This may take a while for a large library.", nil)
                                       buttons:@[NSLocalizedString(@"Organise", nil), NSLocalizedString(@"Cancel", nil)]];

    Preferences * __weak weakSelf = self;
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {
        if (returnCode != NSAlertFirstButtonReturn)
            return;

        NSManagedObjectContext *context = libcontroller.coreDataManager.privateChildManagedObjectContext;
        context.undoManager = nil;
        [[LibraryOrganizer sharedOrganizer] organiseAllGamesInContext:context completion:^(NSUInteger organisedCount) {
            [libcontroller.coreDataManager saveChanges];
            NSAlert *done = [NSAlert alertWithMessage:NSLocalizedString(@"Finished organising games.", nil)
                                                 info:[NSString stringWithFormat: NSLocalizedString(@"%lu games are now in the library folder.", nil), (unsigned long)organisedCount]
                                              buttons:nil];
            [done beginSheetModalForWindow:weakSelf.window completionHandler:nil];
        }];
    }];
}

@end
