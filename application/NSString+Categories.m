//
//  NSString+Categories.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-03-26.
//
//

#import "Game.h"
#import "Metadata.h"
#import "FileSignature.h"

#import "NSString+Categories.h"


@implementation NSString (Categories)

// Computes the game signature, which is a 64-character string unique to the
// game. (In fact it is just the first 64 bytes of the game file, encoded as
// hexadecimal digits.)

- (NSString *)signatureFromFile {
    NSError *error = nil;
    NSData *theData =
    [NSData dataWithContentsOfURL:[NSURL fileURLWithPath:self isDirectory:NO]
                          options:NSDataReadingMappedAlways error:&error];
    if (theData == nil) {
        if (error != nil)
            NSLog(@"signatureFromFile: %@", error);
        return nil;
    }
    return [FileSignature signatureFromData:theData];
}

// Scrubs an NSString for non-parseable characters, mainly
// curly quotes and em dashes, and converts them to their
// "standard" equivalents, i.e. inch and minus symbols.
// This is modified from code by Jeff Henry here:
// https://stackoverflow.com/questions/39729525/replacing-smart-quote-in-nsstring-replace-doesnt-catch-it

- (NSString *)scrubInvalidCharacters {
    NSString *result = self;
    result = [result stringByReplacingOccurrencesOfString:@"\u2010"
                                               withString:@"-"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2011"
                                               withString:@"-"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2012"
                                               withString:@"-"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2013"
                                               withString:@"-"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2014"
                                               withString:@"-"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2015"
                                               withString:@"-"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2017"
                                               withString:@"_"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2018"
                                               withString:@"\'"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2019"
                                               withString:@"\'"];
    result = [result stringByReplacingOccurrencesOfString:@"\u201a"
                                               withString:@","];
    result = [result stringByReplacingOccurrencesOfString:@"\u201b"
                                               withString:@"\'"];
    result = [result stringByReplacingOccurrencesOfString:@"\u201c"
                                               withString:@"\""];
    result = [result stringByReplacingOccurrencesOfString:@"\u201d"
                                               withString:@"\""];
    result = [result stringByReplacingOccurrencesOfString:@"\u201e"
                                               withString:@"\""];
    result = [result stringByReplacingOccurrencesOfString:@"\u2026"
                                               withString:@"..."];
    result = [result stringByReplacingOccurrencesOfString:@"\u2032"
                                               withString:@"\'"];
    result = [result stringByReplacingOccurrencesOfString:@"\u2033"
                                               withString:@"\""];
    result = [result stringByReplacingOccurrencesOfString:@"“"
                                               withString:@"\""];
    result = [result stringByReplacingOccurrencesOfString:@"”"
                                               withString:@"\""];
    return result;
}

// Unescapes HTML escaped characters.
// Code adapted from Daniel Dickison, Walty Yeung et al.
// See replies to https://stackoverflow.com/questions/1105169/html-character-decoding-in-objective-c-cocoa-touch

- (NSString *)stringByDecodingXMLEntities {
    NSUInteger ampIndex = [self rangeOfString:@"&" options:NSLiteralSearch].location;

    // Short-circuit if there are no ampersands.
    if (ampIndex == NSNotFound) {
        return self;
    }
    
    CFStringRef outStr = CFXMLCreateStringByUnescapingEntities(NULL, (CFStringRef)self, NULL);

    return CFBridgingRelease(outStr);
}

+ (NSString *)stringWithSummaryOfGames:(NSArray<Game*> *)games {

    if (!games || games.count == 0)
        return nil;
    Metadata *metadata = games.firstObject.metadata;
    if (metadata == nil)
        return nil;
    NSString *gameString = metadata.title;
    if (gameString.length > 40) {
        gameString = [gameString substringToIndex:39];
        gameString = [NSString stringWithFormat:@"%@…", gameString];
    }
    gameString = [NSString stringWithFormat:@"\"%@\"", gameString];

    NSUInteger number = games.count - 1;

    if (number == 0)
        return gameString;

    if (number == 1) {
        NSString *gameString2 = games[1].metadata.title;
        if (gameString2.length > 40) {
            gameString2 = [gameString2 substringToIndex:39];
            gameString2 = [NSString stringWithFormat:@"%@…", gameString2];
        }
        return [NSString stringWithFormat:@"%@ and \"%@\"", gameString, gameString2];
    }

    NSString *numberString;

    if (number <= 12) {
        //convert to words
        NSNumber *numberValue = @(number); //needs to be NSNumber!
        NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
        numberFormatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
        numberFormatter.numberStyle = NSNumberFormatterSpellOutStyle;
        numberString = [numberFormatter stringFromNumber:numberValue];
    } else {
        numberString = [NSString stringWithFormat:@"%ld", number];
    }

    return [NSString stringWithFormat:@"%@ and %@ other games", gameString, numberString];
}

// Returns an NSArray of range values, including leading and trailing spaces (but strips leading and trailing blank lines)
- (NSArray *)lineRanges {
    return [self lineRangesKeepEmptyLines:NO];
}

- (NSArray *)lineRangesKeepEmptyLines:(BOOL)keepEmpty {
    NSUInteger stringLength = self.length;
    NSRange linerange;
    NSMutableArray<NSValue *> *lines = [[NSMutableArray alloc] init];
    for (NSUInteger index = 0; index < stringLength;) {
        // This includes newlines
        linerange = [self lineRangeForRange:NSMakeRange(index, 0)];
        index = NSMaxRange(linerange);
        [lines addObject:[NSValue valueWithRange:linerange]];
    }

    if (lines.count && !keepEmpty) {
        //Strip leading blank lines
        BOOL firstLineBlank;
        do {
            firstLineBlank = [self rangeIsEmpty:lines.firstObject];
            if (firstLineBlank) {
                [lines removeObjectAtIndex:0];
                // If all lines are blank, we are not in a menu
                if (!lines.count)
                    return lines;
            }
        } while (firstLineBlank);

        //Strip trailing blank lines
        BOOL lastLineBlank;
        do {
            lastLineBlank = [self rangeIsEmpty:lines.lastObject];
            if (lastLineBlank) {
                [lines removeLastObject];
            }
        } while (lastLineBlank);
    }
    return lines;
}


// Returns yes if range only consists of whitespace or newlines
- (BOOL)rangeIsEmpty:(NSValue *)rangeValue {
    if (!self.length)
        return YES;
    NSRange range = rangeValue.rangeValue;
    NSRange allText = NSMakeRange(0, self.length);
    range = NSIntersectionRange(allText, range);
    NSString *trimmedString = [self substringWithRange:range];
    trimmedString = [trimmedString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return (trimmedString.length == 0);
}

- (NSString *)substringWithRangeValue:(NSValue *)val {
    NSRange range = val.rangeValue;
    NSRange allText = NSMakeRange(0, self.length);
    range = NSIntersectionRange(allText, range);
    NSString *string = [self substringWithRange:range];
    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return string;
}


@end
