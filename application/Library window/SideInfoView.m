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
#import "ImageView.h"
#import "NSFont+Categories.h"

@interface SideInfoView ()
{
    NSTextField *titleField;

    CGFloat totalHeight;
}
@property NSButton *downloadButton;
@end

@implementation SideInfoView

+ (BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        [self registerForDraggedTypes:ImageView.acceptableDragTypes.allObjects];

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

    CGRect contentRect = [attrString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, CGFLOAT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
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

    textField.attributedStringValue = attrString;

    [self addSubview:textField];

    NSLayoutYAxisAnchor *above = lastView ? ((NSView *)lastView).bottomAnchor : self.topAnchor;

    [NSLayoutConstraint activateConstraints:@[
        [textField.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:10],
        [textField.topAnchor constraintEqualToAnchor:above constant:space],
        [textField.widthAnchor constraintEqualToAnchor:self.widthAnchor constant:-20],
        [textField.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-10],
        [textField.heightAnchor constraintGreaterThanOrEqualToConstant:contentRect.size.height + 1]
    ]];

    totalHeight += NSHeight(textField.bounds) + space;

    return textField;
}

- (void)updateSideViewWithGame:(Game *)somegame
{
    Metadata *somedata = somegame.metadata;

    if (somedata.blurb.length == 0 && somedata.author.length == 0 && somedata.headline.length == 0 && somedata.cover == nil) {
        _game = somegame;
        [self updateSideViewWithString:somedata.title];
        return;
    }

    if (!somedata.title) { // Every game will have a title unless something is broken
        NSLog(@"Error! No title!");
    }

    totalHeight = 0;

    NSClipView *clipView = [SideInfoView addTopConstraintsToView:self];

    [self.widthAnchor constraintGreaterThanOrEqualToConstant:200].active = YES;

    CGFloat superViewWidth = clipView.frame.size.width;

    NSView *lastView;
    NSLayoutConstraint *topSpacerYConstraint = nil;

    NSImage *theImage = [SideInfoView coverImageForMetadata:somedata];
    if (theImage) {
        lastView = [self addImageViewForGame:somegame image:theImage width:superViewWidth];
    } else {
        _imageView = nil;
        NSBox *topSpacer = [self addTopSpacerWithWidth:superViewWidth];
        topSpacerYConstraint = [topSpacer.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor constant:clipView.frame.size.height/4];
        topSpacerYConstraint.active = YES;
        lastView = topSpacer;
    }

    if (!somedata.title) {
        titleField = nil;
        return;
    }

    NSFont *font = [SideInfoView titleFontForTitle:somedata.title width:superViewWidth - 24];

    CGFloat spaceBefore = [@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height * 0.7;

    titleField = [self addSubViewWithtext:somedata.title andFont:font andSpaceBefore:spaceBefore andLastView:lastView];

    lastView = [self addDividerBelow:titleField space:spaceBefore * 0.9 width:superViewWidth];

    if (somedata.headline)
    {
        lastView = [self addSubViewWithtext:(somedata.headline).lowercaseString andFont:[SideInfoView headlineFont] andSpaceBefore:4 andLastView:lastView];
    }

    if (somedata.author)
    {
        font = [NSFont fontWithName:@"Gentium Plus Italic" size:14.f];

        lastView = [self addSubViewWithtext:somedata.author andFont:font andSpaceBefore:25 andLastView:lastView];
    }

    if (somedata.blurb)
    {
        font = [NSFont fontWithName:@"Gentium Plus" size:14.f];

        lastView = [self addSubViewWithtext:somedata.blurb andFont:font andSpaceBefore:23 andLastView:lastView];
    }

    [self.bottomAnchor constraintEqualToAnchor:lastView.bottomAnchor].active = YES;

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
    }

    _game = somegame;
}

// The cover image of metadata, or nil. A cover that can't be decoded
// and came from the cover art URL is deleted.
+ (NSImage *)coverImageForMetadata:(Metadata *)metadata {
    if (!metadata.cover.data)
        return nil;
    NSImage *image = [[NSImage alloc] initWithData:(NSData *)metadata.cover.data];
    if (image == nil && [metadata.coverArtURL isEqualToString:metadata.cover.originalURL]) {
        [metadata.cover.managedObjectContext deleteObject:metadata.cover];
        metadata.coverArtURL = nil;
    }
    return image;
}

- (ImageView *)addImageViewForGame:(Game *)game image:(NSImage *)image width:(CGFloat)width {
    CGFloat ratio = image.size.width / image.size.height;

    _imageView = [[ImageView alloc] initWithGame:game image:image];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.frame = NSMakeRect(0,0, width, width / ratio);
    _imageView.intrinsic = NSMakeSize(NSViewNoIntrinsicMetric, _imageView.frame.size.height);

    [self addSubview:_imageView];

    NSLayoutConstraint *widthConstraint = [_imageView.widthAnchor constraintGreaterThanOrEqualToAnchor:self.widthAnchor];
    widthConstraint.priority = 500;

    NSLayoutConstraint *heightConstraint = [_imageView.heightAnchor constraintLessThanOrEqualToAnchor:_imageView.widthAnchor multiplier:(1 / ratio)];
    heightConstraint.priority = 499;

    [NSLayoutConstraint activateConstraints:@[
        [_imageView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_imageView.topAnchor constraintEqualToAnchor:self.topAnchor],
        widthConstraint,
        heightConstraint
    ]];

    return _imageView;
}

// A separator line in place of a missing cover image. The caller
// positions it vertically.
- (NSBox *)addTopSpacerWithWidth:(CGFloat)width {
    NSBox *topSpacer = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, width, 1)];
    topSpacer.boxType = NSBoxSeparator;
    topSpacer.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:topSpacer];

    [NSLayoutConstraint activateConstraints:@[
        [topSpacer.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [topSpacer.widthAnchor constraintGreaterThanOrEqualToAnchor:self.widthAnchor]
    ]];

    return topSpacer;
}

