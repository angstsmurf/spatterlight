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

#import "DebugLog.h"

@interface InfoView ()
{
    NSBox *topSpacer;

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

    if ([date isToday] || [date isYesterday]) {
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

    NSLayoutConstraint *rightMarginConstraint =
    [NSLayoutConstraint constraintWithItem:textField
                                 attribute:NSLayoutAttributeTrailing
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTrailing
                                multiplier:1.0
                                  constant:-10];

    rightMarginConstraint.priority = 900;

    textField.attributedStringValue = attrString;

    [textField setContentCompressionResistancePriority:20 forOrientation:NSLayoutConstraintOrientationHorizontal];

    [self addSubview:textField];

    [self addConstraints:@[ xPosConstraint, yPosConstraint, rightMarginConstraint ]];

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

#pragma mark Building blocks of the two layouts

// The invisible top spacer every layout hangs from. Returns its top
// constraint, a >= that finishLayoutBelow: swaps for an == when the content
// is shorter than the clip view.
- (NSLayoutConstraint *)addTopSpacerWithWidth:(CGFloat)width
                              leadingPriority:(NSLayoutPriority)leadingPriority
                                  topPriority:(NSLayoutPriority)topPriority {
    topSpacer = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, width, 0)];
    topSpacer.boxType = NSBoxSeparator;

    [topSpacer setContentCompressionResistancePriority:20 forOrientation:NSLayoutConstraintOrientationHorizontal];

    [self addSubview:topSpacer];

    topSpacer.frame = NSMakeRect(0,0, width, 1);

    topSpacer.translatesAutoresizingMaskIntoConstraints = NO;

    NSLayoutConstraint *xPosConstraint =
    [NSLayoutConstraint constraintWithItem:topSpacer
                                 attribute:NSLayoutAttributeLeading
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeLeading
                                multiplier:1.0
                                  constant:0];
    xPosConstraint.priority = leadingPriority;

    NSLayoutConstraint *topSpacerYConstraint =
    [NSLayoutConstraint constraintWithItem:topSpacer
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationGreaterThanOrEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:0];
    topSpacerYConstraint.priority = topPriority;

    NSLayoutConstraint *widthConstraint =
    [NSLayoutConstraint constraintWithItem:topSpacer
                                 attribute:NSLayoutAttributeTrailing
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTrailing
                                multiplier:1.0
                                  constant:0];

    [self addConstraints:@[xPosConstraint, topSpacerYConstraint, widthConstraint]];

    return topSpacerYConstraint;
}

// The title, its font shrunk if its longest word won't fit. Sets spaceBefore
// to spaceFactor times the height of a line of it, which the divider reuses.
- (NSView *)addTitle:(NSString *)title font:(NSFont *)font width:(CGFloat)width spaceFactor:(CGFloat)spaceFactor spaceBefore:(CGFloat *)spaceBefore lastView:(NSView *)lastView {
    // The magic number -24 means 10 points of margin and two points of textfield border on each side.
    font = [font fontFittingLongestWordOf:title inWidth:width - 24];

    *spaceBefore = [@"X" sizeWithAttributes:@{NSFontAttributeName:font}].height * spaceFactor;

    return [self addSubViewWithtext:title andFont:font andSpaceBefore:*spaceBefore andLastView:lastView];
}

// Hangs the cover image, already added by addImage, below lastView.
- (void)pinImageBelow:(NSView *)lastView {
    NSLayoutConstraint *yPosConstraint =
    [NSLayoutConstraint constraintWithItem:_imageView
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:lastView
                                 attribute:NSLayoutAttributeBottom
                                multiplier:1.0
                                  constant:10];
    [self addConstraint:yPosConstraint];
}

// The star rating, if there is one, pulled up a little towards the title.
- (NSView *)addStarRating:(NSInteger)rating lastView:(NSView *)lastView {
    NSAttributedString *starString = [InfoView starString:rating alignment:NSTextAlignmentCenter];

    if (starString.length) {
        CGFloat offset = [starString boundingRectWithSize:CGSizeMake(self.frame.size.width - 24, FLT_MAX) options:NSStringDrawingUsesLineFragmentOrigin].size.height * 0.3;

        lastView = [self addSubViewWithAttributedString:starString andSpaceBefore:-offset andLastView:lastView];
    }
    return lastView;
}

