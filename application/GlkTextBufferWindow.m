/*
 * NSTextView associated with a glk window
 */

#import "main.h"
#import "GlkHyperlink.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif

@implementation MyAttachmentCell

- (instancetype) initImageCell:(NSImage *)image andAlignment:(NSInteger)analignment andAttStr:(NSAttributedString *)anattrstr at:(NSInteger)apos
{
	self = [super initImageCell:image];
	if (self)
	{
		align = analignment;
		attrstr = anattrstr;
		pos = apos;
	}
	return self;
}

- (BOOL) wantsToTrackMouse
{
    return NO;
}

- (NSPoint)cellBaselineOffset
{
	NSDictionary *attributes = [attrstr attributesAtIndex:pos effectiveRange:nil];
	NSFont *font = [attributes valueForKey:NSFontAttributeName];

	CGFloat xheight = font.ascender;
//	NSLog(@"Xheight = %f", xheight);
	//
	//[@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height;

	if (align == imagealign_InlineCenter)
		return NSMakePoint(0, -(self.image.size.height / 2) + font.xHeight / 2);

	if (align == imagealign_InlineDown || align == imagealign_MarginLeft)
		return NSMakePoint(0, -self.image.size.height + xheight);

	return [super cellBaselineOffset];
}

@end


@interface FlowBreak : NSObject
{
	BOOL recalc;
}

- (instancetype) initWithPos: (NSInteger)pos;
- (NSRect) boundsWithLayout: (NSLayoutManager*)layout;

@property NSRect bounds;
@property NSInteger pos;

@end

@implementation FlowBreak

- (instancetype) init
{
	return [self initWithPos:0];
}

- (instancetype) initWithPos: (NSInteger)apos
{
	self = [super init];
	if (self)
	{
		_pos = apos;
		_bounds = NSZeroRect;
		recalc = YES;
	}
	return self;
}

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout
{
	NSRange ourglyph;
	NSRange ourline;
	NSRect theline;

	if (recalc && _pos != 0)
	{
		recalc = NO;	/* don't infiniloop in here, settle for the first result */

		/* force layout and get position of anchor glyph */
		ourglyph = [layout glyphRangeForCharacterRange: NSMakeRange(_pos, 1)
								  actualCharacterRange: &ourline];
		theline = [layout lineFragmentRectForGlyphAtIndex: ourglyph.location
										   effectiveRange: nil];

		_bounds =  NSMakeRect(0, theline.origin.y, FLT_MAX, theline.size.height);

		/* invalidate our fake layout *after* we set the bounds ... to avoid infiniloop */
        [layout invalidateLayoutForCharacterRange: ourline
                             actualCharacterRange: nil];
	}

	return _bounds;
}

- (void) uncacheBounds
{
	recalc = YES;
	_bounds = NSZeroRect;
}

@end

/* ------------------------------------------------------------ */

/*
 * Extend NSTextContainer to handle images in the margins,
 * with the text flowing around them like in HTML.
 */

@interface MarginImage : NSObject
{
	BOOL recalc;
	NSTextContainer *container;
}

@property (strong) NSImage *image;
@property (readonly) NSInteger alignment;
@property NSInteger pos;
@property NSRect bounds;
@property NSUInteger linkid;

- (instancetype) initWithImage: (NSImage*)animage align: (NSInteger)analign linkid:(NSInteger)linkid at: (NSInteger)apos sender:(id)sender;

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout;

@end

@implementation MarginImage

- (instancetype) init
{
	return [self initWithImage:[[NSImage alloc] initWithContentsOfFile:@"../Resources/Question.png"] align:kAlignLeft linkid:0 at:0 sender:self];
}

- (instancetype) initWithImage: (NSImage*)animage align: (NSInteger)analign linkid:(NSInteger)linkid at: (NSInteger)apos sender:(id)sender
{
    self = [super init];
    if (self)
    {
		_image = animage;
        _alignment = analign;
        _bounds = NSZeroRect;
		_linkid = linkid;
		_pos = apos;
        recalc = YES;
		container = sender;
    }
    return self;
}

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout
{
    NSRange ourglyph;
    NSRange ourline;
    NSRect theline;
    NSSize size = _image.size;

    if (recalc)
    {
        recalc = NO;	/* don't infiniloop in here, settle for the first result */

        _bounds = NSZeroRect;

        /* force layout and get position of anchor glyph */
        ourglyph = [layout glyphRangeForCharacterRange: NSMakeRange(_pos, 1)
                                  actualCharacterRange: &ourline];
        theline = [layout lineFragmentRectForGlyphAtIndex: ourglyph.location
                                           effectiveRange: nil];

        /* set bounds to be at the same line as anchor but in left/right margin */
        if (_alignment == imagealign_MarginRight)
        {
            CGFloat rightMargin = container.textView.frame.size.width - container.textView.textContainerInset.width * 2 - container.lineFragmentPadding - 10 * (NSAppKitVersionNumber <= 1139);
            // The extra last 10 points is to prevent the scrollbar from cutting off the right edge of right-aligned
            // margin images on at least 10.7. It creates an ugly asymmetric right margin, so it would be nice to
            // find another way.

            _bounds = NSMakeRect(rightMargin - size.width,
                                 theline.origin.y,
                                 size.width,
                                 size.height);

            //NSLog(@"rightMargin = %f, _bounds = %@", rightMargin, NSStringFromRect(_bounds));

            // If the above places the image outside margin, move it within
            if (NSMaxX(_bounds) > rightMargin)
            {
                _bounds.origin.x = rightMargin - size.width;
                //NSLog(@"_bounds outside right margin. Moving it to %@", NSStringFromRect(_bounds));
            }
        }
        else
        {
            _bounds = NSMakeRect(theline.origin.x + container.lineFragmentPadding,
                                 theline.origin.y,
                                 size.width,
                                 size.height);
        }
        
        /* invalidate our fake layout *after* we set the bounds ... to avoid infiniloop */
        [layout invalidateLayoutForCharacterRange: ourline
                             actualCharacterRange: nil];
        
    }
    
    [(MarginContainer *)container unoverlap:self];
    return _bounds;
}

- (void) uncacheBounds
{
    recalc = YES;
    _bounds = NSZeroRect;
}

