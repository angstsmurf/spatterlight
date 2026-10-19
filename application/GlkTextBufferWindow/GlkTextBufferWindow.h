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

// Set while a scroll restore (after autorestore or a full screen change) is
// pending. See restoreScroll:.
@property BOOL pendingScrollRestore;

@property (nullable) GlkTextGridWindow *quoteBox;

- (void)recalcBackground;
- (void)echo:(BOOL)val;
- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset animate:(BOOL)animate;
- (void)scrollToTop;
- (void)scrollToBottomAnimated:(BOOL)animate;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL scrolledToBottom;
- (void)storeScrollOffset;
- (void)restoreScrollBarStyle;
- (void)restoreScroll:(nullable id)sender;
- (void)forceLayout;

// The scrollback character limit for this window, taken from the current
// theme's scrollbackLimit attribute (clamped to a sensible floor). 0 means
// unlimited (the scrollback is never trimmed).
- (NSUInteger)scrollbackLimit;

// Clamp a raw scrollback-limit value into the supported range (0 = unlimited,
// otherwise at least the minimum floor). Exposed for the Preferences UI.
+ (NSUInteger)clampScrollbackLimit:(NSInteger)value;

- (void)padWithNewlines:(NSUInteger)lines;

- (void)scrollWheelchanged:(NSEvent *)event fromPosition:(CGFloat)oldPosition;
- (void)userDidScrollUpFromPosition:(CGFloat)oldPosition;
- (void)updateImageAttachmentsWithXScale:(CGFloat)xscale yScale:(CGFloat)yscale;

- (void)resetLastSpokenString;

// Used by JourneyMenuHandler
- (NSString *)lastMoveString;
@property NSInteger lastNewTextOnTurn;

// Used by InfocomV6MenuHandler
- (void)movesRangesFromV6Menu:(NSArray<NSString *> *)menuStrings;

@property (NS_NONATOMIC_IOSONLY, readonly) NSRange editableRange;
@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger numberOfColumns;
@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger numberOfLines;

#pragma mark Testing

// Compute a front-trim cut point at a paragraph boundary for capping the
// scrollback. Returns the number of leading characters that may be deleted
// (always at a line boundary, never past safeLimit), or 0 if no trim should
// happen.
+ (NSUInteger)scrollbackCutPointForString:(NSString *)string
                               targetKeep:(NSUInteger)targetKeep
                                safeLimit:(NSUInteger)safeLimit;

// How much text a scrollback trim keeps, and the text length above which a
// trim happens (at least a minimum slack above the kept amount, so small
// limits do not trim on every turn).
+ (NSUInteger)scrollbackKeepForLimit:(NSUInteger)limit;
+ (NSUInteger)scrollbackTrimThresholdForLimit:(NSUInteger)limit;

@end

NS_ASSUME_NONNULL_END
