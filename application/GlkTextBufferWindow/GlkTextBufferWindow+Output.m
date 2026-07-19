#import "GlkTextBufferWindowPrivate.h"

#import "GlkController.h"
#import "ZColor.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "BufferTextView.h"
#include "glkimp.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

// Hard cap on the *uncommitted* output buffer (bufferTextstorage), applied in
// putString before the text is flushed to the live text storage. This is a
// last-resort guard against a single unbroken burst of output from a runaway
// game; it is independent of the theme-driven rolling scrollback trim
// (trimScrollbackIfNeeded:), which bounds the committed text storage. When the
// buffer grows past the cap, the oldest kOutputBufferHardCapTrim characters are
// dropped from the front.
static const NSUInteger kOutputBufferHardCap = 50000;
static const NSUInteger kOutputBufferHardCapTrim = 25000;

@implementation GlkTextBufferWindow (Output)

// Mark the window for clearing. The actual clear happens in flushDisplay via
// reallyClear. Resets the background color based on Z-machine color state.
- (void)clear {
    _pendingClear = YES;
    storedNewline = nil;
    bufferTextstorage = [[NSMutableAttributedString alloc] init];
    if (currentZColor && currentZColor.bg != zcolor_Current)
        bgnd = currentZColor.bg;
    [self recalcBackground];
    [self resetLastSpokenString];
}

// Perform the actual clear: reset the fence, lastseen marker, margin images,
// move ranges (VoiceOver history), and invalidate the layout.
- (void)reallyClear {
    [_textview resetTextFinder];
    fence = 0;
    pendingEchoDeleteRange = NSMakeRange(0, 0);
    _lastseen = 0;
    docHeightAtLastSeen = 0;
    _lastchar = '\n';
    lastAtBottom = NO;
    lastAtTop = NO;
    lastVisible = 0;
    rewrapLastseenChar = NSNotFound;
    rewrapFollowupScroll = NO;
    scrollAdjustTimeStamp = [NSDate distantPast];
    _printPositionOnInput = 0;
    [container clearImages];

    self.moveRanges = [[NSMutableArray alloc] init];
    moveRangeIndex = 0;
    [container invalidateLayout:nil];
    _pendingClear = NO;
}

// Clear old text, keeping only the current prompt and input. Finds the second
// newline from the end of the text and deletes everything before it, preserving
// the most recent prompt line and any in-progress input.
- (void)clearScrollback:(id)sender {
    [self flushDisplay];
    if (storedNewline) {
        [textstorage appendAttributedString:storedNewline];
    }
    storedNewline = nil;

    NSString *string = textstorage.string;
    NSUInteger length = string.length;
    BOOL save_request = line_request;

    [_textview resetTextFinder];

    NSUInteger prompt;
    NSUInteger i;

    NSUInteger charsAfterFence = length - fence;
    BOOL found = NO;

    // find the second newline from the end
    for (i = 0; i < length; i++) {
        if ([string characterAtIndex:length - i - 1] == '\n' || [string characterAtIndex:length - i - 1] == '\r') {
            if (found) {
                break;
            } else {
                found = YES;
            }
        }
    }
    if (i < length) {
        prompt = i;
    } else {
        // Found no newline
        if (length > 1000) {
            prompt = 0;
        } else {
            prompt = length;
        }
    }

    line_request = NO;

    [textstorage deleteCharactersInRange:NSMakeRange(0, length - prompt)];

    _lastseen = 0;
    docHeightAtLastSeen = 0;
    _lastchar = '\n';
    _printPositionOnInput = 0;
    if (textstorage.length < charsAfterFence)
        fence = 0;
    else
        fence = textstorage.length - charsAfterFence;

    line_request = save_request;

    [container clearImages];
    for (NSView *view in _textview.subviews)
        if ([view isKindOfClass:[MarginImage class]])
            [view removeFromSuperview];
    self.moveRanges = [[NSMutableArray alloc] init];
}

// Append a string to the window with the given Glk style. This is the main
// entry point for game text output. Includes a safety valve that trims the
// buffer if it exceeds 50K characters (keeping the last 25K) to prevent
// memory issues with extremely verbose games.
- (void)putString:(NSString *)str style:(NSUInteger)stylevalue {

    if (!str.length) {
        NSLog(@"Null string!");
        return;
    }

    // Prevent unbounded buffer growth for games that print enormous amounts.
    // See kOutputBufferHardCap; this is separate from the committed-text
    // scrollback trim in trimScrollbackIfNeeded:.
    if (bufferTextstorage.length > kOutputBufferHardCap)
        bufferTextstorage = [bufferTextstorage attributedSubstringFromRange:NSMakeRange(kOutputBufferHardCapTrim, bufferTextstorage.length - kOutputBufferHardCapTrim)].mutableCopy;

    if (line_request)
        NSLog(@"Printing to text buffer window during line request");

    [self printToWindow:str style:stylevalue];

    if (self.glkctl.gameID == kGameIsDeadCities && line_request && [[str substringFromIndex:str.length - 1] isEqualToString:@"\n"]) {
        // This is against the Glk spec but makes
        // hyperlinks in Dead Cities work.
        // Turn this off by disabling game specific hacks in preferences.
        NSString *line = [textstorage.string substringFromIndex:fence];
        [self sendInputLine:line withTerminator:0];
    }
}

