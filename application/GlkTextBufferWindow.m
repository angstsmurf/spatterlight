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
	NSFont *font = attributes[NSFontAttributeName];

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
	NSInteger pos;
	NSRect bounds;
	BOOL recalc;

}

- (instancetype) initWithPos: (NSInteger)pos NS_DESIGNATED_INITIALIZER;

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout;

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
		pos = apos;
		bounds = NSZeroRect;
		recalc = YES;
	}
	return self;
}

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout
{
	NSRange ourglyph;
	NSRange ourline;
	NSRect theline;

	if (recalc && pos != 0)
	{
		recalc = NO;	/* don't infiniloop in here, settle for the first result */

		/* force layout and get position of anchor glyph */
		ourglyph = [layout glyphRangeForCharacterRange: NSMakeRange(pos, 1)
								  actualCharacterRange: &ourline];
		theline = [layout lineFragmentRectForGlyphAtIndex: ourglyph.location
										   effectiveRange: nil];

		bounds = theline;

		/* invalidate our fake layout *after* we set the bounds ... to avoid infiniloop */
		[layout invalidateLayoutForCharacterRange: ourline
										   isSoft: NO
							 actualCharacterRange: nil];
	}

	return NSMakeRect(0, bounds.origin.y, FLT_MAX, bounds.size.height);
}

- (void) uncacheBounds
{
	recalc = YES;
	bounds = NSZeroRect;
}

@end

/* ------------------------------------------------------------ */

/*
 * Extend NSTextContainer to handle images in the margins,
 * with the text flowing around them like in HTML.
*/



@implementation MarginImage

- (instancetype) init
{
	return [self initWithImage:[[NSImage alloc] initWithContentsOfFile:@"../Resources/Question.png"] align:kAlignLeft at:0 size:NSZeroSize sender:self];
}

- (instancetype) initWithImage: (NSImage*)animage align: (NSInteger)analign at: (NSInteger)apos size: (NSSize)asize sender:(id)sender
{
    self = [super init];
    if (self)
    {
        image = animage;
        align = analign;
        pos = apos;
        size = asize;
        _bounds = NSZeroRect;
		_linkid = 0;
        recalc = YES;
		container = sender;
    }
    return self;
}

