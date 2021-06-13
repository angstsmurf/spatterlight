//
//  CoverImageWindow.h
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


@property (weak) CoverImageHandler *delegate;

- (void)createImage;

@end

NS_ASSUME_NONNULL_END
