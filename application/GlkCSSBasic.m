//
//  GlkCSSBasic.m
//  Spatterlight
//

#import "GlkCSSBasic.h"
#import "Theme.h"
#import "GlkStyle.h"

@implementation GlkCSSBasic

+ (NSMutableArray<NSMutableDictionary *> *)deepCopyOfCSSHintArray:(NSArray *)array {
    NSMutableArray *out = [NSMutableArray arrayWithCapacity:array.count];
    for (NSDictionary *dict in array) {
        [out addObject:[dict mutableCopy]];
    }
    return out;
}

+ (nullable NSColor *)colorFromCSSValue:(NSString *)value {
    if (!value.length)
        return nil;

    NSString *v = [[value stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];

    if ([v isEqualToString:@"transparent"])
        return [NSColor colorWithDeviceRed:0 green:0 blue:0 alpha:0];

    static NSDictionary<NSString *, NSNumber *> *named = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        named = @{
            @"black": @(0x000000), @"silver": @(0xc0c0c0), @"gray": @(0x808080),
            @"grey": @(0x808080), @"white": @(0xffffff), @"maroon": @(0x800000),
            @"red": @(0xff0000), @"purple": @(0x800080), @"fuchsia": @(0xff00ff),
            @"green": @(0x008000), @"lime": @(0x00ff00), @"olive": @(0x808000),
            @"yellow": @(0xffff00), @"navy": @(0x000080), @"blue": @(0x0000ff),
            @"teal": @(0x008080), @"aqua": @(0x00ffff), @"orange": @(0xffa500),
            @"cyan": @(0x00ffff), @"magenta": @(0xff00ff),
        };
    });

    NSNumber *rgb = named[v];
    if (rgb) {
        NSInteger c = rgb.integerValue;
        return [NSColor colorWithDeviceRed:((c >> 16) & 0xff) / 255.0
                                     green:((c >> 8) & 0xff) / 255.0
                                      blue:(c & 0xff) / 255.0
                                     alpha:1.0];
    }

    if (![v hasPrefix:@"#"])
        return nil;

    NSString *hex = [v substringFromIndex:1];
    NSUInteger len = hex.length;
    unsigned int r = 0, g = 0, b = 0, a = 255;

    NSScanner *scanner = [NSScanner scannerWithString:hex];
    unsigned int raw = 0;
    if (![scanner scanHexInt:&raw])
        return nil;

    if (len == 3) {
        r = ((raw >> 8) & 0xf) * 0x11;
        g = ((raw >> 4) & 0xf) * 0x11;
        b = (raw & 0xf) * 0x11;
    } else if (len == 4) {
        r = ((raw >> 12) & 0xf) * 0x11;
        g = ((raw >> 8) & 0xf) * 0x11;
        b = ((raw >> 4) & 0xf) * 0x11;
        a = (raw & 0xf) * 0x11;
    } else if (len == 6) {
        r = (raw >> 16) & 0xff;
        g = (raw >> 8) & 0xff;
        b = raw & 0xff;
    } else if (len == 8) {
        r = (raw >> 24) & 0xff;
        g = (raw >> 16) & 0xff;
        b = (raw >> 8) & 0xff;
        a = raw & 0xff;
    } else {
        return nil;
    }

    return [NSColor colorWithDeviceRed:r / 255.0
                                 green:g / 255.0
                                  blue:b / 255.0
                                 alpha:a / 255.0];
}

