/* TextGrid window controller */

@class MyGridTextView;
@class MyFieldEditor;
@class MyGridTextField;

@interface GlkTextGridWindow
    : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
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

    NSInteger terminator;
}

@property NSRange restoredSelection;
@property NSUInteger selectedRow;
@property NSUInteger selectedCol;
@property NSString *selectedString;

@property MyGridTextField *input;
@property (readonly) MyFieldEditor *fieldEditor;
@property NSColor *pendingBackgroundCol;
@property NSMutableAttributedString *bufferTextStorage;

@property NSString *enteredTextSoFar;

@property BOOL usingStyles;

- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (IBAction)speakStatus:(id)sender;
- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;

@end
