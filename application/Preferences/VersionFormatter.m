// Custom formatter adapted from code by Jonathan Mitchell
// See
// https://stackoverflow.com/questions/827014/how-to-limit-nstextfield-text-length-and-keep-it-always-upper-case
//

#import "VersionFormatter.h"

@implementation VersionFormatter

#pragma mark -
#pragma mark Textual Representation of Cell Content

- (NSString *)stringForObjectValue:(id)object
{
    NSString *stringValue = nil;
    if ([object isKindOfClass:[NSString class]]) {

        // A new NSString is perhaps not required here
        // but generically a new object would be generated
        stringValue = [NSString stringWithString:object];
    }

    return stringValue;
}

#pragma mark -
#pragma mark Object Equivalent to Textual Representation

- (BOOL)getObjectValue:(__autoreleasing id * _Nullable)object forString:(NSString *)string errorDescription:(NSString * __autoreleasing *)error
{
    BOOL valid = YES;

    // Be sure to generate a new object here or binding woe ensues
    // when continuously updating bindings are enabled.
    *object = [NSString stringWithString:string];

    return valid;
}

#pragma mark -
#pragma mark Dynamic Cell Editing

- (BOOL)isPartialStringValid:(NSString * __autoreleasing *)partialStringPtr
       proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
              originalString:(NSString *)origString
       originalSelectedRange:(NSRange)origSelRange
            errorDescription:(NSString * __autoreleasing *)error
{
    BOOL valid = YES;

    NSString *proposedString = *partialStringPtr;

    NSCharacterSet *uppercaseSet = [NSCharacterSet characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZ"];


    if (proposedString.length) {
        if ([proposedString rangeOfCharacterFromSet:uppercaseSet].location == NSNotFound) {
            valid = NO;
            NSCharacterSet *lowerCaseSet = [NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyz"];
            if ([proposedString rangeOfCharacterFromSet:lowerCaseSet].location == NSNotFound) {
                return NO;
            } else {
                *partialStringPtr = [proposedString uppercaseString];
                proposedString = *partialStringPtr;
            }
        }
        NSRange newRange = [proposedString rangeOfCharacterFromSet:uppercaseSet];
        proposedString = [proposedString substringFromIndex:newRange.location];
    }


    if ([proposedString length] > 1) {

        // The original string has been modified by one or more characters (via pasting).
        // Either way compute how much of the proposed string can be accommodated.
        NSInteger origLength = (NSInteger)origString.length;
        NSInteger insertLength = 1 - origLength;

        // If a range is selected then characters in that range will be removed
        // so adjust the insert length accordingly
        insertLength += origSelRange.length;

        // Get the string components
        NSString *prefix = [origString substringToIndex:origSelRange.location];
        NSString *suffix = [origString substringFromIndex:origSelRange.location + origSelRange.length];
        NSRange allText = NSMakeRange(0, proposedString.length);
        NSRange insertRange = NSMakeRange(origSelRange.location, (NSUInteger)insertLength);
        insertRange = NSIntersectionRange(allText, insertRange);
        NSString *insert = [proposedString substringWithRange:insertRange];

#ifdef _TRACE

        NSLog(@"Original string: %@", origString);
        NSLog(@"Original selection location: %lu length %lu", (unsigned long)origSelRange.location, (unsigned long)origSelRange.length);

        NSLog(@"Proposed string: %@", proposedString);
        NSLog(@"Proposed selection location: %lu length %lu", (unsigned long)proposedSelRangePtr->location, (unsigned long)proposedSelRangePtr->length);

        NSLog(@"Prefix: %@", prefix);
        NSLog(@"Suffix: %@", suffix);
        NSLog(@"Insert: %@", insert);
#endif

        // Assemble the final string
        *partialStringPtr = [[NSString stringWithFormat:@"%@%@%@", prefix, insert, suffix] uppercaseString];

        // Fix-up the proposed selection range
        proposedSelRangePtr->location = origSelRange.location + (NSUInteger)insertLength;
        proposedSelRangePtr->length = 0;

#ifdef _TRACE

        NSLog(@"Final string: %@", *partialStringPtr);
        NSLog(@"Final selection location: %lu length %lu", (unsigned long)proposedSelRangePtr->location, (unsigned long)proposedSelRangePtr->length);

#endif
        valid = NO;
    }

    if ([*partialStringPtr length] > 1) {
        *partialStringPtr = [*partialStringPtr uppercaseString];
        NSRange newRange = [*partialStringPtr rangeOfCharacterFromSet:uppercaseSet];
        newRange.length = 1;
        *partialStringPtr = [*partialStringPtr substringWithRange:newRange];
        *proposedSelRangePtr = NSMakeRange(0, 1);
        return NO;
    }

    return valid;
}

@end