// The separator line below the title and rating.
- (NSView *)addDividerWithWidth:(CGFloat)width space:(CGFloat)space topPriority:(NSLayoutPriority)topPriority lastView:(NSView *)lastView {
    NSBox *divider = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, width, 1)];

    divider.boxType = NSBoxSeparator;
    divider.translatesAutoresizingMaskIntoConstraints = NO;

    NSLayoutConstraint *xPosConstraint =
    [NSLayoutConstraint constraintWithItem:divider
                                 attribute:NSLayoutAttributeLeading
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeLeading
                                multiplier:1.0
                                  constant:0];

    NSLayoutConstraint *yPosConstraint =
    [NSLayoutConstraint constraintWithItem:divider
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:lastView
                                 attribute:NSLayoutAttributeBottom
                                multiplier:1.0
                                  constant:space];
    yPosConstraint.priority = topPriority;

    NSLayoutConstraint *widthConstraint =
    [NSLayoutConstraint constraintWithItem:divider
                                 attribute:NSLayoutAttributeTrailing
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self
                                 attribute:NSLayoutAttributeTrailing
                                multiplier:1.0
                                  constant:0];

    NSLayoutConstraint *heightConstraint =
    [NSLayoutConstraint constraintWithItem:divider
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:nil
                                 attribute:NSLayoutAttributeNotAnAttribute
                                multiplier:1.0
                                  constant:1];

    [divider setContentCompressionResistancePriority:20 forOrientation:NSLayoutConstraintOrientationHorizontal];

    [self addSubview:divider];

    [self addConstraints:@[xPosConstraint, yPosConstraint, widthConstraint, heightConstraint]];

    return divider;
}

- (NSView *)addHeadline:(NSString *)headline lastView:(NSView *)lastView {
    NSFont *font = [NSFont systemFontOfSize:12];
    font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSSmallCapsFontMask];

    return [self addSubViewWithtext:headline.uppercaseString andFont:font andSpaceBefore:4 andLastView:lastView];
}

- (NSView *)addAuthor:(NSString *)author lastView:(NSView *)lastView {
    NSFont *font = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:14] toHaveTrait:NSItalicFontMask];

    return [self addSubViewWithtext:author andFont:font andSpaceBefore:25 andLastView:lastView];
}

- (NSView *)addBlurb:(NSString *)blurb lastView:(NSView *)lastView {
    return [self addSubViewWithtext:blurb andFont:[NSFont systemFontOfSize:12.5 weight:NSFontWeightLight] andSpaceBefore:23 andLastView:lastView];
}

// The IFID, small and dimmed, last of all.
- (NSView *)addIFIDLine:(NSString *)ifid lastView:(NSView *)lastView {
    NSTextField *field = [self addSubViewWithtext:[NSString stringWithFormat:@"IFID: %@\n", ifid.uppercaseString] andFont:[NSFont systemFontOfSize:11] andSpaceBefore:23 andLastView:lastView];

    NSMutableAttributedString *mutAttrStr = field.attributedStringValue.mutableCopy;
    [mutAttrStr addAttribute:NSForegroundColorAttributeName value:[NSColor colorNamed:@"customControlColor"] range:NSMakeRange(0, mutAttrStr.length)];
    field.attributedStringValue = mutAttrStr;
    return field;
}

// Pins the bottom to lastView, and adds the == top spacer constraint that
// replaces the >= one when everything fits in the clip view.
- (void)finishLayoutBelow:(NSView *)lastView clipView:(NSClipView *)clipView topSpacerConstant:(CGFloat)topSpacerConstant topSpacerYConstraint:(NSLayoutConstraint *)topSpacerYConstraint {
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
                                  constant:topSpacerConstant];
    newTopSpacerConstraint.active = NO;

    [self addConstraint:newTopSpacerConstraint];

    if (clipView.frame.size.height > self.frame.size.height) {
        topSpacerYConstraint.active = NO;
        newTopSpacerConstraint.active = YES;
    }
}

