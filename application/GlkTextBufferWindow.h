/*
 * Extend NSTextContainer to have images in the margins with
 * the text flowing around them.
 * TODO: check for the flowbreak character.
 */

@interface MarginContainer : NSTextContainer
{
    NSMutableArray *margins;
}

- (id) initWithContainerSize: (NSSize)size;
- (void) clearImages;
- (void) addImage: (NSImage*)image align: (int)align at: (int)top size: (NSSize)size;
- (void) flowBreakAt: (int)pos;
- (void) drawRect: (NSRect)rect;
- (void) invalidateLayout;

@end

/*
 * TextBuffer window controller
 */

#define HISTORYLEN 50

@interface GlkTextBufferWindow : GlkWindow
{
    NSScrollView *scrollview;
    NSTextStorage *textstorage;
    NSLayoutManager *layoutmanager;
    MarginContainer *container;
    NSTextView *textview;
    
    int char_request;
    int line_request;
    int fence;		/* for input line editing */
    int lastseen;	/* for more paging */
    int lastchar;	/* for smart formatting */
    
    NSString *history[HISTORYLEN];
    int historypos;
    int historyfirst, historypresent;    
}

- (void) recalcBackground;
- (void) onKeyDown: (NSEvent*)evt;
- (int) lastchar;

@end