- (NSImage*) image { return image; }
- (NSInteger) position { return pos; }
- (NSInteger) alignment { return align; }

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout
{
    NSRange ourglyph;
    NSRange ourline;
    NSRect theline;

    if (recalc)
    {
        recalc = NO;	/* don't infiniloop in here, settle for the first result */
        
        _bounds = NSZeroRect;
        
        /* force layout and get position of anchor glyph */
        ourglyph = [layout glyphRangeForCharacterRange: NSMakeRange(pos, 1)
                                  actualCharacterRange: &ourline];
        theline = [layout lineFragmentRectForGlyphAtIndex: ourglyph.location
                                           effectiveRange: nil];

        /* set bounds to be at the same line as anchor but in left/right margin */
        if (align == imagealign_MarginRight)
        {
            _bounds = NSMakeRect(NSMaxX(theline) - size.width,
                                theline.origin.y,
                                size.width,
                                size.height);
        }
        else
        {
            _bounds = NSMakeRect(theline.origin.x,
                                theline.origin.y,
                                size.width,
                                size.height);
        }

        /* invalidate our fake layout *after* we set the bounds ... to avoid infiniloop */
        [layout invalidateLayoutForCharacterRange: ourline
                                           isSoft: NO
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

- (instancetype) initWithSize:(NSSize)size
{
    return [self initWithContainerSize:size];
}

- (instancetype) initWithCoder:(NSCoder *)coder
{
    return [self initWithContainerSize:NSZeroSize];
}

- (instancetype) initWithContainerSize: (NSSize)size
{
    self = [super initWithSize: size];
    
    if (self)
	{
        margins = [[NSMutableArray alloc] init];
		flowbreaks = [[NSMutableArray alloc] init];
		recalc = YES;
//		self.lineFragmentPadding = 10;
	}
    
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
    NSInteger count, i;
    
    count = margins.count;
    for (i = 0; i < count; i++)
        [margins[i] uncacheBounds];

	count = flowbreaks.count;
	for (i = 0; i < count; i++)
		[flowbreaks[i] uncacheBounds];
    
    [self.layoutManager textContainerChangedGeometry: self];
}

- (void) addImage: (NSImage*)image align: (NSInteger)align at: (NSInteger)top size: (NSSize)size linkid: (NSUInteger)linkid
{
	MarginImage *mi = [[MarginImage alloc] initWithImage: image align: align at: top size: size sender: self];
	mi.linkid = linkid;
    [margins addObject: mi];
    [self.layoutManager textContainerChangedGeometry: self];
	[self adjustTextviewHeightForLowImages];
}

- (void) flowBreakAt: (NSInteger)pos
{
	FlowBreak *f = [[FlowBreak alloc] initWithPos: pos];
	[flowbreaks addObject: f];
	NSLog (@"MarginContainer: added flowbreak at pos %ldl", (long)pos);

	[self.layoutManager textContainerChangedGeometry: self];
}

- (BOOL) isSimpleRectangularTextContainer
{
    return margins.count == 0;
}

- (NSRect) lineFragmentRectForProposedRect: (NSRect) proposed
							sweepDirection: (NSLineSweepDirection) sweepdir
						 movementDirection: (NSLineMovementDirection) movementdir
							 remainingRect: (NSRect*) remaining
{
	NSRect bounds;
	NSRect rect, flowrect;
	MarginImage *image;

	rect = [super lineFragmentRectForProposedRect: proposed
								   sweepDirection: sweepdir
								movementDirection: movementdir
									remainingRect: remaining];

//	NSRange range = [[NSATSTypesetter sharedTypesetter] paragraphGlyphRange];

	BOOL overlapped = YES;
	while (overlapped && rect.size.width > 0)
	{
		overlapped = NO;

		NSEnumerator *enumerator = [margins reverseObjectEnumerator];
		while (image = [enumerator nextObject])
		{
			[image boundsWithLayout: self.layoutManager];


//			if (NSLocationInRange(image.position, range) && !NSIntersectsRect(bounds, NSMakeRect(0, rect.origin.y, FLT_MAX, rect.size.height)))
//			{
//				if (!NSIntersectsRect(rect, NSMakeRect(0, bounds.origin.y, FLT_MAX, bounds.size.height)))
//				{
//					NSLog(@"The anchor for image %ld is above its image. Move it down.", [margins indexOfObjectIdenticalTo:image] );
//					rect.origin.y = bounds.origin.y ;
////					rect.origin.x = bounds.origin.x;
////					rect.size.width = proposed.size.width - bounds.origin.x;
//				}
////				else
////				{
//////					NSLog(@"Image %ld is above its anchor. Move it down.", [margins indexOfObjectIdenticalTo:image] );
//////					bounds.origin.y = rect.origin.y;
//////					image.bounds = NSMakeRect(image.bounds.origin.x, rect.origin.y, image.bounds.size.width, image.bounds.size.height);
//////					rect.origin.x = NSMaxX(bounds);
//////					rect.size.width = proposed.size.width - NSMaxX(bounds);
////				}
//			}

//			[self unoverlap:image];
			bounds = image.bounds;


			if (NSIntersectsRect(bounds, rect))
			{

//							NSLog(@"MarginContainer:The bounds of image at %@ intersect with the rect %@", NSStringFromRect(bounds),  NSStringFromRect(rect));
				FlowBreak *f;
				MarginImage *img2;
				for (f in flowbreaks)
				{
					flowrect = [f boundsWithLayout:self.layoutManager];
//					NSLog(@"MarginContainer: looking for an image that intersects flowbreak %ld (%@)", [flowbreaks indexOfObject:f], NSStringFromRect(flowrect));

					if (NSIntersectsRect(flowrect, rect))
					{
						BOOL hit = NO;
						MarginImage *flowbreakimage = image;
//						NSLog(@"MarginContainer: hit flowbreak!");
						for (img2 in margins)
						{
							if (NSIntersectsRect(flowrect, img2.bounds))
							{
								flowbreakimage = img2;
//								NSLog(@"Hit flowbreak at image %ld.", [margins indexOfObject:flowbreakimage]);
								hit = YES;

							}
//							else 	NSLog(@"Image %ld (%@) did not intersect flowrect %ld.", [margins indexOfObject:flowbreakimage],NSStringFromRect(img2.bounds),  [flowbreaks indexOfObject:f]);

						}
						if (hit)
						{
//							NSLog(@"Decided on image %ld for flowbreak. Moving below it.", [margins indexOfObjectIdenticalTo:flowbreakimage]);
							if (NSMaxY(flowbreakimage.bounds) > rect.origin.y)
								rect.origin.y = NSMaxY(flowbreakimage.bounds) + 1;
						}
//						else NSLog(@"Found no intersecting image.");
					}
				}

				// We may have moved the rect down, so we need to check again
				if (NSIntersectsRect(bounds, rect))
				{
					if (image.alignment == imagealign_MarginLeft)
						rect.origin.x += bounds.size.width;
					rect.size.width -= bounds.size.width;
					overlapped = YES;
				}

			}
		}
	}

	return rect;
}

- (void) unoverlap: (MarginImage *)image
{
	// Skip if we only have one image, or none, or bounds are empty
	if (margins.count < 2 || NSIsEmptyRect(image.bounds))
		return;

	CGFloat leftMargin = self.textView.textContainerInset.width;
	CGFloat rightMargin = self.textView.frame.size.width  - self.textView.textContainerInset.width * 2;

	NSRect adjustedBounds = image.bounds;

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
					adjustedBounds.origin.x += img2.bounds.size.width;
					// Move it one image width the right
				}
				else
				{
					adjustedBounds.origin.x -= img2.bounds.size.width;
					// Move it one image width the left
				}
			}
		}

		// If outside margins, move inside margins and down
		if (image.alignment == imagealign_MarginLeft && NSMaxX(adjustedBounds) > rightMargin - 20)
		{
			adjustedBounds.origin.x = 0;
			if (NSMaxY(img2.bounds) > adjustedBounds.origin.y)
				adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
		}
		else if (image.alignment == imagealign_MarginRight && adjustedBounds.origin.x < leftMargin + 20)
		{
			adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
			if (NSMaxY(img2.bounds) > adjustedBounds.origin.y)
				adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
		}
		// If still overlapping, move to margins and down

		if (NSIntersectsRect(img2.bounds, adjustedBounds))
		{
			if (image.alignment == imagealign_MarginLeft)
			{
				adjustedBounds.origin.x = 0;
			}
			else
			{
				adjustedBounds.origin.x = rightMargin - adjustedBounds.size.width;
			}
			if (NSMaxY(img2.bounds) > adjustedBounds.origin.y)
				adjustedBounds.origin.y = NSMaxY(img2.bounds) + 1;
		}
	}
	image.bounds = adjustedBounds;
}


