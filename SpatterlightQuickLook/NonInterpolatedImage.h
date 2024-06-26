//
//  NonInterpolatedImage.h
//  SpatterlightQuickLook
//
//  Created by Administrator on 2021-10-18.
//

#import <Cocoa/Cocoa.h>

@class Image;

NS_ASSUME_NONNULL_BEGIN

@interface NonInterpolatedImage : NSView

- (void)addImageFromManagedObject:(Image *)imageObject;
- (void)addImageFromData:(NSData *)imageData;

@property BOOL hasImage;
@property NSSize originalImageSize;
@property CGFloat ratio;

@end

NS_ASSUME_NONNULL_END
