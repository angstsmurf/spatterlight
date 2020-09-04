/*
 * NSTextView associated with a glk window
 */

#import "Compatibility.h"
#import "NSString+Categories.h"
#import "NSColor+integer.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "GlkStyle.h"
#import "ZColor.h"
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

    NSUInteger lastCharPos = pos - 1;
    
    if (lastCharPos > _attrstr.length)
        lastCharPos = 0;
    
    NSDictionary *attributes = [_attrstr attributesAtIndex:lastCharPos
                                  effectiveRange:nil];

    NSFont *font = attributes[NSFontAttributeName];

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
//            NSLog(@"MarginContainer: looking for an image that intersects "
//                  @"flowbreak %ld (%@)",
//                  [flowbreaks indexOfObject:f], NSStringFromRect(flowrect));

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
        MarginImage *img2 = margins[(NSUInteger)i];

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
                substringWithRange:((NSValue *)(self.selectedRanges)[0])
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
        self.styleHints = [self deepCopyOfStyleHintsArray:self.glkctl.bufferStyleHints];

        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        for (i = 0; i < style_NUMSTYLES; i++) {
            if (self.theme.doStyles) {
                styleDict = [((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]) attributesWithHints:self.styleHints[i]];
            } else {
                styleDict = ((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]).attributeDict;
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
        lastLineheight = self.theme.bufferCellHeight;

        for (i = 0; i < HISTORYLEN; i++)
            history[i] = nil;

        moveRanges = [[NSMutableArray alloc] init];
        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];

        [self restoreScrollBarStyle];

        /* construct text system manually */

        textstorage = [[NSTextStorage alloc] init];

        layoutmanager = [[NSLayoutManager alloc] init];
//        layoutmanager.backgroundLayoutEnabled = YES;
        [textstorage addLayoutManager:layoutmanager];

        container = [[MarginContainer alloc]
            initWithContainerSize:NSMakeSize(0, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        _textview =
            [[MyTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000)
                                textContainer:container];

        _textview.minSize = NSMakeSize(1, 10000000);
        _textview.maxSize = NSMakeSize(10000000, 10000000);

        container.textView = _textview;

        scrollview.documentView = _textview;

        /* now configure the text stuff */

        container.widthTracksTextView = YES;
        container.heightTracksTextView = NO;

        _textview.horizontallyResizable = NO;
        _textview.verticallyResizable = YES;

        _textview.autoresizingMask = NSViewWidthSizable;

        _textview.allowsImageEditing = NO;
        _textview.allowsUndo = NO;
        _textview.usesFontPanel = NO;
        _textview.usesFindBar = YES;
        _textview.incrementalSearchingEnabled = YES;

        _textview.smartInsertDeleteEnabled = NO;

        _textview.delegate = self;
        textstorage.delegate = self;

        _textview.textContainerInset = NSMakeSize(marginX, marginY);
        _textview.backgroundColor = styles[style_Normal][NSBackgroundColorAttributeName];
        _textview.insertionPointColor = styles[style_Normal][NSForegroundColorAttributeName];

        NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
        linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
        _textview.linkTextAttributes = linkAttributes;

        [_textview enableCaret:nil];

        [self addSubview:scrollview];
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        NSUInteger i;
        _textview = [decoder decodeObjectForKey:@"textview"];
        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        container = (MarginContainer *)_textview.textContainer;
        if (!layoutmanager)
            NSLog(@"layoutmanager nil!");
        if (!textstorage)
            NSLog(@"textstorage nil!");
        if (!container)
            NSLog(@"container nil!");
        scrollview = _textview.enclosingScrollView;
        if (!scrollview)
            NSLog(@"scrollview nil!");

        scrollview.documentView = _textview;
        _textview.delegate = self;
        textstorage.delegate = self;

        if (_textview.textStorage != textstorage)
            NSLog(@"Error! _textview.textStorage != textstorage");

        [self restoreScrollBarStyle];

        line_request = [decoder decodeBoolForKey:@"line_request"];
        _textview.editable = line_request;
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];

        echo_toggle_pending = [decoder decodeBoolForKey:@"echo_toggle_pending"];
        echo = [decoder decodeBoolForKey:@"echo"];

        fence = (NSUInteger)[decoder decodeIntegerForKey:@"fence"];

        NSMutableArray *historyarray = [decoder decodeObjectForKey:@"history"];

        for (i = 0; i < historyarray.count; i++) {
            history[i] = historyarray[i];
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
        _textview.selectedRange = _restoredSelection;

        _restoredAtBottom = [decoder decodeBoolForKey:@"scrolledToBottom"];
        _restoredAtTop = [decoder decodeBoolForKey:@"scrolledToTop"];
        _restoredLastVisible = (NSUInteger)[decoder decodeIntegerForKey:@"lastVisible"];
        _restoredScrollOffset = [decoder decodeDoubleForKey:@"scrollOffset"];

        _textview.insertionPointColor =
            [decoder decodeObjectForKey:@"insertionPointColor"];
        _textview.shouldDrawCaret =
            [decoder decodeBoolForKey:@"shouldDrawCaret"];
        _restoredSearch = [decoder decodeObjectForKey:@"searchString"];
        _restoredFindBarVisible = [decoder decodeBoolForKey:@"findBarVisible"];
        storedNewline = [decoder decodeObjectForKey:@"storedNewline"];

        [self destroyTextFinder];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_textview forKey:@"textview"];
    NSValue *rangeVal = [NSValue valueWithRange:_textview.selectedRange];
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

    [encoder encodeBool:lastAtBottom forKey:@"scrolledToBottom"];
    [encoder encodeBool:lastAtTop forKey:@"scrolledToTop"];

    if (!lastAtBottom && !lastAtTop) {
        [self storeScrollOffset];
        [encoder encodeInteger:(NSInteger)lastVisible forKey:@"lastVisible"];
        [encoder encodeDouble:lastScrollOffset forKey:@"scrollOffset"];
    }

    [encoder encodeObject:_textview.insertionPointColor
                   forKey:@"insertionPointColor"];
    [encoder encodeBool:_textview.shouldDrawCaret forKey:@"shouldDrawCaret"];
    NSSearchField *searchField = [self findSearchFieldIn:self];
    if (searchField) {
        [encoder encodeObject:searchField.stringValue forKey:@"searchString"];
    }
    [encoder encodeBool:scrollview.findBarVisible forKey:@"findBarVisible"];
    [encoder encodeObject:storedNewline forKey:@"storedNewline"];
}

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

- (void)recalcBackground {
    NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];

    if (self.theme.doStyles && bgnd > -1) {
        bgcolor = [NSColor colorFromInteger:bgnd];
    }
    
    if (!bgcolor)
        bgcolor = self.theme.bufferBackground;

    NSColor *fgcolor = styles[style_Normal][NSForegroundColorAttributeName];
    if (!fgcolor)
        fgcolor = self.theme.bufferNormal.color;

    if (!fgcolor || !bgcolor) {
        NSLog(@"GlkTexBufferWindow recalcBackground: color error!");
    }

    _textview.editable = YES;
    _textview.backgroundColor = bgcolor;
    _textview.insertionPointColor = fgcolor;

    [self hideInsertionPoint];
    [self.glkctl setBorderColor:bgcolor fromWindow:self];
}

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;
    [self recalcBackground];
}

