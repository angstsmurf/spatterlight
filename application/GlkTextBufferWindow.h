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
    NSTextFinder* _textFinder; // define your own text finder
}

- (instancetype) initWithFrame:(NSRect)rect textContainer:(NSTextContainer *)container textBuffer: (GlkTextBufferWindow *)textbuffer;
- (void) superKeyDown: (NSEvent*)evt;
- (void) scrollToBottom;
- (void) performScroll;
- (void) temporarilyHideCaret;
- (BOOL) scrolledToBottom;
- (void) resetTextFinder; // A method to reset the view's text finder when you change the text storage

@property BOOL shouldDrawCaret;
@property CGFloat bottomPadding;
@property (readonly) NSTextFinder* textFinder;

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
- (BOOL) hasMarginImages;
- (NSMutableAttributedString *) marginsToAttachmentsInString: (NSMutableAttributedString *)string;
- (NSUInteger) findHyperlinkAt: (NSPoint)p;

@end

/*
 * TextBuffer window controller
 */

#define HISTORYLEN 50

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

    NSInteger fence;		/* for input line editing */

    NSString *history[HISTORYLEN];
    NSInteger historypos;
    NSInteger historyfirst, historypresent;
}

- (void) recalcBackground;
- (void) onKeyDown: (NSEvent*)evt;
- (void) echo: (BOOL)val;
- (BOOL) myMouseDown: (NSEvent*)theEvent;

@property (readonly) NSInteger lastchar; /* for smart formatting */
@property (readonly) NSInteger lastseen; /* for more paging */

@end

