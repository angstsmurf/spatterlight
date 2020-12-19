#import "GlkWindow.h"

/* TextGrid window controller */

@class InputTextField, InputHistory;

@interface MyGridTextView : NSTextView <NSAccessibilityNavigableStaticText>

@end

@interface GlkTextGridWindow
    : GlkWindow <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
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

    /* For command history */
    InputHistory *history;

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
@property BOOL hasNewText;

- (BOOL)myMouseDown:(NSEvent *)theEvent;

- (BOOL)setLastMove;
- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;
- (void)speakStatus;
- (void)speakMostRecent;

@end
