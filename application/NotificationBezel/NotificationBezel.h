//
//  NotificationBezel.h
//  BezelTest
//
//  Created by Administrator on 2021-06-29.
//

#import <Cocoa/Cocoa.h>
#import "BezelContentView.h"

@class BezelParameters;

NS_ASSUME_NONNULL_BEGIN

@interface NotificationBezel : NSObject

- (instancetype)initWithScreen:(NSScreen *)screen;

- (void)showGameOver;

- (void)showStandardWithText:(NSString *)text;

- (void)showCoverImage:(NSData *)imageData interpolation:(kImageInterpolationType)interpolation;


@property NSScreen *screen;

@end

NS_ASSUME_NONNULL_END
