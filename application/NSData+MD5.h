//
//  NSData+MD5.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-24.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSData (MD5)

- (NSString *)md5String;
- (BOOL)isPlaceHolderImage;

@end

NS_ASSUME_NONNULL_END
