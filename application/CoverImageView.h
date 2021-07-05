//
//  CoverImageView.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-05.
//

#import <Foundation/Foundation.h>

@class GlkController, MyImageWindow, CoverImageHandler;

NS_ASSUME_NONNULL_BEGIN

@interface CoverImageView : NSView

@property NSImage *image;
@property NSSize sizeInPixels;
@property CGFloat ratio;
@property NSImageInterpolation interpolation;

@property (weak) CoverImageHandler *delegate;

- (void)createImage;
- (void)positionImage;
- (void)createAndPositionImage;


@end

NS_ASSUME_NONNULL_END
