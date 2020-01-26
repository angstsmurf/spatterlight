/*
 * NSTextView associated with a glk window
 */

#import "Compatibility.h"
#import "GlkHyperlink.h"
#import "NSString+Categories.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif // DEBUG

@implementation MyAttachmentCell

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)analignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos {
    self = [super initImageCell:image];
    if (self) {
        align = analignment;
        _attrstr = anattrstr;
        pos = apos;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        align = [decoder decodeIntegerForKey:@"align"];
        _attrstr = [decoder decodeObjectForKey:@"attstr"];
        pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:align forKey:@"align"];
    [encoder encodeObject:_attrstr forKey:@"attrstr"];
    [encoder encodeInteger:(NSInteger)pos forKey:@"pos"];
}

- (BOOL)wantsToTrackMouse {
    return NO;
}

- (NSPoint)cellBaselineOffset {
    NSDictionary *attributes = [_attrstr attributesAtIndex:pos
                                            effectiveRange:nil];
    NSFont *font = [attributes objectForKey:NSFontAttributeName];

    CGFloat xheight = font.ascender;

    if (align == imagealign_InlineCenter)
        return NSMakePoint(0, -(self.image.size.height / 2) + font.xHeight / 2);

    if (align == imagealign_InlineDown || align == imagealign_MarginLeft)
        return NSMakePoint(0, -self.image.size.height + xheight);

    return [super cellBaselineOffset];
}

@end

@interface FlowBreak : NSObject {
    BOOL recalc;
}

- (instancetype)initWithPos:(NSUInteger)pos;
- (NSRect)boundsWithLayout:(NSLayoutManager *)layout;

@property NSRect bounds;
@property NSUInteger pos;

@end

@implementation FlowBreak

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

/* ------------------------------------------------------------ */

/*
 * Extend NSTextContainer to handle images in the margins,
 * with the text flowing around them like in HTML.
 */

@interface MarginImage : NSObject {
    BOOL recalc;
}

@property(strong) NSImage *image;
@property(readonly) NSInteger alignment;
@property NSUInteger pos;
@property NSRect bounds;
@property NSUInteger linkid;
@property MarginContainer *container;

- (instancetype)initWithImage:(NSImage *)animage
                        align:(NSInteger)analign
                       linkid:(NSUInteger)linkid
                           at:(NSUInteger)apos
                       sender:(id)sender;

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout;

@end

@implementation MarginImage

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
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _image = [decoder decodeObjectForKey:@"image"];
    _alignment = [decoder decodeIntegerForKey:@"alignment"];
    _bounds = [decoder decodeRectForKey:@"bounds"];
    _linkid = (NSUInteger)[decoder decodeIntegerForKey:@"linkid"];
    _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
    recalc = [decoder decodeBoolForKey:@"recalc"];
    ;

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_image forKey:@"image"];
    [encoder encodeInteger:_alignment forKey:@"alignment"];
    [encoder encodeRect:_bounds forKey:@"bounds"];
    [encoder encodeInteger:(NSInteger)_linkid forKey:@"linkid"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeBool:recalc forKey:@"recalc"];
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

@end

/* ------------------------------------------------------------ */

@implementation MarginContainer

- (id)initWithContainerSize:(NSSize)size {
    self = [super initWithContainerSize:size];

    margins = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        margins = [decoder decodeObjectForKey:@"margins"];
        for (MarginImage *img in margins)
            img.container = self;
        flowbreaks = [decoder decodeObjectForKey:@"flowbreaks"];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:margins forKey:@"margins"];
    [encoder encodeObject:flowbreaks forKey:@"flowbreaks"];
}

- (void)clearImages {
    margins = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];
    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)invalidateLayout {
    for (MarginImage *i in margins)
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
    [margins addObject:mi];
    [self.layoutManager textContainerChangedGeometry:self];
}

- (void)flowBreakAt:(NSUInteger)pos {
    FlowBreak *f = [[FlowBreak alloc] initWithPos:pos];
    [flowbreaks addObject:f];
    [self.layoutManager textContainerChangedGeometry:self];
}

