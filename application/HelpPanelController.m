//
//  HelpPanelController.m
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//

#import "HelpPanelController.h"


@implementation HelpPanelController

- (IBAction)copyButton:(id)sender {
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard clearContents];
    [pasteBoard writeObjects:@[_textView.string]];
    
    // Selecting all the text is pretty useless, but better than no feedback at all I guess
    [_textView setSelectedRange:NSMakeRange(0, _textView.string.length)];
    
}

- (void) showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title
{
    NSWindow *helpWindow = [self window];
    
    helpWindow.title = title;
    
    // Do nothing if we are already showing the text
    if ((![helpWindow isVisible]) || (![text.string isEqualToString:_textView.string]))
    {
        CGFloat oldheight = helpWindow.frame.size.height;

        NSString *string = text.string;
        
        CGFloat maxWidth = [self optimalWidthForText:string];
        
        [_textView setTextContainerInset:NSMakeSize(10, 0)];
        NSSize size = _textView.textContainer.containerSize;
        
        size.width = maxWidth - 60;
        
        [helpWindow.contentView setFrameSize: NSMakeSize(maxWidth, [[NSScreen mainScreen] visibleFrame].size.height)];
        
        NSRect winrect = helpWindow.frame;
        winrect.size.width = maxWidth;
        
        
        if (NSMaxX(winrect) > NSMaxX([[NSScreen mainScreen] visibleFrame]))
            winrect.origin.x = NSMaxX([[NSScreen mainScreen] visibleFrame]) - maxWidth;
        
        
        [_textView setFrameSize:size];
        
        _textView.string = string;
        
        float width = _textView.frame.size.width - 2 * _textView.textContainerInset.width;
        
        float proposedHeight = [self heightForString:_textView.string font:_textView.font andWidth:width
                                           andPadding:_textView.textContainer.lineFragmentPadding];
        
        winrect.size.height = proposedHeight + 125;
        
        if (winrect.size.height > [[NSScreen mainScreen] visibleFrame].size.height && [[NSScreen mainScreen] visibleFrame].size.height/2 > oldheight)
            winrect.size.height = [[NSScreen mainScreen] visibleFrame].size.height/2;
        

        // When we reuse the window it will remember our last scroll position,
        // so we reset it here
        [_scrollView.documentView scrollPoint:NSMakePoint(0.0, 0.0)];
        [_textView scrollRectToVisible: NSMakeRect(0, 0, 0, 0)];
        
        [self showWindow: helpWindow];
        

        CGFloat offset = winrect.size.height - oldheight;
        
        winrect.origin.y -= offset;
        
        [helpWindow setFrame:winrect display:YES animate:YES];
        [_textView scrollRectToVisible: NSMakeRect(0, 0, 0, 0)];
        
    }
    
    [helpWindow makeKeyAndOrderFront:nil];
    
}

- (CGFloat)optimalWidthForText:(NSString *)string
{
    
    NSString *longestLine;
    
    NSUInteger index, stringLength = [string length];
    CGFloat maxWidth = 0;
    NSRange range;
    NSFont *standardFont = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    
    for (index = 0; index < stringLength;)
    {
        range = [string lineRangeForRange:NSMakeRange(index, 0)];
        index = NSMaxRange(range);
        longestLine = [string substringWithRange:range];
        CGSize stringSize = [longestLine sizeWithAttributes:@{NSFontAttributeName:standardFont}];
        CGFloat width = stringSize.width;
        
        if (width > maxWidth)
        {
            maxWidth = width;
        }
    }
    
    if (maxWidth > [[NSScreen mainScreen] visibleFrame].size.width)
        return [[NSScreen mainScreen] visibleFrame].size.width / 2;
    
    return maxWidth + 65; //This is needed to fit the longest line. There must be a way to calculate this.
}

- (float)heightForString:(NSString *)myString font:(NSFont *)myFont andWidth:(float)myWidth andPadding:(float)padding {
    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithString:myString];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(myWidth, FLT_MAX)];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];
    [textStorage addAttribute:NSFontAttributeName value:myFont
                        range:NSMakeRange(0, textStorage.length)];
    textContainer.lineFragmentPadding = padding;
    
    (void) [layoutManager glyphRangeForTextContainer:textContainer];
    return [layoutManager usedRectForTextContainer:textContainer].size.height;
}


- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame
{
    CGFloat oldheight = window.frame.size.height;
    CGFloat maxWidth = [self optimalWidthForText:_textView.string];
        
    float width = _textView.frame.size.width - 2 * _textView.textContainerInset.width;

    
    float proposedHeight = [self heightForString:_textView.string font:_textView.font andWidth:width
                                      andPadding:_textView.textContainer.lineFragmentPadding];
    
    CGSize size = NSMakeSize(maxWidth - 59, proposedHeight + 125);
    
    CGFloat newheight = size.height;
    
    newFrame.size.width = maxWidth;
    newFrame.size.height = newheight;
    
    CGFloat offset = newFrame.size.height - oldheight;
    
    newFrame.origin.y = window.frame.origin.y - offset;
    
    newFrame.origin.x = ([[NSScreen mainScreen] visibleFrame].size.width - maxWidth) / 2;
    
    return newFrame;

};




@end
