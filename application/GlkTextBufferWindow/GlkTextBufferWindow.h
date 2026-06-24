#import "GlkWindow.h"

@class MarginImage, InputHistory, GlkTextGridWindow, BufferTextView;

NS_ASSUME_NONNULL_BEGIN

/*
 * Text Buffer window controller
 */

@interface GlkTextBufferWindow
    : GlkWindow <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate, NSLayoutManagerDelegate>

@property BufferTextView *textview;

@property(readonly) unichar lastchar; /* for smart formatting */
@property(readonly) NSInteger lastseen; /* for more paging */
// for keeping track of previous moves
@property NSUInteger printPositionOnInput;

/* For autorestoring scroll position */
@property NSUInteger restoredLastVisible;
@property CGFloat restoredScrollOffset;
@property BOOL restoredAtBottom;
@property BOOL restoredAtTop;

@property NSRange restoredSelection;
@property NSString *restoredSearch;
@property BOOL restoredFindBarVisible;
@property BOOL pendingScroll;
@property BOOL pendingEditable;
@property BOOL pendingClear;
@property BOOL pendingScrollRestore;

@property NSAttributedString *restoredInput;
@property NSDictionary *inputAttributes;

@property (nullable) GlkTextGridWindow *quoteBox;

- (void)recalcBackground;
- (void)echo:(BOOL)val;
- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset animate:(BOOL)animate;
- (void)scrollToTop;
- (void)scrollToBottomAnimated:(BOOL)animate;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL scrolledToBottom;
- (void)storeScrollOffset;
- (void)restoreTextFinder;
- (void)restoreScrollBarStyle;
- (void)restoreScroll:(nullable id)sender;
- (void)forceLayout;

// Compute a front-trim cut point at a paragraph boundary for capping the
// scrollback. Returns the number of leading characters that may be deleted
// (always at a line boundary, never past safeLimit), or 0 if no trim should
// happen. Exposed for unit testing.
+ (NSUInteger)scrollbackCutPointForString:(NSString *)string
                               targetKeep:(NSUInteger)targetKeep
                                safeLimit:(NSUInteger)safeLimit;

// The scrollback character limit for this window, taken from the current
// theme's scrollbackLimit attribute (clamped to a sensible floor). 0 means
// unlimited (the scrollback is never trimmed).
- (NSUInteger)scrollbackLimit;

// Clamp a raw scrollback-limit value into the supported range (0 = unlimited,
// otherwise at least the minimum floor). Exposed for the Preferences UI.
+ (NSUInteger)clampScrollbackLimit:(NSInteger)value;

- (void)padWithNewlines:(NSUInteger)lines;

- (void)scrollWheelchanged:(NSEvent *)event;
- (void)updateImageAttachmentsWithXScale:(CGFloat)xscale yScale:(CGFloat)yscale;

- (void)resetLastSpokenString;

// Only used by JourneyMenuHandler
- (NSString *)lastMoveString;
- (void)movesRangesFromV6Menu:(NSArray<NSString *> *)menuStrings;
@property NSInteger lastNewTextOnTurn;

@property (NS_NONATOMIC_IOSONLY, readonly) NSRange editableRange;
@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger numberOfColumns;
@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger numberOfLines;

@end

NS_ASSUME_NONNULL_END
