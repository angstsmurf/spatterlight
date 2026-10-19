//
//  GlkTextGridWindowPrivate.h
//  Spatterlight
//
//  The class extension shared by GlkTextGridWindow.m and its categories.
//

#import "GlkTextGridWindow.h"
#import "GlkTextGridWindow+Styles.h"
#import "GlkTextGridWindow+Input.h"
#import "Constants.h"

#include "glkimp.h"

#import "DebugLog.h"

// Debug builds log to stderr without the NSLog timestamp prefix.
#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#endif

@interface GlkTextGridWindow () {
    NSScrollView *scrollview;
    NSTextStorage *textstorage;
    NSLayoutManager *layoutmanager;
    NSTextContainer *container;
    NSUInteger rows, cols;
    NSUInteger xpos, ypos;
    NSUInteger maxInputLength;
    BOOL transparent;
    NSInteger terminator;
    // The text of the deferredInitLine: request still waiting, if any, so a
    // newer one can cancel it.
    NSString *deferredLineText;

    // Backing ivars of properties the categories use directly, declared here
    // because auto-synthesized ones are only visible in the main @implementation.
    GridTextView *_textview;
    NSMutableAttributedString *_bufferTextStorage;
    NSString *_enteredTextSoFar;
    NSSize _quoteboxSize;
    __weak NSScrollView *_quoteboxParent;
    NSArray<NSDictionary *> *_quoteboxColumns;
    NSColor *_pendingBackgroundCol;
    BOOL _hasNewText;
    NSDate *_keyPressTimeStamp;
    NSString *_lastKeyPress;
    NSUInteger _quoteboxVerticalOffset;

    // The inherited dirty flag means: _bufferTextStorage, or anything that
    // decides the window background colour, has changed since the last
    // flushDisplay. The controller flushes every window on every interpreter
    // update, so a static grid would otherwise be re-scanned, re-laid-out and
    // repainted on each tick of a timer-driven game.
}

// The selection, saved by encodeWithCoder: and put back after an autorestore
// or a resize.
@property NSRange restoredSelection;
@property NSUInteger selectedRow;
@property NSUInteger selectedCol;
@property NSString *selectedString;

@property NSColor *pendingBackgroundCol;

// Whether anything was printed since setLastMove last asked.
@property BOOL hasNewText;

// For Bureaucracy form accessibility: the last key typed and when, so that
// the letter the game prints back for it is spoken as typing.
@property NSDate *keyPressTimeStamp;
@property NSString *lastKeyPress;

@property NSUInteger quoteboxVerticalOffset;

// Print a string at the cursor, wrapping at newlines. Used for game output
// and for echoing line input.
- (void)printToWindow:(NSString *)string style:(NSUInteger)stylevalue;

// Converting between (column, row) cells and _bufferTextStorage indexes.
// Every row but the last ends with a newline, so a row is cols + 1 long.
- (NSUInteger)indexOfColumn:(NSUInteger)column row:(NSUInteger)row;
- (NSUInteger)rowOfIndex:(NSUInteger)index;
- (NSUInteger)columnOfIndex:(NSUInteger)index;

@end