- (BOOL)isSimpleRectangularTextContainer {
    return margins.count == 0;
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

    if (margins.count == 0)
        return rect;

    BOOL overlapped = YES;
    NSRect newrect = rect;

    while (overlapped) {
        overlapped = NO;
        lastleft = lastright = nil;

        NSEnumerator *enumerator = [margins reverseObjectEnumerator];
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
            NSLog(@"MarginContainer: looking for an image that intersects "
                  @"flowbreak %ld (%@)",
                  [flowbreaks indexOfObject:f], NSStringFromRect(flowrect));

            CGFloat lowest = 0;

            for (img2 in margins)
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
    if (margins.count < 2 || NSIsEmptyRect(image.bounds))
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

    for (NSInteger i = (NSInteger)[margins indexOfObject:image] - 1; i >= 0; i--) {
        MarginImage *img2 = [margins objectAtIndex:(NSUInteger)i];

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

    MyTextView *textview = (MyTextView *)self.textView;
    GlkTextBufferWindow *bufwin = (GlkTextBufferWindow *)textview.delegate;
    NSSize inset = textview.textContainerInset;
    NSSize size;
    NSRect bounds;
    BOOL extendflag = NO;
    CGFloat extendneeded = 0;

    MarginImage *image;
    NSEnumerator *enumerator = [margins reverseObjectEnumerator];
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

            if (NSIntersectsRect(bounds, rect)) {
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
    if (extendflag && bufwin.scrolledToBottom)
        [bufwin scrollToBottom];

    // Remove bottom padding if it is not needed any more
    textview.bottomPadding = extendneeded;
}

- (NSUInteger)findHyperlinkAt:(NSPoint)p;
{
    for (MarginImage *image in margins) {
        if ([self.textView mouse:p inRect:image.bounds]) {
            NSLog(@"Clicked on image %ld with linkid %ld",
                  [margins indexOfObject:image], image.linkid);
            return image.linkid;
        }
    }
    return 0;
}

- (BOOL)hasMarginImages {
    return (margins.count > 0);
}

- (NSMutableAttributedString *)marginsToAttachmentsInString:
    (NSMutableAttributedString *)string {
    NSTextAttachment *att;
    NSFileWrapper *wrapper;
    NSData *tiffdata;
    MarginImage *image;

    NSEnumerator *enumerator = [margins reverseObjectEnumerator];
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

/* ------------------------------------------------------------ */

/*
 * Extend NSTextView to ...
 *   - call onKeyDown on our TextBuffer object
 *   - draw images with high quality interpolation
 */

@implementation MyTextView

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _bottomPadding = [decoder decodeDoubleForKey:@"bottomPadding"];
        _shouldSpeak_10_7 = [decoder decodeBoolForKey:@"shouldSpeak_10_7"];
        NSValue *rangeVal = [decoder decodeObjectForKey:@"rangeToSpeak_10_7"];
        _rangeToSpeak_10_7 = rangeVal.rangeValue;
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeDouble:_bottomPadding forKey:@"bottomPadding"];
    [encoder encodeBool:_shouldSpeak_10_7 forKey:@"shouldSpeak_10_7"];
    NSValue *rangeVal = [NSValue valueWithRange:_rangeToSpeak_10_7];
    [encoder encodeObject:rangeVal forKey:@"rangeToSpeak_10_7"];
}

- (void)superKeyDown:(NSEvent *)evt {
    [super keyDown:evt];
}

- (void)keyDown:(NSEvent *)evt {
    [(GlkTextBufferWindow *)self.delegate onKeyDown:evt];
}

- (void)drawRect:(NSRect)rect {
    [NSGraphicsContext currentContext].imageInterpolation =
        NSImageInterpolationHigh;
    [super drawRect:rect];
    [(MarginContainer *)self.textContainer drawRect:rect];
}

- (void)mouseDown:(NSEvent *)theEvent {
    if (![(GlkTextBufferWindow *)self.delegate myMouseDown:theEvent])
        [super mouseDown:theEvent];
}

- (BOOL)shouldDrawInsertionPoint {
    BOOL result = super.shouldDrawInsertionPoint;

    // Never draw a caret if the system doesn't want to. Super overrides
    // glkTextBuffer.
    if (result && !_shouldDrawCaret)
        result = _shouldDrawCaret;

    return result;
}

- (void)enableCaret:(id)sender {
    _shouldDrawCaret = YES;
}

- (void)temporarilyHideCaret {
    _shouldDrawCaret = NO;
    [NSTimer scheduledTimerWithTimeInterval:0.05
                                     target:self
                                   selector:@selector(enableCaret:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)setFrameSize:(NSSize)newSize {
    // NSLog(@"MyTextView setFrameSize: %@ Old size: %@",
    // NSStringFromSize(newSize), NSStringFromSize(self.frame.size));

    newSize.height += _bottomPadding;
    [super setFrameSize:newSize];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    BOOL isValidItem = NO;
    BOOL waseditable = self.editable;
    self.editable = NO;
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;

    if (menuItem.action == @selector(cut:)) {
        if (self.selectedRange.length &&
            [delegate textView:self
                shouldChangeTextInRange:self.selectedRange
                      replacementString:nil])
            self.editable = waseditable;
    }

    else if (menuItem.action == @selector(paste:)) {
        if ([delegate textView:self
                shouldChangeTextInRange:self.selectedRange
                      replacementString:nil])
            self.editable = waseditable;
    }

    if (menuItem.action == @selector(performTextFinderAction:))
        isValidItem = [self.textFinder validateAction:menuItem.tag];

    // validate other menu items if needed
    // ...
    // and don't forget to call the superclass
    else {
        isValidItem = [super validateMenuItem:menuItem];
    }

    self.editable = waseditable;

    return isValidItem;
}

#pragma mark Text Finder

- (NSTextFinder *)textFinder {
    // Create the text finder on demand
    if (_textFinder == nil) {
        _textFinder = [[NSTextFinder alloc] init];
        _textFinder.client = self;
        _textFinder.findBarContainer = self.enclosingScrollView;
        _textFinder.incrementalSearchingEnabled = YES;
        _textFinder.incrementalSearchingShouldDimContentView = NO;
    }

    return _textFinder;
}

- (void)resetTextFinder {
    if (_textFinder != nil) {
        [_textFinder noteClientStringWillChange];
    }
}

// This is where the commands are actually sent to the text finder
- (void)performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender {
    BOOL waseditable = self.editable;
    self.editable = NO;
    [self.textFinder performAction:sender.tag];
    self.editable = waseditable;
}

#pragma mark Accessibility

- (id)accessibilityAttributeValue:(NSString *)attribute {
    // NSLog(@"MyTextView: accessibilityAttributeValue: %@",attribute);

    NSResponder *firstResponder = self.window.firstResponder;

    if (_shouldSpeak_10_7) {
        if (NSMaxRange(_rangeToSpeak_10_7) != self.textStorage.length)
            _rangeToSpeak_10_7 = NSMakeRange(_rangeToSpeak_10_7.location,
                                             self.textStorage.length -
                                                 _rangeToSpeak_10_7.location);
    }

    if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
        NSString *selectedText = nil;

        if (_shouldSpeak_10_7) {
            if (NSMaxRange(_rangeToSpeak_10_7) <= self.textStorage.length)
                return [self.textStorage.string
                    substringWithRange:_rangeToSpeak_10_7];
        }

        if (self.selectedRanges)
            selectedText = [self.textStorage.string
                substringWithRange:((NSValue *)[self.selectedRanges
                                        objectAtIndex:0])
                                       .rangeValue];

        return (selectedText && selectedText.length)
                   ? selectedText
                   : [self.textStorage.string
                         substringWithRange:
                             ((NSValue *)[super
                                  accessibilityAttributeValue:
                                      NSAccessibilityVisibleCharacterRangeAttribute])
                                 .rangeValue];
    } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
        return
            [NSNumber numberWithBool:firstResponder == self ||
                                     firstResponder ==
                                         (GlkTextBufferWindow *)self.delegate];

    if (_shouldSpeak_10_7 &&
        NSMaxRange(_rangeToSpeak_10_7) <= self.textStorage.length) {
        if ([attribute
                isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
            return [NSValue valueWithRange:_rangeToSpeak_10_7];

        if ([attribute
                isEqualToString:NSAccessibilitySelectedTextRangesAttribute]) {
            NSArray *ranges = @[ [NSValue valueWithRange:_rangeToSpeak_10_7] ];
            return ranges;
        }
    }

    // NSLog (@"Result: %@",[super accessibilityAttributeValue:attribute]);
    return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityAttributeValue:(NSString *)attribute
                     forParameter:(id)parameter {
    // NSLog(@"MyTextView: accessibilityAttributeValue: %@ forParameter:
    // %@",attribute, parameter);
    if (_shouldSpeak_10_7 &&
        NSMaxRange(_rangeToSpeak_10_7) <= self.textStorage.length) {
        if ([attribute isEqualToString:
                           NSAccessibilityLineForIndexParameterizedAttribute])
            return nil;

        if ([attribute isEqualToString:
                           NSAccessibilityRangeForLineParameterizedAttribute])
            return nil;

        if ([attribute
                isEqualToString:
                    NSAccessibilityAttributedStringForRangeParameterizedAttribute]) {
            if (!_rangeToSpeak_10_7.length)
                return @"";
            // NSLog (@"Result: %@",[self.textStorage
            // attributedSubstringFromRange:_rangeToSpeak_10_7]);

            return [self.textStorage
                attributedSubstringFromRange:_rangeToSpeak_10_7];
        }
    }

    if ([attribute isEqualToString:
                       NSAccessibilityRangeForLineParameterizedAttribute]) {
        NSLayoutManager *layoutManager = self.layoutManager;
        unsigned numberOfLines, index;
        unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
        NSRange lineRange;

        for (numberOfLines = 0, index = 0; index < numberOfGlyphs;
             numberOfLines++) {
            (void)[layoutManager lineFragmentRectForGlyphAtIndex:index
                                                  effectiveRange:&lineRange];

            if (((NSNumber *)(parameter)).intValue ==
                (NSInteger)numberOfLines) {
                // NSLog(@"Result: %@ (%@)",NSStringFromRange(lineRange),
                // [self.textStorage.string substringWithRange:lineRange]);
                return [NSValue valueWithRange:lineRange];
            }

            index = NSMaxRange(lineRange);
        }
        return nil;
    }

    // NSLog(@"Result: %@",[super accessibilityAttributeValue:attribute
    // forParameter:parameter]);
    return [super accessibilityAttributeValue:attribute forParameter:parameter];
}

- (NSString *)accessibilityActionDescription:(NSString *)action {
    if ([action isEqualToString:@"Repeat last move"])
        return @"Read the output of the last command entered";
    if ([action isEqualToString:@"Speak move before"])
        return @"Read the output of the command before the last one read";
    if ([action isEqualToString:@"Speak move after"])
        return @"Read the output of the command after the last one read";
    if ([action isEqualToString:@"Speak status bar"])
        return @"Read the text of the status bar";

    return [super accessibilityActionDescription:action];
}

- (NSArray *)accessibilityActionNames {
    NSMutableArray *result = [[super accessibilityActionNames] mutableCopy];

    [result addObjectsFromArray:@[
        @"Repeat last move", @"Speak move before", @"Speak move after",
        @"Speak status bar"
    ]];

    return result;
}

- (void)accessibilityPerformAction:(NSString *)action {
    NSLog(@"GlkTextBufferWindow: accessibilityPerformAction. %@", action);

    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;

    if ([action isEqualToString:@"Repeat last move"])
        [delegate speakMostRecent:nil];
    else if ([action isEqualToString:@"Speak move before"])
        [delegate speakPrevious:nil];
    else if ([action isEqualToString:@"Speak move after"])
        [delegate speakNext:nil];
    else if ([action isEqualToString:@"Speak status bar"])
        [delegate speakStatus:nil];
    else
        [super accessibilityPerformAction:action];
}

@end

/* ------------------------------------------------------------ */

/*
 * Controller for the various objects required in the NSText* mess.
 */

@implementation GlkTextBufferWindow

- (id)initWithGlkController:(GlkController *)glkctl_ name:(NSInteger)name_ {

    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        NSInteger marginX = self.theme.bufferMarginX;
        NSInteger marginY = self.theme.bufferMarginY;

        NSUInteger i;

        NSDictionary *styleDict = nil;
        self.styleHints = self.glkctl.bufferStyleHints;

        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        for (i = 0; i < style_NUMSTYLES; i++) {
            if (self.theme.doStyles) {
                styleDict = [((GlkStyle *)[self.theme valueForKey:[gBufferStyleNames objectAtIndex:i]]) attributesWithHints:[self.styleHints objectAtIndex:i]];
            } else {
                styleDict = ((GlkStyle *)[self.theme valueForKey:[gBufferStyleNames objectAtIndex:i]]).attributeDict;
            }
            if (!styleDict) {
                NSLog(@"GlkTextBufferWindow couldn't create style dict for style %ld", i);
                [styles addObject:[NSNull null]];
            } else {
                [styles addObject:styleDict];
            }
        }
        
        echo = YES;

        _lastchar = '\n';

        // We use lastLineheight to restore scroll position with sub-character size precision
        // after a resize
        lastLineheight = self.theme.bufferNormal.font.boundingRectForFont.size.height;

        for (i = 0; i < HISTORYLEN; i++)
            history[i] = nil;

        hyperlinks = [[NSMutableArray alloc] init];
        moveRanges = [[NSMutableArray alloc] init];
        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];

        [self restoreScrollBarStyle];

        /* construct text system manually */

        textstorage = [[NSTextStorage alloc] init];

        layoutmanager = [[NSLayoutManager alloc] init];
        [textstorage addLayoutManager:layoutmanager];

        container = [[MarginContainer alloc]
            initWithContainerSize:NSMakeSize(0, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        textview =
            [[MyTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000)
                                textContainer:container];

        textview.minSize = NSMakeSize(1, 10000000);
        textview.maxSize = NSMakeSize(10000000, 10000000);

        container.textView = textview;

        scrollview.documentView = textview;

        /* now configure the text stuff */

        container.widthTracksTextView = YES;
        container.heightTracksTextView = NO;

        textview.horizontallyResizable = NO;
        textview.verticallyResizable = YES;

        textview.autoresizingMask = NSViewWidthSizable;

        textview.allowsImageEditing = NO;
        textview.allowsUndo = NO;
        textview.usesFontPanel = NO;
        textview.usesFindBar = YES;
        textview.incrementalSearchingEnabled = YES;

        textview.smartInsertDeleteEnabled = NO;

        textview.delegate = self;
        textstorage.delegate = self;

        textview.textContainerInset = NSMakeSize(marginX, marginY);
        textview.backgroundColor = self.theme.bufferBackground;
        textview.insertionPointColor = self.theme.bufferNormal.color;

        NSMutableDictionary *linkAttributes = [textview.linkTextAttributes mutableCopy];
        [linkAttributes setObject:[[styles objectAtIndex:style_Normal] objectForKey:NSForegroundColorAttributeName] forKey:NSForegroundColorAttributeName];
        textview.linkTextAttributes = linkAttributes;

        [textview enableCaret:nil];

        // disabling screen fonts will force font smoothing and kerning.
        // using screen fonts will render ugly and uneven text and sometimes
        // even bitmapped fonts.
        //layoutmanager.usesScreenFonts = [Preferences useScreenFonts];

        [self addSubview:scrollview];
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        NSUInteger i;
        textview = [decoder decodeObjectForKey:@"textview"];
        layoutmanager = textview.layoutManager;
        textstorage = textview.textStorage;
        container = (MarginContainer *)textview.textContainer;
        if (!layoutmanager)
            NSLog(@"layoutmanager nil!");
        if (!textstorage)
            NSLog(@"textstorage nil!");
        if (!container)
            NSLog(@"container nil!");
        scrollview = textview.enclosingScrollView;
        if (!scrollview)
            NSLog(@"scrollview nil!");

        scrollview.documentView = textview;
        textview.delegate = self;
        textstorage.delegate = self;

        if (textview.textStorage != textstorage)
            NSLog(@"Error! textview.textStorage != textstorage");

        scrollview.backgroundColor = self.theme.bufferBackground;

        [self restoreScrollBarStyle];

        line_request = [decoder decodeBoolForKey:@"line_request"];
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];

        echo_toggle_pending = [decoder decodeBoolForKey:@"echo_toggle_pending"];
        echo = [decoder decodeBoolForKey:@"echo"];

        fence = (NSUInteger)[decoder decodeIntegerForKey:@"fence"];

        NSMutableArray *historyarray = [decoder decodeObjectForKey:@"history"];

        for (i = 0; i < historyarray.count; i++) {
            history[i] = [historyarray objectAtIndex:i];
        }

        while (++i < HISTORYLEN) {
            history[i] = nil;
        }
        historypos = [decoder decodeIntegerForKey:@"historypos"];
        historyfirst = [decoder decodeIntegerForKey:@"historyfirst"];
        historypresent = [decoder decodeIntegerForKey:@"historypresent"];
        moveRanges = [decoder decodeObjectForKey:@"moveRanges"];
        moveRangeIndex = (NSUInteger)[decoder decodeIntegerForKey:@"moveRangeIndex"];
        _lastchar = [decoder decodeIntegerForKey:@"lastchar"];
        _lastseen = [decoder decodeIntegerForKey:@"lastseen"];
        _restoredSelection =
            ((NSValue *)[decoder decodeObjectForKey:@"selectedRange"])
                .rangeValue;
        textview.selectedRange = _restoredSelection;
        _restoredAtBottom = [decoder decodeBoolForKey:@"scrolledToBottom"];
        _restoredLastVisible = (NSUInteger)[decoder decodeIntegerForKey:@"lastVisible"];
        _restoredScrollOffset = [decoder decodeDoubleForKey:@"scrollOffset"];

        textview.insertionPointColor =
            [decoder decodeObjectForKey:@"insertionPointColor"];
        textview.shouldDrawCaret =
            [decoder decodeBoolForKey:@"shouldDrawCaret"];
        _restoredSearch = [decoder decodeObjectForKey:@"searchString"];
        _restoredFindBarVisible = [decoder decodeBoolForKey:@"findBarVisible"];
        [self destroyTextFinder];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:textview forKey:@"textview"];
    NSValue *rangeVal = [NSValue valueWithRange:textview.selectedRange];
    [encoder encodeObject:rangeVal forKey:@"selectedRange"];
    [encoder encodeBool:line_request forKey:@"line_request"];
    [encoder encodeBool:hyper_request forKey:@"hyper_request"];
    [encoder encodeBool:echo_toggle_pending forKey:@"echo_toggle_pending"];
    [encoder encodeBool:echo forKey:@"echo"];
    [encoder encodeInteger:(NSInteger)fence forKey:@"fence"];
    NSMutableArray *historyarray = [[NSMutableArray alloc] init];
    for (NSInteger i = 0; i < HISTORYLEN; i++) {
        if (history[i])
            [historyarray addObject:history[i]];
        else
            break;
    }
    [encoder encodeObject:historyarray forKey:@"history"];
    [encoder encodeInteger:historypos forKey:@"historypos"];
    [encoder encodeInteger:historyfirst forKey:@"historyfirst"];
    [encoder encodeInteger:historypresent forKey:@"historypresent"];
    [encoder encodeObject:moveRanges forKey:@"moveRanges"];
    [encoder encodeInteger:(NSInteger)moveRangeIndex forKey:@"moveRangeIndex"];
    [encoder encodeInteger:_lastchar forKey:@"lastchar"];
    [encoder encodeInteger:_lastseen forKey:@"lastseen"];
    [encoder encodeRect:scrollview.documentVisibleRect forKey:@"visibleRect"];
    [self storeScrollOffset];
    [encoder encodeBool:lastAtBottom forKey:@"scrolledToBottom"];
    [encoder encodeInteger:(NSInteger)lastVisible forKey:@"lastVisible"];
    [encoder encodeDouble:lastScrollOffset forKey:@"scrollOffset"];
    [encoder encodeObject:textview.insertionPointColor
                   forKey:@"insertionPointColor"];
    [encoder encodeBool:textview.shouldDrawCaret forKey:@"shouldDrawCaret"];
    NSSearchField *searchField = [self findSearchFieldIn:self];
    if (searchField) {
        [encoder encodeObject:searchField.stringValue forKey:@"searchString"];
    }
    [encoder encodeBool:scrollview.findBarVisible forKey:@"findBarVisible"];
}

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

- (void)changeDocumentBackgroundColor:(id)sender {
    NSLog(@"changeDocumentBackgroundColor");
}

- (void)recalcBackground {
    NSColor *bgcolor, *fgcolor;

    bgcolor = nil;
    fgcolor = nil;

    if (self.theme.doStyles) {
        NSDictionary *attributes = [styles objectAtIndex:style_Normal];
        bgcolor = [attributes objectForKey:NSBackgroundColorAttributeName];
        fgcolor = [attributes objectForKey:NSForegroundColorAttributeName];
    }

    if (!bgcolor)
        bgcolor = self.theme.bufferBackground;

    if (!fgcolor)
        fgcolor = self.theme.bufferNormal.color;

    if (!fgcolor || !bgcolor) {
        NSLog(@"GlkTexBufferWindow recalcBackground: color error!");
    }

    textview.backgroundColor = bgcolor;
    textview.insertionPointColor = fgcolor;

    [self.glkctl setBorderColor:bgcolor];
}

- (void)prefsDidChange {
    NSRange range = NSMakeRange(0, 0);
    NSRange linkrange = NSMakeRange(0, 0);
    NSUInteger x;
    NSDictionary *attributes;

    [self storeScrollOffset];

    styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {

        if (self.theme.doStyles) {
            attributes = [((GlkStyle *)[self.theme valueForKey:[gBufferStyleNames objectAtIndex:i]]) attributesWithHints:[self.styleHints objectAtIndex:i]];
        } else {
            //We're not doing styles, so use the raw style attributes
            attributes = ((GlkStyle *)[self.theme valueForKey:[gBufferStyleNames objectAtIndex:i]]).attributeDict;
        }

        if (attributes)
            [styles addObject:[attributes copy]];
        else
            [styles addObject:[NSNull null]];
    }

    NSInteger marginX = self.theme.bufferMarginX;
    NSInteger marginY = self.theme.bufferMarginX;
    textview.textContainerInset = NSMakeSize(marginX, marginY);

    [self recalcBackground];

    [textstorage removeAttribute:NSBackgroundColorAttributeName
                            range:NSMakeRange(0, textstorage.length)];

    /* reassign attribute dictionaries */
    x = 0;
    while (x < textstorage.length) {
        id styleobject = [textstorage attribute:@"GlkStyle"
                                         atIndex:x
                                  effectiveRange:&range];

        attributes = [styles objectAtIndex:(NSUInteger)[styleobject intValue]];
        if ([attributes isEqual:[NSNull null]]) {
            NSLog(@"Error! broken style (%@)", styleobject);
        }

        id image = [textstorage attribute:@"NSAttachment"
                                   atIndex:x
                            effectiveRange:NULL];
        id hyperlink = [textstorage attribute:NSLinkAttributeName
                                       atIndex:x
                                effectiveRange:&linkrange];
        @try {
            [textstorage setAttributes:attributes range:range];
        }
        @catch (NSException *exception) {
            NSLog(@"GlkTextBufferWindow prefsDidChange: exception:%@ attributes:%@", exception, attributes);
        }
        @finally {
        }

        if (image) {
            ((MyAttachmentCell *)((NSTextAttachment *)image).attachmentCell).attrstr = textstorage;
            [textstorage addAttribute:@"NSAttachment"
                                 value:image
                                 range:NSMakeRange(x, 1)];
        }

        if (hyperlink) {
            [textstorage addAttribute:NSLinkAttributeName
                                 value:hyperlink
                                 range:linkrange];
        }

        x = NSMaxRange(range);
    }

//    layoutmanager.usesScreenFonts = [Preferences useScreenFonts];

    NSMutableDictionary *linkAttributes = [textview.linkTextAttributes mutableCopy];
    [linkAttributes setObject:[[styles objectAtIndex:style_Normal] objectForKey:NSForegroundColorAttributeName] forKey:NSForegroundColorAttributeName];
    textview.linkTextAttributes = linkAttributes;
    
    [container invalidateLayout];
    [self restoreScroll];
    lastLineheight = self.theme.bufferNormal.font.boundingRectForFont.size.height;
}

- (void)setFrame:(NSRect)frame {
    //    NSLog(@"GlkTextBufferWindow %ld: setFrame: %@", self.name,
    //    NSStringFromRect(frame));

    if (NSEqualRects(frame, self.frame)) {
//        NSLog(@"GlkTextBufferWindow setFrame: new frame same as old frame. "
//              @"Skipping.");
        return;
    }

    [self storeScrollOffset];
    super.frame = frame;
    [container invalidateLayout];
    [self restoreScroll];
}

- (void)saveAsRTF:(id)sender {
    NSWindow *window = self.glkctl.window;
    BOOL isRtfd = NO;
    NSString *newExtension = @"rtf";
    if (textstorage.containsAttachments || [container hasMarginImages]) {
        newExtension = @"rtfd";
        isRtfd = YES;
    }
    NSString *newName = [window.title.stringByDeletingPathExtension
        stringByAppendingPathExtension:newExtension];

    // Set the default name for the file and show the panel.

    NSSavePanel *panel = [NSSavePanel savePanel];
    //[panel setNameFieldLabel: @"Save Scrollback: "];
    panel.nameFieldLabel = @"Save Text: ";
    panel.allowedFileTypes = @[ newExtension ];
    panel.extensionHidden = NO;
    [panel setCanCreateDirectories:YES];

    panel.nameFieldStringValue = newName;
    NSTextView *localTextView = textview;
    NSAttributedString *localTextStorage = textstorage;
    MarginContainer *localTextContainer = container;
    [panel
        beginSheetModalForWindow:window
               completionHandler:^(NSInteger result) {
                   if (result == NSFileHandlingPanelOKButton) {
                       NSURL *theFile = panel.URL;

                       NSMutableAttributedString *mutattstr =
                           [localTextStorage mutableCopy];

                       mutattstr = [localTextContainer
                           marginsToAttachmentsInString:mutattstr];

                       [mutattstr
                           addAttribute:NSBackgroundColorAttributeName
                                  value:localTextView.backgroundColor
                                  range:NSMakeRange(0, mutattstr.length)];

                       if (isRtfd) {
                           NSFileWrapper *wrapper;
                           wrapper = [mutattstr
                               RTFDFileWrapperFromRange:NSMakeRange(
                                                            0, mutattstr.length)
                                     documentAttributes:@{
                                         NSDocumentTypeDocumentAttribute :
                                             NSRTFDTextDocumentType
                                     }];

                           [wrapper writeToURL:theFile
                                           options:
                                               NSFileWrapperWritingAtomic |
                                               NSFileWrapperWritingWithNameUpdating
                               originalContentsURL:nil
                                             error:NULL];

                       } else {
                           NSData *data;
                           data = [mutattstr
                                     RTFFromRange:NSMakeRange(0,
                                                              mutattstr.length)
                               documentAttributes:@{
                                   NSDocumentTypeDocumentAttribute :
                                       NSRTFTextDocumentType
                               }];
                           [data writeToURL:theFile atomically:NO];
                       }
                   }
               }];
}

- (void)saveHistory:(NSString *)line {
    if (history[historypresent]) {
        history[historypresent] = nil;
    }

    history[historypresent] = line;

    historypresent++;
    if (historypresent >= HISTORYLEN)
        historypresent -= HISTORYLEN;

    if (historypresent == historyfirst) {
        historyfirst++;
        if (historyfirst > HISTORYLEN)
            historyfirst -= HISTORYLEN;
    }

    if (history[historypresent]) {
        history[historypresent] = nil;
    }
}

- (void)travelBackwardInHistory {
    [textview resetTextFinder];

    NSString *cx;

    if (historypos == historyfirst)
        return;

    if (historypos == historypresent) {
        /* save the edited line */
        if (textstorage.length - fence > 0)
            cx = [textstorage.string substringFromIndex:fence];
        else
            cx = nil;
        history[historypos] = cx;
    }

    historypos--;
    if (historypos < 0)
        historypos += HISTORYLEN;

    cx = history[historypos];
    if (!cx)
        cx = @"";

    [textstorage
        replaceCharactersInRange:NSMakeRange(fence, textstorage.length - fence)
                      withString:cx];
}

- (void)travelForwardInHistory {
    [textview resetTextFinder];

    NSString *cx;

    if (historypos == historypresent)
        return;

    historypos++;
    if (historypos >= HISTORYLEN)
        historypos -= HISTORYLEN;

    cx = history[historypos];
    if (!cx)
        cx = @"";

    [textstorage
        replaceCharactersInRange:NSMakeRange(fence, textstorage.length - fence)
                      withString:cx];
}

- (void)onKeyDown:(NSEvent *)evt {
    GlkEvent *gev;
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:0]);

    NSUInteger flags = evt.modifierFlags;

    GlkWindow *win;

    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (win in [self.glkctl.gwindows allValues]) {
            if (win != self && win.wantsFocus) {
                [win grabFocus];
                NSLog(@"GlkTextBufferWindow: Passing on keypress");
                if ([win isKindOfClass:[GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)win onKeyDown:evt];
                else
                    [win keyDown:evt];
                return;
            }
        }

    BOOL commandKeyOnly = ((flags & NSCommandKeyMask) &&
                           !(flags & (NSAlternateKeyMask | NSShiftKeyMask |
                                      NSControlKeyMask | NSHelpKeyMask)));
    BOOL optionKeyOnly = ((flags & NSAlternateKeyMask) &&
                          !(flags & (NSCommandKeyMask | NSShiftKeyMask |
                                     NSControlKeyMask | NSHelpKeyMask)));

    if (ch == keycode_Up) {
        if (optionKeyOnly)
            ch = keycode_PageUp;
        else if (commandKeyOnly)
            ch = keycode_Home;
    } else if (ch == keycode_Down) {
        if (optionKeyOnly)
            ch = keycode_PageDown;
        else if (commandKeyOnly)
            ch = keycode_End;
    }
    //    else if (ch == keycode_Left)
    //    {
    //        if ((flags & (NSAlternateKeyMask | NSCommandKeyMask)) && !(flags &
    //        (NSShiftKeyMask | NSControlKeyMask | NSHelpKeyMask)))
    //        {
    //            NSLog(@"Pressed keyboard shortcut for speakMostRecent!");
    //            [self speakMostRecent:nil];
    //            return;
    //        }
    //    }
    else if (([str isEqualToString:@"f"] || [str isEqualToString:@"F"]) &&
             commandKeyOnly) {
        if (!scrollview.findBarVisible) {
            [self restoreTextFinder];
            return;
        }
    }

    NSNumber *key = @(ch);

    // if not scrolled to the bottom, pagedown or navigate scrolling on each key
    // instead
    if (NSMaxY(textview.visibleRect) <
        NSMaxY(textview.bounds) - 5 - textview.bottomPadding) {
        NSRect promptrect = [layoutmanager
            lineFragmentRectForGlyphAtIndex:textstorage.length - 1
                             effectiveRange:nil];

        // Skip if we are scrolled to input prompt
        if (NSMaxY(textview.visibleRect) < NSMaxY(promptrect)) {
            switch (ch) {
            case keycode_PageUp:
            case keycode_Delete:
                [textview scrollPageUp:nil];
                return;
            case keycode_PageDown:
            case ' ':
                [textview scrollPageDown:nil];
                return;
            case keycode_Up:
                [textview scrollLineUp:nil];
                return;
            case keycode_Down:
            case keycode_Return:
                [textview scrollLineDown:nil];
                return;
            default:
                [self performScroll];
                break;
            }
        }
    }

    if (char_request && ch != keycode_Unknown) {
//        NSLog(@"char event from %ld", (long)self.name);

        //[textview setInsertionPointColor:[Preferences bufferForeground]];

        [self.glkctl markLastSeen];

        gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [self.glkctl queueEvent:gev];

        char_request = NO;
        [textview setEditable:NO];

    } else if (line_request &&
               (ch == keycode_Return ||
                [[currentTerminators objectForKey:key] isEqual:@(YES)])) {
        // NSLog(@"line event from %ld", (long)self.name);

        [textview resetTextFinder];

        textview.insertionPointColor = self.theme.bufferBackground;

        [self.glkctl markLastSeen];

        NSString *line = [textstorage.string substringFromIndex:fence];
        if (echo) {
            [self printToWindow:@"\n"
                          style:style_Input]; // XXX arranger lastchar needs to
                                              // be set
            _lastchar = '\n';
        } else
            [textstorage
                deleteCharactersInRange:NSMakeRange(fence,
                                                    textstorage.length -
                                                        fence)]; // Don't echo
                                                                 // input line

        if (line.length > 0) {
            [self saveHistory:line];
        }

        line = [line scrubInvalidCharacters];

        gev = [[GlkEvent alloc] initLineEvent:line forWindow:self.name];
        [self.glkctl queueEvent:gev];

        fence = textstorage.length;
        line_request = NO;
        [textview setEditable:NO];
    }

    else if (line_request && ch == keycode_Up) {
        [self travelBackwardInHistory];
    }

    else if (line_request && ch == keycode_Down) {
        [self travelForwardInHistory];
    }

    else if (line_request && ch == keycode_PageUp &&
             fence == textstorage.length) {
        [textview scrollPageUp:nil];
        return;
    }

    else {
        if (line_request)
            [self grabFocus];

        [self stopSpeakingText_10_7];
        [[self.glkctl window] makeFirstResponder:textview];
        [textview superKeyDown:evt];
    }
}

- (void)grabFocus {
    MyTextView *localTextView = textview;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.window makeFirstResponder:localTextView];
    });
    // NSLog(@"GlkTextBufferWindow %ld grabbed focus.", self.name);
}

- (BOOL)wantsFocus {
    return char_request || line_request;
}

- (void)clear {
    [textview resetTextFinder];

    id att = [[NSAttributedString alloc] initWithString:@""];
    [textstorage setAttributedString:att];
    fence = 0;
    _lastseen = 0;
    _lastchar = '\n';
    [container clearImages];
    hyperlinks = nil;
    hyperlinks = [[NSMutableArray alloc] init];

    moveRanges = nil;
    moveRanges = [[NSMutableArray alloc] init];
    moveRangeIndex = 0;

    [container invalidateLayout];
}

- (void)clearScrollback:(id)sender {
    NSString *string = textstorage.string;
    NSUInteger length = string.length;
    BOOL save_request = line_request;

    [textview resetTextFinder];

    NSUInteger prompt;
    NSUInteger i;

    if (!line_request)
        fence = string.length;

    /* try to rescue prompt line */
    for (i = 0; i < length; i++) {
        if ([string characterAtIndex:length - i - 1] == '\n')
            break;
    }
    if (i < length)
        prompt = i;
    else
        prompt = 0;

    line_request = NO;

    [textstorage replaceCharactersInRange:NSMakeRange(0, fence - prompt)
                                withString:@""];
    _lastseen = 0;
    _lastchar = '\n';
    fence = prompt;

    line_request = save_request;

    [container clearImages];
    hyperlinks = nil;
    hyperlinks = [[NSMutableArray alloc] init];

    moveRanges = nil;
    moveRanges = [[NSMutableArray alloc] init];
}

- (void)putString:(NSString *)str style:(NSUInteger)stylevalue {
    if (line_request)
        NSLog(@"Printing to text buffer window during line request");

//    if (char_request)
//        NSLog(@"Printing to text buffer window during character request");

    [self printToWindow:str style:stylevalue];
}

- (void)printToWindow:(NSString *)str style:(NSUInteger)stylevalue {
    [textview resetTextFinder];

    NSAttributedString *attstr = [[NSAttributedString alloc]
        initWithString:str
            attributes:[styles objectAtIndex:stylevalue]];

    [textstorage appendAttributedString:attstr];

    _lastchar = [str characterAtIndex:str.length - 1];
}

- (void)initChar {
    // NSLog(@"init char in %d", name);

    fence = textstorage.length;

    char_request = YES;
    textview.insertionPointColor = self.theme.bufferBackground;
    [textview setEditable:YES];

    [textview setSelectedRange:NSMakeRange(fence, 0)];

    [self setLastMove];
    [self speakMostRecent:nil];
}

- (void)cancelChar {
    // NSLog(@"cancel char in %d", name);

    char_request = NO;
    [textview setEditable:NO];
}

- (void)initLine:(NSString *)str {
    // NSLog(@"initLine: %@ in: %d", str, name);

    historypos = historypresent;

    // [glkctl performScroll];

    [textview resetTextFinder];

    if (self.terminatorsPending) {
        currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    if (echo_toggle_pending) {
        echo_toggle_pending = NO;
        echo = !echo;
    }

    if (_lastchar == '>' && self.theme.spaceFormat) {
        [self printToWindow:@" " style:style_Normal];
    }

    fence = textstorage.length;

    id att = [[NSAttributedString alloc]
        initWithString:str
              attributes:self.theme.bufInput.attributeDict];
    [textstorage appendAttributedString:att];

    textview.insertionPointColor =  self.theme.bufferNormal.color;

    [textview setEditable:YES];

    line_request = YES;

    [textview setSelectedRange:NSMakeRange(textstorage.length, 0)];
    [self setLastMove];
    [self speakMostRecent:nil];
}

- (NSString *)cancelLine {
    [textview resetTextFinder];

    textview.insertionPointColor = self.theme.bufferBackground;
    NSString *str = textstorage.string;
    str = [str substringFromIndex:fence];
    if (echo) {
        [self printToWindow:@"\n" style:style_Input];
        _lastchar = '\n'; // [str characterAtIndex: str.length - 1];
    } else
        [textstorage
            deleteCharactersInRange:NSMakeRange(
                                        fence,
                                        textstorage.length -
                                            fence)]; // Don't echo input line

    [textview setEditable:NO];
    line_request = NO;
    return str;
}

- (void)echo:(BOOL)val {
    if ((!(val) && echo) || (val && !(echo))) // Do we need to toggle echo?
        echo_toggle_pending = YES;
}

- (void)terpDidStop {
    [textview setEditable:NO];
}

- (BOOL)textView:(NSTextView *)aTextView
    shouldChangeTextInRange:(NSRange)range
          replacementString:(id)repl {
    if (line_request && range.location >= fence) {
        textview.shouldDrawCaret = YES;
        return YES;
    }

    textview.shouldDrawCaret = NO;
    return NO;
}

- (void)textStorageWillProcessEditing:(NSNotification *)note {
    if (!line_request)
        return;

    if (textstorage.editedRange.location < fence)
        return;

    [textstorage setAttributes:[styles objectAtIndex:style_Input]
                          range:textstorage.editedRange];
}

- (NSRange)textView:(NSTextView *)aTextView
    willChangeSelectionFromCharacterRange:(NSRange)oldrange
                         toCharacterRange:(NSRange)newrange {
    if (line_request) {
        if (newrange.length == 0)
            if (newrange.location < fence)
                newrange.location = fence;
    } else {
        if (newrange.length == 0)
            newrange.location = textstorage.length;
    }
    return newrange;
}

#pragma mark Text finder

- (void)restoreTextFinder {
    BOOL waseditable = textview.editable;
    textview.editable = NO;
    textview.usesFindBar = YES;

    NSTextFinder *newFinder = textview.textFinder;
    newFinder.client = textview;
    newFinder.findBarContainer = scrollview;
    newFinder.incrementalSearchingEnabled = YES;
    newFinder.incrementalSearchingShouldDimContentView = NO;

    if (_restoredFindBarVisible) {
        NSLog(@"Restoring textFinder");
        [newFinder performAction:NSTextFinderActionShowFindInterface];
        NSSearchField *searchField = [self findSearchFieldIn:self];
        if (searchField) {
            searchField.stringValue = _restoredSearch;
            [newFinder cancelFindIndicator];
            [self.glkctl.window makeFirstResponder:textview];
            [searchField sendAction:searchField.action to:searchField.target];
        }
    }
    textview.editable = waseditable;
}

- (void)destroyTextFinder {

    NSView *aView = [self findSearchFieldIn:scrollview];
    if (aView) {
        while (aView.superview != scrollview)
            aView = aView.superview;
        [aView removeFromSuperview];
        NSLog(@"Destroyed textFinder!");
    }
}

- (NSSearchField *)findSearchFieldIn:
(NSView *)theView // search the subviews for a view of class NSSearchField
{
    __block __weak NSSearchField * (^weak_findSearchField)(NSView *);
    NSSearchField * (^findSearchField)(NSView *);
    weak_findSearchField = findSearchField = ^(NSView *view) {
        if ([view isKindOfClass:[NSSearchField class]])
            return (NSSearchField *)view;
        __block NSSearchField *foundView = nil;
        [view.subviews enumerateObjectsUsingBlock:^(
                                                    NSView *subview, NSUInteger idx, BOOL *stop) {
            foundView = weak_findSearchField(subview);
            if (foundView)
                *stop = YES;
        }];
        return foundView;
    };

    return findSearchField(theView);
}

#pragma mark images

- (NSImage *)scaleImage:(NSImage *)src size:(NSSize)dstsize {
    NSSize srcsize = src.size;
    NSImage *dst;

    if (NSEqualSizes(srcsize, dstsize))
        return src;

    dst = [[NSImage alloc] initWithSize:dstsize];
    [dst lockFocus];

    [NSGraphicsContext currentContext].imageInterpolation =
        NSImageInterpolationHigh;

    [src drawInRect:NSMakeRect(0, 0, dstsize.width, dstsize.height)
              fromRect:NSMakeRect(0, 0, srcsize.width, srcsize.height)
             operation:NSCompositeSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];

    [dst unlockFocus];

    return dst;
}

- (void)drawImage:(NSImage *)image
             val1:(NSInteger)align
             val2:(NSInteger)unused
            width:(NSInteger)w
           height:(NSInteger)h {
    NSTextAttachment *att;
    NSFileWrapper *wrapper;
    NSData *tiffdata;
    // NSAttributedString *attstr;

    [textview resetTextFinder];

    if (w == 0)
        w = (NSInteger)image.size.width;
    if (h == 0)
        h = (NSInteger)image.size.height;

    if (align == imagealign_MarginLeft || align == imagealign_MarginRight) {
        if (_lastchar != '\n' && textstorage.length) {
            NSLog(@"lastchar is not line break. Do not add margin image.");
            return;
        }

        //        NSLog(@"adding image to margins");

        unichar uc[1];
        uc[0] = NSAttachmentCharacter;

        [textstorage.mutableString
            appendString:[NSString stringWithCharacters:uc length:1]];

        NSUInteger linkid = 0;
        if (currentHyperlink)
            linkid = currentHyperlink.index;

        image = [self scaleImage:image size:NSMakeSize(w, h)];

        tiffdata = image.TIFFRepresentation;

        [container addImage:[[NSImage alloc] initWithData:tiffdata]
                      align:align
                         at:textstorage.length - 1
                     linkid:linkid];

        //        [container addImage: image align: align at:
        //        textstorage.length - 1 linkid:linkid];

    } else {
        //        NSLog(@"adding image to text");

        image = [self scaleImage:image size:NSMakeSize(w, h)];

        tiffdata = image.TIFFRepresentation;

        wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:tiffdata];
        wrapper.preferredFilename = @"image.tiff";
        att = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
        MyAttachmentCell *cell =
            [[MyAttachmentCell alloc] initImageCell:image
                                       andAlignment:align
                                          andAttStr:textstorage
                                                 at:textstorage.length - 1];
        att.attachmentCell = cell;
        NSMutableAttributedString *attstr =
            (NSMutableAttributedString *)[NSMutableAttributedString
                attributedStringWithAttachment:att];

        [textstorage appendAttributedString:attstr];
    }
}

- (void)flowBreak {
    [textview resetTextFinder];

    // NSLog(@"adding flowbreak");
    unichar uc[1];
    uc[0] = NSAttachmentCharacter;
    [textstorage.mutableString appendString:[NSString stringWithCharacters:uc
                                                                     length:1]];
    [container flowBreakAt:textstorage.length - 1];
}

#pragma mark Hyperlinks

- (void)setHyperlink:(NSUInteger)linkid {
//    NSLog(@"txtbuf: hyperlink %ld set", (long)linkid);

    if (currentHyperlink && currentHyperlink.index != linkid) {
//        NSLog(@"There is a preliminary hyperlink, with index %ld",
//              currentHyperlink.index);
        if (currentHyperlink.startpos >= textstorage.length) {
//            NSLog(@"The preliminary hyperlink started at the end of current "
//                  @"input, so it was deleted. currentHyperlink.startpos == "
//                  @"%ld, textstorage.length == %ld",
//                  currentHyperlink.startpos, textstorage.length);
            currentHyperlink = nil;
        } else {
            [textview resetTextFinder];

            currentHyperlink.range =
                NSMakeRange(currentHyperlink.startpos,
                            textstorage.length - currentHyperlink.startpos);
            [textstorage addAttribute:NSLinkAttributeName
                                 value:@(currentHyperlink.index)
                                 range:currentHyperlink.range];
            [hyperlinks addObject:currentHyperlink];
            currentHyperlink = nil;
        }
    }
    if (!currentHyperlink && linkid) {
        currentHyperlink =
            [[GlkHyperlink alloc] initWithIndex:linkid
                                         andPos:textstorage.length];
//        NSLog(@"New preliminary hyperlink started at position %ld, with link "
//              @"index %ld",
//              currentHyperlink.startpos, linkid);
    }
}

- (void)initHyperlink {
    hyper_request = YES;
    textview.editable = YES;
    //    NSLog(@"txtbuf: hyperlink event requested");
}

- (void)cancelHyperlink {
    hyper_request = NO;
    textview.editable = NO;
    //    NSLog(@"txtbuf: hyperlink event cancelled");
}

// Make margin image links clickable
- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;

    // Don't draw a caret right now, even if we clicked at the prompt
    [textview temporarilyHideCaret];

    // NSLog(@"mouseDown in buffer window.");
    if (hyper_request) {
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;
        p = [textview convertPoint:p fromView:nil];
        p.x -= textview.textContainerInset.width;
        p.y -= textview.textContainerInset.height;

        NSUInteger linkid = [container findHyperlinkAt:p];
        if (linkid) {
            NSLog(@"Clicked margin image hyperlink in buf at %g,%g", p.x, p.y);
            gev = [[GlkEvent alloc] initLinkEvent:linkid forWindow:self.name];
            [self.glkctl queueEvent:gev];
            hyper_request = NO;
            return YES;
        }
    }
    return NO;
}

