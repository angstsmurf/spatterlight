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
#import "TableViewController.h"
#import "ImageView.h"
#import "NSFont+Categories.h"


#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

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

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(windowDidResignKey:)
                                                     name:NSWindowDidResignKeyNotification
                                                   object:self.window];
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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


    if (font.pointSize == 16.f)
    {
        [attrString addAttribute:NSKernAttributeName value:@1.f range:NSMakeRange(0, text.length)];
    }

    [attrString addAttribute:NSForegroundColorAttributeName value:[NSColor textColor] range:NSMakeRange(0, text.length)] ;

    CGRect contentRect = [attrString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
    // I guess the magic number -24 here means that the text field inner width differs 4 points from the outer width. 2-point border?

    NSTextField *textField = [[NSTextField alloc] initWithFrame:contentRect];

    textField.translatesAutoresizingMaskIntoConstraints = NO;

    textField.bezeled=NO;
    textField.drawsBackground = NO;
    textField.editable = NO;
    textField.selectable = YES;
    textField.bordered = NO;
    [textField.cell setUsesSingleLineMode:NO];
    textField.allowsEditingTextAttributes = YES;
    textField.alignment = para.alignment;

    textField.cell.wraps = YES;
    textField.cell.scrollable = NO;

    [textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationVertical];

    NSLayoutConstraint *xPosConstraint = [NSLayoutConstraint constraintWithItem:textField
                                                                      attribute:NSLayoutAttributeLeading
                                                                      relatedBy:NSLayoutRelationEqual
                                                                         toItem:self
                                                                      attribute:NSLayoutAttributeLeading
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
                                                                             attribute:NSLayoutAttributeTrailing
                                                                             relatedBy:NSLayoutRelationEqual
                                                                                toItem:self
                                                                             attribute:NSLayoutAttributeTrailing
                                                                            multiplier:1.0
                                                                              constant:-10];

    textField.attributedStringValue = attrString;

    [self addSubview:textField];

    [self addConstraints:@[ xPosConstraint, yPosConstraint, widthConstraint, rightMarginConstraint ]];

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

- (void)updateSideViewWithGame:(Game *)somegame
{
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
    NSLayoutConstraint *topSpacerYConstraint;

    NSFont *font;
    CGFloat spaceBefore;
    NSView *lastView;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    NSClipView *clipView = [SideInfoView addTopConstraintsToView:self];

    CGFloat superViewWidth = clipView.frame.size.width;

    if (somedata.cover.data)
    {

        NSImage *theImage = [[NSImage alloc] initWithData:(NSData *)somedata.cover.data];

        CGFloat ratio = theImage.size.width / theImage.size.height;

        _imageView = [[ImageView alloc] initWithGame:somegame image:theImage];
        _imageView.translatesAutoresizingMaskIntoConstraints = NO;
        _imageView.frame = NSMakeRect(0,0, superViewWidth, superViewWidth / ratio);
        _imageView.intrinsic = NSMakeSize(NSViewNoIntrinsicMetric, _imageView.frame.size.height);

        [self addSubview:_imageView];

        xPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                      attribute:NSLayoutAttributeLeading
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeLeading
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

        widthConstraint.priority = 500;

        heightConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                        attribute:NSLayoutAttributeHeight
                                                        relatedBy:NSLayoutRelationLessThanOrEqual
                                                           toItem:_imageView
                                                        attribute:NSLayoutAttributeWidth
                                                       multiplier:(1 / ratio)
                                                         constant:0];

        heightConstraint.priority = 499;

        [self addConstraints:@[xPosConstraint, yPosConstraint, widthConstraint, heightConstraint]];

        lastView = _imageView;
    } else {
        _imageView = nil;
        // No image
        topSpacer = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, superViewWidth, 0)];
        topSpacer.boxType = NSBoxSeparator;


        [self addSubview:topSpacer];

        topSpacer.frame = NSMakeRect(0,0, superViewWidth, 1);

        topSpacer.translatesAutoresizingMaskIntoConstraints = NO;


        xPosConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                      attribute:NSLayoutAttributeLeading
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeLeading
                                                     multiplier:1.0
                                                       constant:0];

        topSpacerYConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeTop
                                                     multiplier:1.0
                                                       constant:clipView.frame.size.height/4];

        widthConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                       attribute:NSLayoutAttributeWidth
                                                       relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                          toItem:self
                                                       attribute:NSLayoutAttributeWidth
                                                      multiplier:1.0
                                                        constant:0];


        [self addConstraint:xPosConstraint];
        [self addConstraint:topSpacerYConstraint];
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
                                                  attribute:NSLayoutAttributeLeading
                                                  relatedBy:NSLayoutRelationEqual
                                                     toItem:self
                                                  attribute:NSLayoutAttributeLeading
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

        CGFloat topConstraintConstant = (windowHeight - totalHeight - 180) / 2;
        if (topConstraintConstant < 40)
            topConstraintConstant = 0;

        topSpacerYConstraint.constant = topConstraintConstant;
    }

    if (_game != somegame) {
        if (@available(macOS 11.0, *)) {
        } else {
            NSPoint newOrigin = clipView.bounds.origin;
            newOrigin.y = -55;
            [clipView setBoundsOrigin:newOrigin];
        }

//        [self performSelector:@selector(fixScroll:) withObject:@(clipView.bounds.origin.y) afterDelay:2];
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

- (void)updateSideViewWithString:(NSString *)aString {
    NSFont *font;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    NSClipView *clipView = [SideInfoView addTopConstraintsToView:self];

    [clipView addConstraint:[NSLayoutConstraint constraintWithItem:self
                                                     attribute:NSLayoutAttributeBottom
                                                     relatedBy:NSLayoutRelationEqual
                                                        toItem:clipView
                                                     attribute:NSLayoutAttributeBottom
                                                    multiplier:1.0
                                                      constant:0]];
    if (!aString)
        return;

    [titleField removeFromSuperview];

    NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];

    font = [NSFont titleBarFontOfSize:16];

    para.alignment = NSTextAlignmentCenter;
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

    NSRect rect = NSZeroRect;
    rect.size = [aString sizeWithAttributes:attr];
    titleField = [[NSTextField alloc] initWithFrame:rect];

    titleField.attributedStringValue = attrString;
    titleField.translatesAutoresizingMaskIntoConstraints = NO;
    titleField.bezeled = NO;
    titleField.drawsBackground = NO;
    titleField.bordered = NO;
    titleField.alignment = NSTextAlignmentCenter;
    titleField.editable = NO;
    titleField.allowsEditingTextAttributes = YES;

    [self addSubview:titleField];

    [self addConstraint:[NSLayoutConstraint constraintWithItem:titleField
                                                     attribute:NSLayoutAttributeLeading
                                                     relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                        toItem:self
                                                     attribute:NSLayoutAttributeLeading
                                                    multiplier:1.0
                                                      constant:0]];

    [self addConstraint:[NSLayoutConstraint constraintWithItem:titleField
                                                     attribute:NSLayoutAttributeTrailing
                                                     relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                        toItem:self
                                                     attribute:NSLayoutAttributeTrailing
                                                    multiplier:1.0
                                                      constant:0]];

    [self addConstraint:[NSLayoutConstraint constraintWithItem:titleField
                                                     attribute:NSLayoutAttributeCenterY
                                                     relatedBy:NSLayoutRelationEqual
                                                        toItem:self
                                                     attribute:NSLayoutAttributeCenterY
                                                    multiplier:1.0
                                                      constant:-20]];

    [self addConstraint:[NSLayoutConstraint constraintWithItem:titleField
                                                     attribute:NSLayoutAttributeCenterX
                                                     relatedBy:NSLayoutRelationEqual
                                                        toItem:self
                                                     attribute:NSLayoutAttributeCenterX
                                                    multiplier:1.0
                                                      constant:0]];

    [titleField setContentCompressionResistancePriority:20 forOrientation:NSLayoutConstraintOrientationHorizontal];

    if (_game && !_game.hasDownloaded) {
        if (!_downloadButton)
            _downloadButton = [self createDownloadButton];
        if (_downloadButton.superview != titleField) {
            [self addSubview:_downloadButton];
            _downloadButton.toolTip = NSLocalizedString(@"Download game info", nil);

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

+ (NSClipView *)addTopConstraintsToView:(NSView *)view {

    view.translatesAutoresizingMaskIntoConstraints = NO;

    NSClipView *clipView = (NSClipView *)view.superview;

    [clipView addConstraint:[NSLayoutConstraint constraintWithItem:view
                                                         attribute:NSLayoutAttributeLeading
                                                         relatedBy:NSLayoutRelationEqual
                                                            toItem:clipView
                                                         attribute:NSLayoutAttributeLeading
                                                        multiplier:1.0
                                                          constant:0]];

    [clipView addConstraint:[NSLayoutConstraint constraintWithItem:view
                                                         attribute:NSLayoutAttributeTrailing
                                                         relatedBy:NSLayoutRelationEqual
                                                            toItem:clipView
                                                         attribute:NSLayoutAttributeTrailing
                                                        multiplier:1.0
                                                          constant:0]];

    [clipView addConstraint:[NSLayoutConstraint constraintWithItem:view
                                                         attribute:NSLayoutAttributeTop
                                                         relatedBy:NSLayoutRelationEqual
                                                            toItem:clipView
                                                         attribute:NSLayoutAttributeTop
                                                        multiplier:1.0
                                                          constant:0]];
    return clipView;
}

#pragma mark Download button
- (NSButton *)createDownloadButton {
    // The actual size and position of the button is taken care of
    // by the constraints added in the caller above
    NSButton *button;
    NSArray *topLevelObjects;
    [[NSBundle mainBundle] loadNibNamed:@"DownloadButton" owner:self topLevelObjects:&topLevelObjects];
    for (id object in topLevelObjects) {
        if ([object isKindOfClass:[NSButton class]])
            button = (NSButton *)object;
    }

    button.target = self;
    button.action = @selector(download:);
    return button;
}

- (void)download:(id)sender {
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.4;
        [_downloadButton animator].alphaValue = 0;
    } completionHandler:^{
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"SideviewDownload" object:nil]];
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

- (void)windowDidResignKey:(NSNotification *)notification {
    if (notification.object == self.window)
        [self deselectImage];
}


@end

