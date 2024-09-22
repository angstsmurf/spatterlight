//
//  JourneyMenuHandler.h
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import <Foundation/Foundation.h>

#include "glkimp.h"

@class GlkController, JourneyMenuItem, GlkTextGridWindow;

typedef NS_ENUM(NSInteger, kJourneyDialogType) {
    kJourneyDialogTextEntry,
    kJourneyDialogTextEntryElvish,
    kJourneyDialogSingleChoice,
    kJourneyDialogMultipleChoice
};

NS_ASSUME_NONNULL_BEGIN

@interface JourneyMenuHandler : NSObject

- (instancetype)initWithDelegate:(GlkController *)delegate gridWindow:(GlkTextGridWindow *)gridwindow;
- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *_Nullable)buf length:(NSUInteger)len;

- (void)sendCorrespondingMouseEventForMenuItem:(JourneyMenuItem *)item;

- (void)journeyPartyAction:(id)sender;
- (void)journeyMemberVerbAction:(id)sender;
- (void)deleteAllJourneyMenus;
- (void)recreateJourneyMenus;
- (void)hideJourneyMenus;
- (void)showJourneyMenus;
- (void)captureMembersMenu;
- (void)recreateDialog;
- (BOOL)updateOnBecameKey:(BOOL)recreateDialog;

@property BOOL showsJourneyMenus;
@property (weak) GlkController *delegate;
@property (weak) GlkTextGridWindow *textGridWindow;

@property NSPopUpButton *journeyPopupButton;
@property NSTextField *journeyTextField;

// These must be arrays, because a dictionary won't stay sorted
// in the order items are added to it
@property NSMutableArray<JourneyMenuItem *> *journeyDialogMenuItems;
@property NSMutableArray<JourneyMenuItem *> *journeyPartyMenuItems;

// We need this in order to know when to dispaly a new dialog.
// If we didn't show dialogs, we could have compared to the actual menu instead
@property NSMutableArray<JourneyMenuItem *> *journeyLastPartyMenuItems;
@property NSMutableArray<JourneyMenuItem *> *journeyLastDialogMenuItems;

@property NSDate *journeyDialogClosedTimestamp;

@property NSMutableDictionary<NSNumber *,JourneyMenuItem *> *journeyVerbMenuItems;
@property NSMutableArray<JourneyMenuItem *> *journeyGlueStrings;

@property (nonatomic) NSMenuItem *journeyMembersMenu;
@property (nonatomic) NSMenuItem *journeyPartyMenu;

@property NSMutableArray<NSMenuItem *> *capturedMembersMenu;

@property NSInteger gridTextWinName;
@property BOOL shouldShowDialog;
@property BOOL reallyShowingDialog;
@property BOOL restoredShowingDialog;
@property BOOL skipNextDialog;

@property kJourneyDialogType storedDialogType;
@property NSString *storedDialogText;

@end

NS_ASSUME_NONNULL_END
