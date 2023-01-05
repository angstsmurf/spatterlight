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
#import "NSDate+relative.h"

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

+ (nullable NSAttributedString *)starString:(NSInteger)rating alignment:(NSTextAlignment)alignment {

    NSMutableAttributedString *starString = [NSMutableAttributedString new];

    if (rating == NSNotFound)
        return starString;

    NSUInteger totalNumberOfStars = 5;
    NSFont *currentFont = [NSFont fontWithName:@"SF Pro" size:12];
    BOOL noSF = NO;
    if (!currentFont) {
        currentFont = [NSFont systemFontOfSize:13];
        noSF = YES;
    }

    NSMutableParagraphStyle *para = [NSMutableParagraphStyle new];
    para.alignment = alignment;

    NSDictionary *activeStarFormat = @{
        NSFontAttributeName : currentFont,
        NSForegroundColorAttributeName : [NSColor colorNamed:@"customControlColor"],
        NSParagraphStyleAttributeName : para
    };
    NSDictionary *inactiveStarFormat = @{
        NSFontAttributeName : currentFont,
        NSForegroundColorAttributeName : [NSColor colorNamed:@"customDisabledColor"],
        NSParagraphStyleAttributeName : para
    };

    [starString appendAttributedString:[[NSAttributedString alloc]
                                        initWithString:@"\n\n" attributes:activeStarFormat]];

    for (int i=0; i < totalNumberOfStars; ++i) {
        //Full star
        if (rating >= i+1) {
            [starString appendAttributedString:[[NSAttributedString alloc]
                                                initWithString:noSF ? NSLocalizedString(@"\u2605 ", nil) : NSLocalizedString(@"􀋃 ", nil) attributes:activeStarFormat]];
        }
        //Half star
        else if (rating > i) {
            [starString appendAttributedString:[[NSAttributedString alloc]
                                                initWithString:NSLocalizedString(@"􀋄 ", nil) attributes:activeStarFormat]];
        }
        // Grey star
        else {
            [starString appendAttributedString:[[NSAttributedString alloc]
                                                initWithString:noSF ? NSLocalizedString(@"\u2606 ", nil) : NSLocalizedString(@"􀋂 ", nil) attributes:inactiveStarFormat]];
        }
    }
    return starString;
}

+ (NSString *)formattedStringFromDate:(NSDate *)date {

    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.doesRelativeDateFormatting = YES;

    if([date isToday]) {
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    }
    else if ([date isYesterday]) {
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    }
    else if ([date isThisWeek]) {
        NSDateComponents *weekdayComponents = [[NSCalendar currentCalendar] components:(NSCalendarUnit)kCFCalendarUnitWeekday fromDate:date];
        NSUInteger weekday = (NSUInteger)weekdayComponents.weekday;
        NSString *weekdayString = (dateFormatter.weekdaySymbols)[weekday - 1];
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterNoStyle;
        return [NSString stringWithFormat:@"%@ at %@", weekdayString, [dateFormatter stringFromDate:date]];
    } else {
        dateFormatter.doesRelativeDateFormatting = NO;
        dateFormatter.dateFormat = @"d MMM yyyy HH.mm";
    }

    return [dateFormatter stringFromDate:date];
}

