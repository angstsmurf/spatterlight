//
//  GlkHyperlink.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-16.
//

#import <Foundation/Foundation.h>

@interface GlkHyperlink : NSObject

@property NSUInteger index;
@property NSUInteger startpos;
@property NSRange range;
@property NSRect bounds;

- (instancetype) initWithIndex: (NSUInteger)index andPos: (NSUInteger)pos;
@end
