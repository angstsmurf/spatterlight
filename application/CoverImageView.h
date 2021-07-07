//
//  CoverImageView.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-05.
//

#import "ImageView.h"

@class GlkController, CoverImageHandler;

NS_ASSUME_NONNULL_BEGIN

@interface CoverImageView : ImageView

- (instancetype)initWithFrame:(NSRect)frame delegate:(CoverImageHandler *)delegate;

//@property (nullable, readonly) NSImage *image;
@property NSSize sizeInPixels;
@property CGFloat ratio;
@property NSImageInterpolation interpolation;

@property (weak) CoverImageHandler *delegate;

//- (void)createImage;
- (void)positionImage;
//- (void)createAndPositionImage;


@end

NS_ASSUME_NONNULL_END
