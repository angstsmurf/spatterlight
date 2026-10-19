//
//  JourneyTextFormatter.m
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "JourneyTextFormatter.h"

// ASCII only: the game can't take anything else, and the ctype functions
// are undefined for most unichar values.
static BOOL IsUpper(unichar c) {
    return c >= 'A' && c <= 'Z';
}

static BOOL IsLower(unichar c) {
    return c >= 'a' && c <= 'z';
}

@implementation JourneyTextFormatter

- (instancetype)initWithMaxLength:(NSUInteger)maxLength elvish:(BOOL)elvish {
    if (self = [super initWithMaxLength:maxLength]) {
        self.elvish = elvish;
    }
    return self;
}

- (BOOL)isBadCharacter:(unichar)c {
    if (_elvish) {
        switch (c) {
            case ' ':
            case '-':
            case '\'':
            case '.':
            case ',':
            case '?':
                return NO;
            default:
                break;
        }
    }
    return !IsUpper(c) && !IsLower(c);
}

- (BOOL)isPartialStringValid:(NSString * __autoreleasing *)partialStringPtr
proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
originalString:(NSString *)origString
originalSelectedRange:(NSRange)origSelRange
errorDescription:(NSString * __autoreleasing *)error
{
    BOOL valid = [super isPartialStringValid:partialStringPtr proposedSelectedRange:proposedSelRangePtr originalString:origString originalSelectedRange:origSelRange errorDescription:error];

    NSString *proposedString = *partialStringPtr;
    NSMutableSet<NSString *> *badCharacters = [NSMutableSet new];
    for (NSUInteger i = 0; i < proposedString.length; i++) {
        unichar c = [proposedString characterAtIndex:i];
        if ([self isBadCharacter:c]) {
            [badCharacters addObject:[NSString stringWithCharacters:&c length:1]];
            valid = NO;
        }
    }

    for (NSString *bad in badCharacters) {
        proposedString = [proposedString stringByReplacingOccurrencesOfString:bad withString:@""];
    }

    if (!_elvish && proposedString.length > 0) {
        unichar first = [proposedString characterAtIndex:0];
        if (IsLower(first)) {
            valid = NO;
            first = (unichar)(first - 'a' + 'A');
            proposedString = [NSString stringWithFormat:@"%@%@", [NSString stringWithCharacters:&first length:1], [proposedString substringFromIndex:1]];
        }
        badCharacters = [NSMutableSet new];
        for (NSUInteger i = 1; i < proposedString.length; i++) {
            unichar c = [proposedString characterAtIndex:i];
            if (IsUpper(c)) {
                [badCharacters addObject:[NSString stringWithCharacters:&c length:1]];
                valid = NO;
            }
        }
        // Only the first character may be upper case, so lower-case
        // everything after it, and not the first if it is the same letter
        if (badCharacters.count) {
            NSString *rest = [proposedString substringFromIndex:1];
            for (NSString *bad in badCharacters) {
                unichar lower = (unichar)([bad characterAtIndex:0] - 'A' + 'a');
                rest = [rest stringByReplacingOccurrencesOfString:bad withString:[NSString stringWithCharacters:&lower length:1]];
            }
            proposedString = [[proposedString substringToIndex:1] stringByAppendingString:rest];
        }
    }
    *partialStringPtr = proposedString;

    return valid;
}

@end
