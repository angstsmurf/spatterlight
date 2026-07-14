#import "GlkTextBufferWindowPrivate.h"

#import "GlkController.h"
#import "GlkController+Speech.h"
#import "Theme.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "InfocomV6MenuHandler.h"
#import "MyAttachmentCell.h"
#import "BufferTextView.h"
#include "glkimp.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

@implementation GlkTextBufferWindow (Speech)

// Record the text range of the latest "move" (turn) for VoiceOver speech.
// A move is the text output between two consecutive input prompts. The
// range is computed from _printPositionOnInput (set by markLastSeen) to
// the current end of text. For Infocom V6 menus, delegates to the
// specialized menu handler instead.
- (BOOL)setLastMove {
    NSUInteger maxlength = textstorage.length;

    if (!maxlength) {
        self.moveRanges = [[NSMutableArray alloc] init];
        moveRangeIndex = 0;
        _printPositionOnInput = 0;
        return NO;
    }

    if (self.glkctl.showingInfocomV6Menu) {
        [self.glkctl.infocomV6MenuHandler updateMoveRanges:self];
        return YES;
    }

    NSRange allText = NSMakeRange(0, maxlength);
    NSRange currentMove = allText;

    if (self.moveRanges.lastObject) {
        NSRange lastMove = self.moveRanges.lastObject.rangeValue;
        if (NSMaxRange(lastMove) > maxlength) {
            // Removing last move object
            // (because it goes past the end)
            [self.moveRanges removeLastObject];
        } else if (lastMove.length == maxlength) {
            return NO;
        } else {
            if (lastMove.location == _printPositionOnInput && lastMove.length != maxlength - _printPositionOnInput) {
                // Removing last move object (because it does not go all
                // the way to the end)
                [self.moveRanges removeLastObject];
                currentMove = NSMakeRange(_printPositionOnInput, maxlength);
            } else {
                NSUInteger chainedStart = NSMaxRange(lastMove);
                // A restore (and other full-screen reprints) dumps a block of
                // text that belongs to no command, sometimes spread over several
                // flushes. If a setLastMove fired mid-flush, the previous move's
                // end — and thus this chained start — can land in the middle of a
                // word, which puts every following command "out of phase". A move
                // never legitimately begins mid-word (commands start just after a
                // "> " prompt), so when we detect that, snap forward to the next
                // prompt and extend the previous move to absorb the reprint.
                NSString *text = textstorage.string;
                if (chainedStart > 0 && chainedStart < maxlength &&
                    [[NSCharacterSet alphanumericCharacterSet]
                        characterIsMember:[text characterAtIndex:chainedStart - 1]]) {
                    NSUInteger snapped = [self commandStartForwardFrom:chainedStart
                                                                  upTo:maxlength
                                                              inString:text];
                    if (snapped != NSNotFound && snapped > chainedStart) {
                        self.moveRanges[self.moveRanges.count - 1] =
                            [NSValue valueWithRange:NSMakeRange(lastMove.location,
                                                               snapped - lastMove.location)];
                        chainedStart = snapped;
                    }
                }
                currentMove = NSMakeRange(chainedStart, maxlength);
            }
        }
    }

    currentMove = NSIntersectionRange(allText, currentMove);
    if (currentMove.length == 0)
        return NO;
    NSString *string = [textstorage.string substringWithRange:currentMove];
    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (string.length == 0)
        return NO;
    moveRangeIndex = self.moveRanges.count;
    [self.moveRanges addObject:[NSValue valueWithRange:currentMove]];
    _lastNewTextOnTurn = self.glkctl.turns;

    return YES;
}

// Scan forward from `from` (up to `limit`) for the next prompt — a '>' at the
// start of a line — and return the index where the typed command begins (just
// past the '>' and an optional single space). Returns NSNotFound if there is no
// prompt in range. Used by setLastMove to re-anchor a move boundary that landed
// mid-reprint after a restore onto the next real command.
- (NSUInteger)commandStartForwardFrom:(NSUInteger)from upTo:(NSUInteger)limit inString:(NSString *)s {
    for (NSUInteger i = from; i < limit; i++) {
        if ([s characterAtIndex:i] == '>' && (i == 0 || [s characterAtIndex:i - 1] == '\n')) {
            NSUInteger cmd = i + 1;
            if (cmd < limit && [s characterAtIndex:cmd] == ' ')
                cmd++;
            if (cmd < limit)
                return cmd;
        }
    }
    return NSNotFound;
}

