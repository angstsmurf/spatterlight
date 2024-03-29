//
//  ZColor.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import "ZColor.h"
#import "NSColor+integer.h"
#include "glk.h"

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
    self = [super init];
    if (self) {
    _fg = [decoder decodeIntegerForKey:@"fg"];
    _bg = [decoder decodeIntegerForKey:@"bg"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:_fg forKey:@"fg"];
    [encoder encodeInteger:_bg forKey:@"bg"];
}

- (NSMutableDictionary *)coloredAttributes:(NSMutableDictionary *)dict {
    if (_fg != zcolor_Default && _fg != zcolor_Current && _fg != zcolor_Transparent) {
        dict[NSForegroundColorAttributeName] = [NSColor colorFromInteger:_fg];
    }
    if (_bg != zcolor_Default && _bg != zcolor_Current && _bg != zcolor_Transparent) {
        dict[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:_bg];
    }
    return dict;
}

- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict {
    if (_fg != zcolor_Default && _fg != zcolor_Current && _fg != zcolor_Transparent) {
        dict[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:_fg];
    }
    if (_bg != zcolor_Default && _bg != zcolor_Current && _bg != zcolor_Transparent) {
        dict[NSForegroundColorAttributeName] = [NSColor colorFromInteger:_bg];
    }
    return dict;
}


- (NSString *)description {
    NSString *fgstring, *bgstring;
    if (_fg == zcolor_Current) {
        fgstring = @"zcolor_Current";
    } else if (_fg == zcolor_Default) {
        fgstring = @"zcolor_Default";
    } else if (_fg == zcolor_Transparent) {
        fgstring = @"zcolor_Transparent";
    } else {
        fgstring = [NSString stringWithFormat:@"%lx", (long)_fg];
    }

    if (_bg == zcolor_Current) {
        bgstring = @"zcolor_Current";
    } else if (_bg == zcolor_Default) {
        bgstring = @"zcolor_Default";
    } else if (_bg == zcolor_Transparent) {
        bgstring = @"zcolor_Transparent";
    } else {
        bgstring = [NSString stringWithFormat:@"%lx", (long)_bg];
    }

    return [NSString stringWithFormat:@"fg: %@ bg: %@", fgstring, bgstring];
}

@end
