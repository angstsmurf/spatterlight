//
//  Font3LayoutManager.h
//  Spatterlight
//
//  Z-machine font 3 (the character graphics font Beyond Zork draws its map,
//  boxes and runes with), painted from the Atari ST interpreter's 8×8 bitmaps.
//
//  A font 3 character is stored in the text as its nearest Unicode equivalent
//  (box drawing, block elements, arrows, runes), so VoiceOver, speech and copy
//  see real characters. Its original code goes in Font3CodeAttributeName. The
//  layout manager delegate methods below lay such a character out as an empty
//  cell exactly as wide as a digit of the text font, and Font3LayoutManager
//  paints the bitmap into it, snapped to device pixels. In a grid window the
//  cell runs the full height of its line, so lines and boxes join with no gaps
//  whatever the font.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/// NSNumber: the font 3 code (32–126) of the character at this index.
extern NSAttributedStringKey const Font3CodeAttributeName;

/// NSFont: the text font a font 3 character takes its cell from. Put it in
/// the font 3 style; NSTextStorage replaces NSFontAttributeName with a
/// fallback font for the Unicode characters, which would change the line height.
extern NSAttributedStringKey const Font3BaseFontAttributeName;

@interface Font3LayoutManager : NSLayoutManager

/// YES: cells fill the line fragment (grid windows). NO: cells are one line of
/// the base font high, hanging from the baseline (buffer windows).
@property BOOL fillsLineFragments;

/// The Unicode stand-in for a font 3 code, or 0 outside 32–126.
+ (unichar)unicodeForCode:(unichar)code;

/// The font 3 code of the character at index, or 0 if it is not one.
+ (unichar)codeAtIndex:(NSUInteger)index inText:(NSAttributedString *)text;

/// string (font 3 codes) as Unicode stand-ins with attributes plus a code per
/// character. Characters outside 32–126 are kept as they are, unmarked.
+ (NSAttributedString *)attributedStringWithCodes:(NSString *)string
                                       attributes:(NSDictionary<NSAttributedStringKey, id> *)attributes;

/// Text archived before font 3 was painted holds the raw codes, set in the
/// old FreeFont3. When a restyle has just given such a run (range in string)
/// the font 3 style, this swaps its codes for stand-ins. previousAttributes
/// are the run's attributes before the restyle; a run that already had codes
/// marked is left alone. The length of string does not change.
+ (void)upgradeArchivedCodesIn:(NSMutableAttributedString *)string
                         range:(NSRange)range
            previousAttributes:(NSDictionary<NSAttributedStringKey, id> *)previousAttributes;

/// Call from -textStorage:didProcessEditing:range:changeInLength:.
+ (void)restoreBaseFontsInTextStorage:(NSTextStorage *)textStorage range:(NSRange)range;

/// Call from the NSLayoutManagerDelegate methods of the same names.
+ (NSUInteger)layoutManager:(NSLayoutManager *)layoutManager
       shouldGenerateGlyphs:(const CGGlyph *)glyphs
                 properties:(const NSGlyphProperty *)props
           characterIndexes:(const NSUInteger *)charIndexes
                       font:(NSFont *)aFont
              forGlyphRange:(NSRange)glyphRange;

+ (NSControlCharacterAction)layoutManager:(NSLayoutManager *)layoutManager
                          shouldUseAction:(NSControlCharacterAction)action
               forControlCharacterAtIndex:(NSUInteger)charIndex;

+ (NSRect)layoutManager:(NSLayoutManager *)layoutManager
boundingBoxForControlGlyphAtIndex:(NSUInteger)glyphIndex
       forTextContainer:(NSTextContainer *)textContainer
   proposedLineFragment:(NSRect)proposedRect
          glyphPosition:(NSPoint)glyphPosition
         characterIndex:(NSUInteger)charIndex;

/// Width of a font 3 cell set in font: the advance of a digit zero.
+ (CGFloat)cellWidthForFont:(NSFont *)font;

@end

NS_ASSUME_NONNULL_END
