//
//  CoverImageWindowController.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-29.
//

#import <Cocoa/Cocoa.h>

@class GlkController, CoverImageView;

NS_ASSUME_NONNULL_BEGIN

@interface CoverImageHandler : NSObject

@property BOOL waitingforkey;
@property (weak) GlkController* glkctl;

@property (nullable) CoverImageView *imageView;
@property (nullable) CoverImageView *contentView;

@property (readonly) NSWindow *enterFullscreenWindow;
@property (readonly) NSWindow *exitFullscreenWindow;

- (instancetype)initWithController:(GlkController *)controller;

- (void)forkInterpreterTask;
- (void)showLogoWindow;
- (void)positionImage;

- (void)enterFullScreenWithDuration:(NSTimeInterval)duration;
- (void)exitFullscreenWithDuration:(NSTimeInterval)duration;

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window;
- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window;


@end

NS_ASSUME_NONNULL_END
