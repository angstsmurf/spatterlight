//
//  SideInfoView.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <QuartzCore/QuartzCore.h>

#import "SideInfoView.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "LibController.h"
#import "AppDelegate.h"
#import "NSImage+Categories.h"
#import "ImageView.h"
#import "NSFont+Categories.h"


#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface VerticallyCenteredTextFieldCell : NSTextFieldCell

@end

@implementation VerticallyCenteredTextFieldCell
- (NSRect) titleRectForBounds:(NSRect)frame {

    CGFloat stringHeight = self.attributedStringValue.size.height;
    NSRect titleRect = [super titleRectForBounds:frame];
    CGFloat oldOriginY = frame.origin.y;
    titleRect.origin.y = frame.origin.y + (frame.size.height - stringHeight) / 2.0;
    titleRect.size.height = titleRect.size.height - (titleRect.origin.y - oldOriginY);
    return titleRect;
}

- (void) drawInteriorWithFrame:(NSRect)cFrame inView:(NSView*)cView {
    [super drawInteriorWithFrame:[self titleRectForBounds:cFrame] inView:cView];
}

@end

@interface SideInfoView ()
{
    NSBox *topSpacer;

    NSTextField *titleField;
    NSTextField *headlineField;
    NSTextField *authorField;
    NSTextField *blurbField;
    NSTextField *ifidField;

    NSString *longestWord;

    CGFloat totalHeight;

    NSSet<NSPasteboardType> *acceptableTypes;
    NSSet<NSPasteboardType> *nonURLTypes;
    BOOL isReceivingDrag;
}
@end

@implementation SideInfoView

+ (BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        nonURLTypes = [NSSet setWithObjects:NSPasteboardTypeTIFF, NSPasteboardTypePNG, nil];
        acceptableTypes = [NSSet setWithObject:NSURLPboardType];
        acceptableTypes = [acceptableTypes setByAddingObjectsFromSet:nonURLTypes];
        [self registerForDraggedTypes:acceptableTypes.allObjects];
    }
    return self;
}

- (BOOL) isFlipped { return YES; }


+ (NSMenu *)defaultMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    NSMenuItem *paste = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Paste Image", nil) action:@selector(paste:) keyEquivalent:@""];

    [menu addItem:paste];

    return menu;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    return [self.imageView validateMenuItem:menuItem];
}

- (IBAction)paste:(id)sender {
    [self.imageView paste:sender];
}

- (BOOL)textFieldShouldBeginEditing:(NSTextField *)textField{
    return NO;
}

- (BOOL)textField:(NSTextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
//    return textField != _yourReadOnlyTextField;
    return NO;
}

@synthesize imageView = _imageView;

- (ImageView *)imageView {
        if (_imageView == nil) {
            _imageView = [[ImageView alloc] initWithGame:_game image:nil];
        }

    return _imageView;
}

- (NSTextField *)addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView
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

    [attrString addAttribute:NSForegroundColorAttributeName value:[NSColor textColor] range:NSMakeRange(0, text.length)] ;

    CGRect contentRect = [attrString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
    // I guess the magic number -24 here means that the text field inner width differs 4 points from the outer width. 2-point border?

    NSTextField *textField = [[NSTextField alloc] initWithFrame:contentRect];

//    textField.delegate = self;

    textField.translatesAutoresizingMaskIntoConstraints = NO;

    textField.bezeled=NO;
    textField.drawsBackground = NO;
    textField.editable = NO;
    textField.selectable = YES;
    textField.bordered = NO;
    [textField.cell setUsesSingleLineMode:NO];
    textField.allowsEditingTextAttributes = YES;
    textField.alignment = para.alignment;

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

    [self addConstraints:@[ xPosConstraint, yPosConstraint ,widthConstraint, rightMarginConstraint ]];

    NSLayoutConstraint *heightConstraint = [NSLayoutConstraint constraintWithItem:textField
                                                                        attribute:NSLayoutAttributeHeight
                                                                        relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                                           toItem:nil
                                                                        attribute:NSLayoutAttributeNotAnAttribute
                                                                       multiplier:1.0
                                                                         constant: contentRect.size.height + 1];

    [self addConstraint:heightConstraint];

    totalHeight += NSHeight(textField.bounds) + space;

    return textField;
}

- (void)scrollWheel:(NSEvent *)event {
    [super scrollWheel:event];

    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    if (_animatingScroll) {
        [self cancelScrollAnimation];
    }
}

