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

// VoiceOver landing. When the game window becomes key, VoiceOver lands on
// it, fetches the text of a game window and then describes the window.
// Speech posted before the description begins is lost.
- (void)voiceOverWindowBecameKey;
// The text of a buffer or grid window was fetched through accessibility.
- (void)voiceOverFetchedWindowText;
// Runs block when VoiceOver has begun describing the window it just landed
// on, or after fallback seconds if no landing is in progress or the fetch
// never comes.
- (void)afterVoiceOverLanding:(dispatch_block_t)block fallback:(NSTimeInterval)fallback;
// The "Limit VoiceOver to Last Move" setting was toggled.
- (void)voiceOverTailModeChanged;
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