- (BOOL)textView:(NSTextView *)textview_
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
    NSLog(@"txtbuf: clicked on link: %@", link);

    if (!hyper_request) {
        NSLog(@"txtbuf: No hyperlink request in window.");
        return NO;
    }

    GlkEvent *gev =
        [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                              forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    [textview setEditable:NO];
    return YES;
}

#pragma mark Scrolling

- (void)markLastSeen {
    NSRange glyphs;
    NSRect line;

    glyphs = [layoutmanager glyphRangeForTextContainer:container];

    if (glyphs.length) {
        line = [layoutmanager
            lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphs) - 1
                             effectiveRange:nil];

        _lastseen = (NSInteger)(line.origin.y + line.size.height); // bottom of the line
        // NSLog(@"GlkTextBufferWindow: markLastSeen: %ld", (long)_lastseen);
    }
}

- (void)storeScrollOffset {

    lastAtBottom = self.scrolledToBottom;

    if (lastAtBottom || !textstorage.length) {
        return;
    }

    NSRect visibleRect = scrollview.documentVisibleRect;

    lastVisible = [textview
        characterIndexForInsertionAtPoint:NSMakePoint(NSMaxX(visibleRect),
                                                      NSMaxY(visibleRect))];
    lastVisible--;
    if (lastVisible >= textstorage.length) {
        NSLog(@"lastCharacter index (%ld) is outside textstorage length (%ld)",
              lastVisible, textstorage.length);
        lastVisible = textstorage.length - 1;
    }

    NSRect lastRect =
        [layoutmanager lineFragmentRectForGlyphAtIndex:lastVisible
                                        effectiveRange:nil];

    lastScrollOffset = (NSMaxY(visibleRect) - NSMaxY(lastRect)) /
                    lastLineheight;

    if (isnan(lastScrollOffset) || isinf(lastScrollOffset)|| lastScrollOffset < 0.1)
        lastScrollOffset = 0;
}

