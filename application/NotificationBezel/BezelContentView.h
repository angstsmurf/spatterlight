//
//  BezelContentView.h
//  BezelTest
//
//  Created by Administrator on 2021-06-29.
//

#import <Cocoa/Cocoa.h>
#import "Image.h"

typedef NS_ENUM(NSUInteger, kBezelType) {
    kStandard,
    kGameOver,
    kCoverImage
};

NS_ASSUME_NONNULL_BEGIN

@interface BezelContentView : NSView

@property kBezelType type;
@property NSString *topText;
@property NSString *bottomText;
@property NSImage *image;
@property CALayer *imageLayer;
@property kImageInterpolationType interpolation;

@end

NS_ASSUME_NONNULL_END
