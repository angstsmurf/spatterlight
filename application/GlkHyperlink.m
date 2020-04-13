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
    _index = (NSUInteger)[decoder decodeIntegerForKey:@"index"];
    _startpos = (NSUInteger)[decoder decodeIntegerForKey:@"startpos"];
    NSValue *rangeVal = [decoder decodeObjectForKey:@"range"];
    _range = rangeVal.rangeValue;
    _bounds = [decoder decodeRectForKey:@"bounds"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:(NSInteger)_index forKey:@"index"];
    [encoder encodeInteger:(NSInteger)_startpos forKey:@"startpos"];
    NSValue *rangeVal = [NSValue valueWithRange:_range];
    [encoder encodeObject:rangeVal forKey:@"range"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
}

@end
