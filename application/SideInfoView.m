//
//  SideInfoView.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import "SideInfoView.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "LibController.h"
#import "AppDelegate.h"
#import "VerticallyCenteredTextFieldCell.h"

@implementation SideInfoView

//- (instancetype) initWithFrame:(NSRect)frameRect
//{
//	self = [super initWithFrame:frameRect];
//
//	if (self)
//	{
////		ifidField = libctl.sideIfid;
//	}
//	return self;
//}

- (BOOL) isFlipped { return YES; }

//- (void)controlTextDidEndEditing:(NSNotification *)notification
//{
//	if ([notification.object isKindOfClass:[NSTextField class]])
//	{
//		NSTextField *textfield = notification.object;
//		NSLog(@"controlTextDidEndEditing");
//
//		if (textfield == titleField)
//		{
//			_metadata.title = titleField.stringValue;
//		}
//		else if (textfield == headlineField)
//		{
//			_metadata.headline = headlineField.stringValue;
//		}
//		else if (textfield == authorField)
//		{
//			_metadata.author = authorField.stringValue;
//		}
//		else if (textfield == blurbField)
//		{
//			_metadata.blurb = blurbField.stringValue;
//		}
//		else if (textfield == ifidField)
//		{
//			_metadata.ifid = [ifidField.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
//		}
//
//		dispatch_async(dispatch_get_main_queue(), ^{[textfield.window makeFirstResponder:nil];});
//	}
//	[self viewDidEndLiveResize];
//	[libctl updateTableViews];
//}

- (BOOL)textFieldShouldBeginEditing:(NSTextField *)textField{
    return NO;
}

- (BOOL)textField:(NSTextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
//    return textField != _yourReadOnlyTextField;
    return NO;
}

