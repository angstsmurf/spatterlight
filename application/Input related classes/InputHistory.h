//
//  InputHistory.h
//  Spatterlight
//
//  Created by Administrator on 2020-11-25.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface InputHistory : NSObject <NSSecureCoding>

- (void)reset;
- (void)saveHistory:(NSString *)line;
- (nullable NSString *)travelBackwardInHistory:(nullable NSString *)currentInput;
- (nullable NSString *)travelForwardInHistory;

@end

NS_ASSUME_NONNULL_END
