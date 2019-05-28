//
//  NSString+Signature.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-03-26.
//
// Computes the game signature, which is a 64-character string unique to the
// game. (In fact it is just the first 64 bytes of the game file, encoded as
// hexadecimal digits.)
//

#import "NSString+Signature.h"

@implementation NSString (Signature)

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
            for (NSInteger i = 24;
                 i <= chunkLength + 8 && i < theData.length - 12; i += 12) {
                [theData getBytes:bytes12 range:NSMakeRange(i, 12)];
                if (bytes12[0] == 'E' && bytes12[1] == 'x' &&
                    bytes12[2] == 'e' && bytes12[3] == 'c') {
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
                if (execStart + 8 + 64 <= theData.length) {
                    [theData getBytes:bytes64
                                range:NSMakeRange(execStart + 8, 64)];
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

@end
