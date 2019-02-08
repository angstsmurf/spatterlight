//
//  HelpPanelController.m
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//

#import "HelpPanelController.h"



@implementation HelpTextView

// Text finder command validation (could also be done in method validateUserInterfaceItem: if you prefer)

- (BOOL) validateMenuItem:(NSMenuItem *)menuItem
{
    BOOL isValidItem = NO;

    if (menuItem.action == @selector(performTextFinderAction:)) {
        isValidItem = [self.textFinder validateAction:menuItem.tag];
    }
    // validate other menu items if needed
    // ...
    // and don't forget to call the superclass
    else {
        isValidItem = [super validateMenuItem:menuItem];
    }

    return isValidItem;
}

// Text Finder

- (NSTextFinder*) textFinder
{
    // Create the text finder on demand
    if (_textFinder == nil) {
        _textFinder = [[NSTextFinder alloc] init];
        _textFinder.client = self;
        _textFinder.findBarContainer = self.enclosingScrollView;
        _textFinder.incrementalSearchingEnabled = YES;
        _textFinder.incrementalSearchingShouldDimContentView = NO;
    }

    return _textFinder;
}

- (void) resetTextFinder
{
    if  (_textFinder != nil) {
        [_textFinder noteClientStringWillChange];
    }
}

// This is where the commands are actually sent to the text finder
- (void) performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender
{
    [self.textFinder performAction:sender.tag];
}

@end


@implementation HelpPanelController

- (IBAction)copyButton:(id)sender {
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard clearContents];
    [pasteBoard writeObjects:@[textView.string]];

    // Selecting all the text is pretty useless, but better than no feedback at all I guess
    [textView setSelectedRange:NSMakeRange(0, textView.string.length)];
}

- (void) showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title
{
    NSWindow *helpWindow = self.window;

    helpWindow.title = title;

    // Do nothing if we are already showing the text
    if ((![helpWindow isVisible]) || (![text.string isEqualToString:textView.string]))
    {
        [textView resetTextFinder];

        CGRect screenframe = [NSScreen mainScreen].visibleFrame;

        NSString *string = text.string;

        CGFloat oldheight = helpWindow.frame.size.height;

        textView.string = string;

        CGRect winrect = [self frameForString:string];
        winrect.origin = helpWindow.frame.origin;

        //If the entire text does not fit on screen, don't change height at all
        if (winrect.size.height > screenframe.size.height)
            winrect.size.height = oldheight;

        // When we reuse the window it will remember our last scroll position,
        // so we reset it here

        // Scroll the vertical scroller to top
        scrollView.verticalScroller.floatValue = 0;

        // Scroll the contentView to top
        [scrollView.contentView scrollToPoint:NSMakePoint(0, 0)];

        [self showWindow: helpWindow];

        CGFloat offset = winrect.size.height - oldheight;

        winrect.origin.y -= offset;

        //If window is partly off the screen, move it (just) inside
        if (NSMaxX(winrect) > NSMaxX(screenframe))
            winrect.origin.x = NSMaxX(screenframe) - winrect.size.width;

        if (NSMinY(winrect) < 0)
            winrect.origin.y = NSMinY(screenframe);

        [helpWindow setFrame:winrect display:YES animate:YES];
        [textView scrollRectToVisible: NSMakeRect(0, 0, 0, 0)];
    }

    [helpWindow makeKeyAndOrderFront:nil];
}


- (CGRect)frameForString:(NSString *)string
{
    NSString *proposedLine;

    CGFloat textWidth = 0;
    NSUInteger stringLength = string.length;
    NSRange range;

    for (NSUInteger index = 0; index < stringLength;)
    {
        range = [string lineRangeForRange:NSMakeRange(index, 0)];
        index = NSMaxRange(range);
        proposedLine = [string substringWithRange:range];
        CGSize stringSize = [proposedLine sizeWithAttributes:@{NSFontAttributeName: textView.font}];
        CGFloat width = stringSize.width;

        if (width > textWidth)
        {
            textWidth = width;
        }
    }

    CGRect screenframe = [NSScreen mainScreen].visibleFrame;

    if (textWidth > screenframe.size.width)
        textWidth = screenframe.size.width / 3;

    textWidth = ceil(textWidth);

    CGFloat padding = textView.textContainer.lineFragmentPadding;

    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithString:string];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(textWidth + padding * 2, FLT_MAX)];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];

    [textStorage addAttribute:NSFontAttributeName value:textView.font
                        range:NSMakeRange(0, textStorage.length)];
    textContainer.lineFragmentPadding = padding;

    [layoutManager glyphRangeForTextContainer:textContainer];

    CGRect proposedRect = [layoutManager usedRectForTextContainer:textContainer];
    CGRect contentRect = ((NSView *)self.window.contentView).frame;

    // These magic numbers are the required distance between the scrollview
    // and the window content view edges. I wish a knew a way to calculate them.
    contentRect.size.width = proposedRect.size.width + 40;
    contentRect.size.height = proposedRect.size.height + 81;

    //Hopefully, by using frameRectForContentRect, this code will still work
    //on OS versions with a different window title bar height
    return [self.window frameRectForContentRect:contentRect];
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame
{
    CGRect screenframe = [NSScreen mainScreen].visibleFrame;

    CGFloat oldheight = window.frame.size.height;

    newFrame = [self frameForString:textView.string];

    CGFloat offset = newFrame.size.height - oldheight;

    newFrame.origin.y = window.frame.origin.y - offset;
    newFrame.origin.x = (screenframe.size.width - newFrame.size.width) / 2;
    
    return newFrame;
};

- (id)accessibilityFocusedUIElement
{
    return textView;
}

@end
