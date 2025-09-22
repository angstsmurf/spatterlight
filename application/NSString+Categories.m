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

// Only used for backward compatibility.
// A 64-character string unique to the game. (In fact it is just the first
// 64 bytes of the game file, encoded as hexadecimal digits.)
// This idea was stolen from Lectrote.

- (NSString *)oldSignatureFromFile {
    NSMutableString *hexString = [NSMutableString string];
    NSError * error = nil;
    NSData *theData =
    [NSData dataWithContentsOfURL:[NSURL fileURLWithPath:self isDirectory:NO]
                          options:NSDataReadingMappedAlways error:&error];
    Byte *bytes64 = (Byte *)malloc(64);

    if (theData.length > 64) {
        [theData getBytes:bytes64 length:64];

        if (bytes64[0] == 'F' && bytes64[1] == 'O' && bytes64[2] == 'R' &&
            bytes64[3] == 'M' && bytes64[8] == 'I' && bytes64[9] == 'F' &&
            bytes64[10] == 'R' && bytes64[11] == 'S') {
            // Game file seems to be a Blorb

            if (!(bytes64[12] == 'R' && bytes64[13] == 'I' &&
                  bytes64[14] == 'd' && bytes64[15] == 'x'))
                NSLog(@"signatureFromFile: Missing RIdx index header in Blorb!");

            int chunkLength = bytes64[16] << 24 | (bytes64[17] & 0xff) << 16 |
            (bytes64[18] & 0xff) << 8 | (bytes64[19] & 0xff);

            int numRes = bytes64[20] << 24 | (bytes64[21] & 0xff) << 16 |
            (bytes64[22] & 0xff) << 8 | (bytes64[23] & 0xff);

            if (chunkLength != 4 + numRes * 12)
                NSLog(@"signatureFromFile: Chunk length wrong! Should be 4 + "
                      @"number of resources * 12 (%d)!",
                      4 + numRes * 12);

            Byte *bytes12 = (Byte *)malloc(12);

            int execStart = 0;
            for (NSUInteger i = 24; i <= (NSUInteger)chunkLength + 8 && i < theData.length - 12; i += 12) {
                [theData getBytes:bytes12 range:NSMakeRange(i, 12)];
                if (bytes12[0] == 'E' && bytes12[1] == 'x' && bytes12[2] == 'e' && bytes12[3] == 'c') {
                    execStart = bytes12[8] << 24 | (bytes12[9] & 0xff) << 16 |
                    (bytes12[10] & 0xff) << 8 |
                    (bytes12[11] & 0xff);
                    // Found Exec index at offset i.
                    // Executable chunk starts at execStart.
                    break;
                }
            }

            free(bytes12);

            if (execStart > 0) {
                if (execStart + 8 + 64 <= (int)theData.length) {
                    [theData getBytes:bytes64
                                range:NSMakeRange((NSUInteger)execStart + 8, 64)];
                } else {
                    NSLog(@"signatureFromFile: Executable chunk in %@ too small to make signature!", self);
                }
            } else {
                NSLog(@"signatureFromFile: Found no executable index chunk in file \"%@\"!", self);
            }

        } // Not a blorb

        hexString = [NSMutableString stringWithCapacity:(64 * 2)];

        for (int i = 0; i < 64; ++i) {
            [hexString appendFormat:@"%02x", (unsigned int)bytes64[i]];
        }

    } else {
        NSLog(@"signatureFromFile: File \"%@\" too small to make a signature! Size: %ld bytes.", self, theData.length);
        if (error)
            NSLog(@"%@", error);

    }
    free(bytes64);
    return [NSString stringWithString:hexString];
}



@end
