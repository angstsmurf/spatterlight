//
//  ImageSimilarity.h
//  Spatterlight
//
//  Tells whether two cover images show the same picture, so that a
//  re-encoded, resized or letterboxed copy of a cover does not make us ask
//  the user which one to keep.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageSimilarity : NSObject

/// YES if the two images show the same picture. Uses Vision feature prints
/// on macOS 10.15 and later, and plain byte equality before that.
+ (BOOL)imageData:(NSData *)imageA looksLike:(NSData *)imageB;

@end

NS_ASSUME_NONNULL_END
