//
//  ColorTransformer.m
//  Spatterlight
//
//  Created by Administrator on 2021-03-11.
//

#import "ColorTransformer.h"
#import <AppKit/AppKit.h>

@implementation ColorTransformer

+ (NSArray *)allowedTopLevelClasses {
    return @[[NSColor class]];
}

+ (Class)transformedValueClass {
    return [NSColor class];
}

-(id)description {
    return @"ColorTransformer";
}

- (BOOL)allowsReverseTransformation {
    return YES;
}

- (id)transformedValue: (id)value {

    NSData *data = (NSData *)value;
    if (!data)
        return nil;
    NSError *error = nil;
    NSColor *result = nil;

    if ([value isKindOfClass:[NSData class]]) {
            result = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSColor class] fromData:data error:&error];
        } 
    if (error || !result)
        NSLog(@"Error! %@", error);
    return result;
}


- (id)reverseTransformedValue:(id)value {
    NSColor *colorvalue = (NSColor *)value;
    if (!colorvalue)
        return nil;
    NSError *error = nil;
    NSData *result = nil;
    if ([value isKindOfClass:[NSColor class]]) {
        result = [NSKeyedArchiver archivedDataWithRootObject:colorvalue requiringSecureCoding:YES error:&error];
    }

    if (!result || error) {
        NSLog(@"Error %@", error);
    }
    return result;
}

@end