- (void)prefsDidChange {
//    NSLog(@"GlkTextBufferWindow %ld prefsDidChange", self.name);
    NSDictionary *attributes;

    // Preferences has changed, so first we must redo the styles library
    NSMutableArray *newstyles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    BOOL different = NO;
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        if (self.theme.doStyles) {
            // We're doing styles, so we call the current theme object with our hints array
            // in order to get an attributes dictionary
            attributes = [((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]) attributesWithHints:self.styleHints[i]];
        } else {
            // We're not doing styles, so use the raw style attributes from
            // the theme object's attributeDict object
            attributes = ((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]).attributeDict;
        }

        if (attributes) {
            [newstyles addObject:attributes];
            if (!different && ![newstyles[i] isEqualToDictionary:styles[i]])
                different = YES;
        } else
            [styles addObject:[NSNull null]];
    }

    if (!self.glkctl.previewDummy) {
        NSInteger marginX = self.theme.bufferMarginX;
        NSInteger marginY = self.theme.bufferMarginY;

        _textview.textContainerInset = NSMakeSize(marginX, marginY);
    }

    // We can think of attributes as special characters in the mutable attributed
    // string called textstorage.
    // Here we iterate through the textstorage string to find them all.
    // We have to do it character by character instead of using
    // enumerateAttribute:inRange:options:usingBlock:
    // to make sure that no inline images, hyperlinks or zcolors
    // get lost when we update the Glk Styles.

    if (different) {
        styles = newstyles;
        /* reassign styles to attributedstrings */
        NSMutableAttributedString *backingStorage = [textstorage mutableCopy];

        if (storedNewline) {
            [backingStorage appendAttributedString:storedNewline];
        }
        
        NSRange selectedRange = _textview.selectedRange;

        [textstorage
         enumerateAttributesInRange:NSMakeRange(0, textstorage.length)
         options:0
         usingBlock:^(NSDictionary *attrs, NSRange range, BOOL *stop) {

             // First, we overwrite all attributes with those in our updated
             // styles array
             id styleobject = attrs[@"GlkStyle"];
             if (styleobject) {
                 NSDictionary *stylesAtt = styles[(NSUInteger)[styleobject intValue]];
                 [backingStorage setAttributes:stylesAtt range:range];
             }

             // Then, we re-add all the "non-Glk" style values we want to keep
             // (inline images, hyperlinks, Z-colors and reverse video)
             id image = attrs[@"NSAttachment"];
             if (image) {
                 ((MyAttachmentCell *)((NSTextAttachment *)image).attachmentCell).attrstr = backingStorage;
             }

             id hyperlink = attrs[NSLinkAttributeName];
             if (hyperlink) {
                 [backingStorage addAttribute:NSLinkAttributeName
                                        value:hyperlink
                                        range:range];
             }

             id zcolor = attrs[@"ZColor"];
             if (zcolor) {
                 [backingStorage addAttribute:@"ZColor"
                                        value:zcolor
                                        range:range];
             }

             id reverse = attrs[@"ReverseVideo"];
             if (reverse) {
                 [backingStorage addAttribute:@"ReverseVideo"
                                        value:reverse
                                        range:range];
             }
         }];

        if (self.theme.doStyles)
            backingStorage = [self applyZColorsAndThenReverse:backingStorage];

        if (storedNewline) {
            storedNewline = [[NSAttributedString alloc] initWithString:@"\n" attributes:[backingStorage attributesAtIndex:backingStorage.length - 1 effectiveRange:NULL]];
            [backingStorage deleteCharactersInRange:NSMakeRange(backingStorage.length - 1, 1)];
        }

        [textstorage setAttributedString:backingStorage];

        _textview.selectedRange = selectedRange;
    }

    if (!self.glkctl.previewDummy) {

        if (different) {
            // Set style for hyperlinks
            NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
            linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
            _textview.linkTextAttributes = linkAttributes;

            [self showInsertionPoint];
            lastLineheight = self.theme.bufferNormal.font.boundingRectForFont.size.height;
            [self recalcBackground];
            [container invalidateLayout];
        }
        [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.1];
    } else {
        [self recalcBackground];
    }
}

