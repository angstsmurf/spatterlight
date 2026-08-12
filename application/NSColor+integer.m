//
//  NSColor+integer.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-05-13.
//
//

#import "NSColor+integer.h"

@implementation NSColor (integer)

- (NSInteger)integerColor {
    CGFloat r, g, b, a;
    uint32_t buf[3];
    NSInteger i;
    NSColor *color = [self colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];

    [color getRed:&r green:&g blue:&b alpha:&a];

    // Round rather than truncate: NSKeyedArchiver stores colour components as
    // float32, so a colour that has been through an autosave comes back a hair
    // under its exact value, and truncation would shift the whole channel.
    buf[0] = (uint32_t)fmin(255.0, fmax(0.0, round(r * 255)));
    buf[1] = (uint32_t)fmin(255.0, fmax(0.0, round(g * 255)));
    buf[2] = (uint32_t)fmin(255.0, fmax(0.0, round(b * 255)));

    i = buf[2] + (buf[1] << 8) + (buf[0] << 16);
    return i;
}

+ (NSColor *)colorFromInteger:(NSInteger)c {
    NSColor *color = nil;
    CGFloat r, g, b;

    r = (c >> 16) / 255.0;
    g = (c >> 8 & 0xFF) / 255.0;
    b = (c & 0xFF) / 255.0;

    color = [NSColor colorWithDeviceRed:r green:g blue:b alpha:1.0];

    if (!color)
        color = [NSColor whiteColor];
    return color;
}

+ (NSColor *)colorFromData:(NSData *)data {
    NSColor *color;
    CGFloat r, g, b;
    const unsigned char *buf = data.bytes;

    if (data.length < 3)
        r = g = b = 0;
    else {
        r = buf[0] / 255.0;
        g = buf[1] / 255.0;
        b = buf[2] / 255.0;
    }

    color = [NSColor colorWithDeviceRed:r green:g blue:b alpha:1.0];
    return color;
}

- (BOOL)isEqualToColor:(NSColor *)color {
    NSColor *aColor = [self colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    NSColor *bColor = [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    return ([aColor isEqualTo:bColor]);
}

@end
