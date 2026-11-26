#import "GlkTextBufferWindowPrivate.h"

#import "Constants.h"
#import "GlkController.h"
#import "GlkController+BorderColor.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "ZColor.h"
#import "NSColor+integer.h"
#import "MarginContainer.h"
#import "MyAttachmentCell.h"
#import "Font3LayoutManager.h"
#import "BufferTextView.h"
#include "glkimp.h"

#import "DebugLog.h"

@implementation GlkTextBufferWindow (Styles)

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

// Recalculate and apply the background color for this window. Priority:
// 1. Z-machine color override (bgnd), if styles are enabled
// 2. The Normal style's background color
// 3. The theme's default buffer background
// Also updates the scroll view background and notifies the border color system.
- (void)recalcBackground {
    NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];

    if (self.theme.doStyles && bgnd > -1 && bgnd != zcolor_Default) {
        bgcolor = [NSColor colorFromInteger:[self.glkctl currentValueForMeasuredColor:bgnd hint:stylehint_BackColor grid:NO]];
    }
    if (!bgcolor) {
        if (!self.theme) {
            NSLog(@"recalcBackground: No theme!");
            return;
        }
        if (!self.theme.bufferBackground) {
            NSLog(@"recalcBackground: No self.theme.bufferBackground!");
            return;
        }
        bgcolor = self.theme.bufferBackground;
    }
    _textview.backgroundColor = bgcolor;

    if (line_request)
        [self showInsertionPoint];
    [self.glkctl setBorderColor:bgcolor fromWindow:self];
}

// Configure the scroll view to use overlay scrollers with the buffer
// window's background color. Called during init and after preference changes.
- (void)restoreScrollBarStyle {
    if (scrollview) {
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.scrollerStyle = NSScrollerStyleOverlay;
        scrollview.drawsBackground = YES;
        NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];
        if (!bgcolor)
            bgcolor = self.theme.bufferBackground;
        scrollview.backgroundColor = bgcolor;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = YES;
        scrollview.verticalScroller.alphaValue = 1;
        scrollview.autohidesScrollers = YES;
        scrollview.borderType = NSNoBorder;
    }
}

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;
    [self recalcBackground];
}

// Respond to theme/preference changes. Rebuilds the styles array, re-applies
// all style attributes to existing text (preserving inline images, hyperlinks,
// Z-colors, and reverse video), updates margins, link appearance, and scroll
// position. If the game has ended, adjusts the window frame to fill available space.
// Build the styles array: one NSDictionary of text attributes per Glk style
// (NSNull where the theme has none). When doStyles is on, game-provided style
// hints are applied on top of the theme defaults; otherwise, raw theme
// attributes are used directly.
- (GlkStyle *)themeStyle:(NSUInteger)style {
    return [self.theme valueForKey:gBufferStyleNames[style]];
}

- (int32_t)themeLinkStyle {
    return self.theme.bufLinkStyle;
}

- (NSMutableArray *)buildStyles {
    NSMutableArray *newstyles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        NSDictionary *attributes = [self themeAttributesForStyle:i];
        if (attributes) {
            [newstyles addObject:[self styleAttributesWithLanguage:attributes]];
        } else {
            NSLog(@"GlkTextBufferWindow couldn't create style dict for style %lu", (unsigned long)i);
            [newstyles addObject:[NSNull null]];
        }
    }
    return newstyles;
}