@end

/* ------------------------------------------------------------ */

@implementation MarginContainer

- (id) initWithContainerSize: (NSSize)size
{
    self = [super initWithContainerSize:size];
    
    margins = [[NSMutableArray alloc] init];
    flowbreaks = [[NSMutableArray alloc] init];

    return self;
}

- (void) clearImages
{
    margins = [[NSMutableArray alloc] init];
	flowbreaks = [[NSMutableArray alloc] init];
    [self.layoutManager textContainerChangedGeometry: self];
}

- (void) invalidateLayout
{
    for (MarginImage *i in margins)
        [i uncacheBounds];

    for (FlowBreak *f in flowbreaks)
        [f uncacheBounds];

    [self.layoutManager textContainerChangedGeometry: self];
}

- (void) addImage: (NSImage*)image align: (NSInteger)align at: (NSInteger)top linkid: (NSUInteger)linkid
{
	MarginImage *mi = [[MarginImage alloc] initWithImage: image align: align linkid:linkid at: top sender: self];
    [margins addObject: mi];
    [self.layoutManager textContainerChangedGeometry: self];
}

- (void) flowBreakAt: (NSInteger)pos
{
	FlowBreak *f = [[FlowBreak alloc] initWithPos: pos];
	[flowbreaks addObject: f];
    [self.layoutManager textContainerChangedGeometry: self];
}

- (BOOL) isSimpleRectangularTextContainer
{
    return [margins count] == 0;
}

