//
//  NSView+Categories.m
//  Spatterlight
//

#import "NSView+Categories.h"

@implementation NSView (Categories)

- (nullable NSSearchField *)firstSearchField {
    if ([self isKindOfClass:[NSSearchField class]])
        return (NSSearchField *)self;
    for (NSView *subview in self.subviews) {
        NSSearchField *found = subview.firstSearchField;
        if (found)
            return found;
    }
    return nil;
}

@end