- (NSTextField *)addSubViewWithtext:(NSString *)text andFont:(NSFont *)font andSpaceBefore:(CGFloat)space andLastView:(id)lastView {
    if (!font)
        NSLog(@"Font is nil!");

    NSMutableParagraphStyle *para = [NSMutableParagraphStyle new];

    para.minimumLineHeight = font.pointSize + 3;
    para.maximumLineHeight = para.minimumLineHeight;

    if (font.pointSize > 40)
        para.maximumLineHeight += 3;

    if (font.pointSize > 25)
        para.maximumLineHeight += 3;

    para.alignment = NSTextAlignmentCenter;
    para.lineSpacing = 1;

    if (font.pointSize == 12.5) {
        para.lineSpacing = 2;
    }

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

    NSTextField *textField = [InfoView customTextFieldWithFrame:contentRect];

    textField.alignment = NSTextAlignmentCenter;

    NSLayoutConstraint *xPosConstraint =
    [NSLayoutConstraint constraintWithItem:textField
                                 attribute:NSLayoutAttributeCenterX
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeCenterX
                                multiplier:1.0
                                  constant:0];

    xPosConstraint.priority = 777;

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
                                 relatedBy:NSLayoutRelationLessThanOrEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeWidth
                                multiplier:1.0
                                  constant:-20];

    widthConstraint.priority = 833;

    NSLayoutConstraint *rightMarginConstraint =
    [NSLayoutConstraint constraintWithItem:textField
                                 attribute:NSLayoutAttributeRight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeRight
                                multiplier:1.0
                                  constant:-10];

    rightMarginConstraint.priority = 900;

    textField.attributedStringValue = attrString;

    [textField setContentCompressionResistancePriority:250 forOrientation:NSLayoutConstraintOrientationHorizontal];

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

    heightConstraint.priority = 900;
    [self addConstraint:heightConstraint];

    totalHeight += NSHeight(textField.bounds) + space;

    return textField;
}

- (void)updateWithMetadata:(Metadata *)somedata {
    NSClipView *clipView = (NSClipView *)[InfoView addTopConstraintsToView:self];

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
    xPosConstraint.priority = 900;

    topSpacerYConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                        attribute:NSLayoutAttributeTop
                                                        relatedBy:NSLayoutRelationGreaterThanOrEqual
                                                           toItem:self
                                                        attribute:NSLayoutAttributeTop
                                                       multiplier:1.0
                                                         constant:0];

    topSpacerYConstraint.priority = NSLayoutPriorityDefaultLow;

    widthConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                   attribute:NSLayoutAttributeRight
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeRight
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

    NSInteger rating = NSNotFound;
    if (somedata.starRating.length) {
        rating = somedata.starRating.integerValue;
    } else if (somedata.myRating.length) {
        rating = somedata.myRating.integerValue;
    }
    
    NSAttributedString *starString = [InfoView starString:rating alignment:NSTextAlignmentCenter];

    if (starString.length) {
        CGFloat offset = [starString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin].size.height * 0.3;

        lastView = [self addSubViewWithAttributedString:starString andSpaceBefore:-offset andLastView:lastView];
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
                                                   attribute:NSLayoutAttributeRight
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeRight
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
        lastView = [self addSubViewWithtext:somedata.blurb andFont:[NSFont systemFontOfSize:12.5 weight:NSFontWeightLight] andSpaceBefore:23 andLastView:lastView];
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
        [mutAttrStr addAttribute:NSForegroundColorAttributeName value:[NSColor colorNamed:@"customControlColor"] range:NSMakeRange(0, mutAttrStr.length)];
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
    if (topConstraintConstant < 20 || _imageView.hasImage)
        topConstraintConstant = 0;

    NSLayoutConstraint *newTopSpacerConstraint =
    [NSLayoutConstraint constraintWithItem:topSpacer
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:topConstraintConstant];
    newTopSpacerConstraint.active = NO;

    [self addConstraint:newTopSpacerConstraint];

    if (clipView.frame.size.height > self.frame.size.height) {
        topSpacerYConstraint.active = NO;
        newTopSpacerConstraint.active = YES;
    }
}

