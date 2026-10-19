//
//  BezelWindow.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-29.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface BezelWindow : NSWindow

- (instancetype)initWithContentRect:(NSRect)contentRect;

- (void)showAndFadeOutWithDelay:(CGFloat)delay duration:(CGFloat)duration completion:(nullable dispatch_block_t)completion;

@end

NS_ASSUME_NONNULL_END