- (void)restoreScroll;
{
    if (lastAtBottom) {
        [self scrollToBottom];
        return;
    }
    [self scrollToCharacter:lastVisible withOffset:lastScrollOffset];
    return;
}

- (void)restoreSelection {
    textview.selectedRange = _restoredSelection;
}

- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset {
    NSRect line;

    if (character >= textstorage.length - 1) {
        [self scrollToBottom];
        return;
    }

    offset = offset * self.theme.bufferNormal.font.boundingRectForFont.size.height;
    if (isnan(offset) || isinf(offset)|| offset < 0.1)
        offset = 0;

    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];

    if (textstorage.length) {
        line = [layoutmanager lineFragmentRectForGlyphAtIndex:character
                                               effectiveRange:nil];

        CGFloat charbottom = NSMaxY(line); // bottom of the line
        charbottom = charbottom + offset;
        NSPoint newScrollOrigin = NSMakePoint(0, floor(charbottom - NSHeight(scrollview.frame)));
        [scrollview.contentView scrollToPoint:newScrollOrigin];
        
        [scrollview reflectScrolledClipView:scrollview.contentView];
    }
}

- (void)scrollToBottom {
    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];

    NSPoint newScrollOrigin = NSMakePoint(0, NSMaxY(textview.frame) - NSHeight(scrollview.contentView.bounds));

    [scrollview.contentView  scrollToPoint:newScrollOrigin];
    [scrollview reflectScrolledClipView:scrollview.contentView];
}

