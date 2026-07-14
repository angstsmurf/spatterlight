#import "GlkTextBufferWindowPrivate.h"

#import "GlkEvent.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+Speech.h"
#import "Theme.h"
#import "InputHistory.h"
#import "NSString+Categories.h"
#import "NSColor+integer.h"
#import "ZColor.h"
#import "MarginContainer.h"
#import "BufferTextView.h"
#include "glkimp.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

@implementation GlkTextBufferWindow (Input)

// Handle keyboard input. This is the central key event dispatcher for buffer
// windows. Depending on the current input state, it will:
// - Forward to another window if this window doesn't want focus
// - Map Cmd+Arrow / Opt+Arrow to Home/End/PageUp/PageDown
// - Open the find bar on Cmd+F
// - Scroll page-by-page if not at the bottom (the "more" prompt behavior)
// - Send a character event if char_request is active
// - Submit or terminate line input if line_request is active
// - Navigate command history with Up/Down arrows
// - Buffer keystrokes if no input request is pending
- (void)keyDown:(NSEvent *)evt {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length - 1]);

    NSUInteger flags = evt.modifierFlags;

    if ((flags & NSEventModifierFlagNumericPad) && ch >= '0' && ch <= '9')
        ch = keycode_Pad0 - (ch - '0');

    GlkWindow *win;

    GlkController *glkctl = self.glkctl;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus) {
        for (win in (glkctl.gwindows).allValues) {
            if (win != self && win.wantsFocus) {
                NSLog(@"GlkTextBufferWindow %ld: Passing on keypress to %@ %ld", self.name, win.class, win.name);
                [win grabFocus];
                [win keyDown:evt];
                return;
            }
        }
    }

    // Cancel any in-progress scrolling
    // (because we might want to start a new scroll animated,
    // and that won't work if we are already at the bottom)
    scrollAdjustTimeStamp = [NSDate distantPast];

    BOOL commandKeyOnly = ((flags & NSEventModifierFlagCommand) &&
                           !(flags & (NSEventModifierFlagOption | NSEventModifierFlagShift |
                                      NSEventModifierFlagControl | NSEventModifierFlagHelp)));
    BOOL optionKeyOnly = ((flags & NSEventModifierFlagOption) &&
                          !(flags & (NSEventModifierFlagCommand | NSEventModifierFlagShift |
                                     NSEventModifierFlagControl | NSEventModifierFlagHelp)));

    if (ch == keycode_Up) {
        if (optionKeyOnly)
            ch = keycode_PageUp;
        else if (commandKeyOnly)
            ch = keycode_Home;
    } else if (ch == keycode_Down) {
        if (optionKeyOnly)
            ch = keycode_PageDown;
        else if (commandKeyOnly)
            ch = keycode_End;

    } else if (([str isEqualToString:@"f"] || [str isEqualToString:@"F"]) &&
             commandKeyOnly) {
        if (!scrollview.findBarVisible) {
            _restoredFindBarVisible = YES;
            [self restoreTextFinder];
            return;
        }
    }

    NSNumber *key = @(ch);

    if (self.glkctl.commandScriptRunning)
        [self scrollToBottomAnimated:NO];

    if (!scrolling && !_pendingScroll && !self.scrolledToBottom && !self.glkctl.voiceOverActive && !self.glkctl.commandScriptRunning) {
        // Not scrolled to the bottom, pagedown or navigate scrolling on each key instead
        switch (ch) {
            case keycode_PageUp:
                [_textview scrollPageUp:nil];
                return;
            case keycode_PageDown:
            case ' ':
                [_textview scrollPageDown:nil];
                return;
            case keycode_Up:
                [_textview scrollLineUp:nil];
                return;
            case keycode_Down:
            case keycode_Return:
                [_textview scrollLineDown:nil];
                return;
            case keycode_End:
                [self scrollToBottomAnimated:YES];
                return;
            default:
                if (line_request && !flags) {
                    // Capture the viewport snapshot before the scroll so
                    // the doc-fit anchor inside
                    // scrollToBottomAnimated:respectCaps: kicks in. The
                    // unconditional uncapped scroll-to-bottom otherwise
                    // jumps to (newDoc - V + inset), which loses just
                    // (newDoc - V) lines off the top — the "5 lines too
                    // far" symptom when the doc was only a few lines
                    // taller than the viewport at the time of typing.
                    [self markLastSeen];
                    [self scrollToBottomAnimated:NO respectCaps:YES];
                } else
                    [self performScroll];
                break;
        }
    }

    if (char_request && ch != keycode_Unknown) {
        glkctl.shouldScrollOnCharEvent = YES;
        [self sendKeypress:ch];

    } else if (line_request && (ch == keycode_Return ||
                                [self.currentTerminators[key] isEqual:@(YES)])) {
        NSString *line = [textstorage.string substringFromIndex:fence];
        [self sendInputLine:line withTerminator:ch == keycode_Return ? 0 : key.unsignedIntegerValue];
        return;
    } else if (line_request && (ch == keycode_Up ||
                                // Use Home to travel backward in history when Beyond Zork eats up arrow
                                ((self.glkctl.gameID == kGameIsBeyondZork || [self.glkctl zVersion6]) && self.theme.bZTerminator != kBZArrowsSwapped && ch == keycode_Home))) {
        [self travelBackwardInHistory];
    } else if (line_request && (ch == keycode_Down ||
                                // Use End to travel forward in history when Beyond Zork eats down arrow
                                ((self.glkctl.gameID == kGameIsBeyondZork || [self.glkctl zVersion6]) && self.theme.bZTerminator != kBZArrowsSwapped && ch == keycode_End))) {
        [self travelForwardInHistory];
    } else if (line_request && ch == keycode_PageUp &&
               fence == textstorage.length) {
        [_textview scrollPageUp:nil];
        return;
    }

    else {
        if (line_request) {
            if ((ch == 'v' || ch == 'V') && commandKeyOnly && _textview.selectedRange.location < fence) {
                [glkctl.window makeFirstResponder:_textview];
                NSRange selectedRange = NSIntersectionRange(_textview.selectedRange, self.editableRange);
                if (selectedRange.location == NSNotFound || selectedRange.length == 0)
                    selectedRange = NSMakeRange(textstorage.length, 0);
                _textview.selectedRange = selectedRange;
                [_textview paste:nil];
                return;
            } else if (_textview.selectedRange.length != 0 && ch != keycode_Unknown && (flags & NSEventModifierFlagCommand) != NSEventModifierFlagCommand) {
                // Deselect text and move cursor to end to facilitate typing
                NSRange selectedRange = NSIntersectionRange(_textview.selectedRange, self.editableRange);
                if (selectedRange.location == NSNotFound || (selectedRange.length == 0))
                    selectedRange = NSMakeRange(textstorage.length, 0);
                _textview.selectedRange = selectedRange;
            }
        }

        if (self.window.firstResponder != _textview)
            [self.window makeFirstResponder:_textview];

        // If there is no input request, buffer keyDown events
        // and reissue them on the next line input request
        if (!self.wantsFocus) {
            if (!bufferedEvents)
                bufferedEvents = [NSMutableArray new];
            [bufferedEvents addObject:evt];
        }
        [_textview superKeyDown:evt];
    }
}