- (void)updateWithDictionary:(NSDictionary *)metadict {
    NSClipView *clipView = (NSClipView *)[InfoView addTopConstraintsToView:self];

    totalHeight = 0;

    NSLayoutConstraint *xPosConstraint;
    NSLayoutConstraint *yPosConstraint;
    NSLayoutConstraint *widthConstraint;
    NSLayoutConstraint *heightConstraint;
    NSLayoutConstraint *topSpacerYConstraint;

    NSFont *font;
    CGFloat spaceBefore = 0;
    NSView *lastView;

    CGFloat superViewWidth = clipView.frame.size.width;

    if (superViewWidth < 24)
        return;

    topSpacer = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, superViewWidth, 0)];
    topSpacer.boxType = NSBoxSeparator;

    [topSpacer setContentCompressionResistancePriority:250 forOrientation:NSLayoutConstraintOrientationHorizontal];

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

    widthConstraint = [NSLayoutConstraint constraintWithItem:topSpacer
                                                   attribute:NSLayoutAttributeRight
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeRight
                                                  multiplier:1.0
                                                    constant:0];


    [self addConstraints:@[xPosConstraint, topSpacerYConstraint, widthConstraint]];

    lastView = topSpacer;

    if (!_imageData)
        _imageData = metadict[@"cover"];

    if (_imageData) {
        [self addImage];

        yPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                      attribute:NSLayoutAttributeTop
                                                      relatedBy:NSLayoutRelationEqual
                                                         toItem:lastView
                                                      attribute:NSLayoutAttributeBottom
                                                     multiplier:1.0
                                                       constant:10];
        NSLayoutConstraint *imageWidthConstraint =
        [NSLayoutConstraint constraintWithItem:_imageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationLessThanOrEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeWidth
                                    multiplier:1.0
                                      constant:0];
        imageWidthConstraint.priority = 250;

        [self addConstraints:@[yPosConstraint, imageWidthConstraint]];

        [_imageView addImageFromData:_imageData];
        if (metadict[@"coverArtDescription"])
            _imageView.accessibilityLabel = metadict[@"coverArtDescription"];

        totalHeight += _imageView.frame.size.height + 10;

        lastView = _imageView;
    } else {
        _imageView = nil;
    }

    NSString *title = metadict[@"title"];

    if (title.length) { // Every game will have a title unless something is broken

        font = [NSFont systemFontOfSize:20 weight:NSFontWeightSemibold].copy;

        longestWord = @"";

        for (NSString *word in [title componentsSeparatedByString:@" "]) {
            if (word.length > longestWord.length) longestWord = word;
        }

        // The magic number -24 means 10 points of margin and two points of textfield border on each side.
        if ([longestWord sizeWithAttributes:@{ NSFontAttributeName:font }].width > superViewWidth - 24) {
            font = [font fontToFitWidth:superViewWidth - 24 sampleText:longestWord];
        }

        spaceBefore = [@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height * 0.3;

        lastView = [self addSubViewWithtext:title andFont:font andSpaceBefore:spaceBefore andLastView:lastView];
    } else {
        NSLog(@"Error! No title!");
        return;
    }


    NSInteger rating = NSNotFound;
    if (metadict[@"starRating"]) {
        rating = ((NSNumber *)metadict[@"starRating"]).integerValue;
    } else if (metadict[@"myRating"]) {
        rating = ((NSNumber *)metadict[@"myRating"]).integerValue;
    }

    NSAttributedString *starString = [InfoView starString:rating alignment:NSTextAlignmentCenter];

    if (starString.length) {
        CGFloat offset = [starString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin].size.height * 0.3;

        lastView = [self addSubViewWithAttributedString:starString andSpaceBefore:-offset andLastView:lastView];
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

    yPosConstraint.priority = 900;

    widthConstraint = [NSLayoutConstraint constraintWithItem:divider
                                                   attribute:NSLayoutAttributeRight
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeRight
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

    NSString *headline = metadict[@"headline"];
    if (!headline.length && metadict[@"fileType"]) {
        headline = metadict[@"fileType"];
        if (metadict[@"fileSize"])
            headline = [headline stringByAppendingFormat:@" - %@", metadict[@"fileSize"]];
    }
    if (headline.length) {
        font = [NSFont systemFontOfSize:12];
        font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSSmallCapsFontMask];

        lastView = [self addSubViewWithtext:headline.uppercaseString andFont:font andSpaceBefore:4 andLastView:lastView];
    }

    NSString *author = metadict[@"author"];

    if (!author.length)
        author = metadict[@"AUTH"];

    if (author.length) {
        font = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:14] toHaveTrait:NSItalicFontMask];

        lastView = [self addSubViewWithtext:author andFont:font andSpaceBefore:25 andLastView:lastView];
    }

    NSString *blurb = metadict[@"blurb"];
    if (blurb.length) {
        lastView = [self addSubViewWithtext:blurb andFont:[NSFont systemFontOfSize:12.5 weight:NSFontWeightLight] andSpaceBefore:23 andLastView:lastView];
    }

    spaceBefore = 24;

    font = [NSFont systemFontOfSize:14];

    NSString *annotation = metadict[@"ANNO"];
    if (annotation.length) {
        lastView = [self addSubViewWithtext:annotation andFont:font andSpaceBefore:spaceBefore andLastView:lastView];
        spaceBefore = 5;
    }

    if (metadict[@"(c) "]) {
        NSString *copyright = [NSString stringWithFormat:@"© %@", metadict[@"(c) "]];

        lastView = [self addSubViewWithtext:copyright andFont:font andSpaceBefore:spaceBefore andLastView:lastView];
    }

    if (metadict[@"modificationDate"] || metadict[@"creationDate"] || metadict[@"lastOpenedDate"]) {
        if (_leftDateView) {
            [_leftDateView removeFromSuperview];
            _leftDateView = nil;
            [_rightDateView removeFromSuperview];
            _rightDateView = nil;
        }
        NSDictionary<NSString *, NSString *> *fileInfoDict =
        @{ @"lastOpenedDate"   : @"Last opened",
           @"modificationDate" : @"Last modified",
           @"creationDate"     : @"Created",
           @"lastPlayed"       : @"Last played"
        };
        NSArray *fileInfoStrings = @[@"creationDate", @"modificationDate", @"lastPlayed", @"lastOpenedDate"];
        NSView *oldLastView = lastView;
        for (NSString *fileInfo in fileInfoStrings) {
            NSString *datestring = [InfoView formattedStringFromDate:metadict[fileInfo]];
            if (datestring.length) {
                lastView = [self addDate:metadict[fileInfo] description:fileInfoDict[fileInfo] lastView:oldLastView];
                // Last played and last opened are usually the same date,
                // and if not, last played is usually the more interesting one.
                // So skip last opened if we have last played
                if ([fileInfo isEqualToString:@"lastPlayed"])
                    break;
            }
        }

        totalHeight += NSHeight(_leftDateView.frame) + 24;
    }

    font = [NSFont systemFontOfSize:11];

    NSString *ifid = metadict[@"ifid"];

    if (ifid.length) {
        lastView = [self addSubViewWithtext:[NSString stringWithFormat:@"IFID: %@\n", ifid.uppercaseString] andFont:font andSpaceBefore:23 andLastView:lastView];

        NSTextField *field = (NSTextField *)lastView;
        NSMutableAttributedString *mutAttrStr = field.attributedStringValue.mutableCopy;
        [mutAttrStr addAttribute:NSForegroundColorAttributeName value:[NSColor colorNamed:@"customControlColor"] range:NSMakeRange(0, mutAttrStr.length)];
        field.attributedStringValue = mutAttrStr;
    } else {
        lastView = [self addSubViewWithtext:@"\n" andFont:font andSpaceBefore:0 andLastView:lastView];
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

    NSLayoutConstraint *newTopSpacerConstraint =
    [NSLayoutConstraint constraintWithItem:topSpacer
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:0];
    newTopSpacerConstraint.priority = 1000;
    newTopSpacerConstraint.active = NO;

    [self addConstraint:newTopSpacerConstraint];

    if (clipView.frame.size.height > self.frame.size.height) {
        topSpacerYConstraint.active = NO;
        newTopSpacerConstraint.active = YES;
    }
}

