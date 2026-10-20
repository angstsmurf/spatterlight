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
#import "IFDB.h"

#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"

@implementation GlkController (MenuItems)

- (IBAction)showGameInfo:(id)sender {
    [libcontroller showInfoForGame:self.game toggle:NO];
}

- (IBAction)revealGameInFinder:(id)sender {
    [[NSWorkspace sharedWorkspace] selectFile:self.gamefile
                     inFileViewerRootedAtPath:@""];
}

// game.like: 0 = neither, 1 = liked, 2 = disliked
- (void)toggleLike:(int32_t)value {
    self.game.like = (self.game.like == value) ? 0 : value;
}

- (IBAction)like:(id)sender {
    [self toggleLike:1];
}

- (IBAction)dislike:(id)sender {
    [self toggleLike:2];
}

static void setLikeMenuItem(NSMenuItem *menuItem, BOOL on, NSString *onTitle, NSString *offTitle) {
    menuItem.title = on ? onTitle : offTitle;
    menuItem.state = on ? NSControlStateValueOn : NSControlStateValueOff;
}

- (IBAction)openIfdb:(id)sender {
    NSURL *url = [IFDB viewGameURLForTUID:self.game.metadata.tuid IFID:self.game.ifid iFiction:NO];
    if (url)
        [[NSWorkspace sharedWorkspace] openURL:url];
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

    [[Preferences instance] restoreThemeSelection:theme];

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:theme]];
}

- (IBAction)deleteGame:(id)sender {
    NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Are you sure?", nil)
                                          info:NSLocalizedString(@"Do you want to close this game and delete it from the library?", nil)
                                       buttons:@[ NSLocalizedString(@"Delete", nil), NSLocalizedString(@"Cancel", nil) ]];

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
    [self.journeyMenuHandler partyAction:sender];
}

- (void)journeyMemberVerbAction:(id)sender {
    [self.journeyMenuHandler memberVerbAction:sender];
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
        setLikeMenuItem(menuItem, self.game.like == 1,
                        NSLocalizedString(@"Liked", nil), NSLocalizedString(@"Like", nil));
    } else if (action == @selector(dislike:)) {
        setLikeMenuItem(menuItem, self.game.like == 2,
                        NSLocalizedString(@"Disliked", nil), NSLocalizedString(@"Dislike", nil));
    } else if (action == @selector(applyTheme:)) {
        Preferences *prefs = [Preferences instance];
        if (prefs.darkOverrideActive || prefs.lightOverrideActive)
            return NO;
        for (NSMenuItem *item in libcontroller.mainThemesSubMenu.submenu.itemArray) {
            item.state = [item.title isEqual:self.game.theme.name] ? NSControlStateValueOn : NSControlStateValueOff;
        }
    } else if (action == @selector(deleteGame:)) {
        return (!self.game.hidden);
    }

    return YES;
}

@end
