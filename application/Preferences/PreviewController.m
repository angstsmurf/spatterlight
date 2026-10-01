//
//  PreviewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-02-25.
//

#import "Theme.h"
#import "GlkStyle.h"

#import "PreviewController.h"

@interface PreviewTextView : NSTextView

@end

@implementation PreviewTextView

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if(menuItem.action != @selector(copy:) && menuItem.action != NSSelectorFromString(@"_lookUpDefiniteRangeInDictionaryFromMenu:") && menuItem.action != NSSelectorFromString(@"_searchWithGoogleFromMenu:")) {
        return NO;
    }
    return YES;
}

@end


// The line padding of the text views in game windows
static const CGFloat kLineFragmentPadding = 5;

@interface PreviewController ()

@end

@implementation PreviewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.wantsLayer = YES;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(notePreferencesChanged:) name:@"PreferencesChanged" object:nil];
}

- (void)fixScrollBar {
    NSScrollView *scrollview = _sampleTextView.enclosingScrollView;
    scrollview.scrollerStyle = NSScrollerStyleOverlay;
    scrollview.drawsBackground = YES;
    scrollview.backgroundColor = _sampleTextView.backgroundColor;
    scrollview.hasHorizontalScroller = NO;
    scrollview.hasVerticalScroller = YES;
    scrollview.verticalScroller.alphaValue = 100;
    scrollview.autohidesScrollers = YES;
}


#pragma mark Preview

- (void)notePreferencesChanged:(NSNotification *)notify {
    // Change the theme of the sample text field
    _theme = (Theme *)notify.object;
    [self updatePreviewText];
    _textHeight.constant = MIN(NSHeight(self.view.frame), NSHeight(_sampleTextView.frame));
}

// Insets the text by the theme's border, as in a game window.
- (void)applyBorder {
    NSScrollView *scrollView = _sampleTextView.enclosingScrollView;
    CGFloat border = MAX(_theme.border, 0);
    for (NSLayoutConstraint *constraint in self.view.constraints) {
        if ((constraint.firstItem == scrollView && constraint.firstAttribute == NSLayoutAttributeLeading) ||
            (constraint.secondItem == scrollView && constraint.secondAttribute == NSLayoutAttributeTrailing))
            constraint.constant = border;
    }
    // The status bar spans the whole width, so the margins and line padding
    // of the game's windows are added to the text instead.
    _sampleTextView.textContainerInset = NSZeroSize;
    _sampleTextView.textContainer.lineFragmentPadding = 0;
    CGFloat width = NSWidth(self.view.frame) - 2 * border;
    if (NSWidth(_sampleTextView.frame) != width) {
        NSRect frame = _sampleTextView.frame;
        frame.size.width = width;
        _sampleTextView.frame = frame;
    }
}

- (NSDictionary *)bufferAttributes:(GlkStyle *)style {
    NSMutableDictionary *attributes = style.resolvedAttributeDict.mutableCopy;
    NSMutableParagraphStyle *para = [attributes[NSParagraphStyleAttributeName] mutableCopy] ?: [NSMutableParagraphStyle new];
    para.firstLineHeadIndent = _theme.bufferMarginX + kLineFragmentPadding;
    para.headIndent = para.firstLineHeadIndent;
    para.tailIndent = -para.firstLineHeadIndent;
    para.paragraphSpacingBefore = _theme.bufferMarginY;
    attributes[NSParagraphStyleAttributeName] = para;
    return attributes;
}

- (void)updatePreviewText {
    [self applyBorder];
    NSMutableAttributedString *attrStr = [NSMutableAttributedString new];

    // The room name on a line of its own, like the status bar a game draws in
    // a text grid window above the story. Games draw it in reverse video.
    NSColor *statusInk = _theme.resolvedGridBackground;
    NSColor *statusPaper = _theme.gridNormal.resolvedColor;
    NSTextBlock *statusBar = [NSTextBlock new];
    statusBar.backgroundColor = statusPaper;
    [statusBar setValue:100 type:NSTextBlockPercentageValueType forDimension:NSTextBlockWidth];
    [statusBar setWidth:_theme.gridMarginX + kLineFragmentPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    [statusBar setWidth:_theme.gridMarginX + kLineFragmentPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge];
    [statusBar setWidth:_theme.gridMarginY type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge];
    [statusBar setWidth:_theme.gridMarginY type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge];

    NSMutableDictionary *statusAttributes = _theme.gridNormal.resolvedAttributeDict.mutableCopy;
    NSMutableParagraphStyle *statusPara = [statusAttributes[NSParagraphStyleAttributeName] mutableCopy] ?: [NSMutableParagraphStyle new];
    statusPara.textBlocks = @[ statusBar ];
    statusAttributes[NSParagraphStyleAttributeName] = statusPara;
    [statusAttributes removeObjectForKey:NSBackgroundColorAttributeName];
    if (statusInk)
        statusAttributes[NSForegroundColorAttributeName] = statusInk;

    [attrStr appendAttributedString:[[NSAttributedString alloc] initWithString:[NSLocalizedString(@"Palace Gate", nil) stringByAppendingString:@"\n"] attributes:statusAttributes]];
    [attrStr appendAttributedString:[[NSAttributedString alloc] initWithString:NSLocalizedString(@"A tide of perambulators surges north along the crowded Broad Walk. ", nil) attributes:[self bufferAttributes:_theme.bufferNormal]]];
    [attrStr appendAttributedString:[[NSAttributedString alloc] initWithString:NSLocalizedString(@"(Trinity, Brian Moriarty, Infocom 1986)", nil) attributes:[self bufferAttributes:_theme.bufEmph]]];
    [_sampleTextView.textStorage setAttributedString:attrStr];
    [_sampleTextView.layoutManager ensureLayoutForTextContainer:_sampleTextView.textContainer];
    _textHeight.constant = NSHeight(_sampleTextView.frame);
    _sampleTextView.backgroundColor = _theme.resolvedBufferBackground;
    NSColor *borderColor = _theme.borderBehavior == kUserOverride ? _theme.resolvedBorderColor : _theme.resolvedBufferBackground;
    self.view.layer.backgroundColor = borderColor.CGColor;
    self.view.needsLayout = YES;
}

- (CGFloat)calculateHeight {
    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithAttributedString:[_sampleTextView.textStorage copy]];
    CGFloat textWidth = _sampleTextView.frame.size.width;
    NSTextContainer *textContainer = [[NSTextContainer alloc]
                                      initWithContainerSize:NSMakeSize(textWidth, FLT_MAX)];
    textContainer.lineFragmentPadding = _sampleTextView.textContainer.lineFragmentPadding;

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];

    [layoutManager ensureLayoutForGlyphRange:NSMakeRange(0, textStorage.length)];

    CGRect proposedRect = [layoutManager usedRectForTextContainer:textContainer];
    return ceil(proposedRect.size.height);
}

- (CGFloat)fittingHeight {
    return [self calculateHeight] + 2 * MAX(_theme.border, 0);
}

- (void)viewWillLayout {
    [super viewWillLayout];
    CGFloat constant = [self calculateHeight];
    if ( _textHeight.constant != constant)
        _textHeight.constant = constant;
    if (_textHeight.constant > NSHeight(self.view.frame)) {
        [self fixScrollBar];
        _textHeight.constant = NSHeight(self.view.frame);
    }
    [self scrollToTop];
}

- (void)viewDidLayout {
    [super viewDidLayout];
    [self scrollToTop];
}

- (void)scrollToTop {
    [_sampleTextView.enclosingScrollView.contentView setBoundsOrigin:NSZeroPoint];
}


@end
