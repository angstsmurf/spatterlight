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
// Only used to migrate colours from the old NSUserDefaults format
// (BuiltInThemes.m). Keep it for that.
+ (NSColor *)colorFromData:(NSData *)data;

@property (NS_NONATOMIC_IOSONLY, readonly) NSInteger integerColor;
- (BOOL)isEqualToColor:(NSColor *)color;

@end