- (void) adjustTextviewHeightForLowImages
{
	for (MarginImage *image in margins)
	{
		if (self.textView.frame.size.height < NSMaxY(image.bounds))
		{
			[self.textView setFrameSize:NSMakeSize(self.textView.frame.size.width, NSMaxY(image.bounds) + self.textView.textContainerInset.height)];
//			[self.textView setNeedsDisplay:YES];
		}
	}
}

- (void) drawRect: (NSRect)rect
{
//	NSLog(@"MarginContainer drawRect: %@", NSStringFromRect(rect));

    NSSize inset = self.textView.textContainerInset;
    NSSize size;
    NSRect bounds;

	MarginImage *image;
	NSEnumerator *enumerator = [margins reverseObjectEnumerator];
	while (image = [enumerator nextObject])
    {
		[image boundsWithLayout: self.layoutManager];
		[self unoverlap:image];
		bounds = image.bounds;

		if (!NSIsEmptyRect(bounds))
		{
			bounds.origin.x += inset.width;
			bounds.origin.y += inset.height;

			if (NSIntersectsRect(bounds, rect))
			{
				size = image.image.size;
				if (self.textView.frame.size.height < NSMaxY(bounds))
				{
					[self.textView setFrameSize:NSMakeSize(self.textView.frame.size.width, NSMaxY(bounds) + inset.height)];
					[(MyTextView *)self.textView scrollToBottom];
					[self.textView setNeedsDisplay:YES];
				}
				[image.image drawInRect: bounds
							   fromRect: NSMakeRect(0, 0, size.width, size.height)
							  operation: NSCompositeSourceOver
							   fraction: 1.0
						 respectFlipped:YES
								  hints:nil];
			}
		}
    }

//	for (FlowBreak *flowbreak in flowbreaks)
//	{
//		bounds = [flowbreak boundsWithLayout:self.layoutManager];
//
//		NSLog (@"Drawing flowbreak bounds at %@", NSStringFromRect(bounds));
//		NSColor * red = [NSColor redColor];
//		[red set];
//		NSFrameRectWithWidth ( bounds, 1 );
//	}

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

@end

/* ------------------------------------------------------------ */

/*
 * Extend NSTextView to ...
 *   - call onKeyDown on our TextBuffer object
 *   - draw images with high quality interpolation
 */

@implementation MyTextView

- (void) superKeyDown: (NSEvent*)evt
{
    [super keyDown: evt];
}

- (void) keyDown: (NSEvent*)evt
{
    id view = self.superview;
    while (view && ![view isKindOfClass: [GlkTextBufferWindow class]])
        view = [view superview];
    [(GlkTextBufferWindow*)view onKeyDown: evt];
}

- (void) drawRect: (NSRect)rect
{
    [NSGraphicsContext currentContext].imageInterpolation = NSImageInterpolationHigh;
    [super drawRect: rect];
    [(MarginContainer*)self.textContainer drawRect: rect];
}

- (void)scrollToBottom
{
//	[(MarginContainer *)self.textContainer adjustTextviewHeightForLowImages];
	id view = self.superview;
	while (view && ![view isKindOfClass: [NSScrollView class]])
		view = [view superview];

	NSScrollView *scrollview = (NSScrollView *)view;

	NSPoint newScrollOrigin = NSMakePoint(0.0,NSMaxY(scrollview.documentView.frame)
									- NSHeight(scrollview.contentView.bounds));

	[[scrollview contentView] scrollToPoint:newScrollOrigin];
	[scrollview reflectScrolledClipView:[scrollview contentView]];
//	NSLog(@"Scrolled to bottom of scrollview");
}

- (void) mouseDown: (NSEvent*)theEvent
{
	id view = self.superview;
	while (view && ![view isKindOfClass: [GlkTextBufferWindow class]])
		view = [view superview];

//	if (![(GlkTextBufferWindow *)view myMouseDown:theEvent])
		[super mouseDown:theEvent];
}

@end

/* ------------------------------------------------------------ */

/*
 * Controller for the various objects required in the NSText* mess.
 */

@implementation GlkTextBufferWindow

- (instancetype) initWithGlkController: (GlkController*)glkctl_ name: (NSInteger)name_
{
    
    self = [super initWithGlkController: glkctl_ name: name_];
    
    if (self)
    {
        CGFloat margin = [Preferences bufferMargins];
        NSLog(@"GlkTextBufferWindow: initWithGlkController: set margin to %f", margin);
        NSInteger i;
        
        char_request = NO;
        line_request = NO;
		hyper_request = NO;
        echo_toggle_pending = NO;
        echo = YES;
        
        fence = 0;
        lastseen = 0;
        lastchar = '\n';
        
        for (i = 0; i < HISTORYLEN; i++)
            history[i] = nil;
        historypos = 0;
        historyfirst = 0;
        historypresent = 0;

		hyperlinks = [[NSMutableArray alloc] init];
		currentHyperlink = nil;
        
        scrollview = [[NSScrollView alloc] initWithFrame: NSZeroRect];
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
//        scrollview.hasHorizontalScroller = NO;
//        scrollview.hasVerticalScroller = YES;
		scrollview.autohidesScrollers = YES;
        scrollview.borderType = NSNoBorder;
        
        /* construct text system manually */
        
        textstorage = [[NSTextStorage alloc] init];
        
        layoutmanager = [[NSLayoutManager alloc] init];
        [textstorage addLayoutManager: layoutmanager];
        
        container = [[MarginContainer alloc] initWithContainerSize: NSMakeSize(0, 10000000)];
        
        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer: container];
        
        textview = [[MyTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000) textContainer:container];
        
        textview.minSize = NSMakeSize(1, 10000000);
        textview.maxSize = NSMakeSize(10000000, 10000000);
        
        container.textView = textview;
        
        scrollview.documentView = textview;
        
        /* now configure the text stuff */

        [container setWidthTracksTextView: YES];
        [container setHeightTracksTextView: NO];
        
        [textview setHorizontallyResizable: NO];
        [textview setVerticallyResizable: YES];
        
        textview.autoresizingMask = NSViewWidthSizable;
        
        textview.allowsImageEditing = NO;
		textview.allowsUndo = NO;
        textview.usesFontPanel = NO;
        textview.smartInsertDeleteEnabled = NO;
        textview.usesFindBar = YES;
        
        textview.delegate = self;
        textstorage.delegate = self;
        
        textview.textContainerInset = NSMakeSize(margin - 3, margin);
        textview.backgroundColor = [Preferences bufferBackground];
        textview.insertionPointColor = [Preferences bufferForeground];
        
        
        // disabling screen fonts will force font smoothing and kerning.
        // using screen fonts will render ugly and uneven text and sometimes
        // even bitmapped fonts.
//        layoutmanager.usesScreenFonts = [Preferences useScreenFonts];
        
        [self addSubview: scrollview];
    }
    
    return self;
}