- (void)prefsDidChange {
    if (!_pendingScrollRestore) {
        [self storeScrollOffset];
    }

    GlkController *glkctl = self.glkctl;

    // Adjust terminators for Beyond Zork arrow keys hack
    if (self.mayEatArrowKeys) {
        [self adjustBZTerminators:self.pendingTerminators];
        [self adjustBZTerminators:self.currentTerminators];
    }

    // Preferences has changed, so first we must redo the styles library
    BOOL different = [self syncStyleFlagsWithTheme];

    NSMutableArray *newstyles = [self buildStyles];
    if (!different && ![newstyles isEqualToArray:styles])
        different = YES;

    CGFloat marginX = (CGFloat)self.theme.bufferMarginX;
    CGFloat marginY = (CGFloat)self.theme.bufferMarginY;

    BOOL marginHeightChanged = (marginY != _textview.textContainerInset.height);
    CGFloat heightDiff = marginY - _textview.textContainerInset.height;

    _textview.textContainerInset = NSMakeSize(marginX, marginY);

    // If the Y margin has changed, we must adjust the text view
    // here to make the scrollview aware of this, otherwise we might
    // not be able to scroll to the bottom.
    if (marginHeightChanged) {
        NSRect newTextviewFrame = _textview.frame;
        newTextviewFrame.size.height += heightDiff * 2;
        _textview.frame = newTextviewFrame;
    }

    // We can think of attributes as special characters in the mutable attributed
    // string called textstorage.
    // Here we iterate through the textstorage string to find them all.
    // We have to do it character by character instead of using
    // enumerateAttribute:inRange:options:usingBlock:
    // to make sure that no inline images, hyperlinks or zcolors
    // get lost when we update the Glk Styles.

    if (different) {
        styles = newstyles;
        [self recalcInputAttributes];

        if (glkctl.usesFont3) {
            [self createBeyondZorkStyle];
        }

        /* reassign styles to attributedstrings */
        NSMutableAttributedString *backingStorage = [textstorage mutableCopy];

        if (storedNewline) {
            if (!self.theme.doStyles) {
                NSMutableDictionary *newLineAttributes = [storedNewline attributesAtIndex:0 effectiveRange:nil].mutableCopy;
                ZColor *zcolor = newLineAttributes[@"ZColor"];
                if (zcolor && zcolor.bg != zcolor_Current && zcolor.bg != zcolor_Default && zcolor.bg != zcolor_Transparent) {
                    newLineAttributes[NSBackgroundColorAttributeName] = nil;
                    storedNewline = [[NSAttributedString alloc] initWithString:@"\n" attributes:newLineAttributes];
                }
            }
            [backingStorage appendAttributedString:storedNewline];
        }

        NSRange selectedRange = _textview.selectedRange;

        NSArray __block *blockStyles = styles;
        [textstorage
         enumerateAttributesInRange:NSMakeRange(0, textstorage.length)
         options:0
         usingBlock:^(NSDictionary *attrs, NSRange range, BOOL *stop) {

            // First, we overwrite all attributes with those in our updated
            // styles array
            id styleobject = attrs[@"GlkStyle"];
            if (styleobject) {
                NSDictionary *stylesAtt = blockStyles[(NSUInteger)[styleobject intValue]];
                if ([stylesAtt isKindOfClass:[NSDictionary class]])
                    [backingStorage setAttributes:stylesAtt range:range];
            }

            // Then, we re-add all the "non-Glk" style values we want to keep
            // (inline images, hyperlinks, Z-colors, reverse video and font 3 codes)
            id image = attrs[NSAttachmentAttributeName];
            if (image) {
                [backingStorage addAttribute: NSAttachmentAttributeName
                                       value: image
                                       range: NSMakeRange(range.location, 1)];
                ((MyAttachmentCell *)((NSTextAttachment *)image).attachmentCell).attrstr = backingStorage;
            }

            id hyperlink = attrs[NSLinkAttributeName];
            if (hyperlink) {
                [backingStorage addAttribute:NSLinkAttributeName
                                       value:hyperlink
                                       range:range];
            }

            id zcolor = attrs[@"ZColor"];
            if (zcolor) {
                [backingStorage addAttribute:@"ZColor"
                                       value:zcolor
                                       range:range];
            }

            id reverse = attrs[@"ReverseVideo"];
            if (reverse) {
                [backingStorage addAttribute:@"ReverseVideo"
                                       value:reverse
                                       range:range];
            }

            id font3Code = attrs[Font3CodeAttributeName];
            if (font3Code) {
                [backingStorage addAttribute:Font3CodeAttributeName
                                       value:font3Code
                                       range:range];
            }
        }];

        backingStorage = [self applyZColorsAndThenReverse:backingStorage];

        if (storedNewline) {
            storedNewline = [[NSAttributedString alloc] initWithString:@"\n" attributes:[backingStorage attributesAtIndex:backingStorage.length - 1 effectiveRange:NULL]];
            [backingStorage deleteCharactersInRange:NSMakeRange(backingStorage.length - 1, 1)];
        }

        [textstorage setAttributedString:backingStorage];

        _textview.selectedRange = selectedRange;
    }

    if (self.glkctl.isAlive) {
        if (different) {
            [self updateLinkTextAttributesOf:_textview];

            [self showInsertionPoint];
            lastLineheight = self.theme.bufferNormal.font.boundingRectForFont.size.height;
            [self recalcBackground];
            // With no line input the caret is hidden by painting it the
            // background color, which was read when input ended; after a
            // light/dark switch that would leave a caret of the old
            // background showing. (No-op while line input is active.)
            [self hideInsertionPoint];
            if ([container hasMarginImages])
                [container performSelector:@selector(invalidateLayout:) withObject:nil afterDelay:0.2];
        }
        if (!_pendingScrollRestore) {
            _pendingScrollRestore = YES;
            [self flushDisplay];
            [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
        }
    } else {
        NSRect frame = self.frame;

        if ((self.autoresizingMask & NSViewWidthSizable) == NSViewWidthSizable) {
            frame.size.width = glkctl.gameView.frame.size.width - frame.origin.x;
        }

        if ((self.autoresizingMask & NSViewHeightSizable) == NSViewHeightSizable) {
            frame.size.height = glkctl.gameView.frame.size.height - frame.origin.y;
        }
        self.frame = frame;
        [self flushDisplay];
        [self recalcBackground];
        [self restoreScrollBarStyle];
    }
}

#pragma mark Beyond Zork font

// Beyond Zork's font 3 (style_BlockQuote, see Bocfel's screen.cpp) is the
// normal style marked for Font3LayoutManager, which paints its characters
// (runes, arrows, box drawing) into cells of the normal font.
- (void)createBeyondZorkStyle {
    NSMutableDictionary *beyondZorkStyle = [styles[style_Normal] mutableCopy];
    beyondZorkStyle[@"GlkStyle"] = @(style_BlockQuote);
    beyondZorkStyle[Font3BaseFontAttributeName] = beyondZorkStyle[NSFontAttributeName];
    styles[style_BlockQuote] = beyondZorkStyle;
}

@end