// Realign restored move ranges after an autorestore. An in-game restore in the
// saved session can leave a move boundary in the middle of a word (a partial
// reprint length captured mid-flush), and that gets persisted into the autosave.
// If we spot such a boundary, rebuild the whole list from the reconstructed
// text's prompt boundaries (a '>' at a line start), so the command-history rotor
// and spoken move navigation realign. Clean autosaves (no mid-word boundary) and
// games with no detectable prompts are left untouched.
- (void)repairRestoredMoveRanges {
    NSString *text = textstorage.string;
    NSUInteger length = text.length;
    if (length == 0 || self.moveRanges.count == 0)
        return;

    NSCharacterSet *alnum = [NSCharacterSet alphanumericCharacterSet];
    BOOL misaligned = NO;
    for (NSValue *v in self.moveRanges) {
        NSUInteger start = v.rangeValue.location;
        if (start > 0 && start < length &&
            [alnum characterIsMember:[text characterAtIndex:start - 1]]) {
            misaligned = YES;
            break;
        }
    }
    if (!misaligned)
        return;

    NSMutableArray<NSValue *> *rebuilt = [[NSMutableArray alloc] init];
    NSUInteger moveStart = 0;
    for (NSUInteger i = 0; i < length; i++) {
        if ([text characterAtIndex:i] == '>' && (i == 0 || [text characterAtIndex:i - 1] == '\n')) {
            NSUInteger cmd = i + 1;
            if (cmd < length && [text characterAtIndex:cmd] == ' ')
                cmd++;
            // Stop at the live input prompt (its command begins at fence): the
            // most recent move runs through it to the end, as in live play.
            if (line_request && fence > 0 && cmd >= fence)
                break;
            if (cmd > moveStart && cmd < length) {
                [rebuilt addObject:[NSValue valueWithRange:NSMakeRange(moveStart, cmd - moveStart)]];
                moveStart = cmd;
            }
        }
    }
    if (rebuilt.count == 0)
        return; // No prompts found; keep restored ranges rather than collapse.
    [rebuilt addObject:[NSValue valueWithRange:NSMakeRange(moveStart, length - moveStart)]];

    self.moveRanges = rebuilt;
    moveRangeIndex = rebuilt.count;
}

// Convert a move range to a speakable string. Retrieves the
// accessibility-attributed string, then optionally injects image
// descriptions (parenthesized) at the positions of any image
// attachments. Also strips the command line from the start of the
// string if the "speak command" preference is off.
- (NSString *)stringFromRangeVal:(NSValue *)val {
    NSRange range = val.rangeValue;
    NSAttributedString *attStr = [_textview accessibilityAttributedStringForRange:range];
    NSMutableString *string = attStr.string.mutableCopy;

    if (self.theme.vOSpeakImages != kVOImageNone) {
        NSArray *keys;
        NSDictionary *attachments = [self attachmentsInRange:range withKeys:&keys];
        NSUInteger offset = 0;
        for (NSNumber *num in keys) {
            NSUInteger index = (NSUInteger)num.intValue - range.location + offset;
            MyAttachmentCell *cell = (MyAttachmentCell *)((NSTextAttachment *)attachments[num]).attachmentCell;
            NSString *label = cell.customA11yLabel;
            if (self.theme.vOSpeakImages != kVOImageWithDescriptionOnly || cell.hasDescription) {
                label = [NSString stringWithFormat:@"(%@)", label];
                [string insertString:label atIndex:index];
                offset += label.length;
            }
        }
    }

    // Strip command line if the speak command setting is off
    if (!self.glkctl.theme.vOSpeakCommand && range.location != 0 && !self.glkctl.showingInfocomV6Menu) {
        NSUInteger promptIndex = range.location - 1;
        if ([textstorage.string characterAtIndex:promptIndex] == '>' || (promptIndex > 0 && [textstorage.string characterAtIndex:promptIndex - 1] == '>')) {
            NSRange foundRange = [string rangeOfString:@"\n"];
            if (foundRange.location != NSNotFound) {
                string = [string substringFromIndex:foundRange.location].mutableCopy;
            }
        }
    }


    return string;
}

