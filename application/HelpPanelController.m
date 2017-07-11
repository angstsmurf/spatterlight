//
//  HelpPanelController.m
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//

#import "HelpPanelController.h"


@implementation HelpPanelController

- (void) showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title
{
    NSWindow *helpWindow = [self window];

    helpWindow.title = title;

    // Do nothing if we are already showing the text
    if ((![helpWindow isVisible]) || (![text.string isEqualToString:_textView.string]))
    {
        NSString *string = text.string;

        CGFloat maxWidth = [self optimalWidthForText:string];

        [_textView setTextContainerInset:NSMakeSize(10, 0)];
        NSSize size = _textView.textContainer.size;

        size.width = maxWidth;

        CGFloat margin = maxWidth * 0.096;

        [helpWindow.contentView setFrameSize: NSMakeSize(maxWidth + margin, helpWindow.contentView.frame.size.height)];

        NSRect winrect = helpWindow.frame;
        winrect.size.width = maxWidth + margin;

        if (winrect.size.height < 200)
            winrect.size.height = 300;

        [_textView setFrameSize:size];

        _textView.string = string;

        // When we reuse the window it will remember our last scroll position,
        // so we reset it here
        [_scrollView.documentView scrollPoint:NSMakePoint(0.0, 0.0)];

        [self showWindow: helpWindow];

        [helpWindow setFrame:winrect display:YES animate:YES];

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
    
    return maxWidth;
}


- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame
{
    CGFloat oldheight = window.frame.size.height;
    CGFloat maxWidth = [self optimalWidthForText:_textView.string];
    //I have no idea what I am doing
    CGFloat margin = maxWidth * 0.096;
    CGSize size = [self sizeForText:_textView.textStorage maxWidth:maxWidth+1];

    CGFloat newheight = size.height;

    newFrame.size.width = maxWidth + margin;

    newFrame.size.height = newheight + 100;

    CGFloat offset = newFrame.size.height - oldheight;

    newFrame.origin.y -= offset;


    return newFrame;
};

- (CGSize)sizeForText:(NSAttributedString *)string maxWidth:(CGFloat)width
{
    CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString((__bridge CFAttributedStringRef)string);

    CFIndex offset = 0, length;
    CGFloat y = 0, lineHeight;
    do {
        length = CTTypesetterSuggestLineBreak(typesetter, offset, width);
        CTLineRef line = CTTypesetterCreateLine(typesetter, CFRangeMake(offset, length));

        CGFloat ascent, descent, leading;
        CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

        CFRelease(line);

        offset += length;

        ascent = roundf(ascent);
        descent = roundf(descent);
        leading = roundf(leading);

        lineHeight = ascent + descent + leading;
        lineHeight = lineHeight + ((leading > 0) ? 0 : roundf(0.2*lineHeight)); //add 20% space

        y += lineHeight;

    } while (offset < [string length]);

    CFRelease(typesetter);
    
    return CGSizeMake(width, y);
}

- (IBAction)copyButton:(id)sender {
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard clearContents];
    [pasteBoard writeObjects:@[_textView.string]];

    // Selecting all the text is pretty useless, but better than no feedback at all I guess
    [_textView setSelectedRange:NSMakeRange(0, _textView.string.length)];

}

@end
