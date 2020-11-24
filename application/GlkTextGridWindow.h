/* TextGrid window controller */

#import "InputTextField.h"

@interface MyGridTextView : NSTextView <NSAccessibilityNavigableStaticText>

@end

@interface GlkTextGridWindow
    : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
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

@property MyGridTextView *textview;

@property NSRange restoredSelection;
@property NSUInteger selectedRow;
@property NSUInteger selectedCol;
@property NSString *selectedString;

@property NSColor *pendingBackgroundCol;
@property NSMutableAttributedString *bufferTextStorage;

@property NSString *enteredTextSoFar;

@property BOOL usingStyles;

- (BOOL)myMouseDown:(NSEvent *)theEvent;
- (IBAction)speakStatus:(id)sender;
- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;

@end
