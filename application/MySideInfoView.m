//
//  MySideInfoView.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import "MySideInfoView.h"

@implementation MySideInfoView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    // Drawing code here.
}

- (void)viewDidEndLiveResize
{
	if (_game)
	{
		[[self subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
		[self updateSideViewForGame:_game];
	}

	[super viewDidEndLiveResize];
};

- (BOOL) isFlipped { return YES; }

- (NSTextField *) addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView
{

	NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];

	para.minimumLineHeight = font.pointSize + 3;
	para.maximumLineHeight = para.minimumLineHeight;

	if (font.pointSize > 40)
		para.maximumLineHeight = para.maximumLineHeight + 3;

	if (font.pointSize > 25)
		para.maximumLineHeight = para.maximumLineHeight + 3;

	para.alignment = NSTextAlignmentCenter;
	para.lineSpacing = 1;

	if (font.pointSize > 25)
		para.lineSpacing = 0.2f;

	NSMutableDictionary *attr = [NSMutableDictionary dictionaryWithObjectsAndKeys:
								 font,
								 NSFontAttributeName,
								 para,
								 NSParagraphStyleAttributeName,
								 nil];

	NSMutableAttributedString *attrString = [[NSMutableAttributedString alloc] initWithString:text attributes:attr];


	if (font.pointSize == 13.f)
	{
		[attrString addAttribute:NSKernAttributeName value:@1.f range:NSMakeRange(0, text.length)];
	}

	CGRect contentRect = [attrString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin context:nil];
	// I guess the magic number -24 here means that the text field inner width differs 4 points from the outer width. 2-point border?

	NSTextField *textField = [[NSTextField alloc] initWithFrame:contentRect];

	textField.translatesAutoresizingMaskIntoConstraints = NO;

	[textField setBezeled:NO];
	[textField setDrawsBackground:NO];
	[textField setEditable:YES];
	[textField setSelectable:YES];
	[textField setBordered:NO];
	[textField setUsesSingleLineMode:NO];
	[textField setAllowsEditingTextAttributes:YES];

	textField.cell.wraps = YES;
	textField.cell.scrollable = NO;

	[textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationHorizontal];
	[textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationVertical];

	NSLayoutConstraint *xPosConstraint = [NSLayoutConstraint constraintWithItem:textField
												  attribute:NSLayoutAttributeLeft
												  relatedBy:NSLayoutRelationEqual
													 toItem:self
												  attribute:NSLayoutAttributeLeft
												 multiplier:1.0
												   constant:10];

	NSLayoutConstraint *yPosConstraint;

	if (lastView)
	{
		yPosConstraint = [NSLayoutConstraint constraintWithItem:textField
													  attribute:NSLayoutAttributeTop
													  relatedBy:NSLayoutRelationEqual
														 toItem:lastView
													  attribute:NSLayoutAttributeBottom
													 multiplier:1.0
													   constant:space];
	}
	else
	{
		yPosConstraint = [NSLayoutConstraint constraintWithItem:textField
													  attribute:NSLayoutAttributeTop
													  relatedBy:NSLayoutRelationEqual
														 toItem:self
													  attribute:NSLayoutAttributeTop
													 multiplier:1.0
													   constant:space];
	}

	NSLayoutConstraint *widthConstraint = [NSLayoutConstraint constraintWithItem:textField
												   attribute:NSLayoutAttributeWidth
												   relatedBy:NSLayoutRelationEqual
													  toItem:self
												   attribute:NSLayoutAttributeWidth
												  multiplier:1.0
													constant:-20];

	NSLayoutConstraint *rightMarginConstraint = [NSLayoutConstraint constraintWithItem:textField
																			 attribute:NSLayoutAttributeRight
																			 relatedBy:NSLayoutRelationEqual
																				toItem:self
																			 attribute:NSLayoutAttributeRight
																			multiplier:1.0
																			  constant:-10];

	[textField setAttributedStringValue:attrString];

	[self addSubview:textField];

	[self addConstraint:xPosConstraint];
	[self addConstraint:yPosConstraint];
	[self addConstraint:widthConstraint];
	[self addConstraint:rightMarginConstraint];

	NSLayoutConstraint *heightConstraint = [NSLayoutConstraint constraintWithItem:textField
																		attribute:NSLayoutAttributeHeight
																		relatedBy:NSLayoutRelationGreaterThanOrEqual
																		   toItem:nil
																		attribute:NSLayoutAttributeNotAnAttribute
																	   multiplier:1.0
																		 constant: contentRect.size.height + 1];

	[self addConstraint:heightConstraint];
	return textField;
}


- (void) updateSideViewForGame:(Game *)game
{
	NSLayoutConstraint *xPosConstraint;
	NSLayoutConstraint *yPosConstraint;
	NSLayoutConstraint *widthConstraint;
	NSLayoutConstraint *heightConstraint;
	NSLayoutConstraint *rightMarginConstraint;

	NSFont *font;
	CGFloat spaceBefore;
	NSView *lastView;

	self.translatesAutoresizingMaskIntoConstraints = NO;

	NSClipView *clipView = (NSClipView *)self.superview;
	NSScrollView *scrollView = (NSScrollView *)clipView.superview;
	CGFloat superViewWidth = clipView.frame.size.width;

	[clipView addConstraint:[NSLayoutConstraint constraintWithItem:self
														 attribute:NSLayoutAttributeLeft
														 relatedBy:NSLayoutRelationEqual
															toItem:clipView
														 attribute:NSLayoutAttributeLeft
														multiplier:1.0
														  constant:0]];

	[clipView addConstraint:[NSLayoutConstraint constraintWithItem:self
														 attribute:NSLayoutAttributeRight
														 relatedBy:NSLayoutRelationEqual
															toItem:clipView
														 attribute:NSLayoutAttributeRight
														multiplier:1.0
														  constant:0]];

	[clipView addConstraint:[NSLayoutConstraint constraintWithItem:self
														 attribute:NSLayoutAttributeTop
														 relatedBy:NSLayoutRelationEqual
															toItem:clipView
														 attribute:NSLayoutAttributeTop
														multiplier:1.0
														  constant:0]];

	if (game.metadata.cover.data)
	{

		NSImage *theImage = [[NSImage alloc] initWithData:(NSData *)game.metadata.cover.data];

		CGFloat ratio = theImage.size.width / theImage.size.height;

		// We make the image double size to make enlarging when draggin divider to the right work
		[theImage setSize:NSMakeSize(superViewWidth * 2, superViewWidth * 2 / ratio )];

		NSImageView *imageView = [[NSImageView alloc] initWithFrame:NSMakeRect(0,0,theImage.size.width,theImage.size.height)];

		[self addSubview:imageView];

		imageView.imageScaling = NSImageScaleProportionallyUpOrDown;
		imageView.translatesAutoresizingMaskIntoConstraints = NO;

		imageView.imageScaling = NSImageScaleProportionallyUpOrDown;

		xPosConstraint = [NSLayoutConstraint constraintWithItem:imageView
													  attribute:NSLayoutAttributeLeft
													  relatedBy:NSLayoutRelationEqual
														 toItem:self
													  attribute:NSLayoutAttributeLeft
													 multiplier:1.0
													   constant:0];

		yPosConstraint = [NSLayoutConstraint constraintWithItem:imageView
													  attribute:NSLayoutAttributeTop
													  relatedBy:NSLayoutRelationEqual
														 toItem:self
													  attribute:NSLayoutAttributeTop
													 multiplier:1.0
													   constant:0];

		widthConstraint = [NSLayoutConstraint constraintWithItem:imageView
													   attribute:NSLayoutAttributeWidth
													   relatedBy:NSLayoutRelationGreaterThanOrEqual
														  toItem:self
													   attribute:NSLayoutAttributeWidth
													  multiplier:1.0
														constant:0];

		heightConstraint = [NSLayoutConstraint constraintWithItem:imageView
														attribute:NSLayoutAttributeHeight
														relatedBy:NSLayoutRelationLessThanOrEqual
														   toItem:imageView
														attribute:NSLayoutAttributeWidth
													   multiplier:( 1 / ratio)
														 constant:0];

		rightMarginConstraint = [NSLayoutConstraint constraintWithItem:imageView
															 attribute:NSLayoutAttributeRight
															 relatedBy:NSLayoutRelationEqual
																toItem:self
															 attribute:NSLayoutAttributeRight
															multiplier:1.0
															  constant:0];

		[self addConstraint:xPosConstraint];
		[self addConstraint:yPosConstraint];
		[self addConstraint:widthConstraint];
		[self addConstraint:heightConstraint];
		[self addConstraint:rightMarginConstraint];

		imageView.image = theImage;

		lastView = imageView;

	}
	else
	{
//		NSLog(@"No image");
	}

	if (game.metadata.title) // Every game will have a title unless something is broken
	{

		font = [NSFont fontWithName:@"Playfair Display Black" size:30];
		NSFontDescriptor *descriptor = [font fontDescriptor];

		NSArray *array = @[@{NSFontFeatureTypeIdentifierKey : @(kNumberCaseType),
							 NSFontFeatureSelectorIdentifierKey : @(kUpperCaseNumbersSelector)}];

		descriptor = [descriptor fontDescriptorByAddingAttributes:@{NSFontFeatureSettingsAttribute : array}];

		if (game.metadata.title.length > 9)
		{
			font = [NSFont fontWithDescriptor:descriptor size:30];
			//			NSLog(@"Long title (length = %lu), smaller text.", game.metadata.title.length);
		}
		else
		{
			font = [NSFont fontWithDescriptor:descriptor size:50];
		}

		NSString *longestWord = @"";

		for (NSString *word in [game.metadata.title componentsSeparatedByString:@" "])
		{
			if (word.length > longestWord.length) longestWord = word;
		}
//		NSLog (@"Longest word: %@", longestWord);

		// The magic number -24 means 10 points of margin and two points of textfield border on each side.
		while ([longestWord sizeWithAttributes:@{NSFontAttributeName:font}].width > superViewWidth - 24)
		{
			//			NSLog(@"Font too large! Width %f, max allowed %f", [firstWord sizeWithAttributes:@{NSFontAttributeName:font}].width,  superViewWidth - 24);

			font = [[NSFontManager sharedFontManager] convertFont:font toSize:[font pointSize] - 2];
		}
		//		NSLog(@"Font not too large! Width %f, max allowed %f", [firstWord sizeWithAttributes:@{NSFontAttributeName:font}].width,  superViewWidth - 24);

		spaceBefore = [@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height * 0.7;

		lastView = [self addSubViewWithtext:game.metadata.title andFont:font andSpaceBefore:spaceBefore andLastView:lastView];
	}
	else
	{
		NSLog(@"Error! No title!");
		return;
	}

	NSBox *divider = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, superViewWidth, 1)];

	divider.boxType = NSBoxSeparator;
	divider.translatesAutoresizingMaskIntoConstraints = NO;


	xPosConstraint = [NSLayoutConstraint constraintWithItem:divider
												  attribute:NSLayoutAttributeLeft
												  relatedBy:NSLayoutRelationEqual
													 toItem:self
												  attribute:NSLayoutAttributeLeft
												 multiplier:1.0
												   constant:0];

	yPosConstraint = [NSLayoutConstraint constraintWithItem:divider
												  attribute:NSLayoutAttributeTop
												  relatedBy:NSLayoutRelationEqual
													 toItem:lastView
												  attribute:NSLayoutAttributeBottom
												 multiplier:1.0
												   constant:spaceBefore * 0.9];

	widthConstraint = [NSLayoutConstraint constraintWithItem:divider
												   attribute:NSLayoutAttributeWidth
												   relatedBy:NSLayoutRelationEqual
													  toItem:self
												   attribute:NSLayoutAttributeWidth
												  multiplier:1.0
													constant:0];

	heightConstraint = [NSLayoutConstraint constraintWithItem:divider
													attribute:NSLayoutAttributeHeight
													relatedBy:NSLayoutRelationEqual
													   toItem:nil
													attribute:NSLayoutAttributeNotAnAttribute
												   multiplier:1.0
													 constant:1];

	[self addSubview:divider];

	[self addConstraint:xPosConstraint];
	[self addConstraint:yPosConstraint];
	[self addConstraint:widthConstraint];
	[self addConstraint:heightConstraint];

	lastView = divider;

	if (game.metadata.headline)
	{
		font = [NSFont fontWithName:@"Playfair Display Regular" size:13];

		NSFontDescriptor *descriptor = [font fontDescriptor];

		NSArray *array = @[@{NSFontFeatureTypeIdentifierKey : @(kLowerCaseType),
							 NSFontFeatureSelectorIdentifierKey : @(kLowerCaseSmallCapsSelector)}];

		descriptor = [descriptor fontDescriptorByAddingAttributes:@{NSFontFeatureSettingsAttribute : array}];
		font = [NSFont fontWithDescriptor:descriptor size:13.f];

		lastView = [self addSubViewWithtext:[game.metadata.headline lowercaseString] andFont:font andSpaceBefore:4 andLastView:lastView];
	}
	else
	{
//		NSLog(@"No headline");
	}

	if (game.metadata.author)
	{
		font = [NSFont fontWithName:@"Gentium Plus Italic" size:14.f];
		lastView = [self addSubViewWithtext:game.metadata.author andFont:font andSpaceBefore:25 andLastView:lastView];
	}
	else
	{
//		NSLog(@"No author");
	}

	if (game.metadata.blurb)
	{
		font = [NSFont fontWithName:@"Gentium Plus" size:14.f];
		lastView = [self addSubViewWithtext:game.metadata.blurb andFont:font andSpaceBefore:23 andLastView:lastView];
	}
	else
	{
//		NSLog(@"No blurb.");
	}

	NSLayoutConstraint *bottomPinConstraint = [NSLayoutConstraint constraintWithItem:self
																		   attribute:NSLayoutAttributeBottom
																		   relatedBy:NSLayoutRelationEqual
																			  toItem:lastView
																		   attribute:NSLayoutAttributeBottom
																		  multiplier:1.0
																			constant:0];
	[self addConstraint:bottomPinConstraint];

	if (_game != game)
	{
		[clipView scrollToPoint: NSMakePoint(0.0, 0.0)];
		[scrollView reflectScrolledClipView:clipView];
	}

	_game = game;
}

@end