- (void)performScroll {
    //    NSLog(@"performScroll: scroll down from lastseen");

    CGFloat bottom;
    NSRange range;
    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];

    if (textstorage.length == 0)
        return;

    [layoutmanager textContainerForGlyphAtIndex:0 effectiveRange:&range];

    // then, get the bottom
    bottom = NSHeight(textview.frame);

    // scroll so rect from lastseen to bottom is visible
    if (bottom - _lastseen > NSHeight(scrollview.frame))
        [textview scrollRectToVisible:NSMakeRect(0, _lastseen, 0,
                                             NSHeight(scrollview.frame))];
    else
        [textview scrollRectToVisible:NSMakeRect(0, _lastseen, 0,
                                             bottom - _lastseen)];
}

- (BOOL)scrolledToBottom {
    NSView *clipView = scrollview.contentView;
    return (fabs(clipView.bounds.origin.y + clipView.bounds.size.height -
                 textview.bounds.size.height) < .5);
}


- (void)restoreScrollBarStyle {
    if (scrollview) {

        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.scrollerStyle = NSScrollerStyleOverlay;
        scrollview.drawsBackground = YES;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = YES;
        [[scrollview verticalScroller] setAlphaValue:100];
        scrollview.autohidesScrollers = YES;
        scrollview.borderType = NSNoBorder;
    }
}