- (NSTextField *) addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView
{

    NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];

    para.minimumLineHeight = font.pointSize + 3;
    para.maximumLineHeight = para.minimumLineHeight;

    if (font.pointSize > 40)
        para.maximumLineHeight = para.maximumLineHeight + 3;

    if (font.pointSize > 25)
        para.maximumLineHeight = para.maximumLineHeight + 3;

    para.alignment = NSCenterTextAlignment;
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


    if (font.pointSize == 16.f)
    {
        [attrString addAttribute:NSKernAttributeName value:@1.f range:NSMakeRange(0, text.length)];
    }

    CGRect contentRect = [attrString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
    // I guess the magic number -24 here means that the text field inner width differs 4 points from the outer width. 2-point border?

    NSTextField *textField = [[NSTextField alloc] initWithFrame:contentRect];

    textField.delegate = self;

    textField.translatesAutoresizingMaskIntoConstraints = NO;

    textField.bezeled=NO;
    textField.drawsBackground = NO;
    textField.editable = NO;
    textField.selectable = YES;
    textField.bordered = NO;
    [textField.cell setUsesSingleLineMode:NO];
    textField.allowsEditingTextAttributes = YES;


    [textField.cell setWraps:YES];
    [textField.cell setScrollable:NO];

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

    textField.attributedStringValue = attrString;

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


- (void) updateSideViewWithGame:(Game *)somegame
{
    Metadata *somedata = somegame.metadata;

    if (somedata.blurb == nil && somedata.author == nil && somedata.headline == nil && somedata.cover == nil) {
        ifidField.stringValue = somegame.ifid;
        [self updateSideViewWithString:somedata.title];
        return;
    }

    NSLayoutConstraint *xPosConstraint;
    NSLayoutConstraint *yPosConstraint;
    NSLayoutConstraint *widthConstraint;
    NSLayoutConstraint *heightConstraint;
    NSLayoutConstraint *rightMarginConstraint;
    NSLayoutConstraint *topSpacerYConstraint;

    NSFont *font;
    CGFloat spaceBefore;
    NSView *lastView;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    NSClipView *clipView = (NSClipView *)self.superview;
    NSScrollView *scrollView = (NSScrollView *)clipView.superview;
    CGFloat superViewWidth = clipView.frame.size.width;

    if (superViewWidth < 24)
        return;

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

    if (somedata.cover.data)
    {

        NSImage *theImage = [[NSImage alloc] initWithData:(NSData *)somedata.cover.data];

        CGFloat ratio = theImage.size.width / theImage.size.height;

        // We make the image double size to make enlarging when draggin divider to the right work
        theImage.size = NSMakeSize(superViewWidth * 2, superViewWidth * 2 / ratio );

        imageView = [[NSImageView alloc] initWithFrame:NSMakeRect(0,0,theImage.size.width,theImage.size.height)];

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
        rightMarginConstraint.priority = 999;
        [self addConstraint:rightMarginConstraint];

        imageView.image = theImage;

        lastView = imageView;

    }
    else
    {
        imageView = nil;
        //NSLog(@"No image");
        topSpacer = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, superViewWidth, 0)];
        topSpacer.boxType = NSBoxSeparator;


        [self addSubview:topSpacer];

        topSpacer.frame = NSMakeRect(0,0, superViewWidth, 1);

        topSpacer.translatesAutoresizingMaskIntoConstraints = NO;


        xPosConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                      attribute:NSLayoutAttributeLeft
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeLeft
                                                     multiplier:1.0
                                                       constant:0];

        yPosConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeTop
                                                     multiplier:1.0
                                                       constant:clipView.frame.size.height/4];

        yPosConstraint.priority = NSLayoutPriorityDefaultLow;

        widthConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                       attribute:NSLayoutAttributeWidth
                                                       relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                          toItem:self
                                                       attribute:NSLayoutAttributeWidth
                                                      multiplier:1.0
                                                        constant:0];


        [self addConstraint:xPosConstraint];
        [self addConstraint:yPosConstraint];
        [self addConstraint:widthConstraint];

        lastView = topSpacer;
    }

    if (somedata.title) // Every game will have a title unless something is broken
    {

        font = [NSFont fontWithName:@"Playfair Display Black" size:30];

        NSFontDescriptor *descriptor = font.fontDescriptor;

        NSArray *array = @[@{NSFontFeatureTypeIdentifierKey : @(kNumberCaseType),
                             NSFontFeatureSelectorIdentifierKey : @(kUpperCaseNumbersSelector)}];

        descriptor = [descriptor fontDescriptorByAddingAttributes:@{NSFontFeatureSettingsAttribute : array}];

        if (somedata.title.length > 9)
        {
            font = [NSFont fontWithDescriptor:descriptor size:30];
            //NSLog(@"Long title (length = %lu), smaller text.", agame.metadata.title.length);
        }
        else
        {
            font = [NSFont fontWithDescriptor:descriptor size:50];
        }

        NSString *longestWord = @"";

        for (NSString *word in [somedata.title componentsSeparatedByString:@" "])
        {
            if (word.length > longestWord.length) longestWord = word;
        }
        //NSLog (@"Longest word: %@", longestWord);

        // The magic number -24 means 10 points of margin and two points of textfield border on each side.
        while ([longestWord sizeWithAttributes:@{ NSFontAttributeName:font }].width > superViewWidth - 24)
        {
            //            NSLog(@"Font too large! Width %f, max allowed %f", [longestWord sizeWithAttributes:@{NSFontAttributeName:font}].width,  superViewWidth - 24);
            font = [[NSFontManager sharedFontManager] convertFont:font toSize:font.pointSize - 2];
        }
        //        NSLog(@"Font not too large! Width %f, max allowed %f", [longestWord sizeWithAttributes:@{NSFontAttributeName:font}].width,  superViewWidth - 24);

        spaceBefore = [@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height * 0.7;

        lastView = [self addSubViewWithtext:somedata.title andFont:font andSpaceBefore:spaceBefore andLastView:lastView];

        titleField = (NSTextField *)lastView;
    }
    else
    {
        NSLog(@"Error! No title!");
        titleField = nil;
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

    if (somedata.headline)
    {
        //font = [NSFont fontWithName:@"Playfair Display Regular" size:13];
        font = [NSFont fontWithName:@"HoeflerText-Regular" size:16];

        NSFontDescriptor *descriptor = font.fontDescriptor;

        NSArray *array = @[@{ NSFontFeatureTypeIdentifierKey : @(kLetterCaseType),
                              NSFontFeatureSelectorIdentifierKey : @(kSmallCapsSelector)}];

        descriptor = [descriptor fontDescriptorByAddingAttributes:@{NSFontFeatureSettingsAttribute : array}];
        font = [NSFont fontWithDescriptor:descriptor size:16.f];

        lastView = [self addSubViewWithtext:(somedata.headline).lowercaseString andFont:font andSpaceBefore:4 andLastView:lastView];

        headlineField = (NSTextField *)lastView;
    }
    else
    {
        //        NSLog(@"No headline");
        headlineField = nil;
    }

    if (somedata.author)
    {
        font = [NSFont fontWithName:@"Gentium Plus Italic" size:14.f];

        lastView = [self addSubViewWithtext:somedata.author andFont:font andSpaceBefore:25 andLastView:lastView];

        authorField = (NSTextField *)lastView;
    }
    else
    {
//        NSLog(@"No author");
        authorField = nil;
    }

    if (somedata.blurb)
    {
        font = [NSFont fontWithName:@"Gentium Plus" size:14.f];

        lastView = [self addSubViewWithtext:somedata.blurb andFont:font andSpaceBefore:23 andLastView:lastView];

        blurbField = (NSTextField *)lastView;

    }
    else
    {
//        NSLog(@"No blurb.");
        blurbField = nil;
    }

    NSLayoutConstraint *bottomPinConstraint = [NSLayoutConstraint constraintWithItem:self
                                                                           attribute:NSLayoutAttributeBottom
                                                                           relatedBy:NSLayoutRelationEqual
                                                                              toItem:lastView
                                                                           attribute:NSLayoutAttributeBottom
                                                                          multiplier:1.0
                                                                            constant:0];
    [self addConstraint:bottomPinConstraint];

    if (imageView == nil) {
        CGFloat windowHeight = ((NSView *)self.window.contentView).frame.size.height;

        CGFloat contentHeight = titleField.frame.size.height + headlineField.frame.size.height + authorField.frame.size.height + blurbField.frame.size.height;

        topSpacerYConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                            attribute:NSLayoutAttributeTop
                                                            relatedBy:NSLayoutRelationLessThanOrEqual
                                                               toItem:self
                                                            attribute:NSLayoutAttributeTop
                                                           multiplier:1.0
                                                             constant:(windowHeight - contentHeight) / 3];
        topSpacerYConstraint.priority = 999;

        if (clipView.frame.size.height < self.frame.size.height) {
            topSpacerYConstraint.constant = 0;
            yPosConstraint.constant = 0;
        }

        [self addConstraint:topSpacerYConstraint];

    }

    if (_game != somegame) {

        [clipView scrollToPoint: NSMakePoint(0.0, 0.0)];
        [scrollView reflectScrolledClipView:clipView];

        [self performSelector:@selector(fixScroll:) withObject:nil afterDelay:0.1];
    }

    _game = somegame;
}

