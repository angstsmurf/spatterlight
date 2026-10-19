//
//  NotificationBezel.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-29.
//

#import <Cocoa/Cocoa.h>
#import "Image.h"

NS_ASSUME_NONNULL_BEGIN

@interface NotificationBezel : NSObject

/// A nil screen (e.g. from a hidden or minimised window) falls back to the main screen.
- (instancetype)initWithScreen:(nullable NSScreen *)screen NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)showGameOver;

- (void)showStandardWithText:(NSString *)text;

- (void)showCoverImage:(nullable NSData *)imageData interpolation:(kImageInterpolationType)interpolation;

@property (readonly, nullable) NSScreen *screen;

@end

NS_ASSUME_NONNULL_END