#pragma mark Speech

- (void)setLastMove {
    NSUInteger maxlength = textstorage.length;

    if (!maxlength) {
        moveRanges = [[NSMutableArray alloc] init];
        moveRangeIndex = 0;
        return;
    }
    NSRange currentMove = NSMakeRange(0, maxlength);

    if (moveRanges.lastObject) {
        NSRange lastMove = ((NSValue *)moveRanges.lastObject).rangeValue;
        if (NSMaxRange(lastMove) > maxlength)
            [moveRanges removeLastObject];
        else if (lastMove.length == maxlength)
            return;
        else
            currentMove = NSMakeRange(NSMaxRange(lastMove),
                                      maxlength - NSMaxRange(lastMove) - 1);
    }

    if (NSMaxRange(currentMove) > maxlength)
        currentMove =
            NSMakeRange(currentMove.location, maxlength - currentMove.location);
    moveRangeIndex = moveRanges.count;
    [moveRanges addObject:[NSValue valueWithRange:currentMove]];
}

- (IBAction)speakMostRecent:(id)sender {
    if (!moveRanges.count)
        return;
    moveRangeIndex = moveRanges.count - 1;
    NSRange lastMove = ((NSValue *)moveRanges.lastObject).rangeValue;

    if (lastMove.length <= 0 || NSMaxRange(lastMove) > textstorage.length) {
        NSDictionary *announcementInfo = @{
            NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
            NSAccessibilityAnnouncementKey : @"No last move to speak"
        };

        NSAccessibilityPostNotificationWithUserInfo(
            [NSApp mainWindow],
            NSAccessibilityAnnouncementRequestedNotification, announcementInfo);

        // NSLog(@"No last move to speak");
        return;
    }

    if (NSAppKitVersionNumber < NSAppKitVersionNumber10_9) {
        textview.rangeToSpeak_10_7 = NSMakeRange(lastMove.location, 0);

        textview.shouldSpeak_10_7 = YES;
        NSAccessibilityPostNotification(
            textview, NSAccessibilitySelectedTextChangedNotification);

        [NSTimer scheduledTimerWithTimeInterval:0.5
                                         target:self
                                       selector:@selector(speakHack:)
                                       userInfo:nil
                                        repeats:NO];
    } else {
        NSString *str = [textstorage.string substringWithRange:lastMove];

        NSDictionary *announcementInfo = @{
            NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
            NSAccessibilityAnnouncementKey : str
        };

        NSWindow *mainWin = [NSApp mainWindow];

        if (mainWin)
            NSAccessibilityPostNotificationWithUserInfo(
                mainWin, NSAccessibilityAnnouncementRequestedNotification,
                announcementInfo);
    }
}

