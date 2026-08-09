//
//  GlkCSSBasic.m
//  Spatterlight
//

#import "GlkCSSBasic.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "GlkWindow.h"
#import "Preferences.h"

NSString * const GlkParaBackgroundAttributeName = @"GlkParaBackground";
NSString * const GlkSpanBorderAttributeName = @"GlkSpanBorder";
NSString * const GlkParaBorderAttributeName = @"GlkParaBorder";

@implementation GlkCSSBasic

+ (NSMutableArray<NSMutableDictionary *> *)deepCopyOfCSSHintArray:(NSArray *)array {
    NSMutableArray *out = [NSMutableArray arrayWithCapacity:array.count];
    for (NSDictionary *dict in array) {
        [out addObject:[dict mutableCopy]];
    }
    return out;
}

/** Split light-dark(a, b) inner text on the top-level comma. Returns YES on success. */
+ (BOOL)splitLightDarkArgs:(NSString *)inner
                     light:(NSString * _Nonnull * _Nonnull)lightOut
                      dark:(NSString * _Nonnull * _Nonnull)darkOut {
    NSInteger depth = 0;
    NSInteger commaAt = NSNotFound;
    for (NSUInteger i = 0; i < inner.length; i++) {
        unichar c = [inner characterAtIndex:i];
        if (c == '(')
            depth++;
        else if (c == ')')
            depth--;
        else if (c == ',' && depth == 0) {
            if (commaAt != NSNotFound)
                return NO; /* more than two args */
            commaAt = (NSInteger)i;
        }
    }
    if (commaAt == NSNotFound || depth != 0)
        return NO;
    NSString *light = [[inner substringToIndex:(NSUInteger)commaAt]
                       stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *dark = [[inner substringFromIndex:(NSUInteger)commaAt + 1]
                      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!light.length || !dark.length)
        return NO;
    *lightOut = light;
    *darkOut = dark;
    return YES;
}

+ (nullable NSColor *)colorFromCSSValue:(NSString *)value {
    if (!value.length)
        return nil;

    NSString *v = [[value stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];

    if ([v isEqualToString:@"transparent"])
        return [NSColor colorWithDeviceRed:0 green:0 blue:0 alpha:0];

    /* light-dark(<color>, <color>) — pick by system appearance. */
    if ([v hasPrefix:@"light-dark("] && [v hasSuffix:@")"] && v.length > 12) {
        NSString *inner = [v substringWithRange:NSMakeRange(11, v.length - 12)];
        NSString *lightStr = nil;
        NSString *darkStr = nil;
        if (![self splitLightDarkArgs:inner light:&lightStr dark:&darkStr])
            return nil;
        NSColor *light = [self colorFromCSSValue:lightStr];
        NSColor *dark = [self colorFromCSSValue:darkStr];
        if (!light || !dark)
            return nil;
        return ([Preferences currentSystemMode] == kDarkMode) ? dark : light;
    }

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
            @"cyan": @(0x00ffff), @"magenta": @(0xff00ff), @"pink": @(0xffc0cb),
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
    } else if ([lower isEqualToString:@"cursive"]) {
        candidate = [NSFont fontWithName:@"Apple Chancery" size:font.pointSize]
            ?: [NSFont fontWithName:@"AppleChancery" size:font.pointSize];
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
            if (c)
                attributes[NSForegroundColorAttributeName] = c;
            continue;
        }
        if ([key isEqualToString:@"background-color"]) {
            NSColor *c = [self colorFromCSSValue:val];
            if (allowParagraph) {
                /* CSS_Paragraph: content-box fill (drawn under glyphs). */
                if (!c || c.alphaComponent == 0)
                    [attributes removeObjectForKey:GlkParaBackgroundAttributeName];
                else
                    attributes[GlkParaBackgroundAttributeName] = c;
            } else {
                /* Span / inline: glyph-run background. */
                if (!c || c.alphaComponent == 0)
                    [attributes removeObjectForKey:NSBackgroundColorAttributeName];
                else
                    attributes[NSBackgroundColorAttributeName] = c;
            }
            continue;
        }
        if ([key isEqualToString:@"-iftf-reverse-video"]) {
            if (reverseOut) {
                /* Official Basic values are reverse | none; keep 1/true/yes
                   for earlier draft spellings still used in the wild. */
                *reverseOut = ([lval isEqualToString:@"reverse"]
                               || [lval isEqualToString:@"1"]
                               || [lval isEqualToString:@"true"]
                               || [lval isEqualToString:@"yes"]);
            }
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
                attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleNone);
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
        if ([key isEqualToString:@"border-style"]) {
            /* CSS border-color defaults to currentcolor; we stroke with the
               run's foreground (or labelColor) when drawing. */
            NSString *attrName = allowParagraph
                ? GlkParaBorderAttributeName : GlkSpanBorderAttributeName;
            if ([self isSolidBorderStyle:val])
                attributes[attrName] = @YES;
            else
                [attributes removeObjectForKey:attrName];
            continue;
        }
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

+ (void)drawParagraphBackgroundsInTextView:(NSTextView *)textView
                                glyphRange:(NSRange)glyphsToShow
                                  atOrigin:(NSPoint)origin {
    if (glyphsToShow.length == 0)
        return;

    NSTextStorage *storage = textView.textStorage;
    NSLayoutManager *lm = textView.layoutManager;
    NSTextContainer *container = textView.textContainer;
    if (!storage.length || !lm || !container)
        return;

    /* origin is already the text container origin in view coords (includes inset). */
    CGFloat containerWidth = container.containerSize.width;

    NSRange charRange = [lm characterRangeForGlyphRange:glyphsToShow actualGlyphRange:NULL];
    if (charRange.length == 0)
        return;

    [storage enumerateAttribute:GlkParaBackgroundAttributeName
                        inRange:charRange
                        options:0
                     usingBlock:^(NSColor *color, NSRange range, BOOL *stop) {
        if (!color || color.alphaComponent <= 0)
            return;

        NSRange glyphRange = [lm glyphRangeForCharacterRange:range actualCharacterRange:NULL];
        glyphRange = NSIntersectionRange(glyphRange, glyphsToShow);
        if (glyphRange.length == 0)
            return;

        [lm enumerateLineFragmentsForGlyphRange:glyphRange
                                     usingBlock:^(NSRect fragmentRect, NSRect usedRect, NSTextContainer *tc, NSRange lineGlyphRange, BOOL *stopLines) {
            if (tc != container)
                return;

            NSUInteger charIndex = [lm characterIndexForGlyphAtIndex:lineGlyphRange.location];
            if (charIndex >= storage.length)
                return;
            NSParagraphStyle *ps = [storage attribute:NSParagraphStyleAttributeName
                                              atIndex:charIndex
                                       effectiveRange:NULL];
            CGFloat head = ps ? ps.headIndent : 0;
            CGFloat tail = ps ? ps.tailIndent : 0;
            CGFloat rightInset = (tail < 0) ? -tail : 0;

            NSRect fill = fragmentRect;
            fill.origin.x += origin.x + head;
            fill.origin.y += origin.y;
            fill.size.width = MAX(0, containerWidth - head - rightInset);
            [color setFill];
            NSRectFillUsingOperation(fill, NSCompositingOperationSourceOver);
        }];
    }];
}

+ (void)strokeBorderRect:(NSRect)rect withColor:(NSColor *)color {
    if (NSIsEmptyRect(rect) || rect.size.width < 0.5 || rect.size.height < 0.5)
        return;
    [(color ?: NSColor.labelColor) setStroke];
    NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(rect, 0.5, 0.5)];
    path.lineWidth = 1.0;
    [path stroke];
}

+ (void)drawParagraphBordersInTextView:(NSTextView *)textView
                            glyphRange:(NSRange)glyphsToShow
                              atOrigin:(NSPoint)origin {
    if (glyphsToShow.length == 0)
        return;

    NSTextStorage *storage = textView.textStorage;
    NSLayoutManager *lm = textView.layoutManager;
    NSTextContainer *container = textView.textContainer;
    if (!storage.length || !lm || !container)
        return;

    CGFloat containerWidth = container.containerSize.width;
    NSRange charRange = [lm characterRangeForGlyphRange:glyphsToShow actualGlyphRange:NULL];
    if (charRange.length == 0)
        return;

    [storage enumerateAttribute:GlkParaBorderAttributeName
                        inRange:charRange
                        options:0
                     usingBlock:^(NSNumber *flag, NSRange range, BOOL *stop) {
        if (![flag boolValue])
            return;

        /* Full paragraph glyph range — not clipped to the dirty rect — so the
           stroked box stays complete when AppKit redraws only a slice. */
        NSRange glyphRange = [lm glyphRangeForCharacterRange:range actualCharacterRange:NULL];
        if (glyphRange.length == 0)
            return;
        if (NSIntersectionRange(glyphRange, glyphsToShow).length == 0)
            return;

        __block NSRect unionRect = NSZeroRect;
        __block BOOL have = NO;
        __block NSColor *strokeColor = nil;

        [lm enumerateLineFragmentsForGlyphRange:glyphRange
                                     usingBlock:^(NSRect fragmentRect, NSRect usedRect, NSTextContainer *tc, NSRange lineGlyphRange, BOOL *stopLines) {
            if (tc != container)
                return;

            NSUInteger charIndex = [lm characterIndexForGlyphAtIndex:lineGlyphRange.location];
            if (charIndex >= storage.length)
                return;
            if (!strokeColor) {
                strokeColor = [storage attribute:NSForegroundColorAttributeName
                                         atIndex:charIndex
                                  effectiveRange:NULL];
            }
            NSParagraphStyle *ps = [storage attribute:NSParagraphStyleAttributeName
                                              atIndex:charIndex
                                       effectiveRange:NULL];
            CGFloat head = ps ? ps.headIndent : 0;
            CGFloat tail = ps ? ps.tailIndent : 0;
            CGFloat rightInset = (tail < 0) ? -tail : 0;

            NSRect box = fragmentRect;
            box.origin.x += origin.x + head;
            box.origin.y += origin.y;
            box.size.width = MAX(0, containerWidth - head - rightInset);
            if (!have) {
                unionRect = box;
                have = YES;
            } else {
                unionRect = NSUnionRect(unionRect, box);
            }
        }];

        if (have)
            [self strokeBorderRect:unionRect withColor:strokeColor];
    }];
}

+ (void)drawSpanBordersInTextView:(NSTextView *)textView
                       glyphRange:(NSRange)glyphsToShow
                         atOrigin:(NSPoint)origin {
    if (glyphsToShow.length == 0)
        return;

    NSTextStorage *storage = textView.textStorage;
    NSLayoutManager *lm = textView.layoutManager;
    NSTextContainer *container = textView.textContainer;
    if (!storage.length || !lm || !container)
        return;

    NSRange charRange = [lm characterRangeForGlyphRange:glyphsToShow actualGlyphRange:NULL];
    if (charRange.length == 0)
        return;

    [storage enumerateAttribute:GlkSpanBorderAttributeName
                        inRange:charRange
                        options:0
                     usingBlock:^(NSNumber *flag, NSRange range, BOOL *stop) {
        if (![flag boolValue])
            return;

        NSRange glyphRange = [lm glyphRangeForCharacterRange:range actualCharacterRange:NULL];
        glyphRange = NSIntersectionRange(glyphRange, glyphsToShow);
        if (glyphRange.length == 0)
            return;

        NSColor *strokeColor = [storage attribute:NSForegroundColorAttributeName
                                          atIndex:range.location
                                   effectiveRange:NULL];

        [lm enumerateLineFragmentsForGlyphRange:glyphRange
                                     usingBlock:^(NSRect fragmentRect, NSRect usedRect, NSTextContainer *tc, NSRange lineGlyphRange, BOOL *stopLines) {
            if (tc != container)
                return;

            NSRange fragGlyphs = NSIntersectionRange(lineGlyphRange, glyphRange);
            if (fragGlyphs.length == 0)
                return;

            NSRect bounds = [lm boundingRectForGlyphRange:fragGlyphs
                                          inTextContainer:container];
            bounds.origin.x += origin.x;
            bounds.origin.y += origin.y;
            /* Pad slightly so the stroke clears the glyph ink. */
            bounds = NSInsetRect(bounds, -1.0, -0.5);
            [self strokeBorderRect:bounds withColor:strokeColor];
        }];
    }];
}

+ (NSLayoutManager *)ensureLayoutManagerForTextView:(NSTextView *)textView {
    NSLayoutManager *old = textView.layoutManager;
    if (!textView || [old isKindOfClass:[GlkLayoutManager class]])
        return old;

    NSTextStorage *storage = textView.textStorage;
    if (!storage || !old)
        return old;

    id delegate = old.delegate;
    BOOL bgLayout = old.backgroundLayoutEnabled;
    BOOL nonContig = old.allowsNonContiguousLayout;

    NSArray<NSTextContainer *> *containers = [old.textContainers copy];
    while (old.textContainers.count > 0)
        [old removeTextContainerAtIndex:0];

    GlkLayoutManager *lm = [[GlkLayoutManager alloc] init];
    lm.delegate = delegate;
    lm.backgroundLayoutEnabled = bgLayout;
    lm.allowsNonContiguousLayout = nonContig;

    [storage removeLayoutManager:old];
    [storage addLayoutManager:lm];
    for (NSTextContainer *tc in containers)
        [lm addTextContainer:tc];
    return lm;
}

+ (BOOL)isSolidBorderStyle:(NSString *)value {
    if (!value.length)
        return NO;
    NSString *lval = [[value stringByTrimmingCharactersInSet:
                       [NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];
    return [lval isEqualToString:@"solid"];
}

+ (void)applyBorderStyle:(NSString *)value toView:(NSView *)view {
    if (!view)
        return;
    BOOL solid = [self isSolidBorderStyle:value];
    view.wantsLayer = YES;
    if (solid) {
        view.layer.borderWidth = 1.0;
        view.layer.borderColor = NSColor.textColor.CGColor;
    } else {
        view.layer.borderWidth = 0.0;
        view.layer.borderColor = nil;
    }
}

+ (BOOL)isInheritedProperty:(NSString *)prop {
    if (!prop.length)
        return NO;
    NSString *key = prop.lowercaseString;
    /* Not inherited: box model / window chrome, and reverse (resolved specially). */
    if ([key isEqualToString:@"margin-left"]
        || [key isEqualToString:@"margin-right"]
        || [key isEqualToString:@"background-color"]
        || [key isEqualToString:@"border-style"]
        || [key isEqualToString:@"-iftf-reverse-video"])
        return NO;
    return YES;
}

+ (NSDictionary<NSString *, NSString *> *)inheritablePropertiesFrom:(NSDictionary<NSString *, NSString *> *)props {
    if (!props.count)
        return @{};
    NSMutableDictionary *out = [NSMutableDictionary dictionary];
    for (NSString *rawKey in props) {
        if ([self isInheritedProperty:rawKey])
            out[rawKey] = props[rawKey];
    }
    return out;
}

+ (BOOL)isReverseVideoValue:(NSString *)value {
    if (!value.length)
        return NO;
    NSString *lval = [[value stringByTrimmingCharactersInSet:
                       [NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];
    return ([lval isEqualToString:@"reverse"]
            || [lval isEqualToString:@"1"]
            || [lval isEqualToString:@"true"]
            || [lval isEqualToString:@"yes"]);
}

@end

@implementation GlkLayoutManager

- (void)drawBackgroundForGlyphRange:(NSRange)glyphsToShow atPoint:(NSPoint)origin {
    NSTextView *tv = self.firstTextView;
    BOOL drawCSS = YES;
    id del = tv.delegate;
    if ([del isKindOfClass:[GlkWindow class]])
        drawCSS = ((GlkWindow *)del).theme.doStyles;
    if (tv && drawCSS)
        [GlkCSSBasic drawParagraphBackgroundsInTextView:tv glyphRange:glyphsToShow atOrigin:origin];
    [super drawBackgroundForGlyphRange:glyphsToShow atPoint:origin];
}

- (void)drawGlyphsForGlyphRange:(NSRange)glyphsToShow atPoint:(NSPoint)origin {
    [super drawGlyphsForGlyphRange:glyphsToShow atPoint:origin];
    NSTextView *tv = self.firstTextView;
    BOOL drawCSS = YES;
    id del = tv.delegate;
    if ([del isKindOfClass:[GlkWindow class]])
        drawCSS = ((GlkWindow *)del).theme.doStyles;
    if (!tv || !drawCSS)
        return;
    /* Borders after glyphs so the stroke sits on top of run backgrounds. */
    [GlkCSSBasic drawParagraphBordersInTextView:tv glyphRange:glyphsToShow atOrigin:origin];
    [GlkCSSBasic drawSpanBordersInTextView:tv glyphRange:glyphsToShow atOrigin:origin];
}

@end
