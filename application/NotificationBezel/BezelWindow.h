//
//  BezelWindow.h
//  BezelTest
//
//  Created by Administrator on 2021-06-29.
//

#import <Cocoa/Cocoa.h>

@class BezelParameters;

NS_ASSUME_NONNULL_BEGIN

@interface BezelWindow : NSWindow

- (instancetype)initWithContentRect:(NSRect)contentRect;

- (void)showAndFadeOutWithDelay:(CGFloat)delay duration:(CGFloat)duration;

@end

NS_ASSUME_NONNULL_END
