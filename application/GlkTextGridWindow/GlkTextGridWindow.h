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
// The grid column where this quote box began, used to lay out two
// side-by-side boxes (e.g. Trinity's Clarke/Lebling pair) into one wide box.
@property NSUInteger quoteboxColumn;
@property (weak) NSScrollView *quoteboxParent;

// When two side-by-side boxes are merged into one wide box (see
// mergeSideBySideQuoteBoxAtColumn:), the on-screen text storage interleaves the
// two columns row by row. These hold each column's text de-interleaved, so
// VoiceOver and the speech helpers can present them as two separate blocks.
// Each entry is @{ @"value": NSString, @"offset": NSNumber, @"width": NSNumber }
// where offset/width are in grid cells from the (merged) box's left edge.
@property (nonatomic) NSArray<NSDictionary *> *quoteboxColumns;
// Cached AXStaticText children built from quoteboxColumns, kept stable so
// VoiceOver doesn't lose its cursor between accessibilityChildren queries.
@property (nonatomic) NSArray *quoteboxAXChildren;
// De-interleaved text (left column, then right column) for the speech helpers.
@property (nonatomic, readonly) NSString *deinterleavedQuoteboxString;

- (void)quotebox:(NSUInteger)linesToSkip;
- (void)quoteboxAdjustSize:(id)sender;

@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger indexOfPos;

- (BOOL)myMouseDown:(NSEvent *)theEvent;

- (void)deferredGrabFocus:(id)sender;
- (void)recalcBackground;

- (NSSize)currentSizeInChars;
- (unichar)characterAtPoint:(NSPoint)point;

@end
