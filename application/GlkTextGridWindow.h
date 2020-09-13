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
    NSUInteger maxInputLength;
    BOOL line_request;
    BOOL hyper_request;
    BOOL mouse_request;
    BOOL transparent;

    NSUInteger lastStyle;
    NSInteger terminator;

    NSTextView *fieldEditor;
    NSString *enteredTextSoFar;
}

@property NSRange restoredSelection;

- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (IBAction)speakStatus:(id)sender;
- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;

@end
