//
//  BezelContentView.h
//  Spatterlight
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
@property (nullable) NSString *topText;
@property (nullable) NSString *bottomText;
@property (nullable) NSImage *image;
@property kImageInterpolationType interpolation;

@end

NS_ASSUME_NONNULL_END