// Return the speakable text of the most recent move, positioning the
// move range index to the last entry.
- (NSString *)lastMoveString {
    NSString *str = @"";

    if (self.moveRanges.count) {
        moveRangeIndex = self.moveRanges.count - 1;
        str = [self stringFromRangeVal:self.moveRanges.lastObject];
    }
    return str;
}

// Clear the deduplication state so the next repeatLastMove call will
// always speak, even if the text hasn't changed.
- (void)resetLastSpokenString {
    lastSpokenRange = NSMakeRange(0, 0);
    lastSpokenString = @"";
}

// Speak the latest move text via VoiceOver. Cancels any pending Z-machine
// menu or form timers first. Prepends quote box text if one is visible.
// When called automatically by GlkController (sender == glkctl), deduplicates
// against the last spoken string to avoid repeating identical output.
// When invoked by the user (menu item or shortcut), always speaks.
- (void)repeatLastMove:(id)sender {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];

    NSString *str = [self lastMoveString];

    if (glkctl.quoteBoxes.count) {
        GlkTextGridWindow *box = glkctl.quoteBoxes.lastObject;

        // A merged side-by-side box stores its text interleaved row by row (see
        // mergeSideBySideQuoteBoxAtColumn:); use the de-interleaved columns so
        // the two are read one after the other rather than alternating.
        NSString *boxStr = box.quoteboxColumns.count ? box.deinterleavedQuoteboxString : box.textview.string;
        str = [boxStr stringByAppendingString:str];
        str = [@"QUOTE: \n\n" stringByAppendingString:str];
    }

    // sender is only set to self.glkctl if repeatLastMove is called by
    // the GlkController after entering a command, or the window gets focus,
    // or VoiceOver is activated, i.e. not in response to the player
    // using the shortcut or the menu item.
    if (!str.length && sender != glkctl) {
        [glkctl speakStringNow:@"No last move to speak"];
        return;
    }

    // The GlkController might sometimes think there
    // is new text to speak after a key event even if there isn't,
    // so we perform an extra check for that here.
    if (sender == glkctl && NSEqualRanges(lastSpokenRange, self.moveRanges.lastObject.rangeValue) && [str isEqualToString:lastSpokenString]) {
        return;
    } else {
        lastSpokenRange = self.moveRanges.lastObject.rangeValue;
        lastSpokenString = str;
    }

    [glkctl speakString:str];
}

