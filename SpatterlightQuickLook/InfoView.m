//
//  InfoView.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <QuartzCore/QuartzCore.h>

#import "InfoView.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "Ifid.h"

#import "NonInterpolatedImage.h"

#import "NSFont+Categories.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface InfoView ()
{
    NSBox *topSpacer;

    NSString *longestWord;

    CGFloat totalHeight;
}
@end

@implementation InfoView

+ (BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

- (BOOL) isFlipped { return YES; }

@synthesize imageView = _imageView;

- (NonInterpolatedImage *)imageView {
    if (_imageView == nil) {
        _imageView = [[NonInterpolatedImage alloc] init];
    }

    return _imageView;
}

- (NSTextField *)addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView {
    if (!font)
        NSLog(@"Font is nil!");

    NSMutableParagraphStyle *para = [NSMutableParagraphStyle new];

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
                                 NSColor.textColor,
                                 NSForegroundColorAttributeName,
                                 nil];

    NSMutableAttributedString *attrString = [[NSMutableAttributedString alloc] initWithString:text attributes:attr];

    if (font.pointSize <= 12.f) {
        [attrString addAttribute:NSKernAttributeName value:@2.f range:NSMakeRange(0, text.length)];
    }

    return [self addSubViewWithAttributedString:attrString andSpaceBefore:space andLastView:lastView];
}

