/* TextGrid window controller */

@interface GlkTextGridWindow
    : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate> {
    NSTextField *input;
    NSTextView *textview;
    NSScrollView *scrollview;
    NSTextStorage *textstorage;
    NSLayoutManager *layoutmanager;
    NSTextContainer *container;
    NSUInteger rows, cols;
    NSUInteger xpos, ypos;
    BOOL line_request;
    BOOL hyper_request;
    BOOL mouse_request;
    BOOL dirty;
    BOOL transparent;
}

@property NSRange restoredSelection;
- (void)restoreSelection;

- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (IBAction)speakStatus:(id)sender;

@end
