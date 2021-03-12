//
//  InsecureValueTransformer.m
//  Spatterlight
//
//  Created by Administrator on 2021-03-11.
//

#import "InsecureValueTransformer.h"

@implementation InsecureValueTransformer

+ (Class)transformedValueClass {
    return nil;
}

-(id)description {
    return @"InsecureValueTransformer";
}

- (BOOL)allowsReverseTransformation {
    return YES;
}


- (id)reverseTransformedValue: (id)value {
    NSData *data = (NSData *)value;
    if (!data)
        return nil;
    NSError *error = nil;
    id result = nil;

    if ([value isKindOfClass:[NSData class]]) {
            result = [NSKeyedUnarchiver unarchiveObjectWithData:data];
        }
    if (error || !result)
        NSLog(@"Error! %@", error);
    return result;
}


- (id)transformedValue:(id)value {
    return [NSKeyedArchiver archivedDataWithRootObject:value];;
}

@end
