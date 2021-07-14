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

@property (weak) CoverImageHandler *delegate;

@property BOOL inFullscreenResize;
- (void)positionImage;


@end

NS_ASSUME_NONNULL_END
