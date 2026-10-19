//
//  BezelWindow.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-29.
//
#import <QuartzCore/QuartzCore.h>

#import "BezelWindow.h"

@implementation BezelWindow

- (instancetype)initWithContentRect:(NSRect)contentRect {

    self = [super initWithContentRect:contentRect
                            styleMask: NSWindowStyleMaskBorderless
                              backing: NSBackingStoreBuffered
                                defer: NO];

    if (self) {
        self.contentView = [self makeVisualEffectsBackingViewWithSize:contentRect.size];

        self.releasedWhenClosed = NO;
        self.ignoresMouseEvents = YES;
        self.opaque = NO;
        self.backgroundColor = NSColor.clearColor;
    }
    return self;
}

- (NSVisualEffectView *)makeVisualEffectsBackingViewWithSize:(NSSize)size {
    NSVisualEffectView *visualEffectView = [NSVisualEffectView new];
    visualEffectView.wantsLayer = YES;
    visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    if (@available(macOS 10.14, *)) {
        visualEffectView.material = NSVisualEffectMaterialHUDWindow;
    } else {
        visualEffectView.material = NSVisualEffectMaterialLight;
    }
    visualEffectView.emphasized = YES;
    visualEffectView.state = NSVisualEffectStateActive;
    visualEffectView.maskImage = [self roundedRectMaskWithSize:size cornerRadius:18];

    return visualEffectView;
}

- (NSImage *)roundedRectMaskWithSize:(NSSize)size cornerRadius:(CGFloat)cornerRadius {
    // Drawn on demand, so the corners are rendered at the backing scale.
    NSImage *maskImage = [NSImage imageWithSize:size flipped:NO drawingHandler:^BOOL(NSRect dstRect) {
        [NSColor.blackColor set];
        [[NSBezierPath bezierPathWithRoundedRect:dstRect xRadius:cornerRadius yRadius:cornerRadius] fill];
        return YES;
    }];

    maskImage.capInsets = NSEdgeInsetsMake(cornerRadius, cornerRadius, cornerRadius, cornerRadius);
    maskImage.resizingMode = NSImageResizingModeStretch;

    return maskImage;
}

- (void)showAndFadeOutWithDelay:(CGFloat)delay duration:(CGFloat)duration completion:(nullable dispatch_block_t)completion {
    [self orderFront:nil];

    // The blocks retain the window strongly, which is what keeps it alive
    // until the fade completes. Do not weakify.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            context.duration = duration;
            context.timingFunction = [CAMediaTimingFunction functionWithName:
                                      kCAMediaTimingFunctionLinear];
            self.animator.alphaValue = 0;
        } completionHandler:^{
            [self orderOut:nil];
            if (completion)
                completion();
        }];
    });
}

@end
