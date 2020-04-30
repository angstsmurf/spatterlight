//
//  ZReverseVideo.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import "ZReverseVideo.h"

@implementation ZReverseVideo

- (instancetype)initWithLocation:(NSUInteger)location {
    self = [super init];
    if (self) {
        _startpos = location;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    NSValue *rangeVal = [decoder decodeObjectForKey:@"range"];
    _range = rangeVal.rangeValue;
    _index = (NSUInteger)[decoder decodeIntegerForKey:@"index"];
    _startpos = (NSUInteger)[decoder decodeIntegerForKey:@"startpos"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:(NSInteger)_index forKey:@"index"];
    [encoder encodeInteger:(NSInteger)_startpos forKey:@"startpos"];
    NSValue *rangeVal = [NSValue valueWithRange:_range];
    [encoder encodeObject:rangeVal forKey:@"range"];
}

- (NSDictionary *)reversedAttributes:(NSDictionary *)dict background:(NSColor *)backCol {
    NSMutableDictionary *mutable = [dict mutableCopy];
    NSColor *fg = dict[NSForegroundColorAttributeName];
    NSColor *bg = dict[NSBackgroundColorAttributeName];
    if (!bg)
        bg = backCol;
    if (bg)
        mutable[NSForegroundColorAttributeName] = bg;
    if (fg)
        mutable[NSBackgroundColorAttributeName] = fg;
    return mutable;
}

@end
