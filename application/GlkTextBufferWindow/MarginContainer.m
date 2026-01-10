//
//  MarginContainer.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MarginContainer.h"
#import "MarginImage.h"
#import "FlowBreak.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#include "glk.h"

@interface MarginContainer () <NSSecureCoding> {
    NSMutableArray<FlowBreak *> *flowbreaks;
}
@end

@implementation MarginContainer

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

- (instancetype)initWithContainerSize:(NSSize)size {
    self = [super initWithContainerSize:size];

    _marginImages = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];

    self.widthTracksTextView = YES;
    self.heightTracksTextView = NO;

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _marginImages = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"marginImages"];
        for (MarginImage *img in _marginImages) {
            img.container = self;
        }
        flowbreaks = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"flowbreaks"];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_marginImages forKey:@"marginImages"];
    [encoder encodeObject:flowbreaks forKey:@"flowbreaks"];
}

- (void)clearImages {
    _marginImages = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];
    NSLog(@"MarginContainer clearImages");
    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)invalidateImagesOnly {
    NSLog(@"MarginContainer invalidateImagesOnly");
    for (MarginImage *i in _marginImages)
        [i uncacheBounds];

    for (FlowBreak *f in flowbreaks)
        [f uncacheBounds];
}

- (void)invalidateLayout:(id)sender {
    if (_pendingInvalidate) {
        NSLog(@"MarginContainer invalidateLayout: blocked by _pendingInvalidate!");
        _pendingInvalidate = NO;
        return;
    }
    NSLog(@"MarginContainer invalidateLayout");
    for (MarginImage *i in _marginImages)
        [i uncacheBounds];

    for (FlowBreak *f in flowbreaks)
        [f uncacheBounds];

    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)forcedUpdateBounds {
    for (MarginImage *i in _marginImages)
        [i forcedUpdateBounds];

    for (FlowBreak *f in flowbreaks)
        [f uncacheBounds];

//    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)addImage:(NSImage *)image
       alignment:(NSInteger)alignment
              at:(NSUInteger)pos
          linkid:(NSUInteger)linkid {
    MarginImage *mi = [[MarginImage alloc] initWithImage:image
                                               alignment:alignment
                                                  linkId:linkid
                                                      at:pos
                                                  sender:self];

    mi.closeImagesBefore = [[NSMutableArray<NSNumber *> alloc] init];
    mi.closeImagesAfter = [[NSMutableArray<NSNumber *> alloc] init];
    mi.closeBreaksBefore = [[NSMutableArray<NSNumber *> alloc] init];
    mi.closeBreaksAfter = [[NSMutableArray<NSNumber *> alloc] init];

    NSUInteger index = 0;
    for (MarginImage *mi2 in _marginImages) {
        if (mi2.pos < mi.pos) { // Should always be true?
            if (mi.pos - mi2.pos < 1000) {
                [mi.closeImagesBefore addObject:@(index)];
                [mi2.closeImagesAfter addObject:@(_marginImages.count)];
            }
        }
        index++;
    }

    index = 0;

    for (FlowBreak *f in flowbreaks) {
        if (f.pos < mi.pos) { // Should always be true?
            if (mi.pos - f.pos < 1000) {
                [mi.closeBreaksBefore addObject:@(index)];
                NSLog(@"addImage: Adding flowbreak %ld to closeBreaksBefore of images %ld", index, _marginImages.count);
                [f.closeImagesAfter addObject:@(_marginImages.count)];
                NSLog(@"addImage: Adding image %ld to closeImagesAfter of flowbreak %ld", _marginImages.count, index);
            }
        }
        index++;
    }


//    mi.bounds = [mi boundsWithLayout:self.layoutManager];
    mi.bounds = NSZeroRect;
    [mi uncacheBounds];

    [_marginImages addObject:mi];
//    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)addFlowBreakAt:(NSUInteger)pos {
    FlowBreak *f = [[FlowBreak alloc] initWithPos:pos];

    f.closeImagesBefore = [[NSMutableArray<NSNumber *> alloc] init];
    f.closeImagesAfter = [[NSMutableArray<NSNumber *> alloc] init];

    NSUInteger index = 0;
    for (MarginImage *mi2 in _marginImages) {
        if (mi2.pos < f.pos) { // Should always be true?
            if (f.pos - mi2.pos < 1000) {
                [f.closeImagesBefore addObject:@(index)];
                NSLog(@"addFlowBreakAt: Adding image %ld to closeImagesBefore of flowbreak %ld", index, flowbreaks.count);
                [mi2.closeBreaksAfter addObject:@(flowbreaks.count)];
                NSLog(@"addFlowBreakAt: Adding flowbreak %ld to closeBreaksAfter of image %ld", flowbreaks.count, index);
            }
        }
        index++;
    }

//    f.bounds = [f boundsWithLayout:self.layoutManager];
    f.bounds = NSZeroRect;
    [f uncacheBounds];

    [flowbreaks addObject:f];
//    [self.layoutManager textContainerChangedGeometry:self];
}

- (BOOL)isSimpleRectangularTextContainer {
    return _marginImages.count == 0;
}

- (NSRect)lineFragmentRectForProposedRect:(NSRect)proposed
                           sweepDirection:(NSLineSweepDirection)sweepdir
                        movementDirection:(NSLineMovementDirection)movementdir
                            remainingRect:(NSRect *)remaining {
    NSRect bounds;

    MarginImage *image, *lastleft, *lastright;
//    FlowBreak *f;

    NSRect rect = [super lineFragmentRectForProposedRect:proposed
                                   sweepDirection:sweepdir
                                movementDirection:movementdir
                                    remainingRect:remaining];

//    NSLog(@"lineFragmentRectForProposedRect");

//    if (_marginImages.count == 0 || self.textView.inLiveResize || bufWin.autorestoring)
   if (_marginImages.count == 0)
        return rect;

    BOOL overlapped = YES;
    NSRect newrect = rect;

    while (overlapped) {
        overlapped = NO;
        lastleft = lastright = nil;

        NSEnumerator *enumerator = [_marginImages reverseObjectEnumerator];
        while (image = [enumerator nextObject]) {
            // I'm not quite sure why, but this prevents the flowbreaks from
            // jumping to incorrect positions when resizing the window
            for (FlowBreak *f in flowbreaks) {
                if (f.pos > image.pos) {
                    if (f.pos - image.pos > 1000)
                        break;
                    [f boundsWithLayout:self.layoutManager];
                }
            }

//            NSLog(@"MarginContainer lineFragmentRectForProposedRect calling boundsWithLayout on image %ld", [_marginImages indexOfObject:image]);
            bounds = [image boundsWithLayout:self.layoutManager];
//            if (NSIsEmptyRect(bounds))
//                continue;

            if (NSIntersectsRect(bounds, newrect)) {
                NSLog(@"MarginContainer lineFragmentRectForProposedRect: image bounds %@ intersects with proposed rect %@", NSStringFromRect(bounds), NSStringFromRect(newrect));
                overlapped = YES;

                newrect = [self adjustForBreaks:newrect];

                BOOL overlapped2 = YES;

                // The call above may have moved the rect down, so we need to
                // check if the image still intersects

                while (overlapped2) // = while (NSIntersectsRect(bounds,
                                    // newrect) || newrect.size.width < 50)
                {
                    overlapped2 = NO;

                    if (NSIntersectsRect(bounds, newrect)) {
                        overlapped2 = YES;
                        if (image.glkImgAlign == imagealign_MarginLeft) {
                            // If we intersect with a left-aligned image, cut
                            // off the left end of the rect

                            newrect.size.width -=
                                (NSMaxX(bounds) - newrect.origin.x);
                            if (NSMaxX(bounds) > newrect.origin.x)
                                NSLog(@"Pushing fragment rect to image right at %f", NSMaxX(bounds));
                            newrect.origin.x = NSMaxX(bounds);
                            lastleft = image;
                        } else // If the image is right-aligned, cut off the
                               // right end of line fragment rect
                        {
                            newrect.size.width =
                                bounds.origin.x - newrect.origin.x;
                            lastright = image;
                        }
                    }
                    if (newrect.size.width <= 50) {
                        overlapped2 = YES;
                        // If the rect has now become too narrow, push it down
                        // and restore original width. 50 is a slightly arbitrary
                        // cutoff width
                        newrect.size.width = rect.size.width; // Original width
                        newrect.origin.x = rect.origin.x;
                        if (lastleft && lastright) {
                            newrect.origin.y = MIN(NSMaxY(lastright.bounds),
                                                   NSMaxY(lastleft.bounds));
                            // If the rect is squeezed between a right-aligned
                            // and a left-aligned image, push it down below the
                            // highest of the two
                        } else {
                            if (lastleft)
                                newrect.origin.y = NSMaxY(lastleft.bounds);
                            else
                                newrect.origin.y = NSMaxY(lastright.bounds);
                        }

                        lastleft = lastright = nil;
                    }
                }
            }
        }
    }
    return newrect;
}

- (NSRect)adjustForBreaks:(NSRect)rect {
    FlowBreak *f;
    NSRect flowrect;
    MarginImage *img2;

    NSEnumerator *breakenumerator = [flowbreaks reverseObjectEnumerator];
    while (f = [breakenumerator nextObject]) {
        if (_marginImages.count == 1) {
            if (f.pos - _marginImages.firstObject.pos > 1000)
                continue;
        }
        flowrect = [f boundsWithLayout:self.layoutManager];

        if (NSIntersectsRect(flowrect, rect)) {
//            NSLog(@"MarginContainer: looking for an image that intersects "
//                  @"flowbreak %ld (%@)",
//                  [_flowbreaks indexOfObject:f], NSStringFromRect(flowrect));

            CGFloat lowest = 0;

//            for (img2 in _marginImages)
            for (NSNumber *idx in f.closeImagesBefore) {
                img2 = _marginImages[idx.unsignedIntValue];
                // Moving below the lowest image drawn before the flowbreak.
                // A flowbreak does not affect images drawn after it.
                if (img2.pos < f.pos && NSMaxY(img2.bounds) > lowest &&
                    NSMaxY(img2.bounds) > rect.origin.y) {
                    lowest = NSMaxY(img2.bounds) + 1;
                }
            }

            if (lowest > 0) {
                rect.origin.y = lowest;
            }
        }
    }

    return rect;
}

- (void)unoverlap:(MarginImage *)image {
    NSLog(@"MarginContainer unoverlap marginImage %ld", [_marginImages indexOfObject:image]);
    // Skip if we only have one image, or none, or bounds are empty
    if (_marginImages.count < 2
        || NSIsEmptyRect(image.bounds)
        || [_marginImages indexOfObject:image] == NSNotFound)
        return;

    NSTextView *textview = self.textView;

    CGFloat leftMargin =
        textview.textContainerInset.width + self.lineFragmentPadding;
    CGFloat rightMargin = textview.frame.size.width -
                          textview.textContainerInset.width * 2 -
                          self.lineFragmentPadding;
    NSRect adjustedBounds = image.bounds;

    // If outside margin, move to opposite margin
    if (image.glkImgAlign == imagealign_MarginLeft &&
        NSMaxX(adjustedBounds) > rightMargin + 1) {
        // Left-aligned image outside right margin
        adjustedBounds.origin.x = self.lineFragmentPadding;
    } else if (image.glkImgAlign == imagealign_MarginRight &&
               adjustedBounds.origin.x < leftMargin - 1) {
        // Right-aligned image outside left margin
        adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
    }

    for (NSInteger i = (NSInteger)[_marginImages indexOfObject:image] - 1; i >= 0; i--) {
        MarginImage *img2 = _marginImages[(NSUInteger)i];
//        if (NSIsEmptyRect(img2.bounds))
//            break;
        // If overlapping, shift in opposite alignment direction
        if (NSIntersectsRect(img2.bounds, adjustedBounds)) {
            NSLog(@"Unoverlap adjusting position of image %ld", i);
            if (image.glkImgAlign == img2.glkImgAlign) {
                if (image.glkImgAlign == imagealign_MarginLeft) {
                    adjustedBounds.origin.x =
                        NSMaxX(img2.bounds) + self.lineFragmentPadding;
                    // Move to the right of image if both left-aligned,
                    // or below and to left margin if we get too close to the
                    // right margin
                    if (NSMaxX(adjustedBounds) > rightMargin + 1) {
                        adjustedBounds.origin.x = self.lineFragmentPadding;
                        adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
                    } else if (img2.bounds.origin.y > adjustedBounds.origin.y)
                        adjustedBounds.origin.y = img2.bounds.origin.y;
                    // Try to keep an even upper edge on images pushed down.
                    // This looks nicer and simplifies calculations
                } else {
                    adjustedBounds.origin.x = img2.bounds.origin.x -
                                              adjustedBounds.size.width -
                                              self.lineFragmentPadding;
                    // Move to the left of image if both right-aligned,
                    // or below and to right margin if we get too close to the
                    // left margin
                    if (adjustedBounds.origin.x < leftMargin - 1) {
                        adjustedBounds.origin.x =
                            rightMargin - adjustedBounds.size.width;
                        adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
                    } else if (img2.bounds.origin.y > adjustedBounds.origin.y)
                        adjustedBounds.origin.y = img2.bounds.origin.y;
                }
            } else {
                // If we have collided with an image of opposite alignment,
                // move below it and back to margin
                if (image.glkImgAlign == imagealign_MarginLeft)
                    adjustedBounds.origin.x = self.lineFragmentPadding;
                else
                    adjustedBounds.origin.x =
                        rightMargin - adjustedBounds.size.width;
                adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
            }
        }
    }

    image.bounds = adjustedBounds;
}

- (void)drawRect:(NSRect)rect {
    BufferTextView *textview = (BufferTextView *)self.textView;
//    if (textview.inLiveResize && _marginImages.count > 20)
//        return;
//    NSLog(@"MarginContainer drawRect: %@", NSStringFromRect(rect));
    NSSize inset = textview.textContainerInset;
    NSSize size;
    NSRect bounds;
    CGFloat extendneeded = 0;

    MarginImage *image;
    NSEnumerator *enumerator = [_marginImages reverseObjectEnumerator];
    while (image = [enumerator nextObject]) {
        bounds = image.bounds;
        if (!NSIsEmptyRect(bounds)) {
            bounds.origin.x += inset.width;
            bounds.origin.y += inset.height;

            // Check if we need to add padding to increase textview height to
            // accommodate for low image
            if (textview.frame.size.height <= NSMaxY(bounds)) {
                textview.bottomPadding =
                    NSMaxY(bounds) - textview.frame.size.height + inset.height;
                extendneeded = textview.bottomPadding;
                [textview setFrameSize:textview.frame.size];
            }
            // Check if padding is still needed
            else if (textview.frame.size.height - textview.bottomPadding <=
                     NSMaxY(bounds)) {
                CGFloat bottom =
                    NSMaxY(bounds) - textview.frame.size.height + inset.height;
                if (extendneeded < bottom)
                    extendneeded = bottom;
            }

            if ([textview needsToDrawRect:bounds]) {
//                NSLog(@"MarginContainer drawRect: drawing margin image at %@ (pos %ld)", NSStringFromRect(image.bounds), image.pos);
                size = image.image.size;
                [image.image
                        drawInRect:bounds
                          fromRect:NSMakeRect(0, 0, size.width, size.height)
                 operation:NSCompositingOperationSourceOver
                          fraction:1.0
                    respectFlipped:YES
                             hints:nil];
            }
        }
    }

    // Remove bottom padding if it is not needed any more
    textview.bottomPadding = extendneeded;
}

- (NSUInteger)findHyperlinkAt:(NSPoint)point {
    for (MarginImage *image in _marginImages) {
        if ([self.textView mouse:point inRect:image.bounds]) {
            NSLog(@"Clicked on image %ld with linkid %ld",
                  [_marginImages indexOfObject:image], image.linkid);
            return image.linkid;
        }
    }
    return 0;
}

- (nullable MarginImage *)marginImageAt:(NSPoint)p {
    for (MarginImage *image in _marginImages) {
        if ([self.textView mouse:p inRect:image.bounds]) {
            return image;
        }
    }
    return nil;
}

- (BOOL)hasMarginImages {
    return (_marginImages.count > 0);
}

- (void)printPos {
    NSUInteger pos = _marginImages.lastObject.pos;
    NSLog(@"MarginContainer: pos of last object: %ld", pos);
    NSRect theline = [self.layoutManager lineFragmentRectForGlyphAtIndex:pos
                                       effectiveRange:nil];
    NSLog(@"MarginImage boundsWithLayout: lineFragmentRectForGlyphAtIndex %ld: %@", pos, NSStringFromRect(theline));

    NSLog(@"MarginContainer: bounds of last object: %@", NSStringFromRect(_marginImages.lastObject.bounds));

    NSLog(@"(Number of margin images: %ld Number of flowbreaks: %lu)", _marginImages.count, (unsigned long)flowbreaks.count);
//    if (self.textView.textStorage.length > pos + 6) {
//        NSString *after = [self.textView.textStorage attributedSubstringFromRange:NSMakeRange(pos + 1, 5)].string;
//        NSLog(@"MarginConatainer: Characters after this: \"%@\"", after);
//    }

}

- (void)pruneImages {
    if (_marginImages.count <= 3)
        return;
    NSArray<MarginImage *> *newImages = [_marginImages subarrayWithRange:NSMakeRange(_marginImages.count - 3, 3)];

//    NSLog(@"MarginContainer pruneImages");
//    [self printPos];
//
//    NSArray<MarginImage *> *newImages = @[_marginImages.lastObject];
    NSUInteger pos = newImages.firstObject.pos;
    NSMutableArray<FlowBreak *> *newBreaks = [[NSMutableArray alloc] initWithCapacity:flowbreaks.count];
    for (FlowBreak *f in flowbreaks) {
        if (f.pos > pos)
            [newBreaks addObject:f];
    }
//    _marginImages = newImages.mutableCopy;
//
//    flowbreaks = newBreaks;

    _marginImages = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];


//    _pendingInvalidate = YES;
//
    [self invalidateImagesOnly];
//    [self.layoutManager ensureLayoutForTextContainer:self];
}

- (MarginContainer *)copyForBackgroundWithSize:(NSSize)size {
    MarginContainer* containercopy = [[MarginContainer alloc] initWithContainerSize:size];
//    if (_marginImages.count <= 10) {
        containercopy.marginImages = _marginImages;
        containercopy->flowbreaks = flowbreaks;
//    } else {
//        containercopy.marginImages = [_marginImages subarrayWithRange:NSMakeRange(_marginImages.count - 10, 10)].mutableCopy;
//        containercopy->flowbreaks = [[NSMutableArray alloc] init];
//        for (FlowBreak *f in flowbreaks) {
//            if (f.pos > containercopy.marginImages.firstObject.pos) {
//                [containercopy->flowbreaks addObject:f];
//            }
//        }
//    }

    return containercopy;
}

@end
