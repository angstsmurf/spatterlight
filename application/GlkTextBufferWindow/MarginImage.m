//
//  MarginImage.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MarginImage.h"
#import "MarginContainer.h"
#include "glk.h"

@interface MarginImage () <NSSecureCoding> {
    BOOL recalc;
}
@end

@implementation MarginImage

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    return [self
        initWithImage:[[NSImage alloc]
                          initWithContentsOfFile:@"../Resources/Question.png"]
                align:kAlignLeft
               linkid:0
                   at:0
               sender:self];
}

- (instancetype)initWithImage:(NSImage *)animage
                        align:(NSInteger)analign
                       linkid:(NSUInteger)linkid
                           at:(NSUInteger)apos
                       sender:(id)sender {
    self = [super init];
    if (self) {
        _image = animage;
        _alignment = analign;
        _bounds = NSZeroRect;
        _linkid = linkid;
        _pos = apos;
        recalc = YES;
        _container = sender;

        self.accessibilityParent = _container.textView;

        self.accessibilityRoleDescription = [self customA11yLabel];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _image = [decoder decodeObjectOfClass:[NSImage class] forKey:@"image"];
    _alignment = [decoder decodeIntegerForKey:@"alignment"];
    _bounds = [decoder decodeRectForKey:@"bounds"];
    _linkid = (NSUInteger)[decoder decodeIntegerForKey:@"linkid"];
    _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
    recalc = [decoder decodeBoolForKey:@"recalc"];
    self.accessibilityRoleDescription = [decoder decodeObjectOfClass:[NSString class] forKey:@"accessibilityRoleDescription"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_image forKey:@"image"];
    [encoder encodeInteger:_alignment forKey:@"alignment"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
    [encoder encodeInteger:(NSInteger)_linkid forKey:@"linkid"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeBool:recalc forKey:@"recalc"];
    [encoder encodeObject:self.accessibilityRoleDescription forKey:@"accessibilityRoleDescription"];
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout {
    NSRange ourglyph;
    NSRange ourline;
    NSRect theline;
    NSSize size = _image.size;

    if (recalc) {
        recalc = NO; /* don't infiniloop in here, settle for the first result */

        _bounds = NSZeroRect;
        NSTextView *textview = _container.textView;

        /* force layout and get position of anchor glyph */
        ourglyph = [layout glyphRangeForCharacterRange:NSMakeRange((NSUInteger)_pos, 1)
                                  actualCharacterRange:&ourline];
        theline = [layout lineFragmentRectForGlyphAtIndex:ourglyph.location
                                           effectiveRange:nil];

        /* set bounds to be at the same line as anchor but in left/right margin
         */
        if (_alignment == imagealign_MarginRight) {
            CGFloat rightMargin = textview.frame.size.width -
                                  textview.textContainerInset.width * 2 -
                                  _container.lineFragmentPadding;
            _bounds = NSMakeRect(rightMargin - size.width, theline.origin.y,
                                 size.width, size.height);

            // NSLog(@"rightMargin = %f, _bounds = %@", rightMargin,
            // NSStringFromRect(_bounds));

            // If the above places the image outside margin, move it within
            if (NSMaxX(_bounds) > rightMargin) {
                _bounds.origin.x = rightMargin - size.width;
                // NSLog(@"_bounds outside right margin. Moving it to %@",
                // NSStringFromRect(_bounds));
            }
        } else {
            _bounds =
                NSMakeRect(theline.origin.x + _container.lineFragmentPadding,
                           theline.origin.y, size.width, size.height);
        }

        /* invalidate our fake layout *after* we set the bounds ... to avoid
         * infiniloop */
        [layout invalidateLayoutForCharacterRange:ourline
                             actualCharacterRange:nil];
    }

    [_container unoverlap:self];
    return _bounds;
}

- (void)uncacheBounds {
    recalc = YES;
    _bounds = NSZeroRect;
}

- (void)setAccessibilityFocused:(BOOL)accessibilityFocused {
    if (accessibilityFocused) {
        NSRect bounds = [self boundsWithLayout:_container.layoutManager];
        bounds = NSAccessibilityFrameInView(_container.textView, bounds);
        bounds.origin.x += _container.textView.textContainerInset.width;
        bounds.origin.y -= _container.textView.textContainerInset.height;
        self.accessibilityFrame = bounds;
        NSTextView *parent = (NSTextView *)self.accessibilityParent;
        if (parent) {
            [parent becomeFirstResponder];
        }
    }
}

- (NSString *)customA11yLabel {
    NSString *label = _image.accessibilityDescription;
    if (!label.length) {
        label = [NSString stringWithFormat: @"%@ margin image", _alignment == imagealign_MarginLeft ? @"Left" : @"Right"];
        if (_linkid) {
            label = [label stringByAppendingFormat:@" with link I.D. %ld", _linkid];
        }
    }
    return label;
}

- (BOOL)isAccessibilityElement {
    return YES;
}

@end
