//
//  NSFont+Categories.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-13.
//

#import "NSFont+Categories.h"

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
    [dic setObject:newFont forKey:NSFontAttributeName];
    CGFloat textWidth = [text sizeWithAttributes:dic].width;
    return textWidth;
}

@end
