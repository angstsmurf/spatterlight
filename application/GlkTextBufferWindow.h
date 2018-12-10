@class MarginImage;

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

/*
 * Extend NSTextContainer to have images in the margins with
 * the text flowing around them.
 */

@interface MyTextView : NSTextView
- (void) superKeyDown: (NSEvent*)evt;
- (void) scrollToBottom;
@end


@interface MarginContainer : NSTextContainer
{
    NSMutableArray *margins;
	NSMutableArray *flowbreaks;
	NSInteger recalc;
}

- (instancetype) initWithContainerSize: (NSSize)size NS_DESIGNATED_INITIALIZER;
- (void) clearImages;
- (void) addImage: (NSImage*)image align: (NSInteger)align at: (NSInteger)top size: (NSSize)size linkid: (NSUInteger)linkid;
- (void) drawRect: (NSRect)rect;
- (void) adjustTextviewHeightForLowImages;
- (void) invalidateLayout;
- (void) unoverlap: (MarginImage *)image;
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
    NSTextView *textview;

    NSInteger line_request;
	NSInteger hyper_request;

    BOOL echo_toggle_pending; /* if YES, line echo behavior will be inverted, starting from the next line event*/
    BOOL echo; /* if YES, current line input will be deleted from text view */

    NSInteger fence;		/* for input line editing */
    NSInteger lastseen;	/* for more paging */
    NSInteger lastchar;	/* for smart formatting */

    NSString *history[HISTORYLEN];
    NSInteger historypos;
    NSInteger historyfirst, historypresent;
}

- (void) recalcBackground;
- (void) onKeyDown: (NSEvent*)evt;
- (void) echo: (BOOL)val;
- (BOOL) myMouseDown: (NSEvent*)theEvent;

@property (readonly) NSInteger lastchar;

@end

