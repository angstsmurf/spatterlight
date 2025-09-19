//
//  AttributeDictionaryTransformer.m
//  Spatterlight
//
//  Created by Administrator on 2021-03-11.
//

#import "AttributeDictionaryTransformer.h"
#import <AppKit/AppKit.h>

@implementation AttributeDictionaryTransformer

+ (NSArray *)allowedTopLevelClasses {
    return @[ [NSDictionary class] ];
}

+ (Class)transformedValueClass {
    return [NSDictionary class];
}

-(id)description {
    return @"AttributeDictionaryTransformer";
}

- (BOOL)allowsReverseTransformation {
    return YES;
}


- (id)transformedValue: (id)value {
    NSData *data = (NSData *)value;
    if (!data)
        return nil;
    NSError *error = nil;
    NSDictionary *result = nil;
    
    if ([value isKindOfClass:[NSData class]]) {
        result = [NSKeyedUnarchiver unarchivedObjectOfClasses:[NSSet setWithArray:@[ [NSDictionary class], [NSParagraphStyle class], [NSFont class], [NSColor class], [NSValue class], [NSNumber class], [NSImage class], [NSString class], [NSCursor class], [NSShadow class] ]] fromData:data error:&error];
        NSImage *cursorImage = result[@"cursorImage"];
        if (cursorImage) {
            NSMutableDictionary *mutDict = result.mutableCopy;
            NSPoint hotspot = ((NSValue *)result[@"cursorHotspot"]).pointValue;
            mutDict[NSCursorAttributeName] = [[NSCursor alloc]initWithImage:cursorImage hotSpot:hotspot] ;
            mutDict[@"cursorImage"] = nil;
            mutDict[@"cursorHotspot"] = nil;
            result = mutDict;
        }
        NSSize shadowOffset = NSZeroSize;
        if (result[@"shadowOffset"])
            shadowOffset = ((NSValue *)result[@"shadowOffset"]).sizeValue;
        CGFloat shadowRadius = 0;
        if (result[@"shadowRadius"])
            shadowRadius = ((NSNumber *)result[@"shadowRadius"]).doubleValue;
        NSColor *shadowColor = result[@"shadowColor"];
        if (!NSEqualSizes(shadowOffset, NSZeroSize) || shadowRadius > 0 || shadowColor) {
            NSShadow *shadow = [NSShadow new];
            if (!NSEqualSizes(shadowOffset, NSZeroSize))
                shadow.shadowOffset = shadowOffset;
            shadow.shadowBlurRadius = shadowRadius;
            if (shadowColor)
                shadow.shadowColor = shadowColor;
            NSMutableDictionary *mutDict = result.mutableCopy;
            mutDict[NSShadowAttributeName] = shadow;
            mutDict[@"shadowOffset"] = nil;
            mutDict[@"shadowRadius"] = nil;
            mutDict[@"shadowColor"] = nil;
            result = mutDict;
        }
    }
    if (error || !result)
        NSLog(@"Error! %@", error);
    return result;
}


- (id)reverseTransformedValue:(id)value {
    
    NSError *error = nil;
    
    if ([value isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = (NSDictionary *)value;
        NSCursor *cursor = dict[NSCursorAttributeName];
        if (cursor) {
            NSMutableDictionary *mutDict = dict.mutableCopy;
            mutDict[NSCursorAttributeName] = nil;
            mutDict[@"cursorImage"] = cursor.image;
            mutDict[@"cursorHotspot"] = @(cursor.hotSpot);
            dict = mutDict;
            value = dict;
        }
        NSShadow *shadow = dict[NSShadowAttributeName];
        if (shadow) {
            NSMutableDictionary *mutDict = dict.mutableCopy;
            mutDict[NSShadowAttributeName] = nil;
            mutDict[@"shadowOffset"] = @(shadow.shadowOffset);
            mutDict[@"shadowRadius"] = @(shadow.shadowBlurRadius);
            mutDict[@"shadowColor"] = shadow.shadowColor;
            dict = mutDict;
            value = dict;
        }
        return [NSKeyedArchiver archivedDataWithRootObject:value requiringSecureCoding:YES error:&error];
    }
    
    NSLog(@"Error! %@", error);
    return nil;
}
 
@end
