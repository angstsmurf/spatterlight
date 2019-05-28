//
//  GlkHyperlink.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-16.
//

#import "GlkHyperlink.h"

@implementation GlkHyperlink

- (instancetype)initWithIndex:(NSUInteger)index andPos:(NSUInteger)pos {
    self = [super init];
    if (self) {
        _index = index;
        _startpos = pos;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _index = [decoder decodeIntegerForKey:@"index"];
    _startpos = [decoder decodeIntegerForKey:@"startpos"];
    NSValue *rangeVal = [decoder decodeObjectForKey:@"range"];
    _range = rangeVal.rangeValue;
    _bounds = [decoder decodeRectForKey:@"bounds"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:_index forKey:@"index"];
    [encoder encodeInteger:_startpos forKey:@"startpos"];
    NSValue *rangeVal = [NSValue valueWithRange:_range];
    [encoder encodeObject:rangeVal forKey:@"range"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
}

@end
