//
//  FlowBreak.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "FlowBreak.h"

@interface FlowBreak () <NSSecureCoding> {
    BOOL recalc;
}
@end

@implementation FlowBreak

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    return [self initWithPos:0];
}

- (instancetype)initWithPos:(NSUInteger)apos {
    self = [super init];
    if (self) {
        _pos = apos;
        _bounds = NSZeroRect;
        recalc = YES;
    }
    return self;
}

- (id)initWithCoder:(NSCoder *)decoder {
    _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
    _bounds = [decoder decodeRectForKey:@"bounds"];
    recalc = [decoder decodeBoolForKey:@"recalc"];

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
    [encoder encodeBool:recalc forKey:@"recalc"];
}

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout {
    NSRange ourglyph;
    NSRange ourline;
    NSRect theline;

    if (recalc && _pos != 0) {
        recalc = NO; /* don't infiniloop in here, settle for the first result */

        /* force layout and get position of anchor glyph */
        ourglyph = [layout glyphRangeForCharacterRange:NSMakeRange(_pos, 1)
                                  actualCharacterRange:&ourline];
        theline = [layout lineFragmentRectForGlyphAtIndex:ourglyph.location
                                           effectiveRange:nil];

        _bounds = NSMakeRect(0, theline.origin.y, FLT_MAX, theline.size.height);

        /* invalidate our fake layout *after* we set the bounds ... to avoid
         * infiniloop */
        [layout invalidateLayoutForCharacterRange:ourline
                             actualCharacterRange:nil];
    }

    return _bounds;
}

- (void)uncacheBounds {
    recalc = YES;
    _bounds = NSZeroRect;
}

@end