- (NSTextField *)addSubViewWithAttributedString:(NSAttributedString *)attrString andSpaceBefore:(CGFloat)space andLastView:(id)lastView {
    CGRect contentRect = [attrString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
    // I guess the magic number -24 here means that the text field inner width differs 4 points from the outer width. 2-point border?

    NSTextField *textField = [[NSTextField alloc] initWithFrame:contentRect];

    textField.translatesAutoresizingMaskIntoConstraints = NO;

    textField.bezeled=NO;
    textField.drawsBackground = NO;
    textField.editable = NO;
    textField.selectable = NO;
    textField.bordered = NO;
    [textField.cell setUsesSingleLineMode:NO];
    textField.allowsEditingTextAttributes = YES;
    textField.alignment = NSCenterTextAlignment;

    [textField.cell setWraps:YES];
    [textField.cell setScrollable:NO];

    [textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationVertical];

    NSLayoutConstraint *xPosConstraint =
    [NSLayoutConstraint constraintWithItem:textField
                                 attribute:NSLayoutAttributeLeft
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeLeft
                                multiplier:1.0
                                  constant:10];

    NSLayoutConstraint *yPosConstraint;

    if (lastView) {
        yPosConstraint = [NSLayoutConstraint constraintWithItem:textField
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:lastView
                                                      attribute:NSLayoutAttributeBottom
                                                     multiplier:1.0
                                                       constant:space];
    }
    else {
        yPosConstraint = [NSLayoutConstraint constraintWithItem:textField
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeTop
                                                     multiplier:1.0
                                                       constant:space];
    }

    NSLayoutConstraint *widthConstraint =
    [NSLayoutConstraint constraintWithItem:textField
                                 attribute:NSLayoutAttributeWidth
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeWidth
                                multiplier:1.0
                                  constant:-20];

    NSLayoutConstraint *rightMarginConstraint =
    [NSLayoutConstraint constraintWithItem:textField
                                 attribute:NSLayoutAttributeRight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeRight
                                multiplier:1.0
                                  constant:-10];

    textField.attributedStringValue = attrString;

    [self addSubview:textField];

    [self addConstraints:@[ xPosConstraint, yPosConstraint ,widthConstraint, rightMarginConstraint ]];

    NSLayoutConstraint *heightConstraint =
    [NSLayoutConstraint constraintWithItem:textField
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

- (void)updateWithMetadata:(Metadata *)somedata {
    NSClipView *clipView = (NSClipView *)self.superview;

    Game *somegame = somedata.games.anyObject;

    if(somedata.cover.data)
        _imageData = (NSData *)somedata.cover.data;

    totalHeight = 0;

    NSLayoutConstraint *xPosConstraint;
    NSLayoutConstraint *yPosConstraint;
    NSLayoutConstraint *widthConstraint;
    NSLayoutConstraint *heightConstraint;
    NSLayoutConstraint *topSpacerYConstraint;

    NSFont *font;
    CGFloat spaceBefore = 0;
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

    topSpacerYConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                        attribute:NSLayoutAttributeTop
                                                        relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                           toItem:self
                                                        attribute:NSLayoutAttributeTop
                                                       multiplier:1.0
                                                         constant:0];

    topSpacerYConstraint.priority = NSLayoutPriorityDefaultLow;

    widthConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                   attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeWidth
                                                  multiplier:1.0
                                                    constant:0];


    [self addConstraints:@[xPosConstraint, topSpacerYConstraint, widthConstraint]];

    lastView = topSpacer;

    if (somedata.title) { // Every game will have a title unless something is broken
        font = [NSFont systemFontOfSize:20];

        longestWord = @"";

        for (NSString *word in [somedata.title componentsSeparatedByString:@" "]) {
            if (word.length > longestWord.length) longestWord = word;
        }

        // The magic number -24 means 10 points of margin and two points of textfield border on each side.
        if ([longestWord sizeWithAttributes:@{ NSFontAttributeName:font }].width > superViewWidth - 24) {
            font = [font fontToFitWidth:superViewWidth - 24 sampleText:longestWord];
        }

        spaceBefore = [@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height * 0.5;

        lastView = [self addSubViewWithtext:somedata.title andFont:font andSpaceBefore:spaceBefore andLastView:lastView];
    } else {
        NSLog(@"Error! No title!");
        return;
    }

    if (_imageData) {
        [self addImage];

        yPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:lastView
                                                      attribute:NSLayoutAttributeBottom
                                                     multiplier:1.0
                                                       constant:10];

        [self addConstraint:yPosConstraint];

        if (somedata.cover.data)
            [_imageView addImageFromManagedObject:somedata.cover];
        else
            [_imageView addImageFromData:_imageData];

        totalHeight += _imageView.frame.size.height + 10;

        lastView = _imageView;
    } else {
        _imageView = nil;
    }

    if (_starString.length) {
        NSMutableAttributedString *mutable = _starString.mutableCopy;
        NSMutableParagraphStyle *para = [NSMutableParagraphStyle new];
        para.alignment = NSCenterTextAlignment;
        [mutable addAttribute:NSParagraphStyleAttributeName value:para range:NSMakeRange(0, _starString.length)];

        CGFloat offset = [mutable boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin].size.height * 0.3;

        lastView = [self addSubViewWithAttributedString:mutable andSpaceBefore:-offset andLastView:lastView];
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

    if (somedata.headline) {
        font = [NSFont systemFontOfSize:12];

        font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSSmallCapsFontMask];

        lastView = [self addSubViewWithtext:somedata.headline.uppercaseString andFont:font andSpaceBefore:4 andLastView:lastView];
    }

    if (somedata.author) {
        font = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:14] toHaveTrait:NSItalicFontMask];

        lastView = [self addSubViewWithtext:somedata.author andFont:font andSpaceBefore:25 andLastView:lastView];
    }

    if (somedata.blurb) {
        lastView = [self addSubViewWithtext:somedata.blurb andFont:[NSFont systemFontOfSize:14] andSpaceBefore:23 andLastView:lastView];
    }

    font = [NSFont systemFontOfSize:11];

    NSString *ifid = nil;
    if (somegame)
        ifid = somegame.ifid;
    else
        ifid = somedata.ifids.anyObject.ifidString;

    if (ifid) {
        lastView = [self addSubViewWithtext:[NSString stringWithFormat:@"IFID: %@\n", ifid.uppercaseString] andFont:font andSpaceBefore:23 andLastView:lastView];

        NSTextField *field = (NSTextField *)lastView;
        NSMutableAttributedString *mutAttrStr = field.attributedStringValue.mutableCopy;
        if (@available(macOS 10.13, *)) {
            [mutAttrStr addAttribute:NSForegroundColorAttributeName value:[NSColor colorNamed:@"customControlColor"] range:NSMakeRange(0, mutAttrStr.length)];
        }
        field.attributedStringValue = mutAttrStr;
    }


    NSLayoutConstraint *bottomPinConstraint =
    [NSLayoutConstraint constraintWithItem:self
                                 attribute:NSLayoutAttributeBottom
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:lastView
                                 attribute:NSLayoutAttributeBottom
                                multiplier:1.0
                                  constant:0];
    [self addConstraint:bottomPinConstraint];

    CGFloat topConstraintConstant = (clipView.frame.size.height - totalHeight) / 2;
    if (topConstraintConstant < 20)
        topConstraintConstant = 0;

    NSLayoutConstraint *newTopSpacerConstraint =
    [NSLayoutConstraint constraintWithItem:topSpacer
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:topConstraintConstant];
    newTopSpacerConstraint.priority = 1000;
    newTopSpacerConstraint.active = NO;

    [self addConstraint:newTopSpacerConstraint];

    if (clipView.frame.size.height > self.frame.size.height) {
        topSpacerYConstraint.active = NO;
        newTopSpacerConstraint.active = YES;
    }
}