- (void)setFrame:(NSRect)frame {
//        NSLog(@"GlkTextBufferWindow %ld: setFrame: %@", self.name,
//        NSStringFromRect(frame));

    if (NSEqualRects(frame, self.frame)) {
//        NSLog(@"GlkTextBufferWindow setFrame: new frame same as old frame. "
//              @"Skipping.");
        return;
    }

    super.frame = frame;
    if ([container hasMarginImages])
        [container invalidateLayout];

    if (NSMaxX(self.frame) > NSWidth(self.glkctl.contentView.bounds) && NSWidth(self.frame) > 10) {
        NSLog(@"GlkTextViewBuffer %ld width: something is wrong: contentView width: %f self maxX: %f", (long)self.name, NSWidth(self.glkctl.contentView.bounds), NSMaxX(self.frame));
        NSLog(@"Self frame:%@ contentview frame: %@ _textview.frame %@ scrollview.frame %@", NSStringFromRect(self.frame),  NSStringFromRect(self.glkctl.contentView.frame), NSStringFromRect(_textview.frame), NSStringFromRect(scrollview.frame));
        self.frame = NSMakeRect(frame.origin.x, self.frame.origin.y, NSWidth(self.glkctl.contentView.bounds) - frame.origin.x, frame.size.height);
        _textview.frame = self.frame;
    }
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
    NSTextView *localTextView = _textview;
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
    [_textview resetTextFinder];

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
    [_textview resetTextFinder];

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
//    NSLog(@"GlkTextBufferWindow %ld onKeyDown", self.name);
    GlkEvent *gev;
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:0]);

    NSUInteger flags = evt.modifierFlags;

    GlkWindow *win;

    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus) {
        NSLog(@"%ld does not want focus", self.name);
        for (win in [self.glkctl.gwindows allValues]) {
            if (win != self && win.wantsFocus) {
                NSLog(@"GlkTextBufferWindow: Passing on keypress to window %ld", win.name);
                [win grabFocus];
                if ([win isKindOfClass:[GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)win onKeyDown:evt];
                else
                    [win keyDown:evt];
                return;
            }
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
    BOOL scrolled = NO;

    if (![self scrolledToBottom]) {
//        NSLog(@"Not scrolled to the bottom, pagedown or navigate scrolling on each key instead");
        switch (ch) {
            case keycode_PageUp:
            case keycode_Delete:
                [_textview scrollPageUp:nil];
                return;
            case keycode_PageDown:
            case ' ':
                [_textview scrollPageDown:nil];
                return;
            case keycode_Up:
                [_textview scrollLineUp:nil];
                return;
            case keycode_Down:
            case keycode_Return:
                [_textview scrollLineDown:nil];
                return;
            default:
                [self performScroll];
                // To fix scrolling in the Adrian Mole games
                scrolled = YES;
                break;
        }
    }

    if (char_request && ch != keycode_Unknown) {
        // To fix scrolling in the Adrian Mole games
        if (!scrolled)
            self.glkctl.shouldScrollOnCharEvent = YES;

        [self markLastSeen];

        gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [self.glkctl queueEvent:gev];

        char_request = NO;
        [_textview setEditable:NO];

    } else if (line_request && (ch == keycode_Return ||
                [currentTerminators[key] isEqual:@(YES)])) {
                   [self sendInputLine];
    } else if (line_request && ch == keycode_Up) {
        [self travelBackwardInHistory];
    } else if (line_request && ch == keycode_Down) {
        [self travelForwardInHistory];
    } else if (line_request && ch == keycode_PageUp &&
             fence == textstorage.length) {
        [_textview scrollPageUp:nil];
        return;
    }

    else {
        if (line_request)
            [self grabFocus];

        [self stopSpeakingText_10_7];
        [[self.glkctl window] makeFirstResponder:_textview];
        [_textview superKeyDown:evt];
    }
}

-(void)sendInputLine {
    // NSLog(@"line event from %ld", (long)self.name);

    [_textview resetTextFinder];

    [self.glkctl markLastSeen];

    NSString *line = [textstorage.string substringFromIndex:fence];
    if (echo) {
        [self printToWindow:@"\n"
                      style:style_Input]; // XXX arranger lastchar needs to be set
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

    if (self.glkctl.deadCities) {
        unichar endChar = [line characterAtIndex:line.length - 1];
        if (endChar == '\n' || endChar == '\r')
            line = [line substringToIndex:line.length - 1];
    }
    
    GlkEvent *gev = [[GlkEvent alloc] initLineEvent:line forWindow:self.name];
    [self.glkctl queueEvent:gev];

    fence = textstorage.length;
    line_request = NO;
    [self hideInsertionPoint];
    [_textview setEditable:NO];
}

- (void)grabFocus {
    MyTextView *localTextView = _textview;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.window makeFirstResponder:localTextView];
    });
    // NSLog(@"GlkTextBufferWindow %ld grabbed focus.", self.name);
}

