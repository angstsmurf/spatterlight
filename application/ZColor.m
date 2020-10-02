//
//  ZColor.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import "ZColor.h"
#import "NSColor+integer.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation ZColor

- (instancetype)initWithText:(NSInteger)fg background:(NSInteger)bg {
    self = [super init];
    if (self) {
        _fg = fg;
        _bg = bg;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _fg = [decoder decodeIntegerForKey:@"fg"];
    _bg = [decoder decodeIntegerForKey:@"bg"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:_fg forKey:@"fg"];
    [encoder encodeInteger:_bg forKey:@"bg"];
}

- (NSMutableDictionary *)coloredAttributes:(NSMutableDictionary *)dict {
    if (_fg >= 0) {
        dict[NSForegroundColorAttributeName] = [NSColor colorFromInteger:_fg];
    }
    if (_bg >= 0) {
        dict[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:_bg];
    }
    return dict;
}

- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict {
    if (_fg >= 0) {
        dict[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:_fg];
    }
    if (_bg >= 0) {
        dict[NSForegroundColorAttributeName] = [NSColor colorFromInteger:_bg];
    }
    return dict;
}


- (NSString *)description {
    return [NSString stringWithFormat:@"fg: %@ bg: %@", [NSColor colorFromInteger:_fg], [NSColor colorFromInteger:_bg]];
}

@end
