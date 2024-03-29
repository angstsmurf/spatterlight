//
//  NSColor+integer.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-05-13.
//
//

#import <Cocoa/Cocoa.h>


@interface NSColor (integer)

+ (NSColor *)colorFromInteger:(NSInteger)c;
+ (NSColor *)colorFromData:(NSData *)data;

@property (NS_NONATOMIC_IOSONLY, readonly) NSInteger integerColor;
- (BOOL)isEqualToColor:(NSColor *)color;

@end
