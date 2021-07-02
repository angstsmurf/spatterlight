//
//  NotificationBezel.m
//  BezelTest
//
//  Created by Administrator on 2021-06-29.
//

#import "NotificationBezel.h"
#import "BezelWindow.h"

@implementation NotificationBezel

- (instancetype)initWithScreen:(NSScreen *)screen {
    self = [super init];
    if (self) {
        _screen = screen;
    }
    return self;
}

- (void)showStandardWithText:(NSString *)text {

    NSRect contentRect = [self lowerCenterRectOfSize: NSMakeSize(200, 200)];

    BezelWindow *bezelWindow = [[BezelWindow alloc] initWithContentRect:contentRect];

    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:NSMakeRect(0, 0, contentRect.size.width, contentRect.size.height)];

    NSArray<NSString *> *words = [text componentsSeparatedByString:@" "];

    bezelContentView.type = kStandard;
    bezelContentView.topText = words.firstObject;
    NSUInteger firstWordLength = words.firstObject.length;
    if (text.length > firstWordLength)
    bezelContentView.bottomText = [text substringFromIndex:firstWordLength];

    [bezelWindow.contentView addSubview:bezelContentView];
    [bezelWindow showAndFadeOutWithDelay:0.7 duration:0.25];
    double delayInSeconds = 1;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [self speakString:text];
    });
}

- (void)showGameOver {
    NSRect contentRect = [self lowerCenterRectOfSize: NSMakeSize(200, 200)];

    BezelWindow *bezelWindow = [[BezelWindow alloc] initWithContentRect:contentRect];

    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:NSMakeRect(0, 0, contentRect.size.width, contentRect.size.height)];

    bezelContentView.type = kGameOver;

    [bezelWindow.contentView addSubview:bezelContentView];
    [bezelWindow showAndFadeOutWithDelay:0.7 duration:0.25];
}


- (void)showCoverImage:(NSData *)imageData interpolation:(kImageInterpolationType)interpolation {
    NSImage *image = [[NSImage alloc] initWithData:imageData];
    if (!image)
        return;
    NSImageRep *rep = image.representations.lastObject;

    NSSize size;

    CGFloat ratio = (CGFloat)rep.pixelsHigh / (CGFloat)rep.pixelsWide;
    if (rep.pixelsWide > rep.pixelsHigh) {
        size.height = 200;
        size.width = 200 / ratio;
    } else {
        size.width = 300;
        size.height = 300 * ratio;
    }

    NSRect contentRect = [self lowerCenterRectOfSize: size];

    BezelWindow *bezelWindow = [[BezelWindow alloc] initWithContentRect:contentRect];

    BezelContentView *bezelContentView = [[BezelContentView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];

    bezelContentView.type = kCoverImage;
    bezelContentView.image = image;
    bezelContentView.interpolation = interpolation;

    [bezelWindow.contentView addSubview:bezelContentView];
    [bezelWindow showAndFadeOutWithDelay:1 duration:0.25];
}

- (NSRect)lowerCenterRectOfSize:(NSSize)size {

    CGFloat lowerCenterRectBottomOffset = 140;

    return NSMakeRect(NSMidX(_screen.visibleFrame) - (size.width / 2),
                      NSMinY(_screen.visibleFrame) + lowerCenterRectBottomOffset,
                      size.width, size.height);
}

- (void)speakString:(NSString *)string {
    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey : string
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}



@end
