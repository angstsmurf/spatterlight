//
//  GlkHyperlink.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-16.
//

#import <Foundation/Foundation.h>

@interface GlkHyperlink : NSObject

@property NSInteger index;
@property NSInteger startpos;
@property NSRange range;
@property NSRect bounds;

- (instancetype) initWithIndex: (NSInteger)index andPos: (NSInteger)pos;
//- (instancetype) initWithIndex: (NSInteger)index andPos: (NSInteger)pos andBounds: (NSRect)bounds;
@end