//- (void) setBgColor: (NSInteger)bg
//{
//    [super setBgColor: bg];
//    [self recalcBackground];
//}

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
    
    if ([Preferences stylesEnabled])
    {
        bgcolor = styles[style_Normal].attributes[NSBackgroundColorAttributeName];
        fgcolor = styles[style_Normal].attributes[NSForegroundColorAttributeName];

//        if (bgnd != 0)
//        {
//            bgcolor = [Preferences backgroundColor: (int)(bgnd - 1)];
//            if (bgnd == 1) // black
//                fgcolor = [Preferences foregroundColor: 7];
//            else
//                fgcolor = [Preferences foregroundColor: 0];
//        }
    }
    
    if (!bgcolor)
        bgcolor = [Preferences bufferBackground];
    if (!fgcolor)
        fgcolor = [Preferences bufferForeground];
    
    textview.backgroundColor = bgcolor;
    textview.insertionPointColor = fgcolor;
}

- (void) setStyle: (NSInteger)style windowType: (NSInteger)wintype enable: (NSInteger*)enable value:(NSInteger*)value
{
    [super setStyle:style windowType:wintype enable:enable value:value];
    [self recalcBackground];
}

- (void) prefsDidChange
{
    NSRange range;
    //NSRange attrange;
    NSInteger x;
    
    [super prefsDidChange];
    
    CGFloat margin = [Preferences bufferMargins];
    NSLog(@"GlkTextBufferWindow prefsDidChange margin: %f", margin);
    textview.textContainerInset = NSMakeSize(margin - 3, margin);
    [self recalcBackground];
    
    [textstorage removeAttribute: NSBackgroundColorAttributeName
                           range: NSMakeRange(0, textstorage.length)];
    
    /* reassign attribute dictionaries */
    x = 0;
    while (x < textstorage.length)
    {
        id styleobject = [textstorage attribute:@"GlkStyle" atIndex:x effectiveRange:&range];

		NSDictionary * attributes = [self attributesFromStylevalue:[styleobject intValue]];
        
        id image = [textstorage attribute: @"NSAttachment" atIndex:x effectiveRange:NULL];

		id hyperlink = [textstorage attribute: NSLinkAttributeName atIndex:x effectiveRange:&range];

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
								range: range];
		}

        x = range.location + range.length;
    }

	[container invalidateLayout];

}