// Navigate backward through move history and speak the previous move.
// Announces "At first move" when already at the beginning.
- (void)speakPrevious {
    if (!self.moveRanges.count)
        return;
    NSString *prefix = @"";
    if (moveRangeIndex > 0) {
        moveRangeIndex--;
    } else {
        prefix = @"At first move.\n";
        moveRangeIndex = 0;
    }
    NSString *str = [prefix stringByAppendingString:[self stringFromRangeVal:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

// Navigate forward through move history and speak the next move.
// Announces "At last move" when already at the end.
- (void)speakNext {
    [self setLastMove];
    if (!self.moveRanges.count) {
        return;
    }

    NSString *prefix = @"";

    if (moveRangeIndex < self.moveRanges.count - 1) {
        moveRangeIndex++;
    } else {
        prefix = @"At last move.\n";
        moveRangeIndex = self.moveRanges.count - 1;
    }

    NSString *str = [prefix stringByAppendingString:[self stringFromRangeVal:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

// Speak the entire text content of this buffer window. Used when the
// user requests the status to be read aloud. Cancels pending Z-machine
// menu/form timers to avoid conflicts.
- (void)speakStatus {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];
    [glkctl speakStringNow:textstorage.string];
}

// Build move ranges from Infocom V6 menu item strings. Each menu item
// is located in the text storage by string matching, and its range is
// added to moveRanges so VoiceOver can navigate between menu items.
- (void)movesRangesFromV6Menu:(NSArray<NSString *> *)menuStrings {
    self.moveRanges = [[NSMutableArray<NSValue *> alloc] initWithCapacity:menuStrings.count];
    moveRangeIndex = menuStrings.count - 1;
    [self flushDisplay];
    for (NSString *str in menuStrings) {
        NSRange range = [textstorage.string rangeOfString:str];
        if (range.location != NSNotFound) {
            [self.moveRanges addObject:[NSValue valueWithRange:range]];
        }
    }
}

#pragma mark Accessibility

// This view is a container, not an individual accessibility element.
// The text view and its children provide the actual accessible content.
- (BOOL)isAccessibilityElement {
    return NO;
}

// Collect hyperlink ranges for accessibility, prioritizing recent moves.
// Walks move ranges in reverse order (most recent first) and caps at
// 15 links to avoid overwhelming VoiceOver with too many elements.
- (NSArray <NSValue *> *)links {
    NSRange allText = NSMakeRange(0, textstorage.length);
    if (self.moveRanges.count < 2)
        return [self linksInRange:allText];
    NSMutableArray <NSValue *> *links = [[NSMutableArray alloc] init];

    // Make sure that no text after last moveRange slips through
    NSRange lastMoveRange = self.moveRanges.lastObject.rangeValue;
    NSRange stubRange = NSMakeRange(NSMaxRange(lastMoveRange), textstorage.length);
    stubRange = NSIntersectionRange(allText, stubRange);
    if (stubRange.length) {
        [links addObjectsFromArray:[self linksInRange:stubRange]];
    }

    for (NSValue *rangeVal in [self.moveRanges reverseObjectEnumerator])
    {
        // Print some info
        [links addObjectsFromArray:[self linksInRange:rangeVal.rangeValue]];
        if (links.count > 15)
            break;
    }
    if (links.count > 15)
        links.array = [links subarrayWithRange:NSMakeRange(0, 15)];
    return links;
}

// Find all NSLinkAttributeName ranges within the given text range.
- (NSArray <NSValue *> *)linksInRange:(NSRange)range {
    NSMutableArray <NSValue *> __block *links = [[NSMutableArray alloc] init];
    [textstorage
     enumerateAttribute:NSLinkAttributeName
     inRange:range
     options:0
     usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        [links addObject:[NSValue valueWithRange:subrange]];
    }];
    return links;
}

// Return ranges of all image attachments for accessibility, respecting
// the user's preference for whether images should be announced.
- (NSArray<NSValue *> *)images {
    if (self.theme.vOSpeakImages == kVOImageNone)
        return @[];
    NSArray<NSValue *> *images = [self imagesInRange:NSMakeRange(0,_textview.string.length)];
    return images;
}

// Find image attachment ranges within the given text range. When the
// "description only" preference is set, skips images without descriptions.
// Expands margin image ranges to length 2 so VoiceOver can target them.
- (NSArray<NSValue *> *)imagesInRange:(NSRange)range {
    NSMutableArray<NSValue *> __block *images = [NSMutableArray new];
    BOOL withDescOnly = (self.theme.vOSpeakImages == kVOImageWithDescriptionOnly);

    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:range
     options:0
     usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        MyAttachmentCell *cell = (MyAttachmentCell *)((NSTextAttachment *)value).attachmentCell;
        if (withDescOnly && cell.hasDescription == NO)
            return;
        if (subrange.length < 2 && (cell.alignment == imagealign_MarginRight || cell.alignment == imagealign_MarginLeft))
            subrange.length = 2;
        [images addObject:[NSValue valueWithRange:subrange]];
    }];

    return images;
}

// Collect all text attachments in the given range, keyed by their
// character index. Used by stringFromRangeVal to inject image
// descriptions into the spoken text at the correct positions.
- (NSDictionary <NSNumber *, NSTextAttachment *> *)attachmentsInRange:(NSRange)range withKeys:(NSArray * __autoreleasing *)keys {
    NSMutableDictionary <NSNumber *, NSTextAttachment *> __block *attachments = [NSMutableDictionary new];
    NSMutableArray __block *mutKeys = [NSMutableArray new];
    range = NSIntersectionRange(range, NSMakeRange(0, textstorage.length));
    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:range
     options:0
     usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        [mutKeys addObject:@(subrange.location)];
        attachments[mutKeys.lastObject] = value;
    }];
    *keys = mutKeys;
    return attachments;
}

@end
