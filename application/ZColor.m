//
//  ZColor.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import "ZColor.h"
#import "NSColor+integer.h"

@implementation ZColor

- (instancetype)initWithText:(NSInteger)fg background:(NSInteger)bg
                  andLocation:(NSUInteger)location {
    self = [super init];
    if (self) {
        _fg = fg;
        _bg = bg;
        _startpos = location;
        _range = NSMakeRange(location, 0);
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
        mutable[NSForegroundColorAttributeName] = [NSColor colorFromInteger:_fg];
    }
    if (_bg >= 0) {
        mutable[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:_bg];
    }
    return mutable;
}

- (NSDictionary *)reversedAttributes:(NSDictionary *)dict {
    NSMutableDictionary *mutable = [dict mutableCopy];
    if (_fg >= 0) {
        mutable[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:_fg];
    }
    if (_bg >= 0) {
        mutable[NSForegroundColorAttributeName] = [NSColor colorFromInteger:_bg];
    }
    return mutable;
}



- (NSString *)description {
    return [NSString stringWithFormat:@"fg: %@ bg: %@", [NSColor colorFromInteger:_fg], [NSColor colorFromInteger:_bg]];
}

@end