- (void)updateSideViewWithGame:(Game *)somegame
{
    NSClipView *clipView = (NSClipView *)self.superview;

    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    Metadata *somedata = somegame.metadata;

    if (somedata.blurb.length == 0 && somedata.author.length == 0 && somedata.headline.length == 0 && somedata.cover == nil) {
        ifidField.stringValue = somegame.ifid;
        _game = somegame;
        [self updateSideViewWithString:somedata.title];
        return;
    }

    totalHeight = 0;

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

        _imageView = [[ImageView alloc] initWithGame:somegame image:theImage];

        _imageView.translatesAutoresizingMaskIntoConstraints = NO;

        _imageView.frame = NSMakeRect(0,0, superViewWidth * 2, superViewWidth * 2 / ratio);
        _imageView.intrinsic = _imageView.frame.size;

        [self addSubview:_imageView];

        xPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                      attribute:NSLayoutAttributeLeft
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeLeft
                                                     multiplier:1.0
                                                       constant:0];

        yPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeTop
                                                     multiplier:1.0
                                                       constant:0];

        widthConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                       attribute:NSLayoutAttributeWidth
                                                       relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                          toItem:self
                                                       attribute:NSLayoutAttributeWidth
                                                      multiplier:1.0
                                                        constant:0];

        heightConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                        attribute:NSLayoutAttributeHeight
                                                        relatedBy:NSLayoutRelationLessThanOrEqual
                                                           toItem:_imageView
                                                        attribute:NSLayoutAttributeWidth
                                                       multiplier:( 1 / ratio)
                                                         constant:0];

        rightMarginConstraint = [NSLayoutConstraint constraintWithItem:_imageView
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

        lastView = _imageView;
    } else {
        _imageView = nil;
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

        longestWord = @"";

        for (NSString *word in [somedata.title componentsSeparatedByString:@" "])
        {
            if (word.length > longestWord.length) longestWord = word;
        }
        //NSLog (@"Longest word: %@", longestWord);

        // The magic number -24 means 10 points of margin and two points of textfield border on each side.
        if ([longestWord sizeWithAttributes:@{ NSFontAttributeName:font }].width > superViewWidth - 24)
        {
            //            NSLog(@"Font too large! Width %f, max allowed %f", [longestWord sizeWithAttributes:@{NSFontAttributeName:font}].width,  superViewWidth - 24);
            font = [font fontToFitWidth:superViewWidth - 24 sampleText:longestWord];
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

    [self addConstraints:@[xPosConstraint, yPosConstraint, widthConstraint, heightConstraint]];

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

    if (somedata.cover.data == nil) {
        CGFloat windowHeight = ((NSView *)self.window.contentView).frame.size.height;

        CGFloat topConstraintConstant = (windowHeight - totalHeight - 60) / 2;
        if (topConstraintConstant < 40)
            topConstraintConstant = 0;

        topSpacerYConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                            attribute:NSLayoutAttributeTop
                                                            relatedBy:NSLayoutRelationLessThanOrEqual
                                                               toItem:self
                                                            attribute:NSLayoutAttributeTop
                                                           multiplier:1.0
                                                             constant:topConstraintConstant];
//        topSpacerYConstraint.priority = 999;

        if (clipView.frame.size.height < self.frame.size.height) {
            topSpacerYConstraint.constant = 0;
            yPosConstraint.constant = 0;
        }

        [self addConstraint:topSpacerYConstraint];

    }

    if (_game != somegame) {
        [self performSelector:@selector(fixScroll:) withObject:@(clipView.bounds.origin.y) afterDelay:2];
    }

    _game = somegame;
}

- (void)updateTitle {
    NSMutableAttributedString *titleString = titleField.attributedStringValue.mutableCopy;
    NSDictionary *attributes = [titleString attributesAtIndex:0 effectiveRange:nil];
    NSClipView *clipView = (NSClipView *)self.superview;

    NSUInteger superViewWidth = (NSUInteger)clipView.frame.size.width - 24;

    NSFont *font = attributes[NSFontAttributeName];

    if ([longestWord sizeWithAttributes:attributes].width > superViewWidth)
    {
        font = [font fontToFitWidth:superViewWidth sampleText:longestWord];
    } else {
        return;
    }

    [titleString addAttribute:NSFontAttributeName value:font range:NSMakeRange(0, titleString.length)];
    titleField.attributedStringValue = titleString;
}

