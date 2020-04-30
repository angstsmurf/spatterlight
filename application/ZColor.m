//
//  ZColor.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import "ZColor.h"

@implementation ZColor

- (instancetype)initWithText:(NSInteger)fg background:(NSInteger)bg
                  andLocation:(NSUInteger)location {
    self = [super init];
    if (self) {
        _fg = fg;
        _bg = bg;
        _startpos = location;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _fg = [decoder decodeIntegerForKey:@"fg"];
    _bg = [decoder decodeIntegerForKey:@"bg"];
    NSValue *rangeVal = [decoder decodeObjectForKey:@"range"];
    _range = rangeVal.rangeValue;
    _index = (NSUInteger)[decoder decodeIntegerForKey:@"index"];
    _startpos = (NSUInteger)[decoder decodeIntegerForKey:@"startpos"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:(NSInteger)_index forKey:@"index"];
    [encoder encodeInteger:(NSInteger)_startpos forKey:@"startpos"];
    [encoder encodeInteger:_fg forKey:@"fg"];
    [encoder encodeInteger:_bg forKey:@"bg"];
    NSValue *rangeVal = [NSValue valueWithRange:_range];
    [encoder encodeObject:rangeVal forKey:@"range"];
}

- (NSDictionary *)coloredAttributes:(NSDictionary *)dict {
    NSMutableDictionary *mutable = [dict mutableCopy];
    if (_fg >= 0) {
        mutable[NSForegroundColorAttributeName] = [self colorFromInteger:_fg];
    }
    if (_bg >= 0) {
        mutable[NSBackgroundColorAttributeName] = [self colorFromInteger:_bg];
    }
    return mutable;
}

- (NSDictionary *)reversedAttributes:(NSDictionary *)dict {
    NSMutableDictionary *mutable = [dict mutableCopy];
    if (_fg >= 0) {
        mutable[NSBackgroundColorAttributeName] = [self colorFromInteger:_fg];
    }
    if (_bg >= 0) {
        mutable[NSForegroundColorAttributeName] = [self colorFromInteger:_bg];
    }
    return mutable;
}

- (NSColor *)colorFromInteger:(NSInteger)value {
    NSInteger r,g,b;
    r = (value >> 16) & 0xff;
    g = (value >> 8) & 0xff;
    b = (value >> 0) & 0xff;
    return [NSColor colorWithCalibratedRed:r / 255.0
                                      green:g / 255.0
                                       blue:b / 255.0
                                      alpha:1.0];

}

- (NSString *)description {
    return [NSString stringWithFormat:@"fg: %@ bg: %@", [self colorFromInteger:_fg], [self colorFromInteger:_bg]];
}

@end
