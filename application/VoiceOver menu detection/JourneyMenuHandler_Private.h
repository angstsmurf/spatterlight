//
//  JourneyMenuHandler_Private.h
//  Spatterlight
//
//  State shared by JourneyMenuHandler.m and JourneyMenuHandler+Coding.m.
//

#import "JourneyMenuHandler.h"

@class JourneyDialogPresenter;

typedef NS_ENUM(NSInteger, JourneyDialogState) {
    // No question, or one that recreateDialog is about to show.
    JourneyDialogStateIdle,
    // The game has asked a question. It is shown at the next display
    // update, when the window becomes key or when VoiceOver is turned on.
    // Choosing from a menu, or a menu that is not a question, drops it.
    JourneyDialogStatePending,
    // A question was pending or showing when the game was autosaved. It
    // stays until it is shown or a menu item is chosen, whatever menus the
    // interpreter sends in the meantime.
    JourneyDialogStateRestored,
    // The dialog is on screen.
    JourneyDialogStateShowing
};

NS_ASSUME_NONNULL_BEGIN

@interface JourneyMenuHandler ()

@property JourneyDialogState dialogState;
@property JourneyDialogType dialogType;
// The question, without the last move that is prepended when it is shown.
@property (nullable) NSString *dialogText;
// Whether the dialog shown last had the last move prepended.
@property BOOL dialogIncludesLastMove;
// Set on decoding when the archived dialog had the last move prepended, so
// that the restored one gets it too.
@property BOOL restoredDialogIncludesLastMove;
@property NSDate *dialogClosedTimestamp;

// Arrays, to keep the order the items came in.
@property NSMutableArray<JourneyMenuItem *> *dialogMenuItems;
@property NSMutableArray<JourneyMenuItem *> *partyMenuItems;
// The party menu that was last shown as a dialog, so that the same menu
// sent again isn't shown again.
@property NSMutableArray<JourneyMenuItem *> *lastPartyMenuItems;
@property NSMutableDictionary<NSNumber *, JourneyMenuItem *> *verbMenuItems;
@property NSMutableArray<JourneyMenuItem *> *glueStrings;
@property NSMutableArray<NSMenuItem *> *capturedMembersMenu;

// Whether the next item of each kind starts a new menu.
@property BOOL startNewPartyMenu;
@property BOOL startNewMembersMenu;
@property BOOL startNewDialogMenu;
// The next verb tag. Tags identify verbs across autorestores.
@property NSInteger tagCounter;

@property NSInteger gridTextWinName;
@property NSInteger bufferTextWinName;

@property (nonatomic) JourneyDialogPresenter *presenter;

// Drops a pending question; one that is restored or showing stays.
- (void)cancelPendingDialog;
- (void)prepareDialogWithText:(NSString *)text type:(JourneyDialogType)type;
- (BOOL)shouldRecreateDialog;
- (void)sendMouseEventForMenuItem:(nullable JourneyMenuItem *)item;

@end

NS_ASSUME_NONNULL_END
