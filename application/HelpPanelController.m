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
		CGRect screenframe = [[NSScreen mainScreen] visibleFrame];
		CGRect contentrect = ((NSView *)helpWindow.contentView).frame;

		NSString *string = text.string;

		CGFloat textWidth = [self longestLine:string];

		CGFloat oldheight = helpWindow.frame.size.height;

		_textView.string = string;

        NSSize containersize = _textView.textContainer.containerSize;

        if (textWidth < screenframe.size.width)
        {
            containersize.width = textWidth;
            [_textView.textContainer setContainerSize:containersize];
        }
        else
        {
            textWidth = containersize.width;
        }

        CGFloat windowWidth = textWidth + 19;

        [helpWindow.contentView setFrameSize: NSMakeSize(windowWidth, contentrect.size.height)];

		float proposedHeight = [self heightForString:_textView.string font:_textView.font andWidth:textWidth
										  andPadding:_textView.textContainer.lineFragmentPadding];

        contentrect = ((NSView *)helpWindow.contentView).frame;
		contentrect.size.height = proposedHeight + 82; // 1 x standard 20pt margin above, 3 x below, 1pt border x 2

		//Hopefully, by using frameRectForContentRect, this code will still work
		//on OS versions with a different window title bar height
		NSRect winrect = [helpWindow frameRectForContentRect:contentrect];

		winrect.origin = helpWindow.frame.origin;

		//If the entire text does not fit on screen, don't change height at all
		if (winrect.size.height > screenframe.size.height)
			winrect.size.height = oldheight;

		// When we reuse the window it will remember our last scroll position,
		// so we reset it here

        // Scroll the vertical scroller to top
        if ([_scrollView hasVerticalScroller]) {
            _scrollView.verticalScroller.floatValue = 0;
        }

        // Scroll the contentView to top
        [_scrollView.contentView scrollToPoint:NSMakePoint(0, 0)];

		[self showWindow: helpWindow];

		CGFloat offset = winrect.size.height - oldheight;

		winrect.origin.y -= offset;

		//If window is partly off the screen, move it (just) inside
		if (NSMaxX(winrect) > NSMaxX(screenframe))
			winrect.origin.x = NSMaxX(screenframe) - windowWidth;

		if (NSMinY(winrect) < 0)
			winrect.origin.y = NSMinY(screenframe);

		[helpWindow setFrame:winrect display:YES animate:YES];
		[_textView scrollRectToVisible: NSMakeRect(0, 0, 0, 0)];
	}
	[helpWindow makeKeyAndOrderFront:nil];
}

- (CGFloat)longestLine:(NSString *)string
{
	NSString *proposedLine;

	NSUInteger index, stringLength = [string length];
	CGFloat textWidth = 0;
	NSRange range;
	NSFont *standardFont = [NSFont systemFontOfSize:[NSFont systemFontSize]];

	for (index = 0; index < stringLength;)
	{
		range = [string lineRangeForRange:NSMakeRange(index, 0)];
		index = NSMaxRange(range);
		proposedLine = [string substringWithRange:range];
		CGSize stringSize = [proposedLine sizeWithAttributes:@{NSFontAttributeName:standardFont}];
		CGFloat width = stringSize.width;

		if (width > textWidth)
		{
			textWidth = width;
		}
	}

	// Unless we add 2, the last word on the longest line will break. Some kind of 1-point border on each side, I guess. It is not the line fragment padding, that is 5.

	textWidth += 2;
	return textWidth;
}

- (float)heightForString:(NSString *)myString font:(NSFont *)myFont andWidth:(float)myWidth andPadding:(float)padding
{
	NSTextStorage *textStorage = [[NSTextStorage alloc] initWithString:myString];
	NSTextContainer *textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(myWidth, FLT_MAX)];
	NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
	[layoutManager addTextContainer:textContainer];
	[textStorage addLayoutManager:layoutManager];
	[textStorage addAttribute:NSFontAttributeName value:myFont
						range:NSMakeRange(0, textStorage.length)];
	textContainer.lineFragmentPadding = padding;

	(void) [layoutManager glyphRangeForTextContainer:textContainer];
	return [layoutManager usedRectForTextContainer:textContainer].size.height + 2;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
						defaultFrame:(NSRect)newFrame
{
	CGFloat oldheight = window.frame.size.height;
	CGFloat windowWidth = [self longestLine:_textView.string];
	float proposedHeight = [self heightForString:_textView.string font:_textView.font andWidth:windowWidth
									  andPadding:_textView.textContainer.lineFragmentPadding];

	CGRect dummyrect = ((NSView *)window.contentView).frame;
	dummyrect.size.height = proposedHeight + 82; // 1 standard 20pt margin above, 3 below, 1pt border

	CGRect winrect = [window frameRectForContentRect:dummyrect];
	CGFloat newheight = winrect.size.height;

	newFrame.size.width = windowWidth + 19;
	newFrame.size.height = newheight;

	CGFloat offset = newFrame.size.height - oldheight;

	newFrame.origin.y = window.frame.origin.y - offset;
	newFrame.origin.x = ([[NSScreen mainScreen] visibleFrame].size.width - windowWidth) / 2;

	return newFrame;
};

@end
