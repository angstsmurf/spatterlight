#import "GlkWindow.h"

@class MarginImage, InputHistory, GlkTextGridWindow, BufferTextView;

NS_ASSUME_NONNULL_BEGIN

/*
 * Text Buffer window controller
 */

@interface GlkTextBufferWindow
    : GlkWindow <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate>

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
@property BOOL usingStyles;
@property BOOL pendingScroll;
@property BOOL pendingClear;
@property BOOL pendingScrollRestore;

@property NSAttributedString *restoredInput;
@property NSDictionary *inputAttributes;

@property (nullable) GlkTextGridWindow *quoteBox;

// A dictionary of arrays of inline image descriptions, keyed by range
@property NSMutableDictionary <NSValue *, NSArray *> *imageDescriptionRanges;
@property NSMutableArray <NSString *> *imageDescriptionsToSpeak;

- (void)recalcBackground;
- (void)echo:(BOOL)val;
- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset;
- (void)scrollToTop;
- (void)scrollToBottom;
- (BOOL)scrolledToBottom;
- (void)storeScrollOffset;
- (void)restoreTextFinder;
- (void)restoreScrollBarStyle;
- (void)restoreScroll:(nullable id)sender;
- (void)forceLayout;

- (void)padWithNewlines:(NSUInteger)lines;

- (IBAction)saveAsRTF:(id)sender;

@end

NS_ASSUME_NONNULL_END
