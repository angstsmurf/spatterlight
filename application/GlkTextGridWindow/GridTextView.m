//
//  GridTextView.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "GridTextView.h"
#import "GlkTextGridWindow.h"
#import "GlkController.h"
#import "InputTextField.h"

/*
 * Extend NSTextView to ...
 *   - call keyDown and mouseDown on our GlkTextGridWindow object
 */

@implementation GridTextView

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
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

 - (NSArray *)accessibilityChildren {
    NSArray *children = [super accessibilityChildren];
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

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    GlkTextGridWindow *delegate = (GlkTextGridWindow *)self.delegate;
    NSArray *actions = [delegate.glkctl accessibilityCustomActions];
    return actions;
}

@end