- (BOOL)wantsFocus {
    return char_request || line_request;
}

- (void)clear {
    [_textview resetTextFinder];

    id att = [[NSAttributedString alloc] initWithString:@""];
    [textstorage setAttributedString:att];
    fence = 0;
    _lastseen = 0;
    _lastchar = '\n';
    [container clearImages];

    moveRanges = nil;
    moveRanges = [[NSMutableArray alloc] init];
    moveRangeIndex = 0;

    [self recalcBackground];
    [container invalidateLayout];
}

- (void)clearScrollback:(id)sender {
    NSString *string = textstorage.string;
    NSUInteger length = string.length;
    BOOL save_request = line_request;

    [_textview resetTextFinder];

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

    moveRanges = nil;
    moveRanges = [[NSMutableArray alloc] init];
}

- (void)putString:(NSString *)str style:(NSUInteger)stylevalue {
    if (line_request)
        NSLog(@"Printing to text buffer window during line request");

//    if (char_request)
//        NSLog(@"Printing to text buffer window during character request");

    [self printToWindow:str style:stylevalue];

    if (self.glkctl.deadCities && line_request && [[str substringFromIndex:str.length - 1] isEqualToString:@"\n"]) {
        // This is against the Glk spec but makes
        // hyperlinks in Dead Cities work.
        // There should be some kind of switch for this.
        [self sendInputLine];
    }
}

- (void)printToWindow:(NSString *)str style:(NSUInteger)stylevalue {

//    NSLog(@"\nPrinting %ld chars at position %ld with style %@", str.length, textstorage.length, gBufferStyleNames[stylevalue]);

    // A lot of code to not print single newlines
    // at the bottom
    if (storedNewline) {
        [textstorage appendAttributedString:storedNewline];
        storedNewline = nil;
    }

    NSMutableDictionary *attributes = [styles[stylevalue] mutableCopy];

    if (currentZColor) {
        attributes[@"ZColor"] = currentZColor;
        if (self.theme.doStyles) {
            if ([self.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)]) {
                attributes = [currentZColor reversedAttributes:attributes];
                //            NSLog(@"Because the style has reverseColor hint, we apply the zcolors in reverse");
            } else {
                attributes = [currentZColor coloredAttributes:attributes];
                //            NSLog(@"We apply the zcolors normally");
            }
        }
    }

    if (self.currentReverseVideo) {
        attributes[@"ReverseVideo"] = @(YES);
        if (self.theme.doStyles) {
            if ( [self.styleHints[stylevalue][stylehint_ReverseColor] isNotEqualTo:@(1)]) {
                // Current style has stylehint_ReverseColor unset, so we reverse colors
                attributes = [self reversedAttributes:attributes background:self.theme.bufferBackground];
            }
        }
    }

    if (self.currentHyperlink) {
        attributes[NSLinkAttributeName] = @(self.currentHyperlink);
    }

    if (str.length > 1) {
        _lastchar = [str characterAtIndex:str.length - 1];
        if (_lastchar == '\n') {
            storedNewline = [[NSAttributedString alloc]
                             initWithString:@"\n"
                             attributes:attributes];

            str = [str substringWithRange:NSMakeRange(0, str.length - 1)];
        }
    }


    NSAttributedString *attstr = [[NSAttributedString alloc]
                                  initWithString:str
                                  attributes:attributes];

    [_textview resetTextFinder];
    [textstorage appendAttributedString:attstr];
    dirty = YES;
}

