//
//  NSData+Categories.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-24.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSData (Categories)

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString * _Nonnull md5String;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString * _Nonnull sha256String;
@property (NS_NONATOMIC_IOSONLY, getter=isPlaceHolderImage, readonly) BOOL placeHolderImage;
@property (NS_NONATOMIC_IOSONLY, getter=isPNG, readonly) BOOL PNG;
@property (NS_NONATOMIC_IOSONLY, getter=isJPEG, readonly) BOOL JPEG;
- (nullable NSData *)reduceImageDimensionsTo:(NSSize)size;

+ (nullable NSData *)imageDataFromRetroURL:(NSURL *)url;
+ (nullable NSData *)imageDataFromNeoURL:(NSURL *)url;
+ (nullable NSData *)imageDataFromMG1URL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END