- (void) setFrame: (NSRect)frame
{
    if (NSEqualRects(frame, self.frame))
        return;
    [super setFrame: frame];
    [container invalidateLayout];
}


- (void) saveAsRTF: (id)sender
{
    NSWindow* window = glkctl.window;
    BOOL isRtfd = NO;
    NSString* newExtension = @"rtf";
    if ([textstorage containsAttachmentsInRange:NSMakeRange(0, textstorage.length)])
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
            NSURL*  theFile = panel.URL;
            if (isRtfd)
            {
                NSFileWrapper *wrapper;
                wrapper = [textstorage RTFDFileWrapperFromRange: NSMakeRange(0, textstorage.length)
                                             documentAttributes: @{NSDocumentTypeDocumentAttribute: NSRTFDTextDocumentType}];
                
                [wrapper writeToURL:theFile options: NSFileWrapperWritingAtomic | NSFileWrapperWritingWithNameUpdating originalContentsURL:nil error:NULL];
                
            }
            else
            {
                NSData *data;
                data = [textstorage RTFFromRange: NSMakeRange(0, textstorage.length)
                              documentAttributes:@{NSDocumentTypeDocumentAttribute: NSRTFTextDocumentType}];
                [data writeToURL: theFile atomically:NO];
            }
        }
    }];
}


