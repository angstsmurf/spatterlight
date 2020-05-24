/* TextGrid window controller */

@class MyGridTextView;
@class MyGridTextField;

@interface GlkTextGridWindow
    : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
    MyGridTextField *input;
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

    NSUInteger lastStyle;

    NSTextView *fieldEditor;
    NSString *enteredTextSoFar;
}

@property NSRange restoredSelection;

- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (IBAction)speakStatus:(id)sender;
- (void)deferredGrabFocus:(id)sender;

@end
