#import "GlkController_Private.h"
#import "GlkController+Speech.h"

#import "Game.h"
#import "Theme.h"
#import "Metadata.h"
#import "Preferences.h"
#import "InfoController.h"
#import "TableViewController.h"
#import "TableViewController+GameActions.h"
#import "TableViewController+InfoWindows.h"
#import "JourneyMenuHandler.h"
#import "AppDelegate.h"
#import "LibController.h"

#import "Fetches.h"

#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"

#ifndef DEBUG
#define NSLog(...)
#endif

@implementation GlkController (MenuItems)

- (IBAction)showGameInfo:(id)sender {
    [libcontroller showInfoForGame:self.game toggle:NO];
}

- (IBAction)revealGameInFinder:(id)sender {
    [[NSWorkspace sharedWorkspace] selectFile:self.gamefile
                     inFileViewerRootedAtPath:@""];
}

- (IBAction)like:(id)sender {
    if (self.game.like == 1)
        self.game.like = 0;
    else
        self.game.like = 1;
}

- (IBAction)dislike:(id)sender {
    if (self.game.like == 2)
        self.game.like = 0;
    else
        self.game.like = 2;
}

- (IBAction)openIfdb:(id)sender {
    NSString *urlString;
    if (self.game.metadata.tuid)
        urlString = [@"https://ifdb.tads.org/viewgame?id=" stringByAppendingString:self.game.metadata.tuid];
    else
        urlString = [@"https://ifdb.tads.org/viewgame?ifid=" stringByAppendingString:self.game.ifid];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: urlString]];
}

- (IBAction)download:(id)sender {
    [libcontroller downloadMetadataForGames:@[self.game]];
}

- (IBAction)applyTheme:(id)sender {
    NSString *name = ((NSMenuItem *)sender).title;

    Theme *theme = [Fetches findTheme:name inContext:self.game.managedObjectContext];

    if (!theme) {
        NSLog(@"applyTheme: found no theme with name %@", name);
        return;
    }

    Preferences *prefwin = [Preferences instance];
    if (prefwin) {
        [prefwin restoreThemeSelection:theme];
    }

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
}

- (IBAction)deleteGame:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:NSLocalizedString(@"Are you sure?", nil)];
    alert.informativeText = NSLocalizedString(@"Do you want to close this game and delete it from the library?", nil);
    [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSInteger choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        [self.window close];
        if (self.game) {
            self.game.hidden = YES;
            [self.game.managedObjectContext deleteObject:self.game];
        }
    }
}

- (void)journeyPartyAction:(id)sender {
    [self.journeyMenuHandler journeyPartyAction:sender];
}

- (void)journeyMemberVerbAction:(id)sender {
    [self.journeyMenuHandler journeyMemberVerbAction:sender];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {

    SEL action = menuItem.action;

    if (action == @selector(speakMostRecent:) || action == @selector(speakPrevious:) || action == @selector(speakNext:) || action == @selector(speakStatus:)) {
        return (self.voiceOverActive);
    } else if (action == @selector(saveAsRTF:)) {
        for (GlkWindow *win in self.gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextGridWindow class]] || [win isKindOfClass:[GlkTextBufferWindow class]])
                return YES;
        }
        return NO;
    } else if (action == @selector(like:)) {
        if (self.game.like == 1) {
            menuItem.title = NSLocalizedString(@"Liked", nil);
            menuItem.state = NSOnState;
        } else {
            menuItem.title = NSLocalizedString(@"Like", nil);
            menuItem.state = NSOffState;
        }
    } else if (action == @selector(dislike:)) {
        if (self.game.like == 2) {
            menuItem.title = NSLocalizedString(@"Disliked", nil);
            menuItem.state = NSOnState;
        } else {
            menuItem.title = NSLocalizedString(@"Disike", nil);
            menuItem.state = NSOffState;
        }
    } else if (action == @selector(applyTheme:)) {
        if ([Preferences instance].darkOverrideActive || [Preferences instance].lightOverrideActive)
            return NO;
        for (NSMenuItem *item in libcontroller.mainThemesSubMenu.submenu.itemArray) {
            if ([item.title isEqual:self.game.theme.name])
                item.state = NSOnState;
            else
                item.state = NSOffState;
        }
    } else if (action == @selector(deleteGame:)) {
        return (!self.game.hidden);
    }

    return YES;
}

@end
