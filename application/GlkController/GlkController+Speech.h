#import "GlkController.h"

@interface GlkController (Speech)

// Accessibility
- (BOOL)isAccessibilityElement;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *accessibilityCustomActions;
- (IBAction)saveAsRTF:(id)sender;

// ZMenu
- (void)checkZMenuAndSpeak:(BOOL)speak;
- (BOOL)showingInfocomV6Menu;

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
@property (NS_NONATOMIC_IOSONLY, readonly, strong) GlkWindow *largestWithMoves;

// Custom rotors
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *createCustomRotors;
- (NSString *)gameTitle;

@end