- (NSBox *)addDividerBelow:(NSView *)lastView space:(CGFloat)space width:(CGFloat)width {
    NSBox *divider = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, width, 1)];

    divider.boxType = NSBoxSeparator;
    divider.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:divider];

    [NSLayoutConstraint activateConstraints:@[
        [divider.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [divider.topAnchor constraintEqualToAnchor:lastView.bottomAnchor constant:space],
        [divider.widthAnchor constraintEqualToAnchor:self.widthAnchor],
        [divider.heightAnchor constraintEqualToConstant:1]
    ]];

    return divider;
}

// Playfair Display Black with upper-case numbers, large for short titles,
// shrunk if needed so that the longest word fits in maxWidth.
+ (NSFont *)titleFontForTitle:(NSString *)title width:(CGFloat)maxWidth {
    NSFont *font = [NSFont fontWithName:@"Playfair Display Black" size:30];

    NSFontDescriptor *descriptor = font.fontDescriptor;

    NSArray *array = @[@{NSFontFeatureTypeIdentifierKey : @(kNumberCaseType),
                         NSFontFeatureSelectorIdentifierKey : @(kUpperCaseNumbersSelector)}];

    descriptor = [descriptor fontDescriptorByAddingAttributes:@{NSFontFeatureSettingsAttribute : array}];

    font = [NSFont fontWithDescriptor:descriptor size:(title.length > 9) ? 30 : 50];

    // The caller's magic number -24 means 10 points of margin and two points of textfield border on each side.
    return [font fontFittingLongestWordOf:title inWidth:maxWidth];
}

// Hoefler Text in small caps
+ (NSFont *)headlineFont {
    NSFont *font = [NSFont fontWithName:@"HoeflerText-Regular" size:16];

    NSFontDescriptor *descriptor = font.fontDescriptor;

    NSArray *array = @[@{ NSFontFeatureTypeIdentifierKey : @(kLetterCaseType),
                          NSFontFeatureSelectorIdentifierKey : @(kSmallCapsSelector)}];

    descriptor = [descriptor fontDescriptorByAddingAttributes:@{NSFontFeatureSettingsAttribute : array}];
    return [NSFont fontWithDescriptor:descriptor size:16.f];
}

- (void)updateSideViewWithString:(NSString *)aString {
    NSFont *font;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    NSClipView *clipView = [SideInfoView addTopConstraintsToView:self];

    [self.bottomAnchor constraintEqualToAnchor:clipView.bottomAnchor].active = YES;

    if (!aString)
        return;

    [titleField removeFromSuperview];

    [self.widthAnchor constraintGreaterThanOrEqualToConstant:200].active = YES;

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

    [NSLayoutConstraint activateConstraints:@[
        [titleField.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
        [titleField.trailingAnchor constraintGreaterThanOrEqualToAnchor:self.trailingAnchor],
        [titleField.centerYAnchor constraintEqualToAnchor:self.centerYAnchor constant:-20],
        [titleField.centerXAnchor constraintEqualToAnchor:self.centerXAnchor]
    ]];

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

            // Anchors can't express a multiplier on a position
            NSLayoutConstraint *yPosConstraint = [NSLayoutConstraint constraintWithItem:_downloadButton attribute:NSLayoutAttributeCenterY
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeCenterY
                                                  multiplier:1.07
                                                    constant:25];

            [NSLayoutConstraint activateConstraints:@[
                [_downloadButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
                yPosConstraint,
                [_downloadButton.widthAnchor constraintEqualToConstant:25],
                [_downloadButton.heightAnchor constraintEqualToConstant:25]
            ]];
        }
    } else {
        [_downloadButton removeFromSuperview];
    }
}

+ (NSClipView *)addTopConstraintsToView:(NSView *)view {

    view.translatesAutoresizingMaskIntoConstraints = NO;

    NSClipView *clipView = (NSClipView *)view.superview;

    [NSLayoutConstraint activateConstraints:@[
        [view.leadingAnchor constraintEqualToAnchor:clipView.leadingAnchor],
        [view.trailingAnchor constraintEqualToAnchor:clipView.trailingAnchor],
        [view.topAnchor constraintEqualToAnchor:clipView.topAnchor]
    ]];

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

