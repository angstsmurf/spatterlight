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
#import "BufferTextView.h"
#include "glkimp.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

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
        bgcolor = [NSColor colorFromInteger:bgnd];
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

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;
    [self recalcBackground];
}

// Respond to theme/preference changes. Rebuilds the styles array, re-applies
// all style attributes to existing text (preserving inline images, hyperlinks,
// Z-colors, and reverse video), updates margins, link appearance, and scroll
// position. If the game has ended, adjusts the window frame to fill available space.
- (void)prefsDidChange {
    NSDictionary *attributes;
    if (!_pendingScrollRestore) {
        [self storeScrollOffset];
    }

    GlkController *glkctl = self.glkctl;

    // Adjust terminators for Beyond Zork arrow keys hack
    if (glkctl.gameID == kGameIsBeyondZork || [glkctl zVersion6]) {
        [self adjustBZTerminators:self.pendingTerminators];
        [self adjustBZTerminators:self.currentTerminators];
    }

    // Preferences has changed, so first we must redo the styles library
    NSMutableArray *newstyles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    BOOL different = NO;
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        if (self.theme.doStyles) {
            // We're doing styles, so we call the current theme object with our hints array
            // in order to get an attributes dictionary
            attributes = [((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]) attributesWithHints:self.styleHints[i]];
        } else {
            // We're not doing styles, so use the raw style attributes from
            // the theme object's attributeDict object
            attributes = ((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]).attributeDict;
        }

        if (usingStyles != self.theme.doStyles) {
            different = YES;
            usingStyles = self.theme.doStyles;
        }

        if (underlineLinks != (self.theme.bufLinkStyle != NSUnderlineStyleNone)) {
            different = YES;
            underlineLinks = (self.theme.bufLinkStyle != NSUnderlineStyleNone);
        }

        if (attributes) {
            [newstyles addObject:attributes];
            if (!different && ![newstyles[i] isEqualToDictionary:styles[i]])
                different = YES;
        } else
            [newstyles addObject:[NSNull null]];
    }

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
                [backingStorage setAttributes:stylesAtt range:range];
            }

            // Then, we re-add all the "non-Glk" style values we want to keep
            // (inline images, hyperlinks, Z-colors and reverse video)
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
            // Set style for hyperlinks
            NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
            linkAttributes[NSUnderlineStyleAttributeName] = @(self.theme.bufLinkStyle);
            linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
            _textview.linkTextAttributes = linkAttributes;

            [self showInsertionPoint];
            lastLineheight = self.theme.bufferNormal.font.boundingRectForFont.size.height;
            [self recalcBackground];
            if ([container hasMarginImages])
                [container performSelector:@selector(invalidateLayout:) withObject:nil afterDelay:0.2];
        }
        if (!_pendingScrollRestore) {
            _pendingScrollRestore = YES;
            [self flushDisplay];
            [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
        }
    } else {
        if (!glkctl.isAlive) {
            NSRect frame = self.frame;

            if ((self.autoresizingMask & NSViewWidthSizable) == NSViewWidthSizable) {
                frame.size.width = glkctl.gameView.frame.size.width - frame.origin.x;
            }

            if ((self.autoresizingMask & NSViewHeightSizable) == NSViewHeightSizable) {
                frame.size.height = glkctl.gameView.frame.size.height - frame.origin.y;
            }
            self.frame = frame;
        }
        [self flushDisplay];
        [self recalcBackground];
        [self restoreScrollBarStyle];
    }
}

#pragma mark Beyond Zork font

// Create a scaled version of the "FreeFont3" bitmap font used by Beyond Zork
// for pseudo-graphical characters (map symbols, compass rose, runes, etc.).
// The font is scaled to match the normal text's character cell dimensions and
// assigned to style_BlockQuote, which Beyond Zork uses for Font3 output.
- (void)createBeyondZorkStyle {
    CGFloat pointSize = ((NSFont *)(styles[style_Normal][NSFontAttributeName])).pointSize;
    NSFont *zorkFont = [NSFont fontWithName:@"FreeFont3" size:pointSize];
    if (!zorkFont) {
        NSLog(@"Error! No Zork Font Found!");
        return;
    }

    NSMutableDictionary *beyondZorkStyle = [styles[style_BlockQuote] mutableCopy];

    beyondZorkStyle[NSBaselineOffsetAttributeName] = @(0);

    beyondZorkStyle[NSFontAttributeName] = zorkFont;

    NSSize size = [@"6" sizeWithAttributes:beyondZorkStyle];
    NSSize wSize = [@"W" sizeWithAttributes:styles[style_Normal]];

    NSAffineTransform *transform = [[NSAffineTransform alloc] init];
    [transform scaleBy:pointSize];

    CGFloat xscale = wSize.width / size.width;
    if (xscale < 1) xscale = 1;
    CGFloat yscale = wSize.height / size.height;
    if (yscale < 1) yscale = 1;

    [transform scaleXBy:xscale yBy:yscale];
    NSFontDescriptor *descriptor = [NSFontDescriptor fontDescriptorWithName:@"FreeFont3" size:pointSize];
    zorkFont = [NSFont fontWithDescriptor:descriptor textTransform:transform];
    if (!zorkFont)
        NSLog(@"Failed to create Zork Font!");
    beyondZorkStyle[NSFontAttributeName] = zorkFont;
    NSMutableParagraphStyle *para = [beyondZorkStyle[NSParagraphStyleAttributeName] mutableCopy];
    para.lineSpacing = 0;
    para.paragraphSpacing = 0;
    para.paragraphSpacingBefore = 0;
    para.maximumLineHeight = [layoutmanager defaultLineHeightForFont:self.theme.bufferNormal.font];
    beyondZorkStyle[NSParagraphStyleAttributeName] = para;
    beyondZorkStyle[NSKernAttributeName] = @(-2);

    styles[style_BlockQuote] = beyondZorkStyle;
}

// Mapping from Beyond Zork's Font3 ASCII characters to their Unicode
// equivalents (arrows and Anglo-Saxon runes used for the in-game alphabet).
- (NSDictionary *)font3ToUnicode {
    // Built once and cached: this is consulted per character during Beyond Zork
    // Font3 output, so rebuilding the dictionary on every call is wasteful.
    static NSDictionary *font3 = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        font3 = @{
        @"!" : @"←",
        @"\"" : @"→",
        @"\\" : @"↑",
        @"]" : @"↓",
        @"a" : @"ᚪ",
        @"b" : @"ᛒ",
        @"c" : @"ᛇ",
        @"d" : @"ᛞ",
        @"e" : @"ᛖ",
        @"f" : @"ᚠ",
        @"g" : @"ᚷ",
        @"h" : @"ᚻ",
        @"i" : @"ᛁ",
        @"j" : @"ᛄ",
        @"k" : @"ᛦ",
        @"l" : @"ᛚ",
        @"m" : @"ᛗ",
        @"n" : @"ᚾ",
        @"o" : @"ᚩ",
        @"p" : @"ᖾ",
        @"q" : @"ᚳ",
        @"r" : @"ᚱ",
        @"s" : @"ᛋ",
        @"t" : @"ᛏ",
        @"u" : @"ᚢ",
        @"v" : @"ᛠ",
        @"w" : @"ᚹ",
        @"x" : @"ᛉ",
        @"y" : @"ᚥ",
        @"z" : @"ᛟ"
        };
    });
    return font3;
}

@end
