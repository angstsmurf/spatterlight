//
//  Font3LayoutManager.m
//  Spatterlight
//

#import "Font3LayoutManager.h"

#import <CoreText/CoreText.h>

NSAttributedStringKey const Font3CodeAttributeName = @"Font3Code";
NSAttributedStringKey const Font3BaseFontAttributeName = @"Font3BaseFont";

typedef struct {
    uint8_t rows[8]; // top row first, most significant bit leftmost
    unichar unicode; // stand-in for VoiceOver, speech and copy
} Font3Glyph;

// Font 3 as drawn by Infocom's Atari ST interpreter (beyond_zork.ST, the table
// at 0x42320). The Unicode stand-ins follow Bocfel's zscii_to_font3, except
// that the single corner dots (71–74) get the matching quadrant and the double
// line (79) gets ═ where Bocfel has U+FFFD and a space. The reversed
// arrows and question mark (123–126) come out reversed from their bitmaps.
static const Font3Glyph font3Glyphs[95] = {
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x0020}, //  32 ' '  
    {{0x00, 0x00, 0x20, 0x60, 0xFF, 0x60, 0x20, 0x00}, 0x2190}, //  33 '!' ←
    {{0x00, 0x00, 0x04, 0x06, 0xFF, 0x06, 0x04, 0x00}, 0x2192}, //  34 '"' →
    {{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}, 0x2571}, //  35 '#' ╱
    {{0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}, 0x2572}, //  36 '$' ╲
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x0020}, //  37 '%'  
    {{0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00}, 0x2500}, //  38 '&' ─
    {{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}, 0x2500}, //  39 "'" ─
    {{0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, 0x2502}, //  40 '(' │
    {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, 0x2502}, //  41 ')' │
    {{0x08, 0x08, 0x08, 0xFF, 0x00, 0x00, 0x00, 0x00}, 0x2534}, //  42 '*' ┴
    {{0x00, 0x00, 0x00, 0x00, 0xFF, 0x08, 0x08, 0x08}, 0x252C}, //  43 '+' ┬
    {{0x08, 0x08, 0x08, 0x08, 0x0F, 0x08, 0x08, 0x08}, 0x251C}, //  44 ',' ├
    {{0x10, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10}, 0x2524}, //  45 '-' ┤
    {{0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00, 0x00}, 0x2514}, //  46 '.' └
    {{0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x10, 0x10}, 0x250C}, //  47 '/' ┌
    {{0x00, 0x00, 0x00, 0xF8, 0x08, 0x08, 0x08, 0x08}, 0x2510}, //  48 '0' ┐
    {{0x08, 0x08, 0x08, 0x08, 0xF8, 0x00, 0x00, 0x00}, 0x2518}, //  49 '1' ┘
    {{0x10, 0x10, 0x10, 0x10, 0x1F, 0x20, 0x40, 0x80}, 0x2514}, //  50 '2' └
    {{0x80, 0x40, 0x20, 0x1F, 0x10, 0x10, 0x10, 0x10}, 0x250C}, //  51 '3' ┌
    {{0x01, 0x02, 0x04, 0xF8, 0x08, 0x08, 0x08, 0x08}, 0x2510}, //  52 '4' ┐
    {{0x08, 0x08, 0x08, 0x08, 0xF8, 0x04, 0x02, 0x01}, 0x2518}, //  53 '5' ┘
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x2588}, //  54 '6' █
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00}, 0x2580}, //  55 '7' ▀
    {{0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x2584}, //  56 '8' ▄
    {{0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8}, 0x258C}, //  57 '9' ▌
    {{0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 0x2590}, //  58 ':' ▐
    {{0x08, 0x08, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x2584}, //  59 ';' ▄
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x08, 0x08, 0x08}, 0x2580}, //  60 '<' ▀
    {{0xF8, 0xF8, 0xF8, 0xF8, 0xFF, 0xF8, 0xF8, 0xF8}, 0x258C}, //  61 '=' ▌
    {{0x1F, 0x1F, 0x1F, 0x1F, 0xFF, 0x1F, 0x1F, 0x1F}, 0x2590}, //  62 '>' ▐
    {{0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00}, 0x259D}, //  63 '?' ▝
    {{0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 0x2597}, //  64 '@' ▗
    {{0x00, 0x00, 0x00, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8}, 0x2596}, //  65 'A' ▖
    {{0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0x00, 0x00, 0x00}, 0x2598}, //  66 'B' ▘
    {{0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x20, 0x40, 0x80}, 0x259D}, //  67 'C' ▝
    {{0x80, 0x40, 0x20, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 0x2597}, //  68 'D' ▗
    {{0x01, 0x02, 0x04, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8}, 0x2596}, //  69 'E' ▖
    {{0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0x04, 0x02, 0x01}, 0x2598}, //  70 'F' ▘
    {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x259D}, //  71 'G' ▝
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, 0x2597}, //  72 'H' ▗
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, 0x2596}, //  73 'I' ▖
    {{0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x2598}, //  74 'J' ▘
    {{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x2594}, //  75 'K' ▔
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, 0x2581}, //  76 'L' ▁
    {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, 0x258F}, //  77 'M' ▏
    {{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, 0x2595}, //  78 'N' ▕
    {{0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00}, 0x2550}, //  79 'O' ═
    {{0x00, 0xFF, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00}, 0x258F}, //  80 'P' ▏
    {{0x00, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0x00}, 0x258E}, //  81 'Q' ▎
    {{0x00, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0xFF, 0x00}, 0x258D}, //  82 'R' ▍
    {{0x00, 0xFF, 0xF0, 0xF0, 0xF0, 0xF0, 0xFF, 0x00}, 0x258C}, //  83 'S' ▌
    {{0x00, 0xFF, 0xF8, 0xF8, 0xF8, 0xF8, 0xFF, 0x00}, 0x258B}, //  84 'T' ▋
    {{0x00, 0xFF, 0xFC, 0xFC, 0xFC, 0xFC, 0xFF, 0x00}, 0x258A}, //  85 'U' ▊
    {{0x00, 0xFF, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0x00}, 0x2589}, //  86 'V' ▉
    {{0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00}, 0x2588}, //  87 'W' █
    {{0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00}, 0x2595}, //  88 'X' ▕
    {{0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 0x258F}, //  89 'Y' ▏
    {{0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81}, 0x2573}, //  90 'Z' ╳
    {{0x08, 0x08, 0x08, 0x08, 0xFF, 0x08, 0x08, 0x08}, 0x253C}, //  91 '[' ┼
    {{0x08, 0x1C, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08}, 0x2191}, //  92 '\\' ↑
    {{0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x1C, 0x08}, 0x2193}, //  93 ']' ↓
    {{0x08, 0x1C, 0x3E, 0x08, 0x08, 0x3E, 0x1C, 0x08}, 0x2195}, //  94 '^' ↕
    {{0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}, 0x2395}, //  95 '_' ⎕
    {{0x38, 0x44, 0x04, 0x08, 0x10, 0x10, 0x00, 0x10}, 0x003F}, //  96 '`' ?
    {{0x00, 0xC4, 0xA8, 0x90, 0xC0, 0xA0, 0x90, 0x80}, 0x16AA}, //  97 'a' ᚪ
    {{0x00, 0x70, 0x48, 0x44, 0x7C, 0x44, 0x48, 0x70}, 0x16D2}, //  98 'b' ᛒ
    {{0x00, 0x18, 0x14, 0x12, 0x10, 0x90, 0x50, 0x30}, 0x16C7}, //  99 'c' ᛇ
    {{0x00, 0x82, 0xC6, 0xAA, 0x92, 0xAA, 0xC6, 0x82}, 0x16DE}, // 100 'd' ᛞ
    {{0x00, 0x82, 0xC6, 0xAA, 0x92, 0x82, 0x82, 0x82}, 0x16D6}, // 101 'e' ᛖ
    {{0x00, 0x92, 0xA4, 0xC8, 0x90, 0xA0, 0xC0, 0x80}, 0x16A0}, // 102 'f' ᚠ
    {{0x00, 0x82, 0x44, 0x6C, 0x92, 0x6C, 0x44, 0x82}, 0x16B7}, // 103 'g' ᚷ
    {{0x00, 0xC4, 0xA4, 0x94, 0xCC, 0xA4, 0x94, 0x8C}, 0x16BB}, // 104 'h' ᚻ
    {{0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, 0x16C1}, // 105 'i' ᛁ
    {{0x00, 0x10, 0x38, 0x54, 0x92, 0x54, 0x38, 0x10}, 0x16C4}, // 106 'j' ᛄ
    {{0x00, 0x40, 0x40, 0x60, 0x50, 0x48, 0x44, 0x44}, 0x16E6}, // 107 'k' ᛦ
    {{0x00, 0x60, 0x50, 0x48, 0x44, 0x40, 0x40, 0x40}, 0x16DA}, // 108 'l' ᛚ
    {{0x00, 0xC6, 0xAA, 0x92, 0xAA, 0xC6, 0x82, 0x82}, 0x16D7}, // 109 'm' ᛗ
    {{0x00, 0x90, 0x50, 0x30, 0x10, 0x18, 0x14, 0x12}, 0x16BE}, // 110 'n' ᚾ
    {{0x00, 0xC6, 0xAA, 0x92, 0xC4, 0xA8, 0x90, 0x80}, 0x16A9}, // 111 'o' ᚩ
    {{0x00, 0x40, 0x40, 0x40, 0x44, 0x4C, 0x54, 0x64}, 0x15BE}, // 112 'p' ᖾ
    {{0x00, 0xBA, 0x54, 0x10, 0x10, 0x10, 0x10, 0x10}, 0x16B3}, // 113 'q' ᚳ
    {{0x00, 0x70, 0x48, 0x44, 0x48, 0x70, 0x48, 0x44}, 0x16B1}, // 114 'r' ᚱ
    {{0x00, 0x80, 0x86, 0x8A, 0x92, 0xA2, 0xC2, 0x02}, 0x16CB}, // 115 's' ᛋ
    {{0x00, 0x38, 0x54, 0x92, 0x10, 0x10, 0x10, 0x10}, 0x16CF}, // 116 't' ᛏ
    {{0x00, 0x60, 0x50, 0x48, 0x44, 0x44, 0x44, 0x44}, 0x16A2}, // 117 'u' ᚢ
    {{0x00, 0x38, 0x44, 0x28, 0x10, 0x28, 0x44, 0x82}, 0x16E0}, // 118 'v' ᛠ
    {{0x00, 0x70, 0x48, 0x44, 0x48, 0x70, 0x40, 0x40}, 0x16B9}, // 119 'w' ᚹ
    {{0x00, 0x10, 0x10, 0x10, 0x38, 0x54, 0x92, 0x92}, 0x16C9}, // 120 'x' ᛉ
    {{0x00, 0xF0, 0x98, 0xD4, 0xB2, 0x9A, 0x96, 0x92}, 0x16A5}, // 121 'y' ᚥ
    {{0x00, 0x3E, 0x40, 0x80, 0x40, 0x20, 0x10, 0x0E}, 0x16DF}, // 122 'z' ᛟ
    {{0xF7, 0xE3, 0xC1, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7}, 0x2191}, // 123 '{' ↑
    {{0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xC1, 0xE3, 0xF7}, 0x2193}, // 124 '|' ↓
    {{0xF7, 0xE3, 0xC1, 0xF7, 0xF7, 0xC1, 0xE3, 0xF7}, 0x2195}, // 125 '}' ↕
    {{0xC7, 0xBB, 0xFB, 0xF7, 0xEF, 0xEF, 0xFF, 0xEF}, 0x003F}, // 126 '~' ?
};

static unichar font3CodeAtIndex(NSAttributedString *text, NSUInteger index) {
    NSNumber *value = [text attribute:Font3CodeAttributeName atIndex:index effectiveRange:NULL];
    if (!value)
        return 0;
    unichar code = value.unsignedShortValue;
    if (code < 32 || code > 126)
        return 0;
    // The attribute can outlive its character: the grid gives a replacement
    // character the attributes of the one it replaces.
    if ([text.string characterAtIndex:index] != font3Glyphs[code - 32].unicode)
        return 0;
    return code;
}

static NSFont *baseFontAtIndex(NSAttributedString *text, NSUInteger index) {
    NSFont *font = [text attribute:Font3BaseFontAttributeName atIndex:index effectiveRange:NULL];
    if (!font)
        font = [text attribute:NSFontAttributeName atIndex:index effectiveRange:NULL];
    if (!font)
        font = [NSFont userFixedPitchFontOfSize:0];
    return font;
}

@implementation Font3LayoutManager

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (unichar)unicodeForCode:(unichar)code {
    if (code < 32 || code > 126)
        return 0;
    return font3Glyphs[code - 32].unicode;
}

+ (unichar)codeAtIndex:(NSUInteger)index inText:(NSAttributedString *)text {
    return index < text.length ? font3CodeAtIndex(text, index) : 0;
}

+ (NSAttributedString *)attributedStringWithCodes:(NSString *)string
                                       attributes:(NSDictionary<NSAttributedStringKey, id> *)attributes {
    NSMutableDictionary *markedAttributes = [attributes mutableCopy];
    if (!markedAttributes[Font3BaseFontAttributeName] && attributes[NSFontAttributeName])
        markedAttributes[Font3BaseFontAttributeName] = attributes[NSFontAttributeName];

    NSMutableAttributedString *result = [[NSMutableAttributedString alloc] init];
    for (NSUInteger i = 0; i < string.length; i++) {
        unichar code = [string characterAtIndex:i];
        unichar unicode = [self unicodeForCode:code];
        if (unicode) {
            markedAttributes[Font3CodeAttributeName] = @(code);
            [result appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:[NSString stringWithCharacters:&unicode length:1]
                                            attributes:markedAttributes]];
        } else {
            [result appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:[NSString stringWithCharacters:&code length:1]
                                            attributes:attributes]];
        }
    }
    return result;
}

+ (void)restoreBaseFontsInTextStorage:(NSTextStorage *)textStorage range:(NSRange)range {
    if (!textStorage.length)
        return;
    // Font fixing works on whole paragraphs.
    range = [textStorage.string paragraphRangeForRange:NSIntersectionRange(range, NSMakeRange(0, textStorage.length))];
    [textStorage enumerateAttribute:Font3BaseFontAttributeName
                            inRange:range
                            options:0
                         usingBlock:^(NSFont *base, NSRange baseRange, BOOL *stop) {
        if (!base)
            return;
        [textStorage enumerateAttribute:NSFontAttributeName
                                inRange:baseRange
                                options:0
                             usingBlock:^(NSFont *font, NSRange fontRange, BOOL *stop2) {
            if (font != base && ![font isEqual:base])
                [textStorage addAttribute:NSFontAttributeName value:base range:fontRange];
        }];
    }];
}

+ (NSUInteger)layoutManager:(NSLayoutManager *)layoutManager
       shouldGenerateGlyphs:(const CGGlyph *)glyphs
                 properties:(const NSGlyphProperty *)props
           characterIndexes:(const NSUInteger *)charIndexes
                       font:(NSFont *)aFont
              forGlyphRange:(NSRange)glyphRange {
    NSTextStorage *textStorage = layoutManager.textStorage;
    NSGlyphProperty *newProps = NULL;
    for (NSUInteger i = 0; i < glyphRange.length; i++) {
        if (!font3CodeAtIndex(textStorage, charIndexes[i]))
            continue;
        if (!newProps) {
            newProps = malloc(sizeof(NSGlyphProperty) * glyphRange.length);
            memcpy(newProps, props, sizeof(NSGlyphProperty) * glyphRange.length);
        }
        // Laid out as whitespace of our own width (see below) and drawn by
        // -drawGlyphsForGlyphRange:atPoint:.
        newProps[i] = NSGlyphPropertyControlCharacter;
    }
    if (!newProps)
        return 0;
    [layoutManager setGlyphs:glyphs properties:newProps characterIndexes:charIndexes font:aFont forGlyphRange:glyphRange];
    free(newProps);
    return glyphRange.length;
}

+ (NSControlCharacterAction)layoutManager:(NSLayoutManager *)layoutManager
                          shouldUseAction:(NSControlCharacterAction)action
               forControlCharacterAtIndex:(NSUInteger)charIndex {
    if (font3CodeAtIndex(layoutManager.textStorage, charIndex))
        return NSControlCharacterActionWhitespace;
    return action;
}

+ (NSRect)layoutManager:(NSLayoutManager *)layoutManager
boundingBoxForControlGlyphAtIndex:(NSUInteger)glyphIndex
       forTextContainer:(NSTextContainer *)textContainer
   proposedLineFragment:(NSRect)proposedRect
          glyphPosition:(NSPoint)glyphPosition
         characterIndex:(NSUInteger)charIndex {
    CGFloat width = [self cellWidthForFont:baseFontAtIndex(layoutManager.textStorage, charIndex)];
    return NSMakeRect(glyphPosition.x, glyphPosition.y, width, 0);
}

+ (CGFloat)cellWidthForFont:(NSFont *)font {
    CTFontRef ctFont = (__bridge CTFontRef)font;
    UniChar zero = '0';
    CGGlyph glyph;
    if (CTFontGetGlyphsForCharacters(ctFont, &zero, &glyph, 1))
        return CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationHorizontal, &glyph, NULL, 1);
    return font.maximumAdvancement.width;
}

#pragma mark Drawing

- (void)drawGlyphsForGlyphRange:(NSRange)glyphsToShow atPoint:(NSPoint)origin {
    [super drawGlyphsForGlyphRange:glyphsToShow atPoint:origin];

    NSTextStorage *textStorage = self.textStorage;
    NSRange charRange = [self characterRangeForGlyphRange:glyphsToShow actualGlyphRange:NULL];
    if (!charRange.length)
        return;
    [textStorage enumerateAttribute:Font3CodeAttributeName
                            inRange:charRange
                            options:0
                         usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (!value)
            return;
        for (NSUInteger i = range.location; i < NSMaxRange(range); i++)
            [self drawFont3CharacterAtIndex:i origin:origin];
    }];
}