- (void)updateWithImage:(Image *)image {
    NSClipView *clipView = (NSClipView *)self.superview;

    totalHeight = 0;

    NSLayoutConstraint *yPosConstraint;

    NSFont *font;
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

    if (!_imageData && image.data)
        _imageData = (NSData *)image.data;

    if (_imageData)
    {
        [self addImage];

        yPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:self
                                                      attribute:NSLayoutAttributeTop
                                                     multiplier:1.0
                                                       constant:20];
        yPosConstraint.priority = 500;

        [self addConstraint:yPosConstraint];

        [_imageView addImageFromManagedObject:image];

        lastView = _imageView;
        totalHeight += NSHeight(_imageView.frame);
    } else {
        _imageView = nil;
        NSLog(@"No image data! Image:%@", image);
        return;
    }

    if (image.imageDescription)
    {
        font = [NSFont systemFontOfSize:14];

        lastView = [self addSubViewWithtext:image.imageDescription andFont:font andSpaceBefore:15 andLastView:lastView];
    } else {
        NSLog(@"No description!");
    }

    NSLayoutConstraint *bottomPinConstraint =
    [NSLayoutConstraint constraintWithItem:self
                                 attribute:NSLayoutAttributeBottom
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:lastView
                                 attribute:NSLayoutAttributeBottom
                                multiplier:1.0
                                  constant:0];
    [self addConstraint:bottomPinConstraint];

    CGFloat topConstraintConstant = (clipView.frame.size.height - totalHeight) / 2;
    if (topConstraintConstant < 0)
        topConstraintConstant = 0;

    NSLayoutConstraint *topSpacerYConstraint =
    [NSLayoutConstraint constraintWithItem:_imageView
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:topConstraintConstant];
    topSpacerYConstraint.priority = 1000;
    [self addConstraint:topSpacerYConstraint];

    if (clipView.frame.size.height < self.frame.size.height) {
        topSpacerYConstraint.active = NO;
    } else {
        yPosConstraint.active = NO;
    }
}

- (void)addImage {
    NSImage *theImage = [[NSImage alloc] initWithData:_imageData];

    if (!theImage)
        return;

    CGFloat ratio = theImage.size.width / theImage.size.height;

    CGFloat maxHeight = NSHeight(self.superview.frame) * 0.67;
    CGFloat maxWidth = NSWidth(self.superview.frame) - 40;

    NSRect imageFrame = NSMakeRect(0,0, maxHeight * ratio, maxHeight);

    if (imageFrame.size.width > maxWidth) {
        imageFrame = NSMakeRect(0,0, maxWidth, maxWidth / ratio);
    }

    _imageView = [[NonInterpolatedImage alloc] initWithFrame:imageFrame];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_imageView];

    NSLayoutConstraint *xPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                  attribute:NSLayoutAttributeCenterX
                                                  relatedBy:NSLayoutRelationEqual
                                                     toItem:self
                                                  attribute:NSLayoutAttributeCenterX
                                                 multiplier:1.0
                                                   constant:0];

    NSLayoutConstraint *widthConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                   attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:nil
                                                   attribute:NSLayoutAttributeNotAnAttribute
                                                  multiplier:1.0
                                                    constant:NSWidth(_imageView.frame)];

    NSLayoutConstraint *heightConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                    attribute:NSLayoutAttributeHeight
                                                    relatedBy:NSLayoutRelationEqual
                                                       toItem:nil
                                                    attribute:NSLayoutAttributeNotAnAttribute
                                                   multiplier:1.0
                                                     constant:NSHeight(_imageView.frame)];

    [self addConstraints:@[xPosConstraint, widthConstraint, heightConstraint]];
}

@end

