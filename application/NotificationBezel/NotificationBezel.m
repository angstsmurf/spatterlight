//
//  NotificationBezel.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-29.
//

#import "NotificationBezel.h"
#import "BezelWindow.h"
#import "BezelContentView.h"

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
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"ShowBezels"];
}

- (void)showStandardWithText:(NSString *)text {
    // Announce once the bezel has faded. VoiceOver users get the
    // announcement even with the visual bezel turned off.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        [self speakString:text];
    });

    if (!NotificationBezel.bezelsEnabled)
        return;

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

    [self presentContentView:bezelContentView delay:0.7];
}

- (void)showGameOver {
    if (!NotificationBezel.bezelsEnabled)
        return;

    NSSize size = NSMakeSize(kStandardBezelSize, kStandardBezelSize);
    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:(NSRect){NSZeroPoint, size}];
    bezelContentView.type = kGameOver;

    [self presentContentView:bezelContentView delay:0.7];
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

    [self presentContentView:bezelContentView delay:1];
}

- (void)presentContentView:(BezelContentView *)contentView delay:(CGFloat)delay {
    NSRect contentRect = [self lowerCenterRectOfSize:contentView.frame.size];
    BezelWindow *bezelWindow = [[BezelWindow alloc] initWithContentRect:contentRect];
    [bezelWindow.contentView addSubview:contentView];
    [bezelWindow showAndFadeOutWithDelay:delay duration:kFadeDuration];
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
    NSWindow *mainWin = NSApp.mainWindow;
    if (!mainWin)
        return;

    NSAccessibilityPostNotificationWithUserInfo(mainWin,
                                                NSAccessibilityAnnouncementRequestedNotification,
                                                @{ NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
                                                   NSAccessibilityAnnouncementKey: string });
}

@end