// Send a command line from a command script (not typed by the user).
// Optionally echoes the text before submitting.
- (void)sendCommandLine:(NSString *)line {
    if (echo) {
        NSAttributedString *att = [[NSAttributedString alloc]
                                   initWithString:line
                                   attributes:_inputAttributes];
        [textstorage appendAttributedString:att];
    }
    [self sendInputLine:line withTerminator:0];
}


// Submit the completed input line to the interpreter. Echoes a newline (if
// echo is enabled) or deletes the input text (if echo is off), saves to
// command history, scrubs invalid characters, maps Beyond Zork Home/End back
// to Up/Down terminators, queues the line event, and resets input state.
- (void)sendInputLine:(NSString *)line withTerminator:(NSUInteger)terminator {
    if (echo) {
        //        [textstorage
        //         addAttribute:NSCursorAttributeName value:[NSCursor arrowCursor] range:[self editableRange]];
        [self printToWindow:@"\n"
                      style:style_Normal]; // XXX arranger lastchar needs to be set
        _lastchar = '\n';
    } else {
        // Defer the deletion until the next output arrives, so the on-screen
        // input stays put until it can be atomically replaced by the game's
        // reprint. Eliminates the flicker that comes from deleting now and
        // appending milliseconds later.
        if (textstorage.length > fence) {
            pendingEchoDeleteRange = NSMakeRange(fence, textstorage.length - fence);
        }
    }
    // input line

    if (line.length > 0) {
        [history saveHistory:line];
    }

    line = [line scrubInvalidCharacters];
    line = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

    if (self.glkctl.gameID == kGameIsBeyondZork || [self.glkctl zVersion6]) {
        if (terminator == keycode_Home) {
            terminator = keycode_Up;
        } else if (terminator == keycode_End) {
            terminator = keycode_Down;
        }
    }

    GlkEvent *gev = [[GlkEvent alloc] initLineEvent:line forWindow:self.name terminator:(NSInteger)terminator];
    [self.glkctl queueEvent:gev];

    // When the echo-off branch deferred the deletion, leave fence at the start
    // of the ghost text so the next flush can delete from there and reset fence.
    if (pendingEchoDeleteRange.length == 0)
        fence = textstorage.length;
    line_request = NO;
    [self hideInsertionPoint];
    _textview.editable = NO;
    [self flushDisplay];
    [_textview resetTextFinder];
    [self.glkctl markLastSeen];
}