+ (CGFloat)lengthValue:(NSString *)value relativeTo:(CGFloat)base {
    NSString *v = [[value stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];
    if (!v.length)
        return 0;

    if ([v hasSuffix:@"%"]) {
        CGFloat pct = [[v substringToIndex:v.length - 1] doubleValue];
        return base * (pct / 100.0);
    }
    if ([v hasSuffix:@"em"]) {
        return base * [[v substringToIndex:v.length - 2] doubleValue];
    }
    if ([v hasSuffix:@"pt"]) {
        return [[v substringToIndex:v.length - 2] doubleValue];
    }
    if ([v hasSuffix:@"px"]) {
        return [[v substringToIndex:v.length - 2] doubleValue];
    }
    return [v doubleValue];
}

+ (NSFont *)font:(NSFont *)font withFamily:(NSString *)family theme:(Theme *)theme {
    NSFontManager *fm = [NSFontManager sharedFontManager];
    NSString *name = [family stringByTrimmingCharactersInSet:
                      [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!name.length)
        return font;

    NSString *lower = name.lowercaseString;
    NSFont *candidate = nil;

    if ([lower isEqualToString:@"monospace"] || [lower isEqualToString:@"mono"]) {
        candidate = theme.bufPre.font ?: [NSFont userFixedPitchFontOfSize:font.pointSize];
    } else if ([lower isEqualToString:@"serif"]) {
        candidate = [NSFont fontWithName:@"Times New Roman" size:font.pointSize]
            ?: [NSFont fontWithName:@"Times" size:font.pointSize]
            ?: [NSFont fontWithName:@"Georgia" size:font.pointSize];
    } else if ([lower isEqualToString:@"sans-serif"] || [lower isEqualToString:@"sans"]) {
        candidate = theme.bufferNormal.font
            ?: [NSFont systemFontOfSize:font.pointSize];
    } else {
        candidate = [NSFont fontWithName:name size:font.pointSize];
        if (!candidate) {
            /* Try without spaces (PostScript-style) and common aliases. */
            NSString *compact = [name stringByReplacingOccurrencesOfString:@" " withString:@""];
            candidate = [NSFont fontWithName:compact size:font.pointSize];
        }
        if (!candidate && ([lower containsString:@"ocr"] || [lower isEqualToString:@"courier"]
                           || [lower hasPrefix:@"courier"] || [lower isEqualToString:@"terminal"]
                           || [lower isEqualToString:@"consolas"]
                           || [lower isEqualToString:@"monaco"] || [lower isEqualToString:@"menlo"])) {
            candidate = theme.bufPre.font ?: [NSFont userFixedPitchFontOfSize:font.pointSize];
        }
        if (!candidate && [lower containsString:@"comic"]) {
            candidate = [NSFont fontWithName:@"Comic Sans MS" size:font.pointSize]
                ?: [NSFont fontWithName:@"ComicSansMS" size:font.pointSize];
        }
        if (!candidate && ([lower containsString:@"times"] || [lower isEqualToString:@"serif"])) {
            candidate = [NSFont fontWithName:@"Times New Roman" size:font.pointSize]
                ?: [NSFont fontWithName:@"Times" size:font.pointSize];
        }
        if (!candidate && [lower isEqualToString:@"arial"]) {
            candidate = [NSFont fontWithName:@"Arial" size:font.pointSize]
                ?: [NSFont fontWithName:@"Helvetica" size:font.pointSize];
        }
    }

    if (!candidate)
        return nil;

    NSFontTraitMask traits = [fm traitsOfFont:font];
    NSFont *styled = [fm convertFont:candidate toSize:font.pointSize];
    if (traits & NSBoldFontMask)
        styled = [fm convertFont:styled toHaveTrait:NSBoldFontMask];
    if (traits & NSItalicFontMask)
        styled = [fm convertFont:styled toHaveTrait:NSItalicFontMask];
    return styled ?: candidate;
}

+ (void)applyProperties:(NSDictionary<NSString *, NSString *> *)props
         toAttributes:(NSMutableDictionary *)attributes
                theme:(Theme *)theme
       allowParagraph:(BOOL)allowParagraph
           reverseOut:(BOOL *)reverseOut {
    if (!props.count || !attributes)
        return;

    NSFontManager *fm = [NSFontManager sharedFontManager];
    NSFont *font = attributes[NSFontAttributeName];
    if (!font)
        font = [NSFont systemFontOfSize:12];
    CGFloat baseSize = font.pointSize;

    NSMutableParagraphStyle *para = nil;
    NSParagraphStyle *existing = attributes[NSParagraphStyleAttributeName];
    if (existing)
        para = [existing mutableCopy];
    else
        para = [[NSMutableParagraphStyle alloc] init];

    BOOL fontChanged = NO;
    BOOL paraChanged = NO;
    /* CSS margin-left / text-indent may arrive in any dict iteration order, or
       across layered applyProperties calls.  Infer the relative text-indent
       already baked into AppKit (firstLine - head), then recompute both. */
    CGFloat marginLeft = para.headIndent;
    CGFloat textIndent = para.firstLineHeadIndent - para.headIndent;
    BOOL marginLeftSet = NO;
    BOOL textIndentSet = NO;

    for (NSString *rawKey in props) {
        NSString *key = rawKey.lowercaseString;
        NSString *val = props[rawKey];
        if (!val)
            continue;
        NSString *lval = [[val stringByTrimmingCharactersInSet:
                           [NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];

        if ([key isEqualToString:@"color"]) {
            NSColor *c = [self colorFromCSSValue:val];
            if (c && c.alphaComponent > 0)
                attributes[NSForegroundColorAttributeName] = c;
            continue;
        }
        if ([key isEqualToString:@"background-color"]) {
            NSColor *c = [self colorFromCSSValue:val];
            if (!c || c.alphaComponent == 0)
                [attributes removeObjectForKey:NSBackgroundColorAttributeName];
            else
                attributes[NSBackgroundColorAttributeName] = c;
            continue;
        }
        if ([key isEqualToString:@"--glk-reverse"] || [key isEqualToString:@"reverse"]) {
            if (reverseOut)
                *reverseOut = ([lval isEqualToString:@"1"] || [lval isEqualToString:@"true"]
                               || [lval isEqualToString:@"yes"]);
            continue;
        }
        if ([key isEqualToString:@"font-weight"]) {
            BOOL bold = ([lval isEqualToString:@"bold"] || [lval isEqualToString:@"700"]
                         || [lval isEqualToString:@"bolder"]);
            BOOL normal = ([lval isEqualToString:@"normal"] || [lval isEqualToString:@"400"]
                           || [lval isEqualToString:@"lighter"]);
            if (bold) {
                font = [fm convertFont:font toHaveTrait:NSBoldFontMask];
                fontChanged = YES;
            } else if (normal) {
                font = [fm convertFont:font toNotHaveTrait:NSBoldFontMask];
                fontChanged = YES;
            }
            continue;
        }
        if ([key isEqualToString:@"font-style"]) {
            if ([lval isEqualToString:@"italic"] || [lval isEqualToString:@"oblique"]) {
                font = [fm convertFont:font toHaveTrait:NSItalicFontMask];
                fontChanged = YES;
            } else if ([lval isEqualToString:@"normal"]) {
                font = [fm convertFont:font toNotHaveTrait:NSItalicFontMask];
                fontChanged = YES;
            }
            continue;
        }
        if ([key isEqualToString:@"text-decoration"]
            || [key isEqualToString:@"text-decoration-line"]) {
            if ([lval containsString:@"underline"])
                attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
            else if ([lval isEqualToString:@"none"])
                [attributes removeObjectForKey:NSUnderlineStyleAttributeName];
            continue;
        }
        if ([key isEqualToString:@"font-size"]) {
            CGFloat size = baseSize;
            if ([lval isEqualToString:@"small"])
                size = baseSize * 0.83;
            else if ([lval isEqualToString:@"medium"])
                size = baseSize;
            else if ([lval isEqualToString:@"large"])
                size = baseSize * 1.2;
            else if ([lval isEqualToString:@"larger"])
                size = baseSize * 1.2;
            else if ([lval isEqualToString:@"smaller"])
                size = baseSize * 0.83;
            else
                size = [self lengthValue:val relativeTo:baseSize];
            if (size > 0) {
                font = [fm convertFont:font toSize:size];
                fontChanged = YES;
                baseSize = size;
            }
            continue;
        }
        if ([key isEqualToString:@"font-family"]) {
            NSArray *families = [val componentsSeparatedByString:@","];
            for (NSString *fam in families) {
                NSString *trimmed = [fam stringByTrimmingCharactersInSet:
                                     [NSCharacterSet whitespaceAndNewlineCharacterSet]];
                /* Strip optional quotes */
                if (([trimmed hasPrefix:@"\""] && [trimmed hasSuffix:@"\""])
                    || ([trimmed hasPrefix:@"'"] && [trimmed hasSuffix:@"'"]))
                    trimmed = [trimmed substringWithRange:NSMakeRange(1, trimmed.length - 2)];
                NSFont *next = [self font:font withFamily:trimmed theme:theme];
                if (next) {
                    font = next;
                    fontChanged = YES;
                    break;
                }
            }
            continue;
        }

        /* CSS Basic paragraph properties — always applied (hints and inline). */
        if ([key isEqualToString:@"text-align"]) {
            if ([lval isEqualToString:@"center"])
                para.alignment = NSTextAlignmentCenter;
            else if ([lval isEqualToString:@"right"])
                para.alignment = NSTextAlignmentRight;
            else if ([lval isEqualToString:@"left"])
                para.alignment = NSTextAlignmentLeft;
            else if ([lval isEqualToString:@"justify"])
                para.alignment = NSTextAlignmentJustified;
            paraChanged = YES;
            continue;
        }
        if ([key isEqualToString:@"margin-left"]) {
            marginLeft = [self lengthValue:val relativeTo:baseSize];
            marginLeftSet = YES;
            paraChanged = YES;
            continue;
        }
        if ([key isEqualToString:@"margin-right"]) {
            /* AppKit: negative tailIndent insets from the trailing margin. */
            para.tailIndent = -[self lengthValue:val relativeTo:baseSize];
            paraChanged = YES;
            continue;
        }
        if ([key isEqualToString:@"text-indent"]) {
            textIndent = [self lengthValue:val relativeTo:baseSize];
            textIndentSet = YES;
            paraChanged = YES;
            continue;
        }

        if (!allowParagraph)
            continue;
    }

    if (marginLeftSet || textIndentSet) {
        para.headIndent = marginLeft;
        para.firstLineHeadIndent = marginLeft + textIndent;
        paraChanged = YES;
    }

    if (fontChanged)
        attributes[NSFontAttributeName] = font;
    if (paraChanged)
        attributes[NSParagraphStyleAttributeName] = para;
}

@end