- (NSRect) lineFragmentRectForProposedRect: (NSRect) proposed
							sweepDirection: (NSLineSweepDirection) sweepdir
						 movementDirection: (NSLineMovementDirection) movementdir
							 remainingRect: (NSRect*) remaining
{
	NSRect bounds;
	NSRect rect;
	MarginImage *image, *lastleft, *lastright;
	FlowBreak *f;

	rect = [super lineFragmentRectForProposedRect: proposed
								   sweepDirection: sweepdir
								movementDirection: movementdir
									remainingRect: remaining];

	if (margins.count == 0)
		return rect;

	BOOL overlapped = YES;
	NSRect newrect = rect;

	while (overlapped)
	{
		overlapped = NO;
		lastleft = lastright = nil;

		NSEnumerator *enumerator = [margins reverseObjectEnumerator];
		while (image = [enumerator nextObject])
		{
			// I'm not quite sure why, but this prevents the flowbreaks from jumping to incorrect
			// positions when resizing the window
			for (f in flowbreaks)
				if (f.pos > image.pos)
					[f boundsWithLayout:self.layoutManager];

			bounds = [image boundsWithLayout: self.layoutManager];

			if (NSIntersectsRect(bounds, newrect))
			{
				overlapped = YES;

				newrect = [self adjustForBreaks:newrect];

                BOOL overlapped2 = YES;

				// The call above may have moved the rect down, so we need to check if the image still intersects

				while (overlapped2) // = while (NSIntersectsRect(bounds, newrect) || newrect.size.width < 50)
				{
                    overlapped2 = NO;

                    if (NSIntersectsRect(bounds, newrect))
                    {
                        overlapped2 = YES;
                        if (image.alignment == imagealign_MarginLeft)
                        {
                            // If we intersect with a left-aligned image, cut off the left end of the rect

                            newrect.size.width -= (NSMaxX(bounds) - newrect.origin.x);
                            newrect.origin.x = NSMaxX(bounds);
                            lastleft = image;
                        }
                        else  // If the image is right-aligned, cut off the right end of line fragment rect
                        {
                            newrect.size.width = bounds.origin.x - newrect.origin.x;
                            lastright = image;
                        }
                    }
					if (newrect.size.width <= 50)
					{
                        overlapped2 = YES;
                        // If the rect has now become too narrow, push it down and restore original width
                        // 50 is a slightly arbitrary cutoff width
                        newrect.size.width = rect.size.width; // Original width
						newrect.origin.x = rect.origin.x;
						if (lastleft && lastright)
						{
							newrect.origin.y = MIN(NSMaxY(lastright.bounds), NSMaxY(lastleft.bounds));
                            // If the rect is squeezed between a right-aligned and a left-aligned image,
                            // push it down below the highest of the two
						}
						else
						{
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

- (NSRect) adjustForBreaks: (NSRect)rect
{
	FlowBreak *f;
	NSRect flowrect;
	MarginImage *img2;

	NSEnumerator *breakenumerator = [flowbreaks reverseObjectEnumerator];
	while (f = [breakenumerator nextObject])
	{
		flowrect = [f boundsWithLayout:self.layoutManager];

		if (NSIntersectsRect(flowrect, rect))
		{
			NSLog(@"MarginContainer: looking for an image that intersects flowbreak %ld (%@)", [flowbreaks indexOfObject:f], NSStringFromRect(flowrect));

			CGFloat lowest = 0;

			for (img2 in margins)
				// Moving below the lowest image drawn before the flowbreak
                // Flowbreaks are not supposed to affect images drawn after it
				if (img2.pos < f.pos && NSMaxY(img2.bounds) > lowest && NSMaxY(img2.bounds) > rect.origin.y)
				{
					lowest = NSMaxY(img2.bounds) + 1;
				}

			if (lowest)
				rect.origin.y = lowest;
		}
	}

	return rect;
}

- (void) unoverlap: (MarginImage *)image
{
	// Skip if we only have one image, or none, or bounds are empty
	if (margins.count < 2 || NSIsEmptyRect(image.bounds))
		return;

	CGFloat leftMargin = self.textView.textContainerInset.width + self.lineFragmentPadding;
	CGFloat rightMargin = self.textView.frame.size.width  - self.textView.textContainerInset.width * 2 - self.lineFragmentPadding - 10 * (NSAppKitVersionNumber <= 1139);
    // The extra last 10 points is to prevent the scrollbar from cutting off the right edge of right-aligned margin
    // images, at least on 10.7. It creates an ugly asymmetric right margin, though, so it would be nice to find
    // another way.

	NSRect adjustedBounds = image.bounds;

    // If outside margin, move to opposite margin
    if (image.alignment == imagealign_MarginLeft && NSMaxX(adjustedBounds) > rightMargin + 1)
    {
        // NSLog(@"Left-aligned image outside right margin");
        adjustedBounds.origin.x = self.lineFragmentPadding;
    }
    else if (image.alignment == imagealign_MarginRight && adjustedBounds.origin.x < leftMargin - 1)
    {
        //  NSLog(@"Right-aligned image outside left margin");
        adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
    }

    for (NSInteger i = [margins indexOfObject:image] - 1; i >= 0; i--)
    {
        MarginImage *img2 = margins[i];
        
        // If overlapping, shift in opposite alignment direction
        if (NSIntersectsRect(img2.bounds, adjustedBounds))
        {
            if (image.alignment == img2.alignment)
            {
                if (image.alignment == imagealign_MarginLeft)
                {
                    adjustedBounds.origin.x = NSMaxX(img2.bounds) + self.lineFragmentPadding;
                    // Move to the right of image if both left-aligned,
                    // or below and to left margin if we get too close to the right margin
                    if (NSMaxX(adjustedBounds) > rightMargin + 1)
                    {
                        adjustedBounds.origin.x = self.lineFragmentPadding;
                        adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
                    }
                    else if (img2.bounds.origin.y > adjustedBounds.origin.y)
                        adjustedBounds.origin.y = img2.bounds.origin.y;
                    // Try to keep an even upper edge on images pushed down.
                    // This looks nicer and simplifies calculations
                }
                else
                {
                    adjustedBounds.origin.x = img2.bounds.origin.x - adjustedBounds.size.width - self.lineFragmentPadding;
                    // Move to the left of image if both right-aligned,
                    // or below and to right margin if we get too close to the left margin
                    if (adjustedBounds.origin.x < leftMargin - 1)
                    {
                        adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
                        adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
                    }
                    else if (img2.bounds.origin.y > adjustedBounds.origin.y)
                        adjustedBounds.origin.y = img2.bounds.origin.y;
                }
            }
            else
            {
                // If we have collided with an image of opposite alignment,
                // move below it and back to margin
                if (image.alignment == imagealign_MarginLeft)
                    adjustedBounds.origin.x = self.lineFragmentPadding;
                else
                    adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
                adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
            }
        }
    }
    
	image.bounds = adjustedBounds;
}

- (void) drawRect: (NSRect)rect
{
//	NSLog(@"MarginContainer drawRect: %@", NSStringFromRect(rect));

    NSSize inset = self.textView.textContainerInset;
    NSSize size;
    NSRect bounds;
    BOOL extendflag = NO;
    CGFloat extendneeded = 0;

	MarginImage *image;
	NSEnumerator *enumerator = [margins reverseObjectEnumerator];
	while (image = [enumerator nextObject])
	{
		[image boundsWithLayout: self.layoutManager];

		bounds = image.bounds;

		if (!NSIsEmptyRect(bounds))
		{
			bounds.origin.x += inset.width;
			bounds.origin.y += inset.height;

            // Check if we need to add padding to increase textview height to accommodate for low image
            if (self.textView.frame.size.height <= NSMaxY(bounds))
            {

                ((MyTextView *)self.textView).bottomPadding = NSMaxY(bounds) - self.textView.frame.size.height + inset.height;
                extendneeded = ((MyTextView *)self.textView).bottomPadding;
                [self.textView setFrameSize:self.textView.frame.size];
                extendflag = YES;
            }
            // Check if padding is still needed
            else if (self.textView.frame.size.height - ((MyTextView *)self.textView).bottomPadding <= NSMaxY(bounds))
            {
                NSInteger bottom = NSMaxY(bounds) - self.textView.frame.size.height + inset.height;
                if (extendneeded < bottom)
                    extendneeded = bottom;
            }

            
			if (NSIntersectsRect(bounds, rect))
			{
                size = image.image.size;
				[image.image drawInRect: bounds
                                 fromRect: NSMakeRect(0, 0, size.width, size.height)
                                operation: NSCompositeSourceOver
                                 fraction: 1.0
                           respectFlipped:YES
                                    hints:nil];
            }
        }
    }
    // If we were at the bottom before, scroll to bottom of extended area so that we are still at bottom
    if (extendflag && [(MyTextView *)self.textView scrolledToBottom])
        [(MyTextView *)self.textView scrollToBottom];

    // Remove bottom padding if it is not needed any more
    ((MyTextView *)self.textView).bottomPadding = extendneeded;
}

- (NSUInteger) findHyperlinkAt: (NSPoint)p;
{
	for (MarginImage *image in margins)
	{
		if ([self.textView mouse:p inRect:image.bounds])
		{
			NSLog(@"Clicked on image %ld with linkid %ld", [margins indexOfObject:image], image.linkid);
			return image.linkid;
		}
	}
	return 0;
}

- (BOOL) hasMarginImages
{
    return (margins.count > 0);
}

- (NSMutableAttributedString *) marginsToAttachmentsInString: (NSMutableAttributedString *)string
{
    NSTextAttachment *att;
	NSFileWrapper *wrapper;
	NSData *tiffdata;
    MarginImage *image;

    NSEnumerator *enumerator = [margins reverseObjectEnumerator];
    while (image = [enumerator nextObject])
	{
        tiffdata = image.image.TIFFRepresentation;

		wrapper = [[NSFileWrapper alloc] initRegularFileWithContents: tiffdata];
		wrapper.preferredFilename = @"image.tiff";
		att = [[NSTextAttachment alloc] initWithFileWrapper: wrapper];
		NSMutableAttributedString *attstr = (NSMutableAttributedString*)[NSMutableAttributedString attributedStringWithAttachment:att];

		[string insertAttributedString: attstr atIndex:image.pos];
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

- (instancetype) initWithFrame:(NSRect)rect textContainer:(NSTextContainer *)container textBuffer: (GlkTextBufferWindow *)textbuffer
{
    self = [super initWithFrame:rect textContainer:container];
    if (self)
    {
        glkTextBuffer = textbuffer;
        _bottomPadding = 0;
    }
    return self;
}

- (void) superKeyDown: (NSEvent*)evt
{
    [super keyDown: evt];
}

- (void) keyDown: (NSEvent*)evt
{
    [glkTextBuffer onKeyDown: evt];
}

- (void) drawRect: (NSRect)rect
{
    [[NSGraphicsContext currentContext] setImageInterpolation: NSImageInterpolationHigh];
    [super drawRect: rect];
    [(MarginContainer*)self.textContainer drawRect: rect];
}

- (void)scrollToBottom
{
	id view = self.superview;
	while (view && ![view isKindOfClass: [NSScrollView class]])
		view = [view superview];

	NSScrollView *scrollview = (NSScrollView *)view;

	NSPoint newScrollOrigin = NSMakePoint(0.0,NSMaxY(self.frame)
									- NSHeight(scrollview.contentView.bounds));

    if ([glkTextBuffer lastseen] < newScrollOrigin.y)
       newScrollOrigin.y = glkTextBuffer.lastseen;

    [[scrollview contentView] scrollToPoint:newScrollOrigin];
	[scrollview reflectScrolledClipView:[scrollview contentView]];
	//NSLog(@"Scrolled to bottom of scrollview");
}

- (void) performScroll
{
    int bottom;
	NSRange range;
    // first, force a layout so we have the correct textview frame
    [self.layoutManager glyphRangeForTextContainer: self.textContainer];

    if (self.textStorage.length == 0)
        return;

	[self.layoutManager textContainerForGlyphAtIndex:0 effectiveRange:&range];

    id view = self.superview;
	while (view && ![view isKindOfClass: [NSScrollView class]])
		view = [view superview];

	NSScrollView *scrollview = (NSScrollView *)view;

    // then, get the bottom
    bottom = NSHeight(self.frame);

    // scroll so rect from lastseen to bottom is visible
    if (bottom - glkTextBuffer.lastseen > NSHeight(scrollview.frame))
        [self scrollRectToVisible: NSMakeRect(0, glkTextBuffer.lastseen, 0, NSHeight(scrollview.frame))];
    else
        [self scrollRectToVisible: NSMakeRect(0, glkTextBuffer.lastseen, 0, bottom - glkTextBuffer.lastseen)];
}

- (BOOL) scrolledToBottom
{
	NSRange range;
    // first, force a layout so we have the correct textview frame
    [self.layoutManager glyphRangeForTextContainer: self.textContainer];

    if (self.textStorage.length == 0)
        return YES;

	[self.layoutManager textContainerForGlyphAtIndex:0 effectiveRange:&range];

    id view = self.superview;
	while (view && ![view isKindOfClass: [NSScrollView class]])
		view = [view superview];

	NSScrollView *scrollview = (NSScrollView *)view;

    CGFloat scrollViewBottomOffset = ((NSTextView *)scrollview.documentView).frame.size.height - scrollview.bounds.size.height;

    return (NSMaxY(scrollview.documentVisibleRect) >= scrollViewBottomOffset);
}

- (void) mouseDown: (NSEvent*)theEvent
{
	if (![glkTextBuffer myMouseDown:theEvent])
		[super mouseDown:theEvent];
}

- (BOOL) shouldDrawInsertionPoint
{
    BOOL result = [super shouldDrawInsertionPoint];
    
    // Never draw a caret if the system doesn't want to. Super overrides glkTextBuffer.
    if (result && !_shouldDrawCaret)
        result = _shouldDrawCaret;
    
    return result;
}

- (void) enableCaret:(id)sender
{
    _shouldDrawCaret = YES;
}

- (void) temporarilyHideCaret
{
    _shouldDrawCaret = NO;
    [NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(enableCaret:) userInfo:nil repeats:NO];
}

- (void)setFrameSize:(NSSize)newSize
{
    newSize.height += _bottomPadding;
    [super setFrameSize:newSize];
}

- (BOOL) validateMenuItem:(NSMenuItem *)menuItem
{
    BOOL isValidItem = NO;
    BOOL waseditable = self.editable;
    self.editable = NO;

    if (menuItem.action == @selector(cut:))
    {
        if (self.selectedRange.length &&
            [glkTextBuffer textView: self
            shouldChangeTextInRange: self.selectedRange
                  replacementString: nil])
           self.editable = waseditable;
    }

    else if (menuItem.action == @selector(paste:))
    {
        if ([glkTextBuffer textView: self
            shouldChangeTextInRange: self.selectedRange
                  replacementString: nil])
            self.editable = waseditable;
    }

    if (menuItem.action == @selector(performTextFinderAction:))
    {
        isValidItem = [self.textFinder validateAction:menuItem.tag];
    }

    // validate other menu items if needed
    // ...
    // and don't forget to call the superclass
    else {
        isValidItem = [super validateMenuItem:menuItem];
    }

    self.editable = waseditable;

    return isValidItem;
}

// Text Finder

- (NSTextFinder*) textFinder
{
    // Create the text finder on demand
    if (_textFinder == nil) {
        _textFinder = [[NSTextFinder alloc] init];
        _textFinder.client = self;
        _textFinder.findBarContainer = [self enclosingScrollView];
        _textFinder.incrementalSearchingEnabled = YES;
        _textFinder.incrementalSearchingShouldDimContentView = NO;
    }

    return _textFinder;
}

- (void) resetTextFinder
{
    if  (_textFinder != nil) {
        [_textFinder noteClientStringWillChange];
    }
}

// This is where the commands are actually sent to the text finder
- (void) performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender
{
    BOOL waseditable = self.editable;
    self.editable = NO;
    [self.textFinder performAction:sender.tag];
    self.editable = waseditable;
}

@end

/* ------------------------------------------------------------ */

/*
 * Controller for the various objects required in the NSText* mess.
 */

@implementation GlkTextBufferWindow

- (id) initWithGlkController: (GlkController*)glkctl_ name: (NSInteger)name_
{

    self = [super initWithGlkController: glkctl_ name: name_];

    if (self)
    {
        NSInteger margin = [Preferences bufferMargins];
        NSInteger i;

        char_request = NO;
        line_request = NO;
		hyper_request = NO;
        echo_toggle_pending = NO;
        echo = YES;
        
        fence = 0;
        _lastseen = 0;
        _lastchar = '\n';

        for (i = 0; i < HISTORYLEN; i++)
            history[i] = nil;
        historypos = 0;
        historyfirst = 0;
        historypresent = 0;

		hyperlinks = [[NSMutableArray alloc] init];
		currentHyperlink = nil;

        scrollview = [[NSScrollView alloc] initWithFrame: NSZeroRect];
        [scrollview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
        [scrollview setHasHorizontalScroller: NO];
        [scrollview setHasVerticalScroller: YES];
        [scrollview setAutohidesScrollers: YES];

        [scrollview setBorderType: NSNoBorder];

        /* construct text system manually */

        textstorage = [[NSTextStorage alloc] init];

        layoutmanager = [[NSLayoutManager alloc] init];
        [textstorage addLayoutManager: layoutmanager];

        container = [[MarginContainer alloc] initWithContainerSize: NSMakeSize(0, 10000000)];

        [container setLayoutManager: layoutmanager];
        [layoutmanager addTextContainer: container];

        textview = [[MyTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000) textContainer:container textBuffer:self];

        [textview setMinSize:NSMakeSize(1, 10000000)];
        [textview setMaxSize:NSMakeSize(10000000, 10000000)];

        [container setTextView: textview];

        [scrollview setDocumentView: textview];

        /* now configure the text stuff */


        [container setWidthTracksTextView: YES];
        [container setHeightTracksTextView: NO];

        [textview setHorizontallyResizable: NO];
        [textview setVerticallyResizable: YES];

        [textview setAutoresizingMask: NSViewWidthSizable];

        [textview setAllowsImageEditing: NO];
        [textview setAllowsUndo: NO];
        [textview setUsesFontPanel: NO];
        [textview setUsesFindBar:YES];
        [textview setIncrementalSearchingEnabled:YES];

        [textview setSmartInsertDeleteEnabled: NO];

        [textview setDelegate: self];
        [textstorage setDelegate: self];

        [textview setTextContainerInset: NSMakeSize(margin - 3, margin)];
        [textview setBackgroundColor: [Preferences bufferBackground]];
        [textview setInsertionPointColor: [Preferences bufferForeground]];

        [textview enableCaret:nil];

        // disabling screen fonts will force font smoothing and kerning.
        // using screen fonts will render ugly and uneven text and sometimes
        // even bitmapped fonts.
        [layoutmanager setUsesScreenFonts: [Preferences useScreenFonts]];

        [self addSubview: scrollview];
    }

    return self;
}

- (BOOL) allowsDocumentBackgroundColorChange { return YES; }

- (void)changeDocumentBackgroundColor:(id)sender
{
	NSLog(@"changeDocumentBackgroundColor");
}

- (void) recalcBackground
{
    NSColor *bgcolor, *fgcolor;

    bgcolor = nil;
    fgcolor = nil;

    [textview resetTextFinder];

    if ([Preferences stylesEnabled])
    {
        bgcolor = [styles[style_Normal] attributes][NSBackgroundColorAttributeName];
        fgcolor = [styles[style_Normal] attributes][NSForegroundColorAttributeName];
    }

    if (!bgcolor)
        bgcolor = [Preferences bufferBackground];
    if (!fgcolor)
        fgcolor = [Preferences bufferForeground];

    [textview setBackgroundColor: bgcolor];
    [textview setInsertionPointColor: fgcolor];
}

- (void) setStyle: (NSInteger)style windowType: (NSInteger)wintype enable: (NSInteger*)enable value:(NSInteger*)value
{
    [super setStyle:style windowType:wintype enable:enable value:value];
    [self recalcBackground];
}

- (void) prefsDidChange
{
    NSRange range = NSMakeRange(0, 0);
    NSRange linkrange = NSMakeRange(0, 0);
    NSInteger x;

    [super prefsDidChange];

    NSInteger margin = [Preferences bufferMargins];
    [textview setTextContainerInset: NSMakeSize(margin - 3, margin)];
    [self recalcBackground];

    [textstorage removeAttribute: NSBackgroundColorAttributeName
                           range: NSMakeRange(0, [textstorage length])];

    /* reassign attribute dictionaries */
    x = 0;
    while (x < [textstorage length])
    {
        id styleobject = [textstorage attribute:@"GlkStyle" atIndex:x effectiveRange:&range];

		NSDictionary * attributes = [self attributesFromStylevalue:[styleobject intValue]];

        id image = [textstorage attribute: @"NSAttachment" atIndex:x effectiveRange:NULL];
		id hyperlink = [textstorage attribute: NSLinkAttributeName atIndex:x effectiveRange:&linkrange];
        [textstorage setAttributes: attributes range: range];

        if (image)
        {
            [textstorage addAttribute: @"NSAttachment"
                                value: image
                                range: NSMakeRange(x, 1)];
        }

		if (hyperlink)
		{
			[textstorage addAttribute: NSLinkAttributeName
								value: hyperlink
								range: linkrange];
		}

        x = range.location + range.length;
    }

    [layoutmanager setUsesScreenFonts: [Preferences useScreenFonts]];
    [container invalidateLayout];
}

- (void) setFrame: (NSRect)frame
{
    if (NSEqualRects(frame, [self frame]))
        return;
    [super setFrame: frame];
    [container invalidateLayout];
}

- (void) saveAsRTF: (id)sender
{
    NSWindow* window = glkctl.window;
    BOOL isRtfd = NO;
    NSString* newExtension = @"rtf";
    if ([textstorage containsAttachments] || [container hasMarginImages])
    {
        newExtension = @"rtfd";
        isRtfd = YES;
    }
    NSString* newName = [window.title.stringByDeletingPathExtension stringByAppendingPathExtension:newExtension];

    // Set the default name for the file and show the panel.

    NSSavePanel* panel = [NSSavePanel savePanel];
    //[panel setNameFieldLabel: @"Save Scrollback: "];
    panel.nameFieldLabel = @"Save Text: ";
    panel.allowedFileTypes=@[newExtension];
    panel.extensionHidden=NO;
    [panel setCanCreateDirectories:YES];
    panel.nameFieldStringValue = newName;
    [panel beginSheetModalForWindow:window completionHandler:^(NSInteger result){
        if (result == NSFileHandlingPanelOKButton)
        {
            NSURL* theFile = panel.URL;

            NSMutableAttributedString *mutattstr = [textstorage mutableCopy];

            mutattstr = [container marginsToAttachmentsInString:mutattstr];

            [mutattstr addAttribute:NSBackgroundColorAttributeName value:textview.backgroundColor range:NSMakeRange(0, mutattstr.length)];
            
            if (isRtfd)
            {
                NSFileWrapper *wrapper;
                wrapper = [mutattstr RTFDFileWrapperFromRange: NSMakeRange(0, mutattstr.length)
                                             documentAttributes: @{NSDocumentTypeDocumentAttribute: NSRTFDTextDocumentType}];

                [wrapper writeToURL:theFile options: NSFileWrapperWritingAtomic | NSFileWrapperWritingWithNameUpdating originalContentsURL:nil error:NULL];

            }
            else
            {
                NSData *data;
                data = [mutattstr RTFFromRange: NSMakeRange(0, mutattstr.length)
                              documentAttributes:@{NSDocumentTypeDocumentAttribute: NSRTFTextDocumentType}];
                [data writeToURL: theFile atomically:NO];
            }
        }
    }];
}

- (NSImage*) scaleImage: (NSImage*)src size: (NSSize)dstsize
{
    NSSize srcsize = [src size];
    NSImage *dst;

    if (NSEqualSizes(srcsize, dstsize))
        return src;

    dst = [[NSImage alloc] initWithSize: dstsize];
    [dst lockFocus];

    [[NSGraphicsContext currentContext] setImageInterpolation: NSImageInterpolationHigh];

    [src drawInRect: NSMakeRect(0, 0, dstsize.width, dstsize.height)
           fromRect: NSMakeRect(0, 0, srcsize.width, srcsize.height)
          operation: NSCompositeSourceOver
           fraction: 1.0 respectFlipped:YES hints:nil];

    [dst unlockFocus];

    return dst;
}

- (void) drawImage: (NSImage*)image val1: (NSInteger)align val2: (NSInteger)unused width: (NSInteger)w height: (NSInteger)h
{
	NSTextAttachment *att;
	NSFileWrapper *wrapper;
	NSData *tiffdata;
	//NSAttributedString *attstr;

    [textview resetTextFinder];

	if (w == 0)
		w = image.size.width;
	if (h == 0)
		h = image.size.height;

	if (align == imagealign_MarginLeft || align == imagealign_MarginRight)
	{
		if (_lastchar != '\n' && textstorage.length)
		{
			NSLog(@"lastchar is not line break. Do not add margin image.");
			return;
		}

//		NSLog(@"adding image to margins");

		unichar uc[1];
		uc[0] = NSAttachmentCharacter;

		[textstorage.mutableString appendString: [NSString stringWithCharacters: uc length: 1]];

		NSUInteger linkid = 0;
		if (currentHyperlink) linkid = currentHyperlink.index;

		image = [self scaleImage: image size: NSMakeSize(w, h)];
        
        tiffdata = image.TIFFRepresentation;

		[container addImage: [[NSImage alloc] initWithData:tiffdata] align: align at: textstorage.length - 1 linkid:linkid];

//        [container addImage: image align: align at: textstorage.length - 1 linkid:linkid];

	}
	else
	{
//		NSLog(@"adding image to text");

		image = [self scaleImage: image size: NSMakeSize(w, h)];

		tiffdata = image.TIFFRepresentation;

		wrapper = [[NSFileWrapper alloc] initRegularFileWithContents: tiffdata];
		wrapper.preferredFilename = @"image.tiff";
		att = [[NSTextAttachment alloc] initWithFileWrapper: wrapper];
		MyAttachmentCell *cell = [[MyAttachmentCell alloc] initImageCell:image andAlignment:align andAttStr:textstorage at:textstorage.length - 1];
		att.attachmentCell = cell;
		NSMutableAttributedString *attstr = (NSMutableAttributedString*)[NSMutableAttributedString attributedStringWithAttachment:att];

		[textstorage appendAttributedString: attstr];
	}
}

- (void) flowBreak
{
    [textview resetTextFinder];

    // NSLog(@"adding flowbreak");
    unichar uc[1];
    uc[0] = NSAttachmentCharacter;
    [[textstorage mutableString] appendString: [NSString stringWithCharacters: uc length: 1]];
    [container flowBreakAt: [textstorage length] - 1];
}

- (void) markLastSeen
{
    NSRange glyphs;
    NSRect line;

    glyphs = [layoutmanager glyphRangeForTextContainer: container];

    if (glyphs.length)
    {
        line = [layoutmanager lineFragmentRectForGlyphAtIndex: glyphs.location + glyphs.length - 1
                                               effectiveRange: nil];

        _lastseen = line.origin.y + line.size.height; // bottom of the line
    }
}

- (void) performScroll
{    
	[(MyTextView *)textview performScroll];
}

- (void)scrollToBottom
{
	[(MyTextView *)textview scrollToBottom];
}

- (void) saveHistory: (NSString*)line
{
    if (history[historypresent])
    {
        history[historypresent] = nil;
    }

    history[historypresent] = line;

    historypresent ++;
    if (historypresent >= HISTORYLEN)
        historypresent -= HISTORYLEN;

    if (historypresent == historyfirst)
    {
        historyfirst ++;
        if (historyfirst > HISTORYLEN)
            historyfirst -= HISTORYLEN;
    }

    if (history[historypresent])
    {
        history[historypresent] = nil;
    }
}

- (void) travelBackwardInHistory
{
    [textview resetTextFinder];

    NSString *cx;

    if (historypos == historyfirst)
        return;

    if (historypos == historypresent)
    {
        /* save the edited line */
        if ([textstorage length] - fence > 0)
            cx = [[textstorage string] substringWithRange: NSMakeRange(fence, [textstorage length] - fence)];
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

    [textstorage replaceCharactersInRange: NSMakeRange(fence, [textstorage length] - fence)
                               withString: cx];
}

- (void) travelForwardInHistory
{
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

    [textstorage replaceCharactersInRange: NSMakeRange(fence, [textstorage length] - fence)
                               withString: cx];
}

- (void) onKeyDown: (NSEvent*)evt
{
    GlkEvent *gev;
    NSString *str = [evt characters];
    unsigned ch = keycode_Unknown;
    if ([str length])
        ch = chartokeycode([str characterAtIndex: 0]);

    NSUInteger flags = [evt modifierFlags];

    GlkWindow *win;

    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (int i = 0; i < MAXWIN; i++)
        {
            win = [glkctl windowWithNum:i];
            if (i != self.name && win && win.wantsFocus)
            {
                [win grabFocus];
                NSLog(@"Passing on keypress");
                if ([win isKindOfClass: [GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)win onKeyDown:evt];
                else
                    [win keyDown:evt];
                return;
            }
        }

    BOOL commandKeyOnly = ((flags & NSCommandKeyMask) && !(flags & (NSAlternateKeyMask | NSShiftKeyMask | NSControlKeyMask |NSHelpKeyMask)));
    BOOL optionKeyOnly = ((flags & NSAlternateKeyMask) && !(flags & (NSCommandKeyMask | NSShiftKeyMask | NSControlKeyMask | NSHelpKeyMask)));

    if (ch == keycode_Up)
    {
        if (optionKeyOnly)
            ch = keycode_PageUp;
        else if (commandKeyOnly)
            ch = keycode_Home;
    }
    else if (ch == keycode_Down)
    {
        if (optionKeyOnly)
            ch = keycode_PageDown;
        else if (commandKeyOnly)
            ch = keycode_End;
    }

	NSNumber *key = @(ch);

    // if not scrolled to the bottom, pagedown or navigate scrolling on each key instead
    if (NSMaxY(textview.visibleRect) < NSMaxY(textview.bounds) - 5 - textview.bottomPadding)
    {
        NSRect promptrect = [layoutmanager lineFragmentRectForGlyphAtIndex: textstorage.length - 1
                                                        effectiveRange: nil];

        // Skip if we are scrolled to input prompt
        if (NSMaxY(textview.visibleRect) < NSMaxY(promptrect))
        {
            switch (ch)
            {
                case keycode_PageUp:
                case keycode_Delete:
                    [textview scrollPageUp: nil];
                    return;
                case keycode_PageDown:
                case ' ':
                    [textview scrollPageDown: nil];
                    return;
                case keycode_Up:
                    [textview scrollLineUp: nil];
                    return;
                case keycode_Down:
                case keycode_Return:
                    [textview scrollLineDown: nil];
                    return;
                default:
                    [self performScroll];
                    break;
            }
        }
    }

    if (char_request && ch != keycode_Unknown)
    {
        NSLog(@"char event from %ld", (long)self.name);

        //[textview setInsertionPointColor:[Preferences bufferForeground]];

        [glkctl markLastSeen];

        gev = [[GlkEvent alloc] initCharEvent: ch forWindow: self.name];
        [glkctl queueEvent: gev];

        char_request = NO;
        [textview setEditable: NO];

    }
    else if (line_request && (ch == keycode_Return || [currentTerminators[key] isEqual: @(YES)]))
    {
        //NSLog(@"line event from %ld", (long)self.name);
        
        [textview resetTextFinder];

        [textview setInsertionPointColor: [Preferences bufferBackground]];

        [glkctl markLastSeen];

        NSString *line = [textstorage.string substringWithRange: NSMakeRange(fence, textstorage.length - fence)];
        if (echo)
        {
            [self putString: @"\n" style: style_Input]; // XXX arranger lastchar needs to be set
            _lastchar = '\n';
        }
        else
            [textstorage deleteCharactersInRange: NSMakeRange(fence, textstorage.length - fence)]; // Don't echo input line

        if ([line length] > 0)
        {
            [self saveHistory: line];
        }

        gev = [[GlkEvent alloc] initLineEvent: line forWindow: self.name];
        [glkctl queueEvent: gev];

        fence = [textstorage length];
        line_request = NO;
        [textview setEditable: NO];
    }

    else if (line_request && ch == keycode_Up)
    {
        [self travelBackwardInHistory];
    }

    else if (line_request && ch == keycode_Down)
    {
        [self travelForwardInHistory];
    }

    else if (line_request && ch == keycode_PageUp && fence == textstorage.length)
    {
        [textview scrollPageUp: nil];
        return;
    }

    else
    {
        if (line_request)
            [self grabFocus];
            
        [textview superKeyDown: evt];
    }
}

- (void) echo: (BOOL)val
{
    if ((!(val) && echo) || (val && !(echo))) // Do we need to toggle echo?
        echo_toggle_pending = YES;
}

- (BOOL) textView: (NSTextView*)textview_ clickedOnLink: (id)link atIndex: (NSUInteger)charIndex
{
    NSLog(@"txtbuf: clicked on link: %@", link);

	if (!hyper_request)
	{
		NSLog(@"txtbuf: No hyperlink request in window.");
		return NO;
	}

	GlkEvent *gev = [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue forWindow: self.name];
	[glkctl queueEvent: gev];

	hyper_request = NO;
	[textview setEditable: NO];
    return YES;
}

// Make margin image links clickable
- (BOOL) myMouseDown: (NSEvent*)theEvent
{
	GlkEvent *gev;

    //Do't draw a caret right now, even if we clicked at the prompt

    [textview temporarilyHideCaret];
    
	//NSLog(@"mouseDown in buffer window.");
	if (hyper_request)
	{
		[glkctl markLastSeen];

		NSPoint p;
		p = theEvent.locationInWindow;
		p = [self convertPoint: p fromView: textview];
		p.x -= textview.textContainerInset.width;
		p.y -= textview.textContainerInset.height;

		NSUInteger linkid = [container findHyperlinkAt:p];
		if (linkid)
		{
			NSLog(@"Clicked margin image hyperlink in buf at %g,%g", p.x, p.y);
			gev = [[GlkEvent alloc] initLinkEvent: linkid forWindow: self.name];
			[glkctl queueEvent: gev];
			hyper_request = NO;
			return YES;
		}
	}
	return NO;
}

- (void) grabFocus
{
    [[self window] makeFirstResponder: textview];
}

- (void) clear
{
    [textview resetTextFinder];

    id att = [[NSAttributedString alloc] initWithString: @""];
    [textstorage setAttributedString: att];
    fence = 0;
    _lastseen = 0;
    _lastchar = '\n';
    [container clearImages];
	hyperlinks = nil;
	hyperlinks = [[NSMutableArray alloc] init];
    [container invalidateLayout];
}

- (void) clearScrollback: (id)sender
{
    NSString *string = [textstorage string];
    NSInteger length = [string length];
    NSInteger save_request = line_request;

    [textview resetTextFinder];

    int prompt;
    int i;

    if (!line_request)
        fence = [string length];

    /* try to rescue prompt line */
    for (i = 0; i < length; i++)
    {
        if ([string characterAtIndex: length - i - 1] == '\n')
            break;
    }
    if (i < length)
        prompt = i;
    else
        prompt = 0;

    line_request = NO;

    [textstorage replaceCharactersInRange: NSMakeRange(0, fence - prompt) withString: @""];
    _lastseen = 0;
    _lastchar = '\n';
    fence = prompt;

    line_request = save_request;

    [container clearImages];
	hyperlinks = nil;
}

- (void) putString:(NSString*)str style:(NSInteger)stylevalue
{
    [textview resetTextFinder];

    NSAttributedString *attstr = [[NSAttributedString alloc] initWithString:str attributes:[self attributesFromStylevalue:stylevalue]];

	[textstorage appendAttributedString: attstr];

    _lastchar = [str characterAtIndex: [str length] - 1];
}

- (void) initChar
{
    //NSLog(@"init char in %d", name);

    // [glkctl performScroll];

    fence = [textstorage length];

    char_request = YES;
    [textview setInsertionPointColor:[Preferences bufferBackground]];
    [textview setEditable: YES];

    [textview setSelectedRange: NSMakeRange(fence, 0)];
}

- (void) cancelChar
{
    //NSLog(@"cancel char in %d", name);

    char_request = NO;
    [textview setEditable: NO];
}

- (void) initLine:(NSString*)str
{
    //NSLog(@"initLine: %@ in: %d", str, name);

    historypos = historypresent;

    // [glkctl performScroll];

    [textview resetTextFinder];

	if (self.terminatorsPending)
	{
		currentTerminators = self.pendingTerminators;
		self.terminatorsPending = NO;
	}

    if (echo_toggle_pending)
    {
        echo_toggle_pending = NO;
        echo = !echo;
    }

    if (_lastchar == '>' && [Preferences spaceFormat])
    {
        [self putString: @" " style: style_Normal];
    }

    fence = [textstorage length];

    id att = [[NSAttributedString alloc] initWithString: str
                                             attributes: styles[style_Input].attributes];
    [textstorage appendAttributedString: att];

    [textview setInsertionPointColor: [Preferences bufferForeground]];

    [textview setEditable: YES];

    line_request = YES;

    [textview setSelectedRange: NSMakeRange([textstorage length], 0)];
}

- (NSString*) cancelLine
{
    [textview resetTextFinder];

    [textview setInsertionPointColor: [Preferences bufferBackground]];
    NSString *str = [textstorage string];
    str = [str substringWithRange: NSMakeRange(fence, [str length] - fence)];
    if (echo)
	{
		[self putString: @"\n" style: style_Input];
		_lastchar = '\n'; // [str characterAtIndex: str.length - 1];
	}
    else
        [textstorage deleteCharactersInRange: NSMakeRange(fence, textstorage.length - fence)]; // Don't echo input line

    [textview setEditable: NO];
    line_request = NO;
    return str;
}

- (void) setHyperlink:(NSInteger)linkid
{
	NSLog(@"txtbuf: hyperlink %ld set", (long)linkid);

	if (currentHyperlink && currentHyperlink.index != linkid)
	{
		NSLog(@"There is a preliminary hyperlink, with index %ld", currentHyperlink.index);
		if (currentHyperlink.startpos >= textstorage.length)
		{
			NSLog(@"The preliminary hyperlink started at the end of current input, so it was deleted. currentHyperlink.startpos == %ld, textstorage.length == %ld", currentHyperlink.startpos, textstorage.length);
			currentHyperlink = nil;
		}
		else
		{
            [textview resetTextFinder];

			currentHyperlink.range = NSMakeRange(currentHyperlink.startpos, textstorage.length - currentHyperlink.startpos);
			[textstorage addAttribute:NSLinkAttributeName value:@(currentHyperlink.index) range:currentHyperlink.range];
			[hyperlinks addObject:currentHyperlink];
			currentHyperlink = nil;
		}
	}
	if (!currentHyperlink && linkid)
	{
		currentHyperlink = [[GlkHyperlink alloc] initWithIndex:linkid andPos:textstorage.length];
		NSLog(@"New preliminary hyperlink started at position %ld, with link index %ld", currentHyperlink.startpos,linkid);

	}
}

- (void) initHyperlink
{
	hyper_request = YES;
	textview.editable = YES;
//	NSLog(@"txtbuf: hyperlink event requested");

}

- (void) cancelHyperlink
{
	hyper_request = NO;
	textview.editable = NO;
//	NSLog(@"txtbuf: hyperlink event cancelled");
}

- (void) terpDidStop
{
    [textview setEditable: NO];
}

- (BOOL) wantsFocus
{
    return char_request || line_request;
}

- (BOOL) textView: (NSTextView*)aTextView
shouldChangeTextInRange: (NSRange)range
replacementString: (id)repl
{
    if (line_request && range.location >= fence)
    {   textview.shouldDrawCaret = YES;
        return YES;
    }
    
    textview.shouldDrawCaret = NO;
    return NO;
}

- (void) textStorageWillProcessEditing: (NSNotification*)note
{
    if (!line_request)
        return;

    if ([textstorage editedRange].location < fence)
        return;

    [textstorage setAttributes: [styles[style_Input] attributes]
                         range: [textstorage editedRange]];
}

- (NSRange) textView: (NSTextView *)aTextView
willChangeSelectionFromCharacterRange: (NSRange)oldrange
    toCharacterRange:(NSRange)newrange
{
    if (line_request)
    {
        if (newrange.length == 0)
            if (newrange.location < fence)
                newrange.location = fence;
    }
    else
    {
        if (newrange.length == 0)
            newrange.location = [textstorage length];
    }
    return newrange;
}

@end