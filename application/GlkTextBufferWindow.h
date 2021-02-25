@class MarginImage, InputHistory, GlkTextGridWindow;

#import "GlkWindow.h"

NS_ASSUME_NONNULL_BEGIN

// I suppose this is necessary to get rid of that ugly Markup menu on attached
// images.

@interface MyAttachmentCell : NSTextAttachmentCell <NSSecureCoding>

@property (weak) NSAttributedString *attrstr;

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)analignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos;

@end

@interface MyTextView : NSTextView <NSTextFinderClient, NSSecureCoding>

- (void)superKeyDown:(NSEvent *)evt;
- (void)temporarilyHideCaret;
- (void)resetTextFinder; // Call after changing the text storage, or search will
                         // break.

@property BOOL shouldDrawCaret;
@property CGFloat bottomPadding;
@property(weak, readonly) NSTextFinder *textFinder;

@end

/*
 * Extend NSTextContainer to have images in the margins with
 * the text flowing around them.
 */

@interface MarginContainer : NSTextContainer <NSSecureCoding>

@property NSMutableArray *marginImages;

- (id)initWithContainerSize:(NSSize)size;
- (void)clearImages;
- (void)addImage:(NSImage *)image
           align:(NSInteger)align
              at:(NSUInteger)top
          linkid:(NSUInteger)linkid;
- (void)drawRect:(NSRect)rect;
- (void)invalidateLayout;
- (void)unoverlap:(MarginImage *)image;
- (BOOL)hasMarginImages;
- (NSMutableAttributedString *)marginsToAttachmentsInString:
    (NSMutableAttributedString *)string;
- (NSUInteger)findHyperlinkAt:(NSPoint)p;

@end

/*
 * TextBuffer window controller
 */

@interface GlkTextBufferWindow
    : GlkWindow <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate>

@property MyTextView *textview;

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
