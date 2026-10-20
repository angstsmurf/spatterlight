#import "GlkController.h"
#import "VoiceOverMenu.h"

@interface GlkController (Speech)

// Accessibility
- (BOOL)isAccessibilityElement;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *accessibilityCustomActions;
- (IBAction)saveAsRTF:(id)sender;

// ZMenu
- (void)checkZMenuAndSpeak:(BOOL)speak;
- (BOOL)showingInfocomV6Menu;
// The menu ZMenu is speaking, if any: the Infocom V6 menu handler or zmenu.
@property (NS_NONATOMIC_IOSONLY, readonly) id<VoiceOverMenu> currentMenu;

// Speak new text
- (void)speakNewText;
- (void)speakLargest:(NSArray *)array;
- (void)speakOnBecomingKey;

// (Re)schedule the new-text announcement, debounced against layout churn so
// it fires only once the UI has settled (see lastLayoutChurnTimestamp).
- (void)scheduleSpeakNewTextAfterDelay:(CGFloat)delay;
- (void)fireSpeakNewText;

// Speak previous moves
- (IBAction)speakMostRecent:(id)sender;
- (void)speakMostRecentAfterDelay;
- (IBAction)speakPrevious:(id)sender;
- (IBAction)speakNext:(id)sender;
- (IBAction)speakStatus:(id)sender;
- (void)forceSpeech;
- (void)speakStringNow:(NSString *)string;
- (void)speakString:(NSString *)string;
// Posts a VoiceOver announcement to announcementWindow, without the checks
// and clean-up speakString: does.
- (void)announce:(NSString *)string priority:(NSAccessibilityPriorityLevel)priority;
// The window announcements are posted to: the game window, or the main
// window while Journey is the current game.
@property (NS_NONATOMIC_IOSONLY, readonly) NSWindow *announcementWindow;
@property (NS_NONATOMIC_IOSONLY, readonly, strong) GlkWindow *largestWithMoves;
// Every Glk window plus the quote box currently shown, if any.
@property (NS_NONATOMIC_IOSONLY, readonly) NSArray<GlkWindow *> *windowsIncludingQuoteBox;

// Custom rotors
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *createCustomRotors;
- (NSString *)gameTitle;

@end
