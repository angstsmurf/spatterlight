//
//  FlowBreak.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface FlowBreak : NSObject <NSSecureCoding>

- (instancetype)initWithPos:(NSUInteger)pos;
- (NSRect)boundsWithLayout:(NSLayoutManager *)layout;
- (void)uncacheBounds;

@property NSRect bounds;
@property NSUInteger pos;

@end

NS_ASSUME_NONNULL_END
