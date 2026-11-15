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
/// Whether the game chose this color itself (not default, current or transparent).
@property (readonly) BOOL setsForeground;
@property (readonly) BOOL setsBackground;
- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict;


@end
