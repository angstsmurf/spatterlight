//
//  MarginImage.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MarginImage.h"
#import "MarginContainer.h"
#import "ImageDragSource.h"

#import "DebugLog.h"

#include "glk.h"

@interface MarginImage () <NSSecureCoding> {
    BOOL recalc;
    ImageDragSource *dragSource;
}
@end

@implementation MarginImage

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithImage:(NSImage *)animage
                    alignment:(NSInteger)imageAlignment
                       linkId:(NSUInteger)linkId
                           at:(NSUInteger)apos
                       sender:(id)sender {
    self = [super init];
    if (self) {
        _image = animage;
        _glkImgAlign = imageAlignment;
        _bounds = NSZeroRect;
        _linkid = linkId;
        _pos = apos;
        recalc = YES;
        _container = sender;
        _uuid = [[NSUUID UUID] UUIDString];

        self.accessibilityParent = _container.textView;
        self.accessibilityRoleDescription = self.customA11yLabel;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        _image = [decoder decodeObjectOfClass:[NSImage class] forKey:@"image"];
        _glkImgAlign = [decoder decodeIntegerForKey:@"alignment"];
        _bounds = [decoder decodeRectForKey:@"bounds"];
        _linkid = (NSUInteger)[decoder decodeIntegerForKey:@"linkid"];
        _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
        _uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"uuid"];
        self.accessibilityRoleDescription = [decoder decodeObjectOfClass:[NSString class] forKey:@"accessibilityRoleDescription"];
        recalc = YES;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_image forKey:@"image"];
    [encoder encodeInteger:_glkImgAlign forKey:@"alignment"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
    [encoder encodeInteger:(NSInteger)_linkid forKey:@"linkid"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeObject:_uuid forKey:@"uuid"];
    [encoder encodeObject:self.accessibilityRoleDescription forKey:@"accessibilityRoleDescription"];
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout {
    NSRect theline;
    NSSize size = _image.size;

    if (recalc) {
        recalc = NO; /* don't infiniloop in here, settle for the first result */

        _bounds = NSZeroRect;
        NSTextView *textview = _container.textView;

        if (_pos >= textview.textStorage.length) {
            NSLog(@"MarginImage boundsWithLayout: _pos: %lu textStorage.length: %lu", (unsigned long)_pos, (unsigned long)textview.textStorage.length);
            return NSZeroRect;
        }

        /* get position of anchor glyph. _pos is a character index, so it must
         * be converted to a glyph index first (mirroring FlowBreak): the two
         * diverge as characters that produce no glyph accumulate above the
         * anchor, which otherwise makes the image slip downward into the text. */
        NSRange glyphRange = [layout glyphRangeForCharacterRange:NSMakeRange(_pos, 1)
                                           actualCharacterRange:NULL];
        theline = [layout lineFragmentRectForGlyphAtIndex:glyphRange.location
                                           effectiveRange:nil];

        NSParagraphStyle *para = [textview.textStorage attribute:NSParagraphStyleAttributeName atIndex:_pos effectiveRange:nil];
        theline.origin.y += para.paragraphSpacingBefore;

        /* set bounds to be at the same line as anchor but in left/right margin
         */
        if (_glkImgAlign == imagealign_MarginRight) {
            CGFloat rightMargin = textview.frame.size.width -
                                  textview.textContainerInset.width * 2 -
                                  _container.lineFragmentPadding;
            _bounds = NSMakeRect(rightMargin - size.width, theline.origin.y,
                                 size.width, size.height);

            // If the above places the image outside margin, move it within
            if (NSMaxX(_bounds) > rightMargin) {
                _bounds.origin.x = rightMargin - size.width;
            }
        } else {
            // Anchor to the stable left content edge (mirroring the way the
            // MarginRight branch anchors to the textview's right edge).  We
            // must NOT use theline.origin.x here: lineFragmentRectForProposedRect:
            // shrinks the anchor line's left edge by this image's own width, so
            // reading it back when the fragment is already laid out shifts the
            // image right by roughly its own width.  Only the vertical position
            // is taken from the line fragment.
            _bounds =
                NSMakeRect(_container.lineFragmentPadding,
                           theline.origin.y, size.width, size.height);
        }
    }

    [_container unoverlap:self];
    return _bounds;
}

- (void)uncacheBounds {
    recalc = YES;
}

-(void)cursorUpdate {
    if (_linkid) {
        [NSCursor.pointingHandCursor set];
    } else {
        [NSCursor.arrowCursor set];
    }
}

#pragma mark Dragging

- (void)dragMarginImageFrom:(NSTextView *)source event:(NSEvent *)event filename:(NSString *)filename rect:(NSRect)rect {
    if (!dragSource)
        dragSource = [ImageDragSource new];
    [dragSource dragImage:_image from:source event:event filename:filename frame:rect];
}

#pragma mark Accessibility

// Computed on demand, as the image moves whenever the text is laid out again.
- (NSRect)accessibilityFrame {
    NSTextView *textView = _container.textView;
    if (!textView)
        return super.accessibilityFrame;
    NSRect bounds = [self boundsWithLayout:_container.layoutManager];
    bounds = NSAccessibilityFrameInView(textView, bounds);
    bounds.origin.x += textView.textContainerInset.width;
    bounds.origin.y -= textView.textContainerInset.height;
    return bounds;
}

- (void)setAccessibilityFocused:(BOOL)accessibilityFocused {
    if (accessibilityFocused) {
        NSTextView *parent = (NSTextView *)self.accessibilityParent;
        if (parent) {
            [parent becomeFirstResponder];
        }
    }
}

- (NSString *)customA11yLabel {
    NSString *label = _image.accessibilityDescription;
    if (!label.length) {
        BOOL left = (_glkImgAlign == imagealign_MarginLeft);
        if (_linkid) {
            label = left ? NSLocalizedString(@"Clickable left margin image", nil) : NSLocalizedString(@"Clickable right margin image", nil);
        } else {
            label = left ? NSLocalizedString(@"Left margin image", nil) : NSLocalizedString(@"Right margin image", nil);
        }
    }
    return label;
}

- (BOOL)isAccessibilityElement {
    return YES;
}

@end
