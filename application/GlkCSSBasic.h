//
//  GlkCSSBasic.h
//  Spatterlight
//
//  CSS Basic → AppKit attribute mapper for the Glk CSS Basic extension.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@class Theme;

@interface GlkCSSBasic : NSObject

/** Apply a CSS property/value map onto an attributes dictionary.
 *  The CSS Basic paragraph properties (text-align, margin-left, margin-right,
 *  text-indent) are always applied: Glk paragraph boundaries are newline-driven
 *  and AppKit paragraph styles are per-run, so inline CSS may carry them too.
 *  When allowParagraph is YES, background-color is stored as GlkParaBackground
 *  for block (content-box) fills; when NO, it sets NSBackgroundColorAttributeName
 *  (glyph/run background).
 *  reverseOut is set when -iftf-reverse-video is present (caller applies reverse).
 *
 *  margin-left / text-indent follow CSS: headIndent = margin-left, and
 *  firstLineHeadIndent = margin-left + text-indent (AppKit's first-line indent
 *  is absolute from the container edge, not relative to headIndent).
 */
+ (void)applyProperties:(NSDictionary<NSString *, NSString *> *)props
         toAttributes:(NSMutableDictionary *)attributes
                theme:(Theme *)theme
       allowParagraph:(BOOL)allowParagraph
           reverseOut:(nullable BOOL *)reverseOut;

/** Attribute key for CSS_Paragraph background-color (NSColor). */
FOUNDATION_EXPORT NSString * const GlkParaBackgroundAttributeName;

/** Attribute key for CSS_Span / CSS_Hyperlink border-style:solid (NSNumber YES). */
FOUNDATION_EXPORT NSString * const GlkSpanBorderAttributeName;

/** Attribute key for CSS_Paragraph border-style:solid (NSNumber YES). */
FOUNDATION_EXPORT NSString * const GlkParaBorderAttributeName;

/** Fill content-box rects for GlkParaBackground (under glyphs). */
+ (void)drawParagraphBackgroundsInTextView:(NSTextView *)textView
                                glyphRange:(NSRange)glyphsToShow
                                  atOrigin:(NSPoint)origin;

/** Stroke CSS_Paragraph border-style boxes for the glyph range. */
+ (void)drawParagraphBordersInTextView:(NSTextView *)textView
                            glyphRange:(NSRange)glyphsToShow
                              atOrigin:(NSPoint)origin;

/** Stroke CSS_Span / CSS_Hyperlink border-style boxes (per line fragment). */
+ (void)drawSpanBordersInTextView:(NSTextView *)textView
                       glyphRange:(NSRange)glyphsToShow
                         atOrigin:(NSPoint)origin;

/** Replace a restored NSLayoutManager with GlkLayoutManager if needed.
 *  Returns the (possibly new) layout manager attached to the text view. */
+ (NSLayoutManager *)ensureLayoutManagerForTextView:(NSTextView *)textView;

/** Parse a CSS color (#rgb/#rrggbb/#rrggbbaa, named, transparent, or
 *  light-dark(<color>, <color>) resolved by system appearance).
 *  Returns nil if unknown. For transparent / #xxxxxx00, returns a color with alpha 0.
 */
+ (nullable NSColor *)colorFromCSSValue:(NSString *)value;

/** YES for border-style:solid. */
+ (BOOL)isSolidBorderStyle:(nullable NSString *)value;

/** Apply border-style solid|none to a view via its layer. */
+ (void)applyBorderStyle:(nullable NSString *)value toView:(NSView *)view;

/** Deep-copy an array of style_NUMSTYLES property dictionaries. */
+ (NSMutableArray<NSMutableDictionary *> *)deepCopyOfCSSHintArray:(NSArray *)array;

/**
 * CSS Basic properties that inherit from CSS_Window into Glk styles.
 * Non-inherited (window-only): margin-left, margin-right, background-color,
 * border-style, and -iftf-reverse-video (resolved into color / window bg).
 */
+ (BOOL)isInheritedProperty:(NSString *)prop;

/** Subset of props that should inherit from CSS_Window onto text styles. */
+ (NSDictionary<NSString *, NSString *> *)inheritablePropertiesFrom:(NSDictionary<NSString *, NSString *> *)props;

/** YES when -iftf-reverse-video means reverse (same tokens as applyProperties). */
+ (BOOL)isReverseVideoValue:(nullable NSString *)value;

@end

/** Layout manager that paints CSS_Paragraph background-color as content-box fills. */
@interface GlkLayoutManager : NSLayoutManager
@end

NS_ASSUME_NONNULL_END
