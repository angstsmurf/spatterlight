@class MarginImage;
@class GlkTextBufferWindow;

// I suppose this is necessary to get rid of that ugly Markup menu on attached images.

@interface MyAttachmentCell : NSTextAttachmentCell
{
    NSInteger align;
    NSInteger pos;
    NSAttributedString *attrstr;
}

- (instancetype) initImageCell:(NSImage *)image andAlignment:(NSInteger)analignment andAttStr:(NSAttributedString *)anattrstr at:(NSInteger)apos;

@property (readonly) BOOL wantsToTrackMouse;

@end

@interface MyTextView : NSTextView <NSTextFinderClient>
{
    GlkTextBufferWindow * glkTextBuffer;
    NSTextFinder* _textFinder;
}

- (instancetype) initWithFrame:(NSRect)rect textContainer:(NSTextContainer *)container textBuffer: (GlkTextBufferWindow *)textbuffer;
- (void) superKeyDown: (NSEvent*)evt;
- (void) scrollToBottom;
- (void) performScroll;
- (void) temporarilyHideCaret;
@property (readonly) BOOL scrolledToBottom;
- (void) resetTextFinder; // Call after changing the text storage, or search will break.

@property BOOL shouldDrawCaret;
@property CGFloat bottomPadding;
@property (weak, readonly) NSTextFinder* textFinder;

@property BOOL shouldSpeak_10_7;
@property NSRange rangeToSpeak_10_7;

@end

/*
 * Extend NSTextContainer to have images in the margins with
 * the text flowing around them.
 */

@interface MarginContainer : NSTextContainer
{
    NSMutableArray *margins;
    NSMutableArray *flowbreaks;
}

- (id) initWithContainerSize: (NSSize)size;
- (void) clearImages;
- (void) addImage: (NSImage*)image align: (NSInteger)align at: (NSInteger)top linkid: (NSUInteger)linkid;
- (void) drawRect: (NSRect)rect;
- (void) invalidateLayout;
- (void) unoverlap: (MarginImage *)image;
@property (readonly) BOOL hasMarginImages;
- (NSMutableAttributedString *) marginsToAttachmentsInString: (NSMutableAttributedString *)string;
- (NSUInteger) findHyperlinkAt: (NSPoint)p;

@end

/*
 * TextBuffer window controller
 */

#define HISTORYLEN 100

@interface GlkTextBufferWindow : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate>
{
    NSScrollView *scrollview;
    NSTextStorage *textstorage;
    NSLayoutManager *layoutmanager;
    MarginContainer *container;
    MyTextView *textview;

    NSInteger line_request;
    NSInteger hyper_request;

    BOOL echo_toggle_pending; /* if YES, line echo behavior will be inverted, starting from the next line event*/
    BOOL echo; /* if YES, current line input will be deleted from text view */

    NSInteger fence;        /* for input line editing */

    NSString *history[HISTORYLEN];
    NSInteger historypos;
    NSInteger historyfirst, historypresent;

    NSMutableArray *moveRanges;
    NSInteger moveRangeIndex;
}

- (void) recalcBackground;
- (void) onKeyDown: (NSEvent*)evt;
- (void) echo: (BOOL)val;
- (BOOL) myMouseDown: (NSEvent*)theEvent;
- (void) stopSpeakingText_10_7;
- (IBAction) speakMostRecent: (id)sender;
- (IBAction) speakPrevious: (id)sender;
- (IBAction) speakNext: (id)sender;
- (IBAction) speakStatus: (id)sender;

@property (readonly) NSInteger lastchar; /* for smart formatting */
@property (readonly) NSInteger lastseen; /* for more paging */

@end

