#import "GlkTextBufferWindowPrivate.h"

#import "GlkController.h"
#import "ZColor.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "BufferTextView.h"
#include "glkimp.h"

#import "DebugLog.h"

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
    lastAtBottom = NO;
    lastAtTop = NO;
    rewrapFollowupScroll = NO;
    scrollAdjustTimeStamp = [NSDate distantPast];
    [self resetScrollState];
    [container invalidateLayout:nil];
    _pendingClear = NO;
}

// State shared by reallyClear and clearScrollback: character indices into
// the old text (lastseen marker, rewrap anchor, VoiceOver move ranges) and
// the margin images, none of which survive deleting the text before them.
- (void)resetScrollState {
    _lastseen = 0;
    docHeightAtLastSeen = 0;
    _lastchar = '\n';
    lastVisible = 0;
    rewrapLastseenChar = NSNotFound;
    _printPositionOnInput = 0;
    [container clearImages];
    self.moveRanges = [[NSMutableArray alloc] init];
    moveRangeIndex = 0;
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

    NSUInteger charsAfterFence = (fence < length) ? length - fence : 0;
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

    // Belt and braces: the delete bypasses the text view delegate, and this
    // keeps willProcessEditing from treating it as input.
    line_request = NO;

    [textstorage deleteCharactersInRange:NSMakeRange(0, length - prompt)];

    if (textstorage.length < charsAfterFence)
        fence = 0;
    else
        fence = textstorage.length - charsAfterFence;

    line_request = save_request;

    [self resetScrollState];
    for (NSView *view in _textview.subviews)
        if ([view isKindOfClass:[MarginImage class]])
            [view removeFromSuperview];
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
    if (bufferTextstorage.length > kOutputBufferHardCap) {
        // Don't cut a surrogate pair or a composed character in two: if the
        // cut falls inside one, drop all of it.
        NSUInteger cut = kOutputBufferHardCapTrim;
        NSRange composed = [bufferTextstorage.string rangeOfComposedCharacterSequenceAtIndex:cut];
        if (composed.location < cut)
            cut = NSMaxRange(composed);
        bufferTextstorage = [bufferTextstorage attributedSubstringFromRange:NSMakeRange(cut, bufferTextstorage.length - cut)].mutableCopy;
    }

    if (line_request)
        NSLog(@"Printing to text buffer window during line request");

    // Output right after a hyperlink click cancelled the line request: the
    // link ran a command.
    if (lineCancelledAfterLink)
        [self linkDidEnterCommand];

    [self printToWindow:str style:stylevalue];

    if (self.glkctl.gameID == kGameIsDeadCities && line_request && [str hasSuffix:@"\n"]) {
        // This is against the Glk spec but makes
        // hyperlinks in Dead Cities work.
        // Turn this off by disabling game specific hacks in preferences.
        [self sendInputLine:[self currentInputText] withTerminator:0];
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
    // The smart-quotes theme option rewrites straight quotes on their way in
    // (handlePrintOnWindow), so the interpreter's record of what it printed
    // still has the straight ones. Compare with both sides straightened.
    // (The option's dash and double-space rewrites change the length, and
    // still defeat the match.)
    NSString * (^straightened)(NSString *) = ^NSString *(NSString *str) {
        NSMutableString *m = str.uppercaseString.mutableCopy;
        NSRange all = NSMakeRange(0, m.length);
        [m replaceOccurrencesOfString:@"\u2018" withString:@"'" options:NSLiteralSearch range:all];
        [m replaceOccurrencesOfString:@"\u2019" withString:@"'" options:NSLiteralSearch range:all];
        [m replaceOccurrencesOfString:@"`" withString:@"'" options:NSLiteralSearch range:all];
        [m replaceOccurrencesOfString:@"\u201c" withString:@"\"" options:NSLiteralSearch range:all];
        [m replaceOccurrencesOfString:@"\u201d" withString:@"\"" options:NSLiteralSearch range:all];
        return m;
    };
    NSString *stringToRemove = [textstorage.string substringFromIndex:textstorage.length - buf.length];
    if ([straightened(stringToRemove) isEqualToString:straightened(buf)]) {
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
        // The same goes for the read boundary, which is a y-coordinate.
        // Left below the shortened text it would count the text printed
        // next, up to the old boundary, as read, and the auto-scroll would
        // page past it; and while nothing has been printed yet,
        // reallyPerformScroll would take it for a restarted game and stop
        // auto-scrolling until the next keypress.
        if (rewrapLastseenChar != NSNotFound && rewrapLastseenChar >= newLength)
            rewrapLastseenChar = newLength ? newLength - 1 : NSNotFound;
        if (_lastseen > 0) {
            NSRange glyphs = [layoutmanager glyphRangeForTextContainer:container];
            NSInteger docBottom = 0;
            if (glyphs.length) {
                NSRect lastLine = [layoutmanager
                                   lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphs) - 1
                                   effectiveRange:nil];
                docBottom = (NSInteger)ceil(NSMaxY(lastLine));
            }
            if (_lastseen > docBottom)
                _lastseen = docBottom;
            if (docHeightAtLastSeen > (CGFloat)docBottom)
                docHeightAtLastSeen = (CGFloat)docBottom;
        }
        // Clip the VoiceOver move ranges too, or speaking the last move
        // would ask for characters that are gone.
        NSMutableArray<NSValue *> *clipped = [[NSMutableArray alloc] initWithCapacity:self.moveRanges.count];
        for (NSValue *value in self.moveRanges) {
            NSRange range = value.rangeValue;
            if (range.location >= newLength)
                break;
            if (NSMaxRange(range) > newLength)
                range.length = newLength - range.location;
            [clipped addObject:[NSValue valueWithRange:range]];
        }
        if (![clipped isEqualToArray:self.moveRanges]) {
            self.moveRanges = clipped;
            if (moveRangeIndex > clipped.count)
                moveRangeIndex = clipped.count;
        }
    }
    return result;
}

// Request a change in echo behavior. The toggle is deferred until the next
// line input request to avoid disrupting the current input.
- (void)echo:(BOOL)val {
    // Applied on the next line request (see initLine). Recomputed on every
    // call, so that turning echo off and back on again before then is a
    // no-op rather than a toggle.
    echo_toggle_pending = (val != echo);
}

@end
