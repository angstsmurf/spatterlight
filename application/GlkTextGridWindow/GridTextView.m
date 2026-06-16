//
//  GridTextView.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "GridTextView.h"
#import "GlkTextGridWindow.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "InputTextField.h"
#import "Theme.h"
#import "Preferences.h"

/*
 * Extend NSTextView to ...
 *   - call keyDown and mouseDown on our GlkTextGridWindow object
 */

@implementation GridTextView

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

- (void)setFrame:(NSRect)frame {
    // For some reason, this text view sometimes wants to shrink a lot
    // when the user resizes the window vertically, so we check for those
    // calls and skip them.
    if (self.inLiveResize && frame.size.height < self.frame.size.height)
        return;
    if (frame.size.height < self.frame.size.height && frame.size.height > 0) {
        Theme *theme = ((GlkTextGridWindow *)self.delegate).theme;
        if (frame.size.height < theme.cellHeight + 2 * theme.gridMarginY)
            return;
    }
    super.frame = frame;
}

- (void)keyDown:(NSEvent *)theEvent {
    [(GlkTextGridWindow *)self.delegate keyDown:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent {
    if (![(GlkTextGridWindow *)self.delegate myMouseDown:theEvent] || ((GlkTextGridWindow *)self.delegate).glkctl.gameID == kGameIsBeyondZork)
    [super mouseDown:theEvent];
}

- (NSArray *)accessibilityCustomRotors  {
   return [((GlkTextGridWindow *)self.delegate).glkctl createCustomRotors];
}

// A merged side-by-side quote box (e.g. Trinity's Clarke/Lebling pair) lays its
// two columns out interleaved in a single text view. Expose each column as its
// own AXStaticText child so VoiceOver reads them as two separate blocks rather
// than alternating line by line. Built lazily and cached on the delegate.
- (NSArray *)buildQuoteboxAXChildren {
    GlkTextGridWindow *delegate = (GlkTextGridWindow *)self.delegate;
    Theme *theme = delegate.theme;
    CGFloat cellW = theme.cellWidth;
    CGFloat cellH = theme.cellHeight;
    CGFloat marginX = theme.gridMarginX;
    CGFloat marginY = theme.gridMarginY;
    CGFloat height = delegate.quoteboxSize.height * cellH;

    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (NSDictionary *col in delegate.quoteboxColumns) {
        NSUInteger offset = [col[@"offset"] unsignedIntegerValue];
        NSUInteger width = [col[@"width"] unsignedIntegerValue];
        NSAccessibilityElement *el =
            [NSAccessibilityElement accessibilityElementWithRole:NSAccessibilityStaticTextRole
                                                           frame:NSZeroRect
                                                           label:nil
                                                          parent:self];
        el.accessibilityValue = col[@"value"];
        el.accessibilityFrameInParentSpace =
            NSMakeRect(marginX + offset * cellW, marginY, width * cellW, height);
        [children addObject:el];
    }
    return children;
}

 - (NSArray *)accessibilityChildren {
    GlkTextGridWindow *delegate = (GlkTextGridWindow *)self.delegate;
    if (delegate.quoteboxColumns.count) {
        if (!delegate.quoteboxAXChildren)
            delegate.quoteboxAXChildren = [self buildQuoteboxAXChildren];
        return delegate.quoteboxAXChildren;
    }
    NSArray *children = super.accessibilityChildren;
    InputTextField *input = ((GlkTextGridWindow *)self.delegate).input;
    if (input) {
        MyFieldEditor *fieldEditor = (((GlkTextGridWindow *)self.delegate).input.fieldEditor);
        if (fieldEditor) {
            if ([children indexOfObject:fieldEditor] == NSNotFound)
                children = [children arrayByAddingObject:fieldEditor];
        }
    }
    return children;
}

- (void)mouseMoved:(NSEvent *)event {
    [super mouseMoved:event];
    if ([[NSCursor currentCursor] isEqualTo:[NSCursor IBeamCursor]])
        [[NSCursor arrowCursor] set];
}

- (NSTouchBar *)makeTouchBar {
  return nil;
}

- (void)changeAttributes:(id)sender {
    [(NSTextView *)[Preferences instance].dummyTextView changeAttributes:sender];
}

- (NSArray *)accessibilityCustomActions {
    GlkTextGridWindow *delegate = (GlkTextGridWindow *)self.delegate;
    NSArray *actions = delegate.glkctl.accessibilityCustomActions;
    return actions;
}

// For a merged side-by-side quote box, present this view as a plain group whose
// children (the two columns) carry the text, so VoiceOver doesn't also read the
// interleaved text-area value.
- (NSAccessibilityRole)accessibilityRole {
    if (((GlkTextGridWindow *)self.delegate).quoteboxColumns.count)
        return NSAccessibilityGroupRole;
    return super.accessibilityRole;
}

- (id)accessibilityValue {
    if (((GlkTextGridWindow *)self.delegate).quoteboxColumns.count)
        return nil;
    return super.accessibilityValue;
}

@end