- (void)unputString:(NSString *)buf {
    NSLog(@"GlkTextBufferWindow %ld unputString %@", self.name, buf);
    NSString *stringToRemove = [textstorage.string substringFromIndex:textstorage.length - buf.length];
    if ([stringToRemove isEqualToString:buf]) {
        [textstorage deleteCharactersInRange:NSMakeRange(textstorage.length - buf.length, buf.length)];
    }
}

- (void)initChar {
//    NSLog(@"GlkTextbufferWindow %ld initChar", (long)self.name);

    fence = textstorage.length;

    char_request = YES;
    [self hideInsertionPoint];

    [self setLastMove];
    [self speakMostRecent:nil];
}

- (void)cancelChar {
    // NSLog(@"cancel char in %d", name);

    char_request = NO;
    [_textview setEditable:NO];
}

- (void)initLine:(NSString *)str {
//    NSLog(@"initLine: %@ in: %ld", str, (long)self.name);

    historypos = historypresent;

    [_textview resetTextFinder];

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

    [_textview setEditable:YES];

    line_request = YES;
    [self showInsertionPoint];

    [_textview setSelectedRange:NSMakeRange(textstorage.length, 0)];
    [self setLastMove];
    [self speakMostRecent:nil];
}

- (NSString *)cancelLine {
    [_textview resetTextFinder];

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

    [_textview setEditable:NO];
    line_request = NO;
    [self hideInsertionPoint];
    return str;
}

- (void)hideInsertionPoint {
    if (!line_request) {
        NSColor *color = _textview.backgroundColor;
        if (!color)
            color = self.theme.bufferBackground;
        _textview.insertionPointColor = color;
    }
}

- (void)showInsertionPoint {
    if (line_request) {
        NSColor *color = styles[style_Normal][NSForegroundColorAttributeName];
        if (!color)
            color = self.theme.bufferNormal.color;
        _textview.insertionPointColor = color;
    }
}

- (void)echo:(BOOL)val {
    if ((!(val) && echo) || (val && !(echo))) // Do we need to toggle echo?
        echo_toggle_pending = YES;
}

- (void)terpDidStop {
    [_textview setEditable:NO];
    [self grabFocus];
}

- (BOOL)textView:(NSTextView *)aTextView
    shouldChangeTextInRange:(NSRange)range
          replacementString:(id)repl {
    if (line_request && range.location >= fence) {
        _textview.shouldDrawCaret = YES;
        return YES;
    }

    _textview.shouldDrawCaret = NO;
    return NO;
}

- (void)textStorageWillProcessEditing:(NSNotification *)note {
    if (!line_request)
        return;

    if (textstorage.editedRange.location < fence)
        return;

    [textstorage setAttributes:styles[style_Input]
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
    BOOL waseditable = _textview.editable;
    _textview.editable = NO;
    _textview.usesFindBar = YES;

    NSTextFinder *newFinder = _textview.textFinder;
    newFinder.client = _textview;
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
            [self.glkctl.window makeFirstResponder:_textview];
            [searchField sendAction:searchField.action to:searchField.target];
        }
    }
    _textview.editable = waseditable;
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
           height:(NSInteger)h
            style:(NSUInteger)style {
    NSTextAttachment *att;
    NSFileWrapper *wrapper;
    NSData *tiffdata;
    // NSAttributedString *attstr;

    if (storedNewline) {
        [textstorage appendAttributedString:storedNewline];
        storedNewline = nil;
        _lastchar = '\n';
    }

    [_textview resetTextFinder];

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

        image = [self scaleImage:image size:NSMakeSize(w, h)];

        tiffdata = image.TIFFRepresentation;

        [container addImage:[[NSImage alloc] initWithData:tiffdata]
                      align:align
                         at:textstorage.length - 1
                     linkid:(NSUInteger)self.currentHyperlink];

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

        if (self.currentHyperlink) {
            [attstr addAttribute:NSLinkAttributeName value:@(self.currentHyperlink) range:NSMakeRange(0, attstr.length)];
        }

        [textstorage appendAttributedString:attstr];
    }
    [textstorage addAttributes:styles[style] range:NSMakeRange(textstorage.length -1, 1)];
}

- (void)flowBreak {
    [_textview resetTextFinder];

    // NSLog(@"adding flowbreak");
    unichar uc[1];
    uc[0] = NSAttachmentCharacter;
    [textstorage.mutableString appendString:[NSString stringWithCharacters:uc
                                                                     length:1]];
    [container flowBreakAt:textstorage.length - 1];
}

#pragma mark Hyperlinks