// Internal method that applies styling and appends text to bufferTextstorage.
// Handles several special cases:
// - Beyond Zork Font3 symbol translation (runic characters)
// - Preformatted style space-collapse prevention (using non-breaking space)
// - Trailing newline deferral (storedNewline) to avoid blank lines at bottom
- (void)printToWindow:(NSString *)str style:(NSUInteger)stylevalue {

    // Beyond Zork uses Font3 (style_BlockQuote) for graphical symbols like
    // arrows and runic letters. Translate them to Unicode equivalents.
    if (self.glkctl.usesFont3 && str.length == 1 && stylevalue == style_BlockQuote) {
        NSDictionary *font3 = [self font3ToUnicode];
        NSString *newString = font3[str];
        if (newString) {
            str = newString;
            stylevalue = style_Normal;
        }
    }

    // With certain fonts and sizes, strings containing only spaces will "collapse."
    // So if the first character is a space, we replace it with a &nbsp;
    if (stylevalue == style_Preformatted && [str hasPrefix:@" "]) {
        const unichar nbsp = 0xa0;
        NSString *nbspstring = [NSString stringWithCharacters:&nbsp length:1];
        str = [str stringByReplacingCharactersInRange:NSMakeRange(0, 1) withString:nbspstring];
    }

    // A lot of code to not print single newlines
    // at the bottom
    if (storedNewline) {
        [bufferTextstorage appendAttributedString:storedNewline];
        storedNewline = nil;
    }

    NSMutableDictionary *attributes = [self getCurrentAttributesForStyle:stylevalue];

    if (str.length > 1) {
        unichar c = [str characterAtIndex:str.length - 1];
        if (c == '\n') {
            storedNewline = [[NSAttributedString alloc]
                             initWithString:@"\n"
                             attributes:attributes];

            str = [str substringWithRange:NSMakeRange(0, str.length - 1)];
        }
    }
    _lastchar = [str characterAtIndex:str.length - 1];

    NSAttributedString *attstr = [[NSAttributedString alloc]
                                  initWithString:str
                                  attributes:attributes];

    [bufferTextstorage appendAttributedString:attstr];
    dirty = YES;
    if (!_pendingClear && textstorage.length && bufferTextstorage.length) {
        [self coalescePendingEchoDeleteWithAppend:bufferTextstorage];
        bufferTextstorage = [[NSMutableAttributedString alloc] init];
    }
}

// Remove a string from the end of the text storage if it matches (case-
// insensitive). Used by the interpreter to retract printed text. Returns the
// number of characters actually removed.
- (NSUInteger)unputString:(NSString *)buf {
    [self flushDisplay];
    // The ghost from a deferred echo-off delete must not be examined by the
    // tail match below, or unputString would either match against the player's
    // input or fail because the tail is the wrong style.
    [self applyPendingEchoDeleteIfValid];
    NSUInteger result = 0;
    // Guard against an underflow: if the string to retract is longer than the
    // whole text storage there is nothing valid to match, and computing
    // textstorage.length - buf.length would wrap and crash substringFromIndex:.
    if (buf.length > textstorage.length)
        return result;
    NSUInteger initialLength = textstorage.length;
    NSString *stringToRemove = [textstorage.string substringFromIndex:textstorage.length - buf.length].uppercaseString;
    if ([stringToRemove isEqualToString:buf.uppercaseString]) {
        [textstorage deleteCharactersInRange:NSMakeRange(textstorage.length - buf.length, buf.length)];
        result = initialLength - textstorage.length;
        // A tail delete invalidates every cached absolute character offset
        // past the new end, the same way a scrollback trim invalidates
        // offsets before the cut (shiftCachedOffsetsAfterTrimOf:). fence in
        // particular: a cancelLine right before this retract leaves it at
        // the old text end, and the next initLine computes the editable
        // range from it.
        NSUInteger newLength = textstorage.length;
        if (fence > newLength)
            fence = newLength;
        if (lastVisible > newLength)
            lastVisible = newLength;
        if (_printPositionOnInput > newLength)
            _printPositionOnInput = newLength;
        _lastchar = newLength ? [textstorage.string characterAtIndex:newLength - 1] : '\n';
    }
    return result;
}

// Request a change in echo behavior. The toggle is deferred until the next
// line input request to avoid disrupting the current input.
- (void)echo:(BOOL)val {
    if ((!(val) && echo) || (val && !(echo)))
        echo_toggle_pending = YES;
}

@end
