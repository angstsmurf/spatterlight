//
//  PreviewController.m
//  Spatterlight
//
//  Created by Administrator on 2023-02-25.
//

#import "Theme.h"
#import "GlkStyle.h"

#import "Preferences.h"
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

- (void)updatePreviewText {
    if (NSWidth(_sampleTextView.frame) != NSWidth(_sampleTextView.enclosingScrollView.frame)) {
        NSRect frame = _sampleTextView.frame;
        frame.size.width = NSWidth(_sampleTextView.enclosingScrollView.frame);
        _sampleTextView.frame = frame;
    }
    NSMutableAttributedString *attrStr = [NSMutableAttributedString new];
    NSMutableDictionary *attributes = _theme.bufSubH.attributeDict.mutableCopy;

    NSParagraphStyle *para = _theme.bufferNormal.attributeDict[NSParagraphStyleAttributeName];
    attributes[NSParagraphStyleAttributeName] = para;
    
    [attrStr appendAttributedString:[[NSAttributedString alloc] initWithString:@"Palace Gate" attributes:attributes]];
    [attrStr appendAttributedString:[[NSAttributedString alloc] initWithString:@" A tide of perambulators surges north along the crowded Broad Walk. " attributes:_theme.bufferNormal.attributeDict]];
    [attrStr appendAttributedString:[[NSAttributedString alloc] initWithString:@"(Trinity, Brian Moriarty, Infocom 1986)" attributes:_theme.bufEmph.attributeDict]];
    [_sampleTextView.textStorage setAttributedString:attrStr];
    [_sampleTextView.layoutManager ensureLayoutForTextContainer:_sampleTextView.textContainer];
    _textHeight.constant = NSHeight(_sampleTextView.frame);
    _sampleTextView.backgroundColor = _theme.bufferBackground;
    self.view.layer.backgroundColor = _theme.bufferBackground.CGColor;
    self.view.needsLayout = YES;
}

- (CGFloat)calculateHeight {
    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithAttributedString:[_sampleTextView.textStorage copy]];
    CGFloat textWidth = _sampleTextView.frame.size.width;
    NSTextContainer *textContainer = [[NSTextContainer alloc]
                                      initWithContainerSize:NSMakeSize(textWidth, FLT_MAX)];

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];

    [layoutManager ensureLayoutForGlyphRange:NSMakeRange(0, textStorage.length)];

    CGRect proposedRect = [layoutManager usedRectForTextContainer:textContainer];
    return ceil(proposedRect.size.height);
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