- (void)initHyperlink {
    hyper_request = YES;
    _textview.editable = YES;
    //    NSLog(@"txtbuf: hyperlink event requested");
}

- (void)cancelHyperlink {
    hyper_request = NO;
    _textview.editable = NO;
    //    NSLog(@"txtbuf: hyperlink event cancelled");
}

// Make margin image links clickable
- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;

    // Don't draw a caret right now, even if we clicked at the prompt
    [_textview temporarilyHideCaret];

    // NSLog(@"mouseDown in buffer window.");
    if (hyper_request) {
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;
        p = [_textview convertPoint:p fromView:nil];
        p.x -= _textview.textContainerInset.width;
        p.y -= _textview.textContainerInset.height;

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
//    NSLog(@"txtbuf: clicked on link: %@", link);

    if (!hyper_request) {
        NSLog(@"txtbuf: No hyperlink request in window.");
//        return NO;
    }

    GlkEvent *gev =
        [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                              forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    [_textview setEditable:NO];
    return YES;
}

#pragma mark ZColors

- (NSMutableAttributedString *)applyZColorsAndThenReverse:(NSMutableAttributedString *)attStr {

    NSUInteger textstoragelength = attStr.length;

    GlkTextBufferWindow * __unsafe_unretained weakSelf = self;

    [attStr
     enumerateAttribute:@"ZColor"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         NSLog(@"Re-Applying zcolors at range %@", NSStringFromRange(range));
         ZColor *z = value;
         [attStr
          enumerateAttributesInRange:range
          options:0
          usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
              NSUInteger stylevalue = (NSUInteger)((NSNumber *)dict[@"GlkStyle"]).integerValue;
              NSMutableDictionary *mutDict = [dict mutableCopy];
              if ([weakSelf.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)]) {
                  // Style has stylehint_ReverseColor set,
                  // So we apply Zcolor with reversed attributes
                  mutDict = [z reversedAttributes:mutDict];
              } else {
                  // Apply Zcolor normally
                  mutDict = [z coloredAttributes:mutDict];
              }
              [attStr addAttributes:mutDict range:range2];
          }];
     }];

    [attStr
     enumerateAttribute:@"ReverseVideo"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         NSLog(@"Re-Applying reverse video at range %@", NSStringFromRange(range));
         [attStr
          enumerateAttributesInRange:range
          options:0
          usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
              NSUInteger stylevalue = (NSUInteger)((NSNumber *)dict[@"GlkStyle"]).integerValue;
              BOOL zcolorValue = (dict[@"ZColor"] != nil);
              if (!([weakSelf.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)] && !zcolorValue)) {
                  NSMutableDictionary *mutDict = [dict mutableCopy];
                  mutDict = [weakSelf reversedAttributes:mutDict background:self.theme.gridBackground];
                  [attStr addAttributes:mutDict range:range2];
              }
          }];
     }];

    return attStr;
}

- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg {
    if (currentZColor && !(currentZColor.fg == fg && currentZColor.bg == bg)) {
        currentZColor = nil;
    }
    if (!currentZColor && !(fg == zcolor_Default && bg == zcolor_Default)) {
        // A run of zcolor started
        currentZColor =
        [[ZColor alloc] initWithText:fg background:bg];
    }
}

#pragma mark Scrolling

- (void)markLastSeen {
    NSRange glyphs;
    NSRect line;

    if (textstorage.length == 0) {
        _lastseen = 0;
        return;
    }

    glyphs = [layoutmanager glyphRangeForTextContainer:container];

    if (glyphs.length) {
        line = [layoutmanager
            lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphs) - 1
                             effectiveRange:nil];

        _lastseen = (NSInteger)ceil(NSMaxY(line)); // bottom of the line
        // NSLog(@"GlkTextBufferWindow: markLastSeen: %ld", (long)_lastseen);
    }
}

