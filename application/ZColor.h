//
//  ZColor.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-04-29.
//
//

#import <Foundation/Foundation.h>

@interface ZColor : NSObject

@property NSInteger fg;
@property NSInteger bg;
@property NSColor *foregroundColor;
@property NSColor *backgroundColor;
@property NSRange range;

- (instancetype)initWithText:(NSInteger)fg background:(NSInteger)bg
                  andLocation:(NSUInteger)location;

- (NSDictionary *)coloredAttributes:(NSDictionary *)dict;
- (NSDictionary *)reversedAttributes:(NSDictionary *)dict;


@end
