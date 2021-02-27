//
//  NonScalingScrollView.m
//  Spatterlight
//
//  Created by Administrator on 2021-02-27.
//

#import "NonScalingScrollView.h"


@interface NonScalingScrollView () {

    BOOL currentScrollGoesNoFurther;

}
@end

@implementation NonScalingScrollView

-(void)scrollWheel:(NSEvent *)theEvent {
    /* Ensure that both scrollbars are flashed when the user taps trackpad with two fingers */
    if (theEvent.phase == NSEventPhaseMayBegin) {
        [super scrollWheel:theEvent];
        [[self nextResponder] scrollWheel:theEvent];
        return;
    }
    /* Check the scroll direction only at the beginning of a gesture for modern scrolling devices */
    /* Check every event for legacy scrolling devices */
    if (theEvent.phase == NSEventPhaseBegan ||
        (theEvent.phase == NSEventPhaseNone && theEvent.momentumPhase == NSEventPhaseNone)) {

        NSView *clipView = self.contentView;
        NSView *documentView = self.documentView;

        BOOL atTop = (clipView.bounds.origin.y <= 0);
        BOOL atBottom = (NSMaxY(clipView.bounds) >= NSHeight(documentView.bounds));

        // Check for horizontal scrolling or up when at top or down when at bottom
        currentScrollGoesNoFurther = (fabs(theEvent.scrollingDeltaX) > fabs(theEvent.scrollingDeltaY) ||
                                      (atTop && theEvent.scrollingDeltaY > 0) ||
                                      (atBottom && theEvent.scrollingDeltaY < 0));
    }
    if ( currentScrollGoesNoFurther ) {
        [[self nextResponder] scrollWheel:theEvent];
    } else {
        [super scrollWheel:theEvent];
    }
}

-(BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

/* Reassure AppKit that ScalingScrollView supports live resize content preservation, even though it's a subclass that could have modified NSScrollView in such a way as to make NSScrollView's live resize content preservation support inoperative. By default this is disabled for NSScrollView subclasses.
 */
- (BOOL)preservesContentDuringLiveResize {
    return [self drawsBackground];
}

@end
