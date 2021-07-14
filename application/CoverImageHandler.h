//
//  CoverImageWindowController.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-29.
//

#import <Cocoa/Cocoa.h>

@class GlkController, CoverImageView, KeyPressView;

NS_ASSUME_NONNULL_BEGIN

@interface CoverImageHandler : NSObject

@property BOOL waitingforkey;
@property (weak) GlkController* glkctl;

@property (nullable) CoverImageView *imageView;
@property (nullable) KeyPressView *backgroundView;

@property (readonly) NSWindow *enterFullscreenWindow;

- (instancetype)initWithController:(GlkController *)controller;

- (void)forkInterpreterTask;
- (void)showLogoWindow;

- (void)enterFullScreenWithDuration:(NSTimeInterval)duration;
- (void)exitFullscreenWithDuration:(NSTimeInterval)duration;

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window;
- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window;


@end

NS_ASSUME_NONNULL_END
