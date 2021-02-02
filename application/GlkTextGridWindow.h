#import "GlkWindow.h"

/* TextGrid window controller */

@class InputTextField, InputHistory;

@interface MyGridTextView : NSTextView <NSAccessibilityNavigableStaticText>

- (void)superMouseDown:(NSEvent *)theEvent;

@end

@interface GlkTextGridWindow
    : GlkWindow <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate>

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

// For Bureacracy form accessibility
@property NSDate *keyPressTimeStamp;
@property NSString *lastKeyPress;
@property NSUInteger lastForm;

@property NSSize quoteboxSize;
@property NSInteger quoteboxAddedOnTurn;
@property NSUInteger quoteboxVerticalOffset;
@property (weak) NSScrollView *quoteboxParent;

- (void)quotebox:(NSUInteger)linesToSkip;
- (void)quoteboxAdjustSize:(id)sender;

- (NSUInteger)indexOfPos;

- (BOOL)myMouseDown:(NSEvent *)theEvent;

- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;
- (void)speakStatus;

- (NSSize)currentSizeInChars;

- (void)saveAsRTF;

@end
