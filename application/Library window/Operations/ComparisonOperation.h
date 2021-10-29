//
//  ComparisonOperation.h
//
//  Created by Administrator on 2021-10-21.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ComparisonOperation : NSOperation

- (instancetype)initWithNewData:(NSData *)newData oldData:(NSData *)oldData force:(BOOL)force completionHandler:(void (^)(void))completionHandler;

@end

NS_ASSUME_NONNULL_END
