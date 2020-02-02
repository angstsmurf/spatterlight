//
//  NSColor+compare.m
//  Spatterlight
//
//  Created by Administrator on 2020-02-02.
//  Based on code by samvermette:
//  https://stackoverflow.com/questions/970475/how-to-compare-uicolors
//

#import "NSColor+compare.h"

@implementation NSColor (compare)

- (BOOL)isEqualToColor:(NSColor *)otherColor {
    CGColorSpaceRef colorSpaceRGB = CGColorSpaceCreateDeviceRGB();

    NSColor *(^convertColorToRGBSpace)(NSColor*) = ^(NSColor *color) {
        if (CGColorSpaceGetModel(CGColorGetColorSpace(color.CGColor)) == kCGColorSpaceModelMonochrome) {
            const CGFloat *oldComponents = CGColorGetComponents(color.CGColor);
            CGFloat components[4] = {oldComponents[0], oldComponents[0], oldComponents[0], oldComponents[1]};
            CGColorRef colorRef = CGColorCreate( colorSpaceRGB, components );

            color = [NSColor colorWithCGColor:colorRef];
            CGColorRelease(colorRef);
            return color;
        } else
            return color;
    };

    NSColor *selfColor = convertColorToRGBSpace(self);
    otherColor = convertColorToRGBSpace(otherColor);
    CGColorSpaceRelease(colorSpaceRGB);

    return [selfColor isEqual:otherColor];
}

@end
