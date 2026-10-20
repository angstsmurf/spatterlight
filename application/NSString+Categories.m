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
#import "DebugLog.h"


@implementation NSString (Categories)

#pragma mark File signatures

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

// Only used for backward compatibility.
// A 64-character string unique to the game. (In fact it is just the first
// 64 bytes of the game file, encoded as hexadecimal digits.)
// This idea was stolen from Lectrote.
// It repeats the Blorb chunk walk in FileSignature, but don't merge the two:
// this must keep giving the old answer, because GameImporter.m and
// GlkController+Autorestore.m match existing library entries against it.

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

#pragma mark Text cleanup

// Scrubs an NSString for non-parseable characters, mainly
// curly quotes and em dashes, and converts them to their
// "standard" equivalents, i.e. inch and minus symbols.
// This is modified from code by Jeff Henry here:
// https://stackoverflow.com/questions/39729525/replacing-smart-quote-in-nsstring-replace-doesnt-catch-it

- (NSString *)scrubInvalidCharacters {
    // None of the replacements contains a character that is itself replaced,
    // so the order does not matter.
    static NSString * const replacements[][2] = {
        { @"\u2010", @"-" },   { @"\u2011", @"-" },  { @"\u2012", @"-" },
        { @"\u2013", @"-" },   { @"\u2014", @"-" },  { @"\u2015", @"-" },
        { @"\u2017", @"_" },   { @"\u2018", @"'" },  { @"\u2019", @"'" },
        { @"\u201a", @"," },   { @"\u201b", @"'" },  { @"\u201c", @"\"" },
        { @"\u201d", @"\"" },  { @"\u201e", @"\"" }, { @"\u2026", @"..." },
        { @"\u2032", @"'" },   { @"\u2033", @"\"" },
    };
    NSString *result = self;
    for (size_t i = 0; i < sizeof(replacements) / sizeof(replacements[0]); i++)
        result = [result stringByReplacingOccurrencesOfString:replacements[i][0]
                                                   withString:replacements[i][1]];
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

// Turns literal "<br>" tags into real line breaks. Some games (any ADRIFT 5
// game, for example) escape the line breaks of their description as "&lt;br&gt;"
// rather than write them as XML elements, so after entity decoding we are left
// with the tags spelled out in the text. A single newline per tag is how ADRIFT
// itself renders <br>, and it also keeps the common "<br><br>" paragraph break
// looking like a paragraph break.

- (NSString *)stringByReplacingBreakTagsWithNewlines {
    // Short-circuit if there are no tags at all.
    if ([self rangeOfString:@"<" options:NSLiteralSearch].location == NSNotFound) {
        return self;
    }

    static NSRegularExpression *breakTag;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        breakTag = [NSRegularExpression regularExpressionWithPattern:@"<br\\s*/?>"
                                                            options:NSRegularExpressionCaseInsensitive
                                                              error:nil];
    });

    return [breakTag stringByReplacingMatchesInString:self
                                              options:0
                                                range:NSMakeRange(0, self.length)
                                         withTemplate:@"\n"];
}

// Titles longer than 40 characters are cut to 39 plus an ellipsis
#pragma mark Display

static NSString *SPTruncatedTitle(NSString *title) {
    if (title.length <= 40)
        return title;
    // Cut at index 39, or before it if that would split a composed character
    NSRange keep = NSMakeRange(0, [title rangeOfComposedCharacterSequenceAtIndex:39].location);
    return [[title substringWithRange:keep] stringByAppendingString:@"…"];
}

+ (NSString *)stringWithSummaryOfGames:(NSArray<Game*> *)games {

    if (!games || games.count == 0)
        return nil;
    Metadata *metadata = games.firstObject.metadata;
    if (metadata == nil)
        return nil;
    NSString *gameString = [NSString stringWithFormat:@"\"%@\"", SPTruncatedTitle(metadata.title)];

    NSUInteger number = games.count - 1;

    if (number == 0)
        return gameString;

    if (number == 1) {
        NSString *gameString2 = SPTruncatedTitle(games[1].metadata.title);
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
#pragma mark Line ranges

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

#pragma mark Construction

+ (NSString *)stringWithLatin1Bytes:(const char *)bytes length:(NSUInteger)length {
    if (bytes == NULL || length == 0)
        return @"";
    NSString *string = [[NSString alloc] initWithBytes:bytes length:length encoding:NSISOLatin1StringEncoding];
    return string ?: @"";
}

+ (NSString *)spacesOfLength:(NSUInteger)length {
    return [@"" stringByPaddingToLength:length withString:@" " startingAtIndex:0];
}

@end