- (void)storeScrollOffset {
//    NSLog(@"GlkTextBufferWindow %ld: storeScrollOffset", self.name);
    if (self.scrolledToBottom) {
        lastAtBottom = YES;
        lastAtTop = NO;
    } else {
        lastAtTop = self.scrolledToTop;
        lastAtBottom = NO;
    }

    if (lastAtBottom || lastAtTop || textstorage.length < 1) {
        return;
    }

    NSRect visibleRect = scrollview.documentVisibleRect;

    lastVisible = [_textview
        characterIndexForInsertionAtPoint:NSMakePoint(NSMaxX(visibleRect),
                                                      NSMaxY(visibleRect))];
    lastVisible--;
    if (lastVisible >= textstorage.length) {
        NSLog(@"lastCharacter index (%ld) is outside textstorage length (%ld)",
              lastVisible, textstorage.length);
        lastVisible = textstorage.length - 1;
    }

//    NSLog(@"storeScrollOffset: lastVisible: %ld", lastVisible);
//
//    NSLog(@"Character %lu is '%@'", lastVisible, [textstorage.string substringWithRange:NSMakeRange(lastVisible, 1)]);
//    if (lastVisible > 11)
//        NSLog(@"Previous 10: '%@'", [textstorage.string substringWithRange:NSMakeRange(lastVisible - 10, 10)]);
//    if (textstorage.length > lastVisible + 12)
//        NSLog(@"Next 10: '%@'", [textstorage.string substringWithRange:NSMakeRange(lastVisible + 1, 10)]);

    NSRect lastRect =
        [layoutmanager lineFragmentRectForGlyphAtIndex:lastVisible
                                        effectiveRange:nil];

    lastScrollOffset = (NSMaxY(visibleRect) - NSMaxY(lastRect)) / self.theme.bufferCellHeight;
                  //  lastLineheight;

//    NSLog(@"(NSMaxY(visibleRect) (%f) - NSMaxY(lastRect)) (%f) / self.theme.bufferCellHeight;: %f = %f", NSMaxY(visibleRect),  NSMaxY(lastRect), self.theme.bufferCellHeight, lastScrollOffset);

    if (isnan(lastScrollOffset) || isinf(lastScrollOffset))
        lastScrollOffset = 0;

//    NSLog(@"lastScrollOffset: %f", lastScrollOffset);
//    NSLog(@"lastScrollOffset as percentage of cell height: %f", (lastScrollOffset / self.theme.bufferCellHeight) * 100);


}

- (void)restoreScroll:(id)sender {
//    NSLog(@"GlkTextBufferWindow %ld restoreScroll", self.name);
//    NSLog(@"lastVisible: %ld lastScrollOffset:%f", lastVisible, lastScrollOffset);
    if (_textview.bounds.size.height <= scrollview.bounds.size.height) {
//        NSLog(@"All of textview fits in scrollview. Returning without scrolling");
        if (_textview.bounds.size.height == scrollview.bounds.size.height) {
            _textview.frame = self.bounds;
        }
        return;
    }

    if (lastAtBottom) {
        [self scrollToBottom];
        return;
    }

    if (lastAtTop) {
        [self scrollToTop];
        return;
    }

    if (!lastVisible) {
        return;
    }

    [self scrollToCharacter:lastVisible withOffset:lastScrollOffset];
    return;
}

- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset {
//    NSLog(@"GlkTextBufferWindow %ld: scrollToCharacter", self.name);

    NSRect line;

    if (character >= textstorage.length - 1 || !textstorage.length) {
        return;
    }

    offset = offset * self.theme.bufferCellHeight;;
    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];

    line = [layoutmanager lineFragmentRectForGlyphAtIndex:character
                                           effectiveRange:nil];

    CGFloat charbottom = NSMaxY(line); // bottom of the line
    if (fabs(charbottom - NSHeight(scrollview.frame)) < self.theme.bufferCellHeight) {
        NSLog(@"scrollToCharacter: too close to the top!");
        [self scrollToTop];
        return;
    }
    charbottom = charbottom + offset;
    NSPoint newScrollOrigin = NSMakePoint(0, floor(charbottom - NSHeight(scrollview.frame)));
    [scrollview.contentView scrollToPoint:newScrollOrigin];
    [scrollview reflectScrolledClipView:scrollview.contentView];
}

- (void)performScroll {
    //    NSLog(@"performScroll: scroll down one screen from _lastseen");

    self.glkctl.shouldScrollOnCharEvent = NO;

    CGFloat bottom;
    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];

    if (textstorage.length == 0)
        return;

    //    [layoutmanager textContainerForGlyphAtIndex:0 effectiveRange:&range];

    // then, get the bottom
    bottom = NSHeight(_textview.frame);

//     scroll so rect from lastseen to bottom is visible
    if (bottom - _lastseen > NSHeight(scrollview.frame)) {
        [_textview scrollRectToVisible:NSMakeRect(0, _lastseen, 0,
                                                  NSHeight(scrollview.frame))];
    } else {
        [self scrollToBottom];
//        [self markLastSeen];
//        [_textview scrollRectToVisible:NSMakeRect(0, _lastseen, 0,
//                                                  bottom - _lastseen)];
    }
}

- (BOOL)scrolledToBottom {
//    NSLog(@"GlkTextBufferWindow %ld: scrolledToBottom?", self.name);
    NSView *clipView = scrollview.contentView;

    // At least the start screen of Kerkerkruip uses a buffer window
    // with height 0 to catch key input.
    if (!clipView || NSHeight(clipView.bounds) == 0) {
        return YES;
    }

    return (NSHeight(_textview.bounds) - NSMaxY(clipView.bounds) < 2 + _textview.textContainerInset.height + _textview.bottomPadding);
}

- (void)scrollToBottom {
    //    NSLog(@"GlkTextBufferWindow %ld scrollToBottom", self.name);

    lastAtTop = NO;
    lastAtBottom = YES;

    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];
    NSPoint newScrollOrigin = NSMakePoint(0, NSMaxY(_textview.frame) - NSHeight(scrollview.contentView.bounds));
    [scrollview.contentView scrollToPoint:newScrollOrigin];
    [scrollview reflectScrolledClipView:scrollview.contentView];
}

