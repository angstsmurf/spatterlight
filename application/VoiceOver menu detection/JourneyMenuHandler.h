//
//  JourneyMenuHandler.h
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//
//  For VoiceOver users, builds the Journey Party and Individual Commands
//  menus from the menu items the interpreter sends, and asks the game's
//  questions as dialogs. Choosing a menu item or answering a dialog clicks
//  the matching place in the grid window.
//

#import <Foundation/Foundation.h>

#import "VoiceOverMenu.h"

#include "glkimp.h"

@class GlkController, JourneyMenuItem, GlkTextGridWindow, GlkTextBufferWindow;

// Archived as an integer: don't reorder.
typedef NS_ENUM(NSInteger, JourneyDialogType) {
    JourneyDialogTypeTextEntry,
    JourneyDialogTypeTextEntryElvish,
    JourneyDialogTypeSingleChoice,
    JourneyDialogTypeMultipleChoice
};

NS_ASSUME_NONNULL_BEGIN

@interface JourneyMenuHandler : NSObject <VoiceOverHelper>

- (instancetype)initWithGlkController:(GlkController *)glkctl gridWindow:(nullable GlkTextGridWindow *)gridWindow bufferWindow:(nullable GlkTextBufferWindow *)bufferWindow;

- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *_Nullable)buf length:(NSUInteger)len;

// The actions of the menu items, passed on by GlkController.
- (void)partyAction:(id)sender;
- (void)memberVerbAction:(id)sender;

- (void)deleteAllMenus;
- (void)recreateMenus;
- (void)hideMenus;
- (void)showMenus;
// Keeps a copy of the Individual Commands menu, which recreateMenus puts
// back if another game window has replaced it.
- (void)captureMembersMenu;
// Shows the pending dialog, if any, after a short delay.
- (void)recreateDialog;
- (void)flushDisplay;
// Looks up the windows again if needed and rebuilds the menus. If
// shouldRecreateDialog is YES, also schedules recreateDialog and returns
// YES; returns NO if nothing was scheduled.
- (BOOL)updateOnBecameKey:(BOOL)shouldRecreateDialog;

// Whether a dialog was closed (or the menus deleted) less than a second ago.
@property (readonly) BOOL dialogClosedRecently;
// Whether the game has asked a question that has not been shown yet, or
// was showing when the game was autosaved.
@property (readonly) BOOL hasPendingDialog;
// Set when a file dialog closes, so that the question the game asks next
// is not shown.
@property BOOL skipNextDialog;

@property (weak, nullable) GlkController *glkctl;
@property (weak, nullable) GlkTextGridWindow *textGridWindow;
@property (weak, nullable) GlkTextBufferWindow *textBufferWindow;

// The names of the two windows when the handler was archived.
@property (readonly) NSInteger gridTextWinName;
@property (readonly) NSInteger bufferTextWinName;

@end

// Not NSSecureCoding: the captured members menu is archived as NSMenuItems,
// which don't adopt it.
@interface JourneyMenuHandler (Coding) <NSCoding>
@end

NS_ASSUME_NONNULL_END
