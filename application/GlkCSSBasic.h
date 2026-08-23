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
 *  allowParagraph is reserved for any future paragraph-only properties.
 *  reverseOut is set when --glk-reverse is present (caller applies reverse).
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

/** Parse a CSS color (#rgb/#rrggbb/#rrggbbaa or named). Returns nil if unknown.
 *  For transparent / #xxxxxx00, returns a color with alpha 0.
 */
+ (nullable NSColor *)colorFromCSSValue:(NSString *)value;

/** Deep-copy an array of style_NUMSTYLES property dictionaries. */
+ (NSMutableArray<NSMutableDictionary *> *)deepCopyOfCSSHintArray:(NSArray *)array;

@end

NS_ASSUME_NONNULL_END
