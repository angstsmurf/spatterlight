//
//  OpenGameOperation.h
//  Spatterlight
//
//  Created by Administrator on 2024-11-24.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface OpenGameOperation : NSOperation

- (instancetype)initWithURL:(NSURL *)gameFileURL completionHandler:(nullable void (^)(NSData * _Nullable,  NSURL * _Nullable))completionHandler;

@end

NS_ASSUME_NONNULL_END