- (void)fixScroll:(id)sender {

    NSClipView *clipView = (NSClipView *)self.superview;

    if ([sender floatValue] != clipView.bounds.origin.y)
        return;

    if (clipView.frame.size.height >= self.frame.size.height) {
        return;
    }

    if (clipView.frame.size.height >= NSMaxY(titleField.frame)) {
        return;
    }

    CGFloat titleYpos;
    if (_game.metadata.cover.data)
        titleYpos = NSMaxY(_imageView.frame);
    else
        titleYpos = 0;

    if (titleYpos < 0) {
        titleYpos = 0;
    }
    if (titleYpos > NSMaxY(self.frame) - clipView.frame.size.height) {
        titleYpos = NSMaxY(self.frame) - clipView.frame.size.height;
    }

    NSPoint newOrigin = [clipView bounds].origin;
//    if (fabs(newOrigin.y - titleYpos) < NSHeight(self.frame) / 10)
//        return;
    newOrigin.y = titleYpos;

    _animatingScroll = YES;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 4;

        [[clipView animator] setBoundsOrigin:newOrigin];
    }
     completionHandler:^{
        self.animatingScroll = NO;
    }];
}

- (void)cancelScrollAnimation {
    _animatingScroll = NO;
    NSClipView *clipView = (NSClipView *)self.superview;
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:0.001];
    [[clipView animator] setBoundsOrigin:clipView.bounds.origin];
    [NSAnimationContext endGrouping];
}

- (void)updateSideViewWithString:(NSString *)aString {
    NSFont *font;
    NSClipView *clipView = (NSClipView *)self.superview;
    if (!aString)
        return;
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

    if (_game && !_game.hasDownloaded) {
        if (!_downloadButton)
            _downloadButton = [self createDownloadButton];
        if (_downloadButton.superview != titleField) {
            [titleField addSubview:_downloadButton];

            // We need to add these constraints in order to make the button
            // stay in position when resizing the side view
            _downloadButton.translatesAutoresizingMaskIntoConstraints = NO;

            NSLayoutConstraint *xPosConstraint = [NSLayoutConstraint constraintWithItem:_downloadButton attribute:NSLayoutAttributeCenterX
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeCenterX
                                                  multiplier:1
                                                    constant:0];

            NSLayoutConstraint *yPosConstraint = [NSLayoutConstraint constraintWithItem:_downloadButton attribute:NSLayoutAttributeCenterY
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeCenterY
                                                  multiplier:1.07
                                                    constant:25];

            NSLayoutConstraint *widthConstraint = [NSLayoutConstraint constraintWithItem:_downloadButton attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:nil
                                                   attribute:NSLayoutAttributeNotAnAttribute
                                                  multiplier:1.0
                                                    constant:25];

            NSLayoutConstraint *heightConstraint = [NSLayoutConstraint constraintWithItem:_downloadButton attribute:NSLayoutAttributeHeight
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:nil
                                                   attribute:NSLayoutAttributeNotAnAttribute
                                                  multiplier:1.0
                                                    constant:25];


            [self addConstraints:@[xPosConstraint, yPosConstraint, widthConstraint, heightConstraint]];
        }
    } else {
        [_downloadButton removeFromSuperview];
    }
}

#pragma mark Download button
- (NSButton *)createDownloadButton {
    // The actual size and position of the button is taken care of
    // by the constraints added in the caller above
    NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(0,0, 25, 25)];
    button.buttonType = NSPushOnPushOffButton;
    NSImage *image = [NSImage imageNamed:@"Download"];
    if (@available(macOS 10.14, *)) {
        button.image = image;
        button.contentTintColor = NSColor.disabledControlTextColor;
    } else {
        NSImage *tintedimage = [image imageWithTint:NSColor.disabledControlTextColor];
        button.image = tintedimage;
        button.alphaValue = 0.5;
    }
    button.imageScaling = NSImageScaleProportionallyUpOrDown;
    button.imagePosition = NSImageOnly;
    button.alignment = NSCenterTextAlignment;
    button.bordered = NO;
    button.bezelStyle = NSShadowlessSquareBezelStyle;
    button.toolTip = NSLocalizedString(@"Download game info", nil);
    button.accessibilityLabel = NSLocalizedString(@"download info", nil);

    button.target = self;
    button.action = @selector(download:);
    return button;
}

- (void)download:(id)sender {
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.3;
        [[_downloadButton animator] setAlphaValue:0];
    } completionHandler:^{
        LibController *libcontroller = ((AppDelegate *)[NSApplication sharedApplication].delegate).libctl;
        [libcontroller download:self.downloadButton];
    }];
}

#pragma mark Drag destination

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    if (_game)
        return [self.imageView draggingEntered:sender];
    else
        return NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    if (_game)
        [self.imageView draggingExited:sender];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    if (_game)
        return [self.imageView prepareForDragOperation:sender];
    else
        return NO;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)draggingInfo {
    if (_game)
        return [self.imageView performDragOperation:draggingInfo];
    else return NO;
}

- (void)deselectImage {
    if (_imageView) {
        [_imageView resignFirstResponder];
    }
}

@end

