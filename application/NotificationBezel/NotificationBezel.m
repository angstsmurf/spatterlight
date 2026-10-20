//
//  NotificationBezel.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-29.
//

#import "NotificationBezel.h"
#import "BezelWindow.h"
#import "BezelContentView.h"
#import "SPAccessibility.h"
#import "Constants.h"

static const CGFloat kStandardBezelSize = 200;
static const CGFloat kCoverBezelSize = 300;
static const CGFloat kCoverBezelSmallSize = 200;
static const CGFloat kBezelBottomOffset = 140;
static const CGFloat kFadeDuration = 0.25;

@implementation NotificationBezel

- (instancetype)initWithScreen:(NSScreen *)screen {
    self = [super init];
    if (self) {
        _screen = screen ?: NSScreen.mainScreen;
    }
    return self;
}

+ (BOOL)bezelsEnabled {
    return [[NSUserDefaults standardUserDefaults] boolForKey:kShowBezelsKey];
}

- (void)showStandardWithText:(NSString *)text {
    // VoiceOver users get the announcement even with the visual bezel
    // turned off.
    if (!NotificationBezel.bezelsEnabled) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
            [self speakString:text];
        });
        return;
    }

    NSSize size = NSMakeSize(kStandardBezelSize, kStandardBezelSize);
    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:(NSRect){NSZeroPoint, size}];

    NSArray<NSString *> *words = [text componentsSeparatedByString:@" "];

    bezelContentView.type = kStandard;
    bezelContentView.topText = words.firstObject;
    NSUInteger firstWordLength = words.firstObject.length;

    if (@available(macOS 10.15, *)) {
    } else {
        if ([bezelContentView.topText isEqualToString:@"?"])
            bezelContentView.image = [NSImage imageNamed:@"Question"];
    }

    if (text.length > firstWordLength)
        bezelContentView.bottomText = [text substringFromIndex:firstWordLength];

    // Announce once the bezel has faded.
    [self presentContentView:bezelContentView delay:0.7 completion:^{
        [self speakString:text];
    }];
}

- (void)showGameOver {
    if (!NotificationBezel.bezelsEnabled)
        return;

    NSSize size = NSMakeSize(kStandardBezelSize, kStandardBezelSize);
    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:(NSRect){NSZeroPoint, size}];
    bezelContentView.type = kGameOver;

    [self presentContentView:bezelContentView delay:0.7 completion:nil];
}

// Not gated by ShowBezels: the cover image is shown as part of the
// "show and fade" cover-art preference, not as a notification.
- (void)showCoverImage:(NSData *)imageData interpolation:(kImageInterpolationType)interpolation {
    if (!imageData)
        return;
    NSImage *image = [[NSImage alloc] initWithData:imageData];
    if (!image)
        return;

    // Vector representations report zero pixel dimensions, so fall back
    // to the image's point size.
    NSImageRep *rep = image.representations.lastObject;
    NSSize original = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);
    if (original.width <= 0 || original.height <= 0)
        original = image.size;
    if (original.width <= 0 || original.height <= 0)
        return;

    NSSize size;
    NSRect visibleFrame = _screen.visibleFrame;

    CGFloat ratio = original.height / original.width;
    if (original.width > original.height) {
        size.height = kCoverBezelSize;
        size.width = kCoverBezelSize / ratio;
        if (size.width > NSWidth(visibleFrame) / 2) {
            size.height = kCoverBezelSmallSize;
            size.width = kCoverBezelSmallSize / ratio;
        }
    } else {
        size.width = kCoverBezelSize;
        size.height = kCoverBezelSize * ratio;
        if (size.height > NSHeight(visibleFrame) / 2) {
            size.height = MAX(kCoverBezelSmallSize, NSHeight(visibleFrame) / 2);
            size.width = size.height / ratio;
        }
    }

    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:(NSRect){NSZeroPoint, size}];

    bezelContentView.type = kCoverImage;
    bezelContentView.image = image;
    bezelContentView.interpolation = interpolation;

    [self presentContentView:bezelContentView delay:1 completion:nil];
}

// Bezels are shown one at a time: a bezel requested while another is on
// screen waits until the earlier ones have faded out.
static NSMutableArray<dispatch_block_t> *pendingBezels;
static BOOL bezelOnScreen = NO;

+ (void)showNextBezel {
    if (bezelOnScreen || !pendingBezels.count)
        return;
    dispatch_block_t next = pendingBezels.firstObject;
    [pendingBezels removeObjectAtIndex:0];
    bezelOnScreen = YES;
    next();
}

- (void)presentContentView:(BezelContentView *)contentView delay:(CGFloat)delay completion:(nullable dispatch_block_t)completion {
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self presentContentView:contentView delay:delay completion:completion];
        });
        return;
    }

    if (!pendingBezels)
        pendingBezels = [NSMutableArray new];

    [pendingBezels addObject:^{
        NSRect contentRect = [self lowerCenterRectOfSize:contentView.frame.size];
        BezelWindow *bezelWindow = [[BezelWindow alloc] initWithContentRect:contentRect];
        [bezelWindow.contentView addSubview:contentView];
        [bezelWindow showAndFadeOutWithDelay:delay duration:kFadeDuration completion:^{
            bezelOnScreen = NO;
            if (completion)
                completion();
            [NotificationBezel showNextBezel];
        }];
    }];
    [NotificationBezel showNextBezel];
}

- (NSRect)lowerCenterRectOfSize:(NSSize)size {
    NSRect visibleFrame = _screen.visibleFrame;
    NSRect result = NSMakeRect(NSMidX(visibleFrame) - (size.width / 2),
                               NSMinY(visibleFrame) + kBezelBottomOffset,
                               size.width, size.height);
    if (!_screen)
        return NSIntegralRect(result);
    return [_screen backingAlignedRect:result options:NSAlignAllEdgesNearest];
}

- (void)speakString:(NSString *)string {
    SPAnnounce(string, NSAccessibilityPriorityHigh, nil);
}

@end
