#import "GlkWindow.h"

@class GridTextView;

/* TextGrid window controller */

@class InputTextField, InputHistory;

@interface GlkTextGridWindow
    : GlkWindow <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate>

@property GridTextView *textview;

@property NSRange restoredSelection;
@property NSUInteger selectedRow;
@property NSUInteger selectedCol;
@property NSString *selectedString;

@property NSColor *pendingBackgroundCol;
@property NSMutableAttributedString *bufferTextStorage;

@property NSString *enteredTextSoFar;

@property BOOL hasNewText;

// For Bureaucracy form accessibility
@property NSDate *keyPressTimeStamp;
@property NSString *lastKeyPress;
@property NSUInteger lastForm;

@property NSSize quoteboxSize;
@property NSInteger quoteboxAddedOnPAC;
@property NSUInteger quoteboxVerticalOffset;
@property (weak) NSScrollView *quoteboxParent;

- (void)quotebox:(NSUInteger)linesToSkip;
- (void)quoteboxAdjustSize:(id)sender;

@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger indexOfPos;

- (BOOL)myMouseDown:(NSEvent *)theEvent;

- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;

- (NSSize)currentSizeInChars;
- (unichar)characterAtPoint:(NSPoint)point;

@end
