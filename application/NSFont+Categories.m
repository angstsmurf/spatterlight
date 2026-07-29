//
//  NSFont+Categories.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-13.
//

#import "NSFont+Categories.h"

#import <CoreText/CoreText.h>

@implementation NSFont (Categories)

// Find a font size to match a certain width in points.
// This is really only necessary on 10.7, which can't give exact character width for fonts,
// but I think it give slightly better results (less gaps) on recent systems as well.

- (NSFont *)fontToFitWidth:(CGFloat)desiredWidth sampleText:(NSString *)text {
    {
        if (!text.length || desiredWidth < 2) {
            return self;
        }

        CGFloat guess;
        CGFloat guessWidth;

        guess = self.pointSize;
        if (guess > 1 && guess < 1000) { guess = 50; }

        guessWidth = [self widthForPointSize:guess sampleText:text];

        if (guessWidth == desiredWidth) {
            return [NSFont fontWithDescriptor:self.fontDescriptor size:guess];
        }

        NSInteger iterations = 4;

        while(iterations > 0) {
            guess = guess * ( desiredWidth / guessWidth );
            guessWidth = [self widthForPointSize:guess sampleText:text];

            if (guessWidth == desiredWidth)
            {
                return [NSFont fontWithDescriptor:self.fontDescriptor size:guess];
            }

            iterations -= 1;
        }
        return [NSFont fontWithDescriptor:self.fontDescriptor size:guess];
    }
}

- (CGFloat)widthForPointSize:(CGFloat)guess sampleText:(NSString *)text {
    NSFont *newFont = [NSFont fontWithDescriptor:self.fontDescriptor size:guess];
    NSMutableDictionary *dic = [NSMutableDictionary dictionary];
    dic[NSFontAttributeName] = newFont;
    CGFloat textWidth = [text sizeWithAttributes:dic].width;
    return textWidth;
}

// Whether this font can draw a non-breaking space (U+00A0). Not every font can:
// asking for one in a font without the glyph gets us the "unprintable character"
// box instead. Glyph coverage does not vary with point size, so we cache the
// answer per font name.
- (BOOL)hasNonBreakingSpaceGlyph {
    static NSMutableDictionary<NSString *, NSNumber *> *cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [[NSMutableDictionary alloc] init];
    });

    NSString *name = self.fontName;
    if (!name)
        return NO;

    NSNumber *cached = cache[name];
    if (cached)
        return cached.boolValue;

    unichar character = 0xa0;
    CGGlyph glyph = 0;
    BOOL found = (CTFontGetGlyphsForCharacters((__bridge CTFontRef)self,
                                               &character, &glyph, 1) && glyph != 0);
    cache[name] = @(found);
    return found;
}

@end
