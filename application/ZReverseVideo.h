//
//  ZReverseVideo.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import <Foundation/Foundation.h>

@interface ZReverseVideo : NSObject

@property NSRange range;

- (instancetype)initWithLocation:(NSUInteger)location;
- (NSDictionary *)reversedAttributes:(NSDictionary *)dict background:(NSColor *)backCol;

@end
