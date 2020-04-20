/* TextGrid window controller */

@class MyGridTextView;

@interface GlkTextGridWindow
    : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
    NSTextField *input;
    MyGridTextView *textview;
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

    NSTextView *fieldEditor;
}

@property NSRange restoredSelection;
- (void)restoreSelection;

- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (IBAction)speakStatus:(id)sender;
- (void)deferredGrabFocus:(id)sender;
- (void)restoreInput;
- (void)resignInput;

@end
