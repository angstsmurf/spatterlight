//
//  NSString+Categories.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-03-26.
//
//

#import "NSString+Categories.h"
#import "Game.h"
#import "Metadata.h"

@implementation NSString (Categories)


// Computes the game signature, which is a 64-character string unique to the
// game. (In fact it is just the first 64 bytes of the game file, encoded as
// hexadecimal digits.)

- (NSString *)signatureFromFile {
    NSMutableString *hexString = [NSMutableString string];
    NSData *theData =
        [NSData dataWithContentsOfURL:[NSURL fileURLWithPath:self]];
    Byte *bytes64 = (Byte *)malloc(64);

    if (theData.length > 64) {
        [theData getBytes:bytes64 length:64];

        if (bytes64[0] == 'F' && bytes64[1] == 'O' && bytes64[2] == 'R' &&
            bytes64[3] == 'M' && bytes64[8] == 'I' && bytes64[9] == 'F' &&
            bytes64[10] == 'R' && bytes64[11] == 'S') {
            // Game file seems to be a Blorb

            if (!(bytes64[12] == 'R' && bytes64[13] == 'I' &&
                  bytes64[14] == 'd' && bytes64[15] == 'x'))
                NSLog(
                    @"signatureFromFile: Missing RIdx index header in Blorb!");

            int chunkLength = bytes64[16] << 24 | (bytes64[17] & 0xff) << 16 |
                              (bytes64[18] & 0xff) << 8 | (bytes64[19] & 0xff);

            // NSLog(@"Index length: %d", chunkLength);

            int numRes = bytes64[20] << 24 | (bytes64[21] & 0xff) << 16 |
                         (bytes64[22] & 0xff) << 8 | (bytes64[23] & 0xff);
            // NSLog(@"Number of resources: %d", numRes);

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
                    // NSLog(@"Found Exec index at offset %ld. Executable chunk
                    // starts at %d.", (long)i, execStart);
                    break;
                }
            }

            free(bytes12);

            if (execStart > 0) {
                if (execStart + 8 + 64 <= (int)theData.length) {
                    [theData getBytes:bytes64
                                range:NSMakeRange((NSUInteger)execStart + 8, 64)];
                } else
                    NSLog(@"signatureFromFile: Executable chunk too small to "
                          @"make signature!");
            } else
                NSLog(@"signatureFromFile: Found no executable index chunk!");

        } // Not a blorb

        hexString = [NSMutableString stringWithCapacity:(64 * 2)];

        for (int i = 0; i < 64; ++i) {
            [hexString appendFormat:@"%02x", (unsigned int)bytes64[i]];
        }

    } else
        NSLog(@"signatureFromFile: File too small to make signature!");

    free(bytes64);
    return [NSString stringWithString:hexString];
}

// Scrubs an NSString for non-parseable characters, mainly
// curly quotes and em dashes, and converts them to their
// "standard" equivalents, i.e. inch and minus symbols.
// This is modified from code by Jeff Henry here:
// https://stackoverflow.com/questions/39729525/replacing-smart-quote-in-nsstring-replace-doesnt-catch-it

- (NSString *)scrubInvalidCharacters {
    NSString *result = self;
    result = [result stringByReplacingOccurrencesOfString:@"\u2013"
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
    NSUInteger myLength = self.length;
    NSUInteger ampIndex = [self rangeOfString:@"&" options:NSLiteralSearch].location;

    // Short-circuit if there are no ampersands.
    if (ampIndex == NSNotFound) {
        return self;
    }
    // Make result string with some extra capacity.
    NSMutableString *result = [NSMutableString stringWithCapacity:(NSUInteger)(myLength * 1.25)];

    // First iteration doesn't need to scan to & since we did that already, but for code simplicity's sake we'll do it again with the scanner.
    NSScanner *scanner = [NSScanner scannerWithString:self];

    [scanner setCharactersToBeSkipped:nil];

    NSCharacterSet *boundaryCharacterSet = [NSCharacterSet characterSetWithCharactersInString:@" \t\n\r;"];

    do {
        // Scan up to the next entity or the end of the string.
        NSString *nonEntityString;
        if ([scanner scanUpToString:@"&" intoString:&nonEntityString]) {
            [result appendString:nonEntityString];
        }
        if ([scanner isAtEnd]) {
            goto finish;
        }
        // Scan either a HTML or numeric character entity reference.
        if ([scanner scanString:@"&amp;" intoString:NULL])
            [result appendString:@"&"];
        else if ([scanner scanString:@"&apos;" intoString:NULL])
            [result appendString:@"'"];
        else if ([scanner scanString:@"&quot;" intoString:NULL])
            [result appendString:@"\""];
        else if ([scanner scanString:@"&lt;" intoString:NULL])
            [result appendString:@"<"];
        else if ([scanner scanString:@"&gt;" intoString:NULL])
            [result appendString:@">"];
        else if ([scanner scanString:@"&#" intoString:NULL]) {
            BOOL gotNumber;
            unsigned charCode;
            NSString *xForHex = @"";

            // Is it hex or decimal?
            if ([scanner scanString:@"x" intoString:&xForHex]) {
                gotNumber = [scanner scanHexInt:&charCode];
            }
            else {
                gotNumber = [scanner scanInt:(int*)&charCode];
            }

            if (gotNumber) {
                [result appendFormat:@"%C", (unichar)charCode];

                [scanner scanString:@";" intoString:NULL];
            }
            else {
                NSString *unknownEntity = @"";

                [scanner scanUpToCharactersFromSet:boundaryCharacterSet intoString:&unknownEntity];

                [result appendFormat:@"&#%@%@", xForHex, unknownEntity];

                NSLog(@"Expected numeric character entity but got &#%@%@;", xForHex, unknownEntity);

            }

        } else {
            NSString *amp;

            [scanner scanString:@"&" intoString:&amp];  //an isolated & symbol
            [result appendString:amp];
        }
        
    }
    while (![scanner isAtEnd]);
    
finish:
    return result;
}

+ (NSString *)stringWithSummaryOf:(NSArray *)games {

    if (!games || games.count == 0)
        return nil;

    NSString *gameString = ((Game *)games[0]).metadata.title;
    if (gameString.length > 40) {
        gameString = [gameString substringToIndex:39];
        gameString = [NSString stringWithFormat:@"%@…", gameString];
    }
    gameString = [NSString stringWithFormat:@"\"%@\"", gameString];
    
    NSUInteger number = games.count - 1;

    if (number == 0)
        return gameString;
    
    if (number == 1) {
        NSString *gameString2 = ((Game *)games[1]).metadata.title;
        if (gameString2.length > 40) {
            gameString2 = [gameString2 substringToIndex:39];
            gameString2 = [NSString stringWithFormat:@"%@…", gameString];
        }
        return [NSString stringWithFormat:@"%@ and \"%@\"", gameString, gameString2];
    }
    
    NSString *numberString;

    if (number <= 12) {
    //convert to words
    NSNumber *numberValue = [NSNumber numberWithInt:number]; //needs to be NSNumber!
    NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
    numberFormatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    numberFormatter.numberStyle = NSNumberFormatterSpellOutStyle;
    numberString = [numberFormatter stringFromNumber:numberValue];
    } else {
        numberString = [NSString stringWithFormat:@"%ld", number];
    }

    return [NSString stringWithFormat:@"%@ and %@ other games", gameString, numberString];
}


@end