- (void)updateWithMetadata:(Metadata *)somedata {
    NSClipView *clipView = (NSClipView *)[InfoView addTopConstraintsToView:self];

    Game *somegame = somedata.game;

    if(somedata.cover.data)
        _imageData = (NSData *)somedata.cover.data;

    totalHeight = 0;

    CGFloat spaceBefore = 0;
    NSView *lastView;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat superViewWidth = clipView.frame.size.width;

    if (superViewWidth < 24)
        return;

    NSLayoutConstraint *topSpacerYConstraint =
    [self addTopSpacerWithWidth:superViewWidth leadingPriority:900 topPriority:NSLayoutPriorityDefaultLow];

    lastView = topSpacer;

    if (somedata.title.length) { // Every game will have a title unless something is broken
        lastView = [self addTitle:somedata.title font:[NSFont systemFontOfSize:20] width:superViewWidth spaceFactor:0.5 spaceBefore:&spaceBefore lastView:lastView];
    } else {
        NSLog(@"Error! No title!");
        return;
    }

    if (_imageData) {
        [self addImage];
        [self pinImageBelow:lastView];

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

    lastView = [self addStarRating:rating lastView:lastView];

    lastView = [self addDividerWithWidth:superViewWidth space:spaceBefore * 0.9 topPriority:NSLayoutPriorityRequired lastView:lastView];

    if (somedata.headline.length)
        lastView = [self addHeadline:somedata.headline lastView:lastView];

    if (somedata.author.length)
        lastView = [self addAuthor:somedata.author lastView:lastView];

    if (somedata.blurb.length)
        lastView = [self addBlurb:somedata.blurb lastView:lastView];

    NSString *ifid = nil;
    if (somegame)
        ifid = somegame.ifid;
    else
        ifid = somedata.ifids.anyObject.ifidString;

    if (ifid.length)
        lastView = [self addIFIDLine:ifid lastView:lastView];

    CGFloat topConstraintConstant = (clipView.frame.size.height - totalHeight) / 2;
    if (topConstraintConstant < 20 || _imageView.hasImage)
        topConstraintConstant = 0;

    [self finishLayoutBelow:lastView clipView:clipView topSpacerConstant:topConstraintConstant topSpacerYConstraint:topSpacerYConstraint];
}

- (void)updateWithDictionary:(NSDictionary *)metadict {
    NSClipView *clipView = (NSClipView *)[InfoView addTopConstraintsToView:self];

    totalHeight = 0;

    NSFont *font;
    CGFloat spaceBefore = 0;
    NSView *lastView;

    CGFloat superViewWidth = clipView.frame.size.width;

    if (superViewWidth < 24)
        return;

    NSLayoutConstraint *topSpacerYConstraint =
    [self addTopSpacerWithWidth:superViewWidth leadingPriority:NSLayoutPriorityRequired topPriority:NSLayoutPriorityRequired];

    lastView = topSpacer;

    if (!_imageData)
        _imageData = metadict[@"cover"];

    if (_imageData) {
        [self addImage];
        [self pinImageBelow:lastView];

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
        lastView = [self addTitle:title font:[NSFont systemFontOfSize:20 weight:NSFontWeightSemibold].copy width:superViewWidth spaceFactor:0.3 spaceBefore:&spaceBefore lastView:lastView];
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

    lastView = [self addStarRating:rating lastView:lastView];

    lastView = [self addDividerWithWidth:superViewWidth space:spaceBefore * 0.9 topPriority:900 lastView:lastView];

    NSString *headline = metadict[@"headline"];
    if (!headline.length && metadict[@"fileType"]) {
        headline = metadict[@"fileType"];
        if (metadict[@"fileSize"])
            headline = [headline stringByAppendingFormat:@" - %@", metadict[@"fileSize"]];
    }
    if (headline.length)
        lastView = [self addHeadline:headline lastView:lastView];

    NSString *author = metadict[@"author"];

    if (!author.length)
        author = metadict[@"AUTH"];

    if (author.length)
        lastView = [self addAuthor:author lastView:lastView];

    NSString *blurb = metadict[@"blurb"];
    if (blurb.length)
        lastView = [self addBlurb:blurb lastView:lastView];

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

    NSString *ifid = metadict[@"ifid"];

    if (ifid.length)
        lastView = [self addIFIDLine:ifid lastView:lastView];
    else
        lastView = [self addSubViewWithtext:@"\n" andFont:[NSFont systemFontOfSize:11] andSpaceBefore:0 andLastView:lastView];

    [self finishLayoutBelow:lastView clipView:clipView topSpacerConstant:0 topSpacerYConstraint:topSpacerYConstraint];
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
                                     attribute:NSLayoutAttributeLeading
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeLeading
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
                                     attribute:NSLayoutAttributeTrailing
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
                                     attribute:NSLayoutAttributeLeading
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
                                     attribute:NSLayoutAttributeTrailing
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeTrailing
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

    [NSLayoutConstraint activateConstraints:@[
        [view.leadingAnchor constraintEqualToAnchor:clipView.leadingAnchor],
        [view.trailingAnchor constraintEqualToAnchor:clipView.trailingAnchor],
        [view.topAnchor constraintEqualToAnchor:clipView.topAnchor]
    ]];

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
                                                                          toItem:self
                                                                       attribute:NSLayoutAttributeWidth
                                                                      multiplier: NSWidth(imageFrame) / NSWidth(self.frame)
                                                                        constant:0];

    NSLayoutConstraint *heightConstraint = [NSLayoutConstraint constraintWithItem:_imageView
                                                                        attribute:NSLayoutAttributeHeight
                                                                        relatedBy:NSLayoutRelationLessThanOrEqual
                                                                           toItem:_imageView
                                                                        attribute:NSLayoutAttributeWidth
                                                                       multiplier:(1 / ratio)
                                                                         constant:0];

    heightConstraint.priority = 499;

    [self addConstraints:@[xPosConstraint, widthConstraint, heightConstraint]];
}

@end