- (BOOL)scrolledToTop {
//    NSLog(@"GlkTextBufferWindow %ld scrolledToTop", self.name);
    NSView *clipView = scrollview.contentView;
    if (!clipView) {
        return NO;
    }
    CGFloat diff = clipView.bounds.origin.y;
    return (diff < self.theme.bufferCellHeight);
}

- (void)scrollToTop {
    lastAtTop = YES;
    lastAtBottom = NO;

//    if (NSHeight(_textview.frame) - _textview.textContainerInset.height * 2 < self.frame.size.height) {
//        [self scrollToMiddle];
//        return;
//    }

//    NSLog(@"scrolling window %ld to top", self.name);
    [scrollview.contentView scrollToPoint:NSZeroPoint];
    [scrollview reflectScrolledClipView:scrollview.contentView];
}

- (void)scrollToMiddle {
    [layoutmanager glyphRangeForTextContainer:container];
    NSPoint newScrollOrigin = NSMakePoint(0, ceil((NSMaxY(_textview.frame) - NSHeight(scrollview.contentView.bounds)) / 2));
    [scrollview.contentView scrollToPoint:newScrollOrigin];
    [scrollview reflectScrolledClipView:scrollview.contentView];
}

- (void)postRestoreScrollAdjustment {
    [container invalidateLayout]; // This fixes a bug with margin images on 10.7
    [self restoreScrollBarStyle]; // Windows restoration will mess up the scrollbar style on 10.7

    lastVisible = _restoredLastVisible;
    lastScrollOffset = _restoredScrollOffset;
    lastAtTop = _restoredAtTop;
    lastAtBottom = _restoredAtBottom;
    [self performSelector:@selector(deferredScrollPosition:) withObject:nil afterDelay:0.1];
}

- (void)deferredScrollPosition:(id)sender {
    if (_restoredAtBottom) {
        [self scrollToBottom];
    } else if (_restoredAtTop) {
        [self scrollToTop];
    } else {
        if (_restoredLastVisible == 0)
            [self scrollToBottom];
        else
            [self scrollToCharacter:_restoredLastVisible withOffset:_restoredScrollOffset];
    }
}

- (void)restoreScrollBarStyle {
    if (scrollview) {
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.scrollerStyle = NSScrollerStyleOverlay;
        scrollview.drawsBackground = YES;
        NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];
        if (!bgcolor)
            bgcolor = self.theme.bufferBackground;
        scrollview.backgroundColor = bgcolor;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = YES;
        scrollview.verticalScroller.alphaValue = 100;
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
        _textview.rangeToSpeak_10_7 = NSMakeRange(lastMove.location, 0);

        _textview.shouldSpeak_10_7 = YES;
        NSAccessibilityPostNotification(
            _textview, NSAccessibilitySelectedTextChangedNotification);

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
    _textview.rangeToSpeak_10_7 =
        NSMakeRange(lastMove.location, textstorage.length - lastMove.location);
    _textview.shouldSpeak_10_7 = YES;
    NSAccessibilityPostNotification(
        self, NSAccessibilityFocusedUIElementChangedNotification);
    NSAccessibilityPostNotification(
        _textview, NSAccessibilitySelectedTextChangedNotification);
    NSAccessibilityPostNotification(_textview,
                                    NSAccessibilityValueChangedNotification);
}

- (IBAction)speakPrevious:(id)sender {
    if (!moveRanges.count)
        return;
    if (moveRangeIndex > 0)
        moveRangeIndex--;
    else
        moveRangeIndex = 0;
    [self speakRange:((NSValue *)moveRanges[moveRangeIndex])
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
    [self speakRange:((NSValue *)moveRanges[moveRangeIndex])
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
    if (_textview.shouldSpeak_10_7) {
        _textview.rangeToSpeak_10_7 = NSMakeRange(textstorage.length, 0);

        _textview.shouldSpeak_10_7 = NO;
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
        return _textview;
    } else if ([attribute
                   isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
        return [NSString
            stringWithFormat:
                @"Text window%@%@%@. %@",
                line_request ? @", waiting for commands" : @"",
                char_request ? @", waiting for a key press" : @"",
                hyper_request ? @", waiting for a hyperlink click" : @"",
                [_textview
                    accessibilityAttributeValue:NSAccessibilityValueAttribute]];
    } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
        return [NSNumber numberWithBool:firstResponder == self ||
                                        firstResponder == _textview];
    } else if ([attribute
                   isEqualToString:NSAccessibilityFocusedUIElementAttribute]) {
        return self.accessibilityFocusedUIElement;
    } else if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        return @[ _textview ];
    }

    return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityFocusedUIElement {
    return _textview;
}

@end
