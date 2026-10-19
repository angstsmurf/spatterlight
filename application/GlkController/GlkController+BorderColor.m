#import "GlkController_Private.h"

#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "Preferences.h"
#import "NSColor+integer.h"

#import "DebugLog.h"

// The size a window has, or is about to get if a resize is pending.
static NSSize currentSizeOfWindow(GlkWindow *win) {
    return win.framePending ? win.pendingFrame.size : win.bounds.size;
}

@implementation GlkController (BorderColor)

- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow {
    if (changedBorderThisTurn)
        return;
    NSSize windowsize = currentSizeOfWindow(aWindow);
    CGFloat relativeSize = (windowsize.width * windowsize.height) / (self.gameView.bounds.size.width * self.gameView.bounds.size.height);
    if (relativeSize < 0.70 && ![aWindow isKindOfClass:[GlkTextBufferWindow class]])
        return;

    if (aWindow == [self largestWindow]) {
        [self setBorderColor:color];
    }
}

- (void)setBorderColor:(NSColor *)color {
    if (!color) {
        NSLog(@"setBorderColor called with a nil color!");
        return;
    }

    Theme *theme = self.theme;
    if (theme.borderBehavior == kAutomatic) {
        self.lastAutoBGColor = color;
    } else {
        color = theme.borderColor;
        if (self.lastAutoBGColor == nil)
            self.lastAutoBGColor = color;
    }

    self.bgcolor = color;
    // The Narcolepsy window mask overrides all border colors
    if (self.gameID == kGameIsNarcolepsy && theme.doStyles && theme.doGraphics) {
        self.borderView.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
        return;
    }

    if (theme.doStyles || [color isEqualToColor:theme.bufferBackground] || [color isEqualToColor:theme.gridBackground] || theme.borderBehavior == kUserOverride) {
        self.borderView.layer.backgroundColor = color.CGColor;

        [Preferences instance].borderColorWell.color = color;
    }
}

- (GlkWindow *)largestWindow {
    GlkWindow *largestWin = nil;
    CGFloat largestSize = 0;

    NSArray *winArray = self.gwindows.allValues;

    // Check for status window plus buffer window arrangement
    // and return the buffer window
    if (self.gwindows.count == 2) {
        if ([winArray[0] isKindOfClass:[GlkTextGridWindow class]] && [winArray[1] isKindOfClass:[GlkTextBufferWindow class]] )
            return winArray[1];
        if ([winArray[0] isKindOfClass:[GlkTextBufferWindow class]] && [winArray[1] isKindOfClass:[GlkTextGridWindow class]] )
            return winArray[0];
    }
    for (GlkWindow *win in winArray) {
        NSSize windowsize = currentSizeOfWindow(win);
        CGFloat winarea = windowsize.width * windowsize.height;
        if (winarea >= largestSize) {
            largestSize = winarea;
            largestWin = win;
        }
    }

    return largestWin;
}

@end
