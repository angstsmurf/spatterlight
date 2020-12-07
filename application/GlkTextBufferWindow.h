@class MarginImage, InputHistory;

#import "GlkWindow.h"

NS_ASSUME_NONNULL_BEGIN

// I suppose this is necessary to get rid of that ugly Markup menu on attached
// images.

@interface MyAttachmentCell : NSTextAttachmentCell {
    NSInteger align;
    NSUInteger pos;
}

@property (weak) NSAttributedString *attrstr;

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)analignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos;

@end

@interface MyTextView : NSTextView <NSTextFinderClient> {
    NSTextFinder *_textFinder;
}

- (void)superKeyDown:(NSEvent *)evt;
- (void)temporarilyHideCaret;
- (void)resetTextFinder; // Call after changing the text storage, or search will
                         // break.

@property BOOL shouldDrawCaret;
@property CGFloat bottomPadding;
@property(weak, readonly) NSTextFinder *textFinder;

@property BOOL shouldSpeak_10_7;
@property NSRange rangeToSpeak_10_7;

@end

/*
 * Extend NSTextContainer to have images in the margins with
 * the text flowing around them.
 */

@interface MarginContainer : NSTextContainer {
    NSMutableArray *flowbreaks;
}

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
    : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate> {
    NSScrollView *scrollview;
    NSLayoutManager *layoutmanager;
    MarginContainer *container;
    NSTextStorage *textstorage;
    NSMutableAttributedString *bufferTextstorage;

    BOOL line_request;
    BOOL hyper_request;

    BOOL echo_toggle_pending; /* if YES, line echo behavior will be inverted,
                                 starting from the next line event*/
    BOOL echo; /* if NO, line input text will be deleted when entered */

    NSUInteger fence; /* for input line editing */

    /* For command history */
    InputHistory *history;

    NSMutableArray *moveRanges;
    NSUInteger moveRangeIndex;

    CGFloat lastLineheight;

    NSAttributedString *storedNewline;

    // for temporarily storing scroll position
    NSUInteger lastVisible; 
    CGFloat lastScrollOffset;
    BOOL lastAtBottom;
    BOOL lastAtTop;
}

@property MyTextView *textview;

@property(readonly) NSInteger lastchar; /* for smart formatting */
@property(readonly) NSInteger lastseen; /* for more paging */

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

- (void)recalcBackground;
- (void)echo:(BOOL)val;
- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (void)stopSpeakingText_10_7;
- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset;
- (void)scrollToTop;
- (void)scrollToBottom;
- (BOOL)scrolledToBottom;
- (void)storeScrollOffset;
- (void)restoreTextFinder;
- (void)restoreScrollBarStyle;
- (void)restoreScroll:(nullable id)sender;

- (IBAction)speakMostRecent:(nullable id)sender;
- (IBAction)speakPrevious:(nullable id)sender;
- (IBAction)speakNext:(nullable id)sender;
- (IBAction)speakStatus:(nullable id)sender;
- (void)setLastMove;

@end

NS_ASSUME_NONNULL_END
