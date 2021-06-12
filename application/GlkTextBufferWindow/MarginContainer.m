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
    NSMutableArray *flowbreaks;
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

- (id)initWithContainerSize:(NSSize)size {
    self = [super initWithContainerSize:size];

    _marginImages = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];

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
    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)invalidateLayout:(id)sender {
    for (MarginImage *i in _marginImages)
        [i uncacheBounds];

    for (FlowBreak *f in flowbreaks)
        [f uncacheBounds];

    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)addImage:(NSImage *)image
           align:(NSInteger)align
              at:(NSUInteger)top
          linkid:(NSUInteger)linkid {
    MarginImage *mi = [[MarginImage alloc] initWithImage:image
                                                   align:align
                                                  linkid:linkid
                                                      at:top
                                                  sender:self];
    [_marginImages addObject:mi];
    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)flowBreakAt:(NSUInteger)pos {
    FlowBreak *f = [[FlowBreak alloc] initWithPos:pos];
    [flowbreaks addObject:f];
    [self.layoutManager textContainerChangedGeometry:self];
}

- (BOOL)isSimpleRectangularTextContainer {
    return _marginImages.count == 0;
}

- (NSRect)lineFragmentRectForProposedRect:(NSRect)proposed
                           sweepDirection:(NSLineSweepDirection)sweepdir
                        movementDirection:(NSLineMovementDirection)movementdir
                            remainingRect:(NSRect *)remaining {
    NSRect bounds;
    NSRect rect;
    MarginImage *image, *lastleft, *lastright;
    FlowBreak *f;

    rect = [super lineFragmentRectForProposedRect:proposed
                                   sweepDirection:sweepdir
                                movementDirection:movementdir
                                    remainingRect:remaining];

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
            for (f in flowbreaks)
                if (f.pos > image.pos)
                    [f boundsWithLayout:self.layoutManager];

            bounds = [image boundsWithLayout:self.layoutManager];

            if (NSIntersectsRect(bounds, newrect)) {
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
                        if (image.alignment == imagealign_MarginLeft) {
                            // If we intersect with a left-aligned image, cut
                            // off the left end of the rect

                            newrect.size.width -=
                                (NSMaxX(bounds) - newrect.origin.x);
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
                        // and restore original width 50 is a slightly arbitrary
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
        flowrect = [f boundsWithLayout:self.layoutManager];

        if (NSIntersectsRect(flowrect, rect)) {
//            NSLog(@"MarginContainer: looking for an image that intersects "
//                  @"flowbreak %ld (%@)",
//                  [flowbreaks indexOfObject:f], NSStringFromRect(flowrect));

            CGFloat lowest = 0;

            for (img2 in _marginImages)
                // Moving below the lowest image drawn before the flowbreak
                // Flowbreaks are not supposed to affect images drawn after it
                if (img2.pos < f.pos && NSMaxY(img2.bounds) > lowest &&
                    NSMaxY(img2.bounds) > rect.origin.y) {
                    lowest = NSMaxY(img2.bounds) + 1;
                }

            if (lowest > 0)
                rect.origin.y = lowest;
        }
    }

    return rect;
}

- (void)unoverlap:(MarginImage *)image {
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
    if (image.alignment == imagealign_MarginLeft &&
        NSMaxX(adjustedBounds) > rightMargin + 1) {
        // NSLog(@"Left-aligned image outside right margin");
        adjustedBounds.origin.x = self.lineFragmentPadding;
    } else if (image.alignment == imagealign_MarginRight &&
               adjustedBounds.origin.x < leftMargin - 1) {
        //  NSLog(@"Right-aligned image outside left margin");
        adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
    }

    for (NSInteger i = (NSInteger)[_marginImages indexOfObject:image] - 1; i >= 0; i--) {
        MarginImage *img2 = _marginImages[(NSUInteger)i];

        // If overlapping, shift in opposite alignment direction
        if (NSIntersectsRect(img2.bounds, adjustedBounds)) {
            if (image.alignment == img2.alignment) {
                if (image.alignment == imagealign_MarginLeft) {
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
                if (image.alignment == imagealign_MarginLeft)
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
    //    NSLog(@"MarginContainer drawRect: %@", NSStringFromRect(rect));

    BufferTextView *textview = (BufferTextView *)self.textView;
    GlkTextBufferWindow *bufwin = (GlkTextBufferWindow *)textview.delegate;
    NSSize inset = textview.textContainerInset;
    NSSize size;
    NSRect bounds;
    BOOL extendflag = NO;
    CGFloat extendneeded = 0;

    MarginImage *image;
    NSEnumerator *enumerator = [_marginImages reverseObjectEnumerator];
    while (image = [enumerator nextObject]) {
        [image boundsWithLayout:self.layoutManager];

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
                extendflag = YES;
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
                size = image.image.size;
                [image.image
                        drawInRect:bounds
                          fromRect:NSMakeRect(0, 0, size.width, size.height)
                 operation:NSCompositeSourceOver
                          fraction:1.0
                    respectFlipped:YES
                             hints:nil];
            }
        }
    }
    // If we were at the bottom before, scroll to bottom of extended area so
    // that we are still at bottom
    if (extendflag && bufwin.scrolledToBottom) {
        NSScrollView *scrollview = textview.enclosingScrollView;
        CGFloat newY = NSMaxY(textview.frame) - NSHeight(scrollview.contentView.bounds);
        NSRect newbounds = scrollview.contentView.bounds;
        newbounds.origin.y = newY;
        scrollview.contentView.bounds = newbounds;
    }

    // Remove bottom padding if it is not needed any more
    textview.bottomPadding = extendneeded;
}

- (NSUInteger)findHyperlinkAt:(NSPoint)p
{
    for (MarginImage *image in _marginImages) {
        if ([self.textView mouse:p inRect:image.bounds]) {
            NSLog(@"Clicked on image %ld with linkid %ld",
                  [_marginImages indexOfObject:image], image.linkid);
            return image.linkid;
        }
    }
    return 0;
}

- (BOOL)hasMarginImages {
    return (_marginImages.count > 0);
}

- (NSMutableAttributedString *)marginsToAttachmentsInString:
    (NSMutableAttributedString *)string {
    NSTextAttachment *att;
    NSFileWrapper *wrapper;
    NSData *tiffdata;
    MarginImage *image;

    NSEnumerator *enumerator = [_marginImages reverseObjectEnumerator];
    while (image = [enumerator nextObject]) {
        tiffdata = image.image.TIFFRepresentation;
        wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:tiffdata];
        wrapper.preferredFilename = @"image.tiff";
        att = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
        NSMutableAttributedString *attstr =
            (NSMutableAttributedString *)[NSMutableAttributedString
                attributedStringWithAttachment:att];

        [string insertAttributedString:attstr atIndex:(NSUInteger)image.pos];
    }
    return string;
}

@end
