//
//  JourneyMenuHandler.h
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import <Foundation/Foundation.h>

#include "glkimp.h"

@class GlkController, JourneyMenuItem, GlkTextGridWindow, GlkTextBufferWindow;

typedef NS_ENUM(NSInteger, kJourneyDialogType) {
    kJourneyDialogTextEntry,
    kJourneyDialogTextEntryElvish,
    kJourneyDialogSingleChoice,
    kJourneyDialogMultipleChoice
};

NS_ASSUME_NONNULL_BEGIN

// Not NSSecureCoding: the captured members menu is archived as NSMenuItems,
// which don't adopt it.
@interface JourneyMenuHandler : NSObject <NSCoding>

- (instancetype)initWithDelegate:(GlkController *)delegate gridWindow:(GlkTextGridWindow *)gridwindow bufferWindow:(GlkTextBufferWindow *)bufferwindow;
- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *_Nullable)buf length:(NSUInteger)len;

- (void)sendCorrespondingMouseEventForMenuItem:(nullable JourneyMenuItem *)item;

- (void)journeyPartyAction:(id)sender;
- (void)journeyMemberVerbAction:(id)sender;
- (void)deleteAllJourneyMenus;
- (void)recreateJourneyMenus;
- (void)hideJourneyMenus;
- (void)showJourneyMenus;
- (void)captureMembersMenu;
- (void)recreateDialog;
- (void)flushDisplay;
// Looks up the windows again if needed and rebuilds the menus. If
// shouldRecreateDialog is YES, also schedules recreateDialog and returns
// YES; returns NO if nothing was scheduled.
- (BOOL)updateOnBecameKey:(BOOL)shouldRecreateDialog;

// Whether a dialog was closed (or menus deleted) less than a second ago.
@property (readonly) BOOL dialogClosedRecently;

@property (weak) GlkController *delegate;
@property (weak) GlkTextGridWindow *textGridWindow;
@property (weak) GlkTextBufferWindow *textBufferWindow;

@property NSDate *journeyDialogClosedTimestamp;

@property NSInteger gridTextWinName;
@property NSInteger bufferTextWinName;
@property BOOL shouldShowDialog;
@property BOOL skipNextDialog;

@end

NS_ASSUME_NONNULL_END