// Begin a character input request. The next keystroke will be sent to the
// interpreter as a character event.
- (void)initChar {
    char_request = YES;
    [self hideInsertionPoint];
}

- (void)cancelChar {
    char_request = NO;
}

// Send a single character event to the interpreter and end the char request.
- (void)sendKeypress:(unsigned)ch {
    [self.glkctl markLastSeen];

    GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
    [self.glkctl queueEvent:gev];

    char_request = NO;
    _textview.editable = NO;
}

// Begin a line input request. Sets up the text view for editing by placing the
// fence at the current end of text, applying input attributes, pre-filling any
// initial text (str), and replaying any buffered keystrokes that arrived while
// no input request was active.
- (void)initLine:(NSString *)str maxLength:(NSUInteger)maxLength
{
    [self flushDisplay];

    [history reset];

    if (self.terminatorsPending) {
        self.currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    if (echo_toggle_pending) {
        echo_toggle_pending = NO;
        echo = !echo;
    }

    // If the previous line was submitted with echo off and no output has
    // arrived since, the typed text is still on screen. Clear it now so the
    // new prompt and fence don't sit after the ghost.
    if ([self applyPendingEchoDeleteIfValid])
        _lastchar = textstorage.length ? [textstorage.string characterAtIndex:textstorage.length - 1] : '\n';

    if (_lastchar == '>' && self.theme.spaceFormat) {
        [self printToWindow:@" " style:style_Normal];
        _lastchar = ' ';
    }

    fence = textstorage.length;

    [self recalcInputAttributes];

    NSAttributedString *att = [[NSAttributedString alloc]
              initWithString:str
              attributes:_inputAttributes];

    [textstorage appendAttributedString:att];
    _textview.editable = YES;

    line_request = YES;
    [self showInsertionPoint];

    _textview.selectedRange = NSMakeRange(textstorage.length, 0);
    if (bufferedEvents.count)  {
        NSMutableArray *copiedEvents = bufferedEvents.copy;
        for (NSEvent *event in copiedEvents) {
            [self keyDown:event];
        }
        bufferedEvents = [NSMutableArray new];
    }
}

- (void)recalcInputAttributes {
    _inputAttributes = [self getCurrentAttributesForStyle:style_Input];
}

// Cancel an active line input request. Returns the text entered so far,
// echoes a newline (or deletes the input if echo is off), and locks editing.
- (NSString *)cancelLine {
    [self flushDisplay];
    [_textview resetTextFinder];

    // A pending echo-off delete from an earlier submission would shift the
    // meaning of fence; apply it before reading the input.
    [self applyPendingEchoDeleteIfValid];

    NSString *str = textstorage.string;
    str = [str substringFromIndex:fence];
    if (echo) {
        [self printToWindow:@"\n" style:style_Input];
        _lastchar = '\n'; // [str characterAtIndex: str.length - 1];
    } else
        [textstorage
         deleteCharactersInRange:NSMakeRange(
                                             fence,
                                             textstorage.length -
                                             fence)]; // Don't echo input line

    _textview.editable = NO;
    line_request = NO;
    [self hideInsertionPoint];
    return str;
}

- (BOOL)hasLineRequest {
    return line_request;
}

#pragma mark Command history

// Replace the current input with the previous command from history.
// Announces via VoiceOver if at the start of history or history is empty.
- (void)travelBackwardInHistory {
    [self flushDisplay];
    NSString *cx;
    if (textstorage.length > fence) {
        cx = [textstorage.string substringFromIndex:fence];
    } else {
        cx = @"";
    }

    cx = [history travelBackwardInHistory:cx];

    if (cx) {
        [textstorage
         replaceCharactersInRange:self.editableRange
         withString:cx];
        [_textview resetTextFinder];
    }

    if (!cx.length) {
        if ([history empty])
            [self.glkctl speakStringNow:@"No commands entered"];
        else
            [self.glkctl speakStringNow:@"Start of command history"];
    }
}

// Replace the current input with the next command from history.
- (void)travelForwardInHistory {
    NSString *cx = [history travelForwardInHistory];
    if (cx) {
        [self flushDisplay];
        [textstorage
         replaceCharactersInRange:self.editableRange
         withString:cx];
        [_textview resetTextFinder];
    }

    if (!cx.length) {
        if ([history empty])
            [self.glkctl speakStringNow:@"No commands entered"];
        else
            [self.glkctl speakStringNow:@"End of command history"];
    }
}

#pragma mark NSTextView customization

// NSTextViewDelegate: Guard edits to enforce the fence boundary.
// Only allow text changes at or beyond the fence during line input.
// Also shows/hides the caret depending on whether the edit is in the
// editable region.
- (BOOL)textView:(NSTextView *)aTextView
shouldChangeTextInRange:(NSRange)range
replacementString:(id)repl {
    if (line_request && range.location >= fence) {
        _textview.shouldDrawCaret = YES;
        return YES;
    }
    if (line_request && range.location == fence - 1)
        _textview.shouldDrawCaret = YES;
    else
        _textview.shouldDrawCaret = NO;
    return NO;
}


// NSTextStorageDelegate: Apply input styling to newly typed text.
// When the user types during a line input request, this ensures the
// characters beyond the fence get the correct input attributes
// (font, color, paragraph style) rather than inheriting from
// adjacent game output.
- (void)textStorage:(NSTextStorage *)textStorage willProcessEditing:(NSTextStorageEditActions)editedMask range:(NSRange)editedRange changeInLength:(NSInteger)delta {
    if ((editedMask & NSTextStorageEditedCharacters) == 0)
        return;
    if (!line_request)
        return;

    if (textstorage.editedRange.location < fence)
        return;

    if (!_inputAttributes)
        [self recalcInputAttributes];

    [textstorage setAttributes:_inputAttributes
                         range:textstorage.editedRange];
}

// NSTextViewDelegate: Clamp the insertion point to the editable region.
// During line input, prevents the caret from moving before the fence.
// When there is no line input, forces the caret to the end of text
// (it shouldn't be visible anyway, but this is a safety measure).
- (NSRange)textView:(NSTextView *)aTextView willChangeSelectionFromCharacterRange:(NSRange)oldrange
   toCharacterRange:(NSRange)newrange {

    if (line_request) {
        if (newrange.length == 0 && newrange.location < fence) {
            newrange.location = fence;
        }
    } else if (newrange.length == 0) {
        newrange.location = textstorage.length;
    }
    return newrange;
}

// Hide the text cursor by setting its color to match the background.
// Called when there is no active line input, so no caret should be visible.
- (void)hideInsertionPoint {
    if (!line_request) {
        NSColor *color = _textview.backgroundColor;
        if (textstorage.length) {
            color = [textstorage attribute:NSBackgroundColorAttributeName atIndex:textstorage.length-1 effectiveRange:nil];
        }
        if (!color) {
            color = self.theme.bufferBackground;
        }
        _textview.insertionPointColor = color;
    }
}

// Make the text cursor visible by setting it to the foreground text color.
// Uses the Z-machine color if set, and falls back to the text color at the
// fence position if the primary color matches the background (which would
// make the caret invisible).
- (void)showInsertionPoint {
    if (line_request) {
        NSColor *color = styles[style_Normal][NSForegroundColorAttributeName];
        if (currentZColor)
            color = [NSColor colorFromInteger: currentZColor.fg];
        if (textstorage.length && [color isEqualToColor:_textview.backgroundColor]) {
            if (fence <= textstorage.length && fence > 0)
                color = [textstorage attribute:NSForegroundColorAttributeName atIndex:fence - 1 effectiveRange:nil];
            else
                color = [textstorage attribute:NSForegroundColorAttributeName atIndex:0 effectiveRange:nil];
        }
        _textview.insertionPointColor = color;
    }
}

// Return the range of user-editable text (everything from fence to end).
// Returns {NSNotFound, 0} when there is no active line input.
- (NSRange)editableRange {
    if (!line_request)
        return NSMakeRange(NSNotFound, 0);
    return NSMakeRange(fence, textstorage.length - fence);
}

// Assumes a fixed-width style is used,
// as in Tads status bars.
- (NSUInteger)numberOfColumns {
    NSRect frame = self.frame;
    if (self.framePending)
        frame = self.pendingFrame;
    CGFloat charwidth = [@"l" sizeWithAttributes:styles[style_Normal]].width;
    NSUInteger cols = (NSUInteger)round((frame.size.width -
                                         (_textview.textContainerInset.width + container.lineFragmentPadding) * 2) /
                                        charwidth);
    return cols;
}

// Count the number of visual lines by walking the layout manager's
// line fragment rects. Used by the interpreter to query the window size.
- (NSUInteger)numberOfLines {
    [self flushDisplay];
    NSUInteger numberOfLines, index, numberOfGlyphs =
    layoutmanager.numberOfGlyphs;
    NSRange lineRange;
    for (numberOfLines = 0, index = 0; index < numberOfGlyphs; numberOfLines++){
        [layoutmanager lineFragmentRectForGlyphAtIndex:index
                                        effectiveRange:&lineRange withoutAdditionalLayout:YES];
        index = NSMaxRange(lineRange);
    }
    return numberOfLines;
}

@end