- (void)speakHack:(id)sender {
    NSValue *v;
    NSRange lastMove = NSMakeRange(0, 0);
    NSEnumerator *movesenumerator = [moveRanges reverseObjectEnumerator];

    while (v = [movesenumerator nextObject]) {
        lastMove = v.rangeValue;
        if (lastMove.length)
            break;
    }

    if (lastMove.length == 0) {
        // NSLog(@"No last move to speak");
        return;
    }
    textview.rangeToSpeak_10_7 =
        NSMakeRange(lastMove.location, textstorage.length - lastMove.location);
    textview.shouldSpeak_10_7 = YES;
    NSAccessibilityPostNotification(
        self, NSAccessibilityFocusedUIElementChangedNotification);
    NSAccessibilityPostNotification(
        textview, NSAccessibilitySelectedTextChangedNotification);
    NSAccessibilityPostNotification(textview,
                                    NSAccessibilityValueChangedNotification);
}

- (IBAction)speakPrevious:(id)sender {
    if (!moveRanges.count)
        return;
    if (moveRangeIndex > 0)
        moveRangeIndex--;
    else
        moveRangeIndex = 0;
    [self speakRange:((NSValue *)[moveRanges objectAtIndex:moveRangeIndex])
                         .rangeValue];
}

- (IBAction)speakNext:(id)sender {
    // NSLog(@"speakNext: moveRangeIndex; %ld", moveRangeIndex);
    if (!moveRanges.count)
        return;
    if (moveRangeIndex < moveRanges.count - 1)
        moveRangeIndex++;
    else
        moveRangeIndex = moveRanges.count - 1;
    [self speakRange:((NSValue *)[moveRanges objectAtIndex:moveRangeIndex])
                         .rangeValue];
}