- (NSTextField *)addDate:(NSDate *)date description:(NSString *)description lastView:(NSView *)lastview {

    NSString *dateString = [InfoView formattedStringFromDate:date];

    if (!_leftDateView) {
        NSMutableDictionary *attributes = [NSMutableDictionary new];
        attributes[NSFontAttributeName] = [NSFont systemFontOfSize:11];
        
        attributes[NSForegroundColorAttributeName]= [NSColor colorNamed:@"customControlColor"];
        
        NSMutableParagraphStyle *leftpara = [NSMutableParagraphStyle new];
        leftpara.alignment = NSTextAlignmentRight;
        leftpara.lineSpacing = 2;

        NSMutableParagraphStyle *rightpara = [NSMutableParagraphStyle new];
        rightpara.alignment = NSTextAlignmentLeft;
        rightpara.lineSpacing = 2;

        attributes[NSParagraphStyleAttributeName] = leftpara;
        NSAttributedString *leftAttributedString = [[NSAttributedString alloc] initWithString:description attributes:attributes];

        attributes[NSParagraphStyleAttributeName] = rightpara;
        NSAttributedString *rightAttributedString = [[NSAttributedString alloc] initWithString:dateString attributes:attributes];

        CGRect contentRect = [leftAttributedString boundingRectWithSize:CGSizeMake(floor((self.frame.size.width - 34) / 2), FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
        contentRect.origin.x = 10;
        contentRect.origin.y = NSMaxY(lastview.frame) + 24;
        _leftDateView = [InfoView customTextFieldWithFrame:contentRect];

        _leftDateView.alignment = NSTextAlignmentRight;

        _leftDateView.attributedStringValue = leftAttributedString;
        [self addSubview:_leftDateView];
        contentRect.origin.x = NSMaxX(_leftDateView.frame) + 10;
        _rightDateView = [InfoView customTextFieldWithFrame:contentRect];

        _rightDateView.alignment = NSTextAlignmentLeft;

        NSLayoutConstraint *xPosConstraint =
        [NSLayoutConstraint constraintWithItem:_leftDateView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1.0
                                      constant:10];

        xPosConstraint.priority = 700;

        NSLayoutConstraint *yPosConstraint =
        [NSLayoutConstraint constraintWithItem:_leftDateView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:lastview
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1.0
                                      constant:24];
        yPosConstraint.priority = 900;

        NSLayoutConstraint *rightMarginConstraint =
        [NSLayoutConstraint constraintWithItem:_leftDateView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeCenterX
                                    multiplier:1.0
                                      constant:-25];

        rightMarginConstraint.priority = 666;

        _rightDateView.attributedStringValue = rightAttributedString;

        [self addSubview:_rightDateView];

        [self addConstraints:@[ xPosConstraint, yPosConstraint, rightMarginConstraint ]];

        NSLayoutConstraint *xPosConstraint2 =
        [NSLayoutConstraint constraintWithItem:_rightDateView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeCenterX
                                    multiplier:1.0
                                      constant:-15];
        xPosConstraint2.priority = 750;

        NSLayoutConstraint *yPosConstraint2 =
        [NSLayoutConstraint constraintWithItem:_rightDateView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:lastview
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1.0
                                      constant:24];
        yPosConstraint.priority = 900;

        NSLayoutConstraint *rightMarginConstraint2 =
        [NSLayoutConstraint constraintWithItem:_rightDateView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1.0
                                      constant:-10];
        rightMarginConstraint2.priority = 650;

        [self addConstraints:@[ xPosConstraint2, yPosConstraint2, rightMarginConstraint2]];
    } else {
        NSDictionary *leftDict = [_leftDateView.attributedStringValue attributesAtIndex:0 effectiveRange:nil];
        NSDictionary *rightDict = [_rightDateView.attributedStringValue attributesAtIndex:0 effectiveRange:nil];
        _leftDateView.attributedStringValue = [[NSAttributedString alloc] initWithString: [NSString stringWithFormat:@"%@\n%@",
                                                _leftDateView.stringValue, description] attributes:leftDict];
        _rightDateView.attributedStringValue = [[NSAttributedString alloc] initWithString: [NSString stringWithFormat:@"%@\n%@",
                                      _rightDateView.stringValue, dateString] attributes:rightDict];

        CGRect contentRect = [_rightDateView.attributedStringValue boundingRectWithSize:CGSizeMake(floor((self.frame.size.width - 34) / 2), FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];
        NSRect leftFrame = _leftDateView.frame;
        leftFrame.size.height = contentRect.size.height;
        NSRect rightFrame = _rightDateView.frame;
        rightFrame.size.height = contentRect.size.height;
        _leftDateView.frame = leftFrame;
        _rightDateView.frame = rightFrame;

    }
    return _rightDateView;
}

+ (NSView *)addTopConstraintsToView:(NSView *)view {

    view.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *clipView = view.superview;

    [clipView addConstraint:[NSLayoutConstraint constraintWithItem:view
                                                         attribute:NSLayoutAttributeLeft
                                                         relatedBy:NSLayoutRelationEqual
                                                            toItem:clipView
                                                         attribute:NSLayoutAttributeLeft
                                                        multiplier:1.0
                                                          constant:0]];

    [clipView addConstraint:[NSLayoutConstraint constraintWithItem:view
                                                         attribute:NSLayoutAttributeRight
                                                         relatedBy:NSLayoutRelationEqual
                                                            toItem:clipView
                                                         attribute:NSLayoutAttributeRight
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

+ (NSTextField *)customTextFieldWithFrame:(NSRect)frame {
    NSTextField *textField = [[NSTextField alloc] initWithFrame:frame];

    textField.translatesAutoresizingMaskIntoConstraints = NO;

    textField.bezeled=NO;
    textField.drawsBackground = NO;
    textField.editable = NO;
    textField.selectable = NO;
    textField.bordered = NO;
    [textField.cell setUsesSingleLineMode:NO];
    textField.allowsEditingTextAttributes = YES;

    [textField.cell setWraps:YES];
    [textField.cell setScrollable:NO];

    [textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [textField setContentCompressionResistancePriority:25 forOrientation:NSLayoutConstraintOrientationVertical];
    return textField;
}

- (void)updateWithImage:(Image *)image {
    NSClipView *clipView = (NSClipView *)[InfoView addTopConstraintsToView:self];

    totalHeight = 0;

    NSLayoutConstraint *yPosConstraint;

    NSFont *font;
    NSView *lastView;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat superViewWidth = clipView.frame.size.width;

    if (superViewWidth < 24)
        return;

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
        yPosConstraint.priority = 950;

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
    bottomPinConstraint.priority = 950;
    [self addConstraint:bottomPinConstraint];

    NSLayoutConstraint *topSpacerYConstraint =
    [NSLayoutConstraint constraintWithItem:_imageView
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:20];
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

    [_imageView setContentCompressionResistancePriority:250 forOrientation:NSLayoutConstraintOrientationHorizontal];

    [self addSubview:_imageView];

    NSLayoutConstraint *xPosConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                  attribute:NSLayoutAttributeCenterX
                                                  relatedBy:NSLayoutRelationEqual
                                                     toItem:self
                                                  attribute:NSLayoutAttributeCenterX
                                                 multiplier:1.0
                                                   constant:0];

    xPosConstraint.priority = 800;

    NSLayoutConstraint *widthConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                   attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:nil
                                                   attribute:NSLayoutAttributeNotAnAttribute
                                                  multiplier:1.0
                                                    constant:NSWidth(_imageView.frame)];

    widthConstraint.priority = 800;

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

