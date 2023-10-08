//
//  GridTextView.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "GridTextView.h"
#import "GlkTextGridWindow.h"
#import "GlkController.h"

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
    super.frame = frame;
}

- (void)keyDown:(NSEvent *)theEvent {
    [(GlkTextGridWindow *)self.delegate keyDown:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent {
    if (![(GlkTextGridWindow *)self.delegate myMouseDown:theEvent] || ((GlkTextGridWindow *)self.delegate).glkctl.beyondZork)
    [super mouseDown:theEvent];
}

- (NSArray *)accessibilityCustomRotors  {
   return [((GlkTextGridWindow *)self.delegate).glkctl createCustomRotors];
}

- (void)mouseMoved:(NSEvent *)event {
    [super mouseMoved:event];
    if ([[NSCursor currentCursor] isEqualTo:[NSCursor IBeamCursor]])
        [[NSCursor arrowCursor] set];
}

- (NSTouchBar *)makeTouchBar {
  return nil;
}

- (NSArray *)accessibilityCustomActions {
    GlkTextGridWindow *delegate = (GlkTextGridWindow *)self.delegate;
    NSArray *actions = delegate.glkctl.accessibilityCustomActions;
    return actions;
}

@end