- (IBAction)speakStatus:(id)sender {
    GlkWindow *win;

    // Try to find status window to pass this on to
    for (win in [self.glkctl.gwindows allValues]) {
        if ([win isKindOfClass:[GlkTextGridWindow class]]) {
            [(GlkTextGridWindow *)win speakStatus:sender];
            return;
        }
    }
    NSLog(@"No status window found");
}

- (void)speakRange:(NSRange)aRange {
    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_9) {

        if (NSMaxRange(aRange) >= textstorage.length)
            aRange = NSMakeRange(0, textstorage.length);

        NSString *str = [textstorage.string substringWithRange:aRange];

        NSDictionary *announcementInfo = @{
            NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
            NSAccessibilityAnnouncementKey : str
        };

        NSWindow *mainWin = [NSApp mainWindow];

        if (mainWin)
            NSAccessibilityPostNotificationWithUserInfo(
                mainWin, NSAccessibilityAnnouncementRequestedNotification,
                announcementInfo);
    }
}

- (void)stopSpeakingText_10_7 {
    if (textview.shouldSpeak_10_7) {
        textview.rangeToSpeak_10_7 = NSMakeRange(textstorage.length, 0);

        textview.shouldSpeak_10_7 = NO;
    }
}

#pragma mark Accessibility

- (NSString *)accessibilityActionDescription:(NSString *)action {
    if ([action isEqualToString:@"Repeat last move"])
        return @"Read the output of the last command entered";
    if ([action isEqualToString:@"Speak move before"])
        return @"Read the output of the command before the last one read";
    if ([action isEqualToString:@"Speak move after"])
        return @"Read the output of the command after the last one read";
    if ([action isEqualToString:@"Speak status bar"])
        return @"Read the text of the status bar";

    return [super accessibilityActionDescription:action];
}

- (NSArray *)accessibilityActionNames {
    NSMutableArray *result = [[super accessibilityActionNames] mutableCopy];

    [result addObjectsFromArray:@[
        @"Repeat last move", @"Speak move before", @"Speak move after",
        @"Speak status bar"
    ]];

    return result;
}

- (void)accessibilityPerformAction:(NSString *)action {
    // NSLog(@"GlkTextBufferWindow: accessibilityPerformAction. %@",action);

    if ([action isEqualToString:@"Repeat last move"])
        [self speakMostRecent:nil];
    else if ([action isEqualToString:@"Speak move before"])
        [self speakPrevious:nil];
    else if ([action isEqualToString:@"Speak move after"])
        [self speakNext:nil];
    else if ([action isEqualToString:@"Speak status bar"])
        [self speakStatus:nil];
    else
        [super accessibilityPerformAction:action];
}

- (NSArray *)accessibilityAttributeNames {

    NSMutableArray *result = [[super accessibilityAttributeNames] mutableCopy];
    if (!result)
        result = [[NSMutableArray alloc] init];

    [result addObjectsFromArray:@[ NSAccessibilityContentsAttribute ]];

    // NSLog(@"GlkTextBufferWindow: accessibilityAttributeNames: %@ ", result);
    return result;
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
    NSResponder *firstResponder = self.window.firstResponder;

    // NSLog(@"GlkTextBufferWindow: accessibilityAttributeValue: %@",attribute);
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
        return NSAccessibilityUnknownRole;
    }

    if ([attribute isEqualToString:NSAccessibilityContentsAttribute]) {
        return textview;
    } else if ([attribute
                   isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
        return [NSString
            stringWithFormat:
                @"Text window%@%@%@. %@",
                line_request ? @", waiting for commands" : @"",
                char_request ? @", waiting for a key press" : @"",
                hyper_request ? @", waiting for a hyperlink click" : @"",
                [textview
                    accessibilityAttributeValue:NSAccessibilityValueAttribute]];
    } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
        return [NSNumber numberWithBool:firstResponder == self ||
                                        firstResponder == textview];
    } else if ([attribute
                   isEqualToString:NSAccessibilityFocusedUIElementAttribute]) {
        return self.accessibilityFocusedUIElement;
    } else if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        return @[ textview ];
    }

    return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityFocusedUIElement {
    return textview;
}

@end