- (void)drawFont3CharacterAtIndex:(NSUInteger)charIndex origin:(NSPoint)origin {
    NSTextStorage *textStorage = self.textStorage;
    unichar code = font3CodeAtIndex(textStorage, charIndex);
    if (!code)
        return;
    const uint8_t *rows = font3Glyphs[code - 32].rows;
    if (!(rows[0] | rows[1] | rows[2] | rows[3] | rows[4] | rows[5] | rows[6] | rows[7]))
        return;

    NSUInteger glyphIndex = [self glyphIndexForCharacterAtIndex:charIndex];
    if (glyphIndex >= self.numberOfGlyphs)
        return;
    NSRect fragment = [self lineFragmentRectForGlyphAtIndex:glyphIndex effectiveRange:NULL withoutAdditionalLayout:YES];
    if (NSIsEmptyRect(fragment))
        return;
    NSPoint location = [self locationForGlyphAtIndex:glyphIndex];
    NSFont *base = baseFontAtIndex(textStorage, charIndex);

    CGFloat left = origin.x + fragment.origin.x + location.x;
    CGFloat width = [Font3LayoutManager cellWidthForFont:base];
    CGFloat top, height;
    if (_fillsLineFragments) {
        top = origin.y + fragment.origin.y;
        height = fragment.size.height;
    } else {
        // location.y is the baseline, measured down from the fragment top.
        top = origin.y + fragment.origin.y + location.y - base.ascender;
        height = [self defaultLineHeightForFont:base];
    }

    NSColor *color = [textStorage attribute:NSForegroundColorAttributeName atIndex:charIndex effectiveRange:NULL];
    if (!color)
        color = NSColor.textColor;

    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    if (!context)
        return;

    // Snap the cell's pixel edges to device pixels, so that neighbouring cells
    // and the lines above and below meet without seams or overlaps.
    CGFloat xs[9], ys[9];
    for (NSUInteger k = 0; k <= 8; k++) {
        CGPoint p = CGContextConvertPointToDeviceSpace(context, CGPointMake(left + width * k / 8, top + height * k / 8));
        p.x = round(p.x);
        p.y = round(p.y);
        p = CGContextConvertPointToUserSpace(context, p);
        xs[k] = p.x;
        ys[k] = p.y;
    }

    CGContextSaveGState(context);
    CGContextSetShouldAntialias(context, false);
    CGContextSetFillColorWithColor(context, color.CGColor);
    for (NSUInteger row = 0; row < 8; row++) {
        uint8_t bits = rows[row];
        NSUInteger column = 0;
        while (column < 8) {
            if (!(bits & (0x80 >> column))) {
                column++;
                continue;
            }
            NSUInteger end = column;
            while (end < 8 && (bits & (0x80 >> end)))
                end++;
            CGRect rect = CGRectStandardize(CGRectMake(xs[column], ys[row], xs[end] - xs[column], ys[row + 1] - ys[row]));
            CGContextFillRect(context, rect);
            column = end;
        }
    }
    CGContextRestoreGState(context);
}

@end
