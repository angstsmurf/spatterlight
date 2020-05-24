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

- (instancetype)initWithText:(NSInteger)fg background:(NSInteger)bg;

- (NSMutableDictionary *)coloredAttributes:(NSMutableDictionary *)dict;
- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict;


@end
