//
//  FileSignature.h
//  Spatterlight
//
//  Created by Administrator on 2025-06-10.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface FileSignature : NSObject

+ (NSString *)sha256StringFromData:(NSData *)data;
// Upper-case hex digits, two per byte.
+ (NSString *)hexStringFromData:(NSData *)data;
+ (NSString *)signatureFromData:(NSData *)theData;

@end

NS_ASSUME_NONNULL_END
