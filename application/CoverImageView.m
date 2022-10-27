//
//  CoverImageView.m
//  Spatterlight
//
//  Created by Administrator on 2021-01-05.
//

#import "CoverImageView.h"
#import "GlkController.h"
#import "CoverImageHandler.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#import <QuartzCore/QuartzCore.h>


@implementation CoverImageView

- (instancetype)initWithFrame:(NSRect)frame delegate:(CoverImageHandler *)delegate {
    Game *game = delegate.glkctl.game;
    self = [super initWithGame:game image:nil];
    if (self) {
        _delegate = delegate;
        self.frame = frame;
    }
    return self;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)keyDown:(NSEvent *)theEvent {
    [_delegate forkInterpreterTask];
}

- (void)myMouseDown:(NSEvent *)event {
    [_delegate forkInterpreterTask];
}

- (void)layout {
    if (self.image && !_delegate.glkctl.ignoreResizes && !_inFullscreenResize) {
            [self positionImage];
    }
    [super layout];
}

- (void)positionImage {
    if (self.ratio == 0)
        return;
    NSView *superview = self.superview;
    NSSize windowSize = superview.frame.size;

    NSRect imageFrame = NSMakeRect(0,0, windowSize.width, windowSize.width / self.ratio);
    if (imageFrame.size.height > windowSize.height) {
        imageFrame = NSMakeRect(0,0, windowSize.height * self.ratio, windowSize.height);
        imageFrame.origin.x = (windowSize.width - imageFrame.size.width) / 2;
    } else {
        imageFrame.origin.y = (windowSize.height - imageFrame.size.height) / 2;
        if (NSMaxY(imageFrame) > NSMaxY(superview.frame))
            imageFrame.origin.y = NSMaxY(superview.frame) - imageFrame.size.height;
    }

    self.frame = imageFrame;

    [self positionImagelayer];
}


- (BOOL)isAccessibilityElement {
    return (self.image != nil);
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}

@end