- (void)fixScroll:(id)sender {

    NSClipView *clipView = (NSClipView *)self.superview;
    NSScrollView *scrollView = (NSScrollView *)clipView.superview;

    if (clipView.frame.size.height >= self.frame.size.height) {
        //        NSLog(@"fixScroll: Sideview fits within scrollview");
        return;
    }

    CGFloat titleYpos;
    if (imageView)
        titleYpos = NSHeight(imageView.frame) + NSHeight(titleField.frame);
    else
        titleYpos = clipView.frame.size.height / 2;
    CGFloat yPoint = titleYpos - (clipView.frame.size.height / 2);
    if (yPoint < 0) {
        NSLog(@"yPoint is %f, clipping to 0", yPoint);
        yPoint = 0;
    }
    if (yPoint > NSMaxY(self.frame) - clipView.frame.size.height) {
        NSLog(@"yPoint is %f, clipping to %f", yPoint, NSMaxY(self.frame) - clipView.frame.size.height);

        yPoint = NSMaxY(self.frame) - clipView.frame.size.height;
    }
//    NSLog(@"Trying to scroll to yPoint:%f titleYpos:%f clipView.frame:%@ self.frame:%@", yPoint, titleYpos, NSStringFromRect(clipView.frame),  NSStringFromRect(self.frame));

    NSPoint newOrigin = [clipView bounds].origin;
    newOrigin.y = yPoint;

    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:0.2];

    [[clipView animator] setBoundsOrigin:newOrigin];
    [scrollView reflectScrolledClipView: [scrollView contentView]]; // may not bee necessary
    [NSAnimationContext endGrouping];
}

- (void) updateSideViewWithString:(NSString *)aString {
    NSFont *font;
    NSClipView *clipView = (NSClipView *)self.superview;

    [titleField removeFromSuperview];
    titleField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, clipView.frame.size.width, clipView.frame.size.height)];
    titleField.drawsBackground = NO;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    titleField.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;


    titleField.cell = [[VerticallyCenteredTextFieldCell alloc] initTextCell:aString];

    [self addSubview:titleField];

    NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];

    font = [NSFont titleBarFontOfSize:16];

    para.alignment = NSCenterTextAlignment;
    para.lineBreakMode = NSLineBreakByTruncatingMiddle;

    NSMutableDictionary *attr = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                                 font,
                                 NSFontAttributeName,
                                 para,
                                 NSParagraphStyleAttributeName,
                                 [NSColor disabledControlTextColor],
                                 NSForegroundColorAttributeName,
                                 nil];

    NSMutableAttributedString *attrString = [[NSMutableAttributedString alloc] initWithString:aString attributes:attr];
    titleField.attributedStringValue = attrString;
}

@end

