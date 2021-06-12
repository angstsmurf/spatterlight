//
//  NSData+Categories.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-24.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSData (Categories)

- (NSString *)md5String;
- (BOOL)isPlaceHolderImage;

+ (nullable NSData *)imageDataFromRetroURL:(NSURL *)url;
+ (nullable NSData *)imageDataFromNeoURL:(NSURL *)url;
+ (nullable NSData *)imageDataFromMG1URL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END