- (NSImage*) scaleImage: (NSImage*)src size: (NSSize)dstsize
{
    NSSize srcsize = src.size;
    NSImage *dst;
    
    if (NSEqualSizes(srcsize, dstsize))
        return src;
    
    dst = [[NSImage alloc] initWithSize: dstsize];
    [dst lockFocus];
    
    [NSGraphicsContext currentContext].imageInterpolation = NSImageInterpolationHigh;
    
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

	if (w == 0)
		w = image.size.width;
	if (h == 0)
		h = image.size.height;

	if (align == imagealign_MarginLeft || align == imagealign_MarginRight)
	{

		if (lastchar != '\n' && textstorage.length)
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

		[container addImage: image align: align at: textstorage.length - 1 size: NSMakeSize(w, h) linkid:linkid];

		[self setNeedsDisplay: YES];

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
//	NSLog(@"GlkTextBufferWindow: adding flowbreak");
    unichar uc[1];
    uc[0] = NSAttachmentCharacter;
    [textstorage.mutableString appendString: [NSString stringWithCharacters: uc length: 1]];
    [container flowBreakAt: textstorage.length - 1];
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
        
        lastseen = line.origin.y + line.size.height; // bottom of the line
    }
}

- (void) performScroll
{
    int bottom;
	NSRange range;
    // first, force a layout so we have the correct textview frame
//    [layoutmanager glyphRangeForTextContainer: container];
	[layoutmanager textContainerForGlyphAtIndex:0 effectiveRange:&range];

	[container adjustTextviewHeightForLowImages];
    
    // then, get the bottom
    bottom = textview.frame.size.height;
    
    // scroll so rect from lastseen to bottom is visible
    //NSLog(@"scroll %d -> %d", lastseen, bottom);
    [textview scrollRectToVisible: NSMakeRect(0, lastseen, 0, bottom - lastseen)];
    
    
    //NSLog(@"perform scroll bottom = %d lastseen = %d", bottom, lastseen);
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
    NSString *cx;
    
    if (historypos == historyfirst)
        return;
    
    if (historypos == historypresent)
    {
        /* save the edited line */
        if (textstorage.length - fence > 0)
            cx = [textstorage.string substringWithRange: NSMakeRange(fence, textstorage.length - fence)];
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
    
    [textstorage replaceCharactersInRange: NSMakeRange(fence, textstorage.length - fence)
                               withString: cx];
}

- (void) travelForwardInHistory
{
    NSString *cx;
    
    if (historypos == historypresent)
        return;
    
    historypos++;
    if (historypos >= HISTORYLEN)
        historypos -= HISTORYLEN;
    
    cx = history[historypos];
    if (!cx)
        cx = @"";
    
    [textstorage replaceCharactersInRange: NSMakeRange(fence, textstorage.length - fence)
                               withString: cx];
}

- (void) onKeyDown: (NSEvent*)evt
{
    GlkEvent *gev;
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex: 0]);

	NSNumber *key = [NSNumber numberWithUnsignedInt:ch];

    GlkWindow *win;
    
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (int i = 0; i < MAXWIN; i++)
        {
            win = [glkctl windowWithNum:i];
            if (i != self.name && win && win.wantsFocus)
            {
                NSLog(@"Passing on keypress");
                if ([win isKindOfClass: [GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)win onKeyDown:evt];
                else
                    [win keyDown:evt];
                [win grabFocus];
                return;
            }
        }
    
    // if not scrolled to the bottom, pagedown or navigate scrolling on each key instead
    if (NSMaxY(textview.visibleRect) < NSMaxY(textview.bounds) - 5)
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
                [self scrollToBottom];
                break;
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
    else if (line_request && (ch == keycode_Return || [[currentTerminators objectForKey:key] isEqual: @YES]))
    {
        NSLog(@"line event from window %ld", (long)self.name);
        
        textview.insertionPointColor = [Preferences bufferBackground];
        
        [glkctl markLastSeen];
        
        NSString *line = [textstorage.string substringWithRange: NSMakeRange(fence, textstorage.length - fence)];
        if (echo)
        {
            [self putString: @"\n" style: style_Input]; // XXX arranger lastchar needs to be set
            lastchar = '\n';
        }
        else
            [textstorage deleteCharactersInRange: NSMakeRange(fence, textstorage.length - fence)]; // Don't echo input line
        
        if (line.length > 0)
        {
            [self saveHistory: line];
        }
        
        gev = [[GlkEvent alloc] initLineEvent: line forWindow: self.name];
        [glkctl queueEvent: gev];
        
        fence = textstorage.length;
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
    
    else
    {
        [(MyTextView*)textview superKeyDown: evt];
    }
}

- (void) echo: (BOOL)val
{
    if ((!(val) && echo) || (val && !(echo))) // Do we need to toggle echo?
        echo_toggle_pending = YES;
}

#ifdef GLK_MODULE_HYPERLINKS

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

	NSLog(@"mouseDown in buffer window.");
	if (hyper_request && theEvent.clickCount == 1)
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

#endif

- (void) grabFocus
{
    [self.window makeFirstResponder: textview];
}

- (void) clear
{
    id att = [[NSAttributedString alloc] initWithString: @""];
    [textstorage setAttributedString: att];
    fence = 0;
    lastseen = 0;
    lastchar = '\n';
    [container clearImages];
	hyperlinks = nil;
	hyperlinks = [[NSMutableArray alloc] init];
}

- (void) clearScrollback: (id)sender
{
    NSString *string = textstorage.string;
    NSInteger length = string.length;
    NSInteger save_request = line_request;
    int prompt;
    int i;
    
    if (!line_request)
        fence = string.length;
    
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
    lastseen = 0;
    lastchar = '\n';
    fence = prompt;
    
    line_request = save_request;
    
    [container clearImages];
	hyperlinks = nil;
}

- (void) putString:(NSString*)str style:(NSInteger)stylevalue
{
    NSAttributedString *attstr = [[NSAttributedString alloc] initWithString:str attributes:[self attributesFromStylevalue:stylevalue]];

	[textstorage appendAttributedString: attstr];
    
    lastchar = [str characterAtIndex: str.length - 1];
}

- (NSInteger) lastchar
{
    return lastchar;
}

- (void) initChar
{
    //NSLog(@"init char in %d", name);
    
    // [glkctl performScroll];
    
    fence = textstorage.length;
    
    
    char_request = YES;
    textview.insertionPointColor = [Preferences bufferBackground];
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
    
    if (lastchar == '>' && [Preferences spaceFormat])
    {
        [self putString: @" " style: style_Normal];
    }
    
    fence = textstorage.length;
    
    id att = [[NSAttributedString alloc] initWithString: str
                                             attributes: styles[style_Input].attributes];	
    [textstorage appendAttributedString: att];
    
    textview.insertionPointColor = [Preferences bufferForeground];
    
    [textview setEditable: YES];
    
    line_request = YES;
    
    [textview setSelectedRange: NSMakeRange(textstorage.length, 0)];
}

- (NSString*) cancelLine
{
    textview.insertionPointColor = [Preferences bufferBackground];
    NSString *str = textstorage.string;
    str = [str substringWithRange: NSMakeRange(fence, str.length - fence)];
    if (echo)
	{
		[self putString: @"\n" style: style_Input];
		lastchar = '\n'; // [str characterAtIndex: str.length - 1];
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
			currentHyperlink.range = NSMakeRange(currentHyperlink.startpos, textstorage.length - currentHyperlink.startpos);
			[textstorage addAttribute:NSLinkAttributeName value:[NSNumber numberWithInteger: currentHyperlink.index] range:currentHyperlink.range];
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
	NSLog(@"txtbuf: hyperlink event requested");

}

- (void) cancelHyperlink
{
	hyper_request = NO;
	textview.editable = NO;
	NSLog(@"txtbuf: hyperlink event cancelled");
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
        return YES;
    return NO;
}

- (void) textStorageWillProcessEditing: (NSNotification*)note
{
    if (!line_request)
        return;
    
    if (textstorage.editedRange.location < fence)
        return;
    
    [textstorage setAttributes: styles[style_Input].attributes
                         range: textstorage.editedRange];
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
            newrange.location = textstorage.length;
    }
    return newrange;
}

@end
