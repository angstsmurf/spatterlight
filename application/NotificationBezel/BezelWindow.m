//
//  BezelWindow.m
//  BezelTest
//
//  Created by Administrator on 2021-06-29.
//
#import <QuartzCore/QuartzCore.h>

#import "BezelWindow.h"
#import "BezelContentView.h"

@implementation BezelWindow

- (instancetype)initWithContentRect:(NSRect)contentRect {

    self = [super initWithContentRect:contentRect
                            styleMask: NSWindowStyleMaskBorderless
                              backing: NSBackingStoreBuffered
                                defer: NO];

    if (self) {
        self.contentView = [self makeVisualEffectsBackingView];

        self.minSize = contentRect.size;
        self.maxSize = contentRect.size;

        self.releasedWhenClosed = NO;
        self.ignoresMouseEvents = YES;
        self.opaque = NO;
        self.backgroundColor = NSColor.clearColor;
        self.contentView.wantsLayer = YES;
    }
    return self;
}


- (NSVisualEffectView *)makeVisualEffectsBackingView {
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
    visualEffectView.maskImage = [self roundedRectMaskWithSize:self.contentView.frame.size cornerRadius:18];

    return visualEffectView;
}

- (NSImage *)roundedRectMaskWithSize:(NSSize)size cornerRadius:(CGFloat)cornerRadius {

    NSImage *maskImage = [[NSImage alloc] initWithSize:size];
    NSRect rect = NSMakeRect(0, 0, size.width, size.height);

    NSBezierPath *bezierPath = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:cornerRadius yRadius:cornerRadius];

    [maskImage lockFocus];

    [NSColor.blackColor set];
    [bezierPath fill];
    [maskImage unlockFocus];

    maskImage.capInsets = NSEdgeInsetsMake(cornerRadius, cornerRadius, cornerRadius, cornerRadius);
    maskImage.resizingMode = NSImageResizingModeStretch;

    return maskImage;
}

- (void)showAndFadeOutWithDelay:(CGFloat)delay duration:(CGFloat)duration {

    [self orderFront:nil];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            context.duration = duration;
            context.timingFunction = [CAMediaTimingFunction functionWithName:
                                      kCAMediaTimingFunctionLinear];
            self.animator.alphaValue = 0;
        } completionHandler:^{
            [self orderOut:nil];
        }];
    });
}


@end
