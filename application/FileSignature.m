//
//  FileSignature.m
//  Spatterlight
//
//  Created by Administrator on 2025-06-10.
//

#import <CommonCrypto/CommonDigest.h>

#import "FileSignature.h"
#import "DebugLog.h"

@implementation FileSignature

// A 64 character string made up of 32 hex values from the digest produced by CC_SHA256()
+ (NSString *)sha256StringFromData:(NSData *)data {
    unsigned char resultCString[CC_SHA256_DIGEST_LENGTH];
    unsigned int length;
    // Cap at 1 MiB to not stall on huge files
    if (data.length > 0x100000) {
        length = 0x100000;
    } else {
        length = (unsigned int)data.length;
    }

    if (length < 32)
        return @"";
    // CC_SHA256 performs digest calculation and places the result in the caller-supplied buffer for digest (resultCString).
    CC_SHA256(data.bytes, length, resultCString);

    return [self hexStringFromData:[NSData dataWithBytesNoCopy:resultCString length:CC_SHA256_DIGEST_LENGTH freeWhenDone:NO]];
}

+ (NSString *)hexStringFromData:(NSData *)data {
    const unsigned char *bytes = data.bytes;
    NSMutableString *hexString = [NSMutableString stringWithCapacity:data.length * 2];
    for (NSUInteger i = 0; i < data.length; i++)
        [hexString appendFormat:@"%02X", bytes[i]];
    return hexString;
}

+ (NSString *)signatureFromData:(NSData *)theData {
    NSData *subData = theData;

    // If the data seems to be a Blorb, we extract the executable chunk
    if (theData.length > 24) {
        Byte bytes24[24];
        [theData getBytes:bytes24 length:24];

        if (bytes24[0] == 'F' && bytes24[1] == 'O' && bytes24[2] == 'R' &&
            bytes24[3] == 'M' && bytes24[8] == 'I' && bytes24[9] == 'F' &&
            bytes24[10] == 'R' && bytes24[11] == 'S') {
            // Data seems to be a Blorb
            if (!(bytes24[12] == 'R' && bytes24[13] == 'I' &&
                  bytes24[14] == 'd' && bytes24[15] == 'x'))
                NSLog(@"signatureFromFile: Missing RIdx index header in Blorb!");

            NSInteger chunkLength = bytes24[16] << 24 | (bytes24[17] & 0xff) << 16 |
            (bytes24[18] & 0xff) << 8 | (bytes24[19] & 0xff);

            int numRes = bytes24[20] << 24 | (bytes24[21] & 0xff) << 16 |
            (bytes24[22] & 0xff) << 8 | (bytes24[23] & 0xff);

            if (chunkLength != 4 + numRes * 12)
                NSLog(@"signature: Chunk length wrong! Should be 4 + "
                      @"number of resources * 12 (%d)!",
                      4 + numRes * 12);

            Byte bytes12[12];
            NSInteger execStart = 0;
            for (NSUInteger i = 24; i <= (NSUInteger)chunkLength + 8 && i < theData.length - 12; i += 12) {
                [theData getBytes:bytes12 range:NSMakeRange(i, 12)];
                if (bytes12[0] == 'E' && bytes12[1] == 'x' && bytes12[2] == 'e' && bytes12[3] == 'c') {
                    execStart = bytes12[8] << 24 | (bytes12[9] & 0xff) << 16 |
                    (bytes12[10] & 0xff) << 8 |
                    (bytes12[11] & 0xff);
                    // Found Exec index at offset i.
                    // Executable chunk starts at execStart.
                } else if (execStart > 0) {
                    NSInteger nextChunkStart = bytes12[8] << 24 | (bytes12[9] & 0xff) << 16 |
                    (bytes12[10] & 0xff) << 8 |
                    (bytes12[11] & 0xff);
                    if (nextChunkStart > execStart)
                        break;
                }
            }

            if (execStart > 0) {
                // The signature always covers everything from the start of
                // the executable chunk to the end of the file, even when
                // other resources follow it. Signatures are stored as
                // hashTags in the library, so changing this to hash only
                // the executable chunk would break matching of existing
                // games. (Earlier versions intended to do that, but got the
                // length negative, which either clipped to the end of the
                // file anyway or raised an NSRangeException.)
                NSUInteger start = MIN((NSUInteger)execStart, theData.length);
                subData = [theData subdataWithRange:NSMakeRange(start, theData.length - start)];
            } else {
                NSLog(@"signature: Found no executable index chunk in data!");
            }

        } // Not a blorb

    } else {
        NSLog(@"signature: Data too small to make a signature! Size: %ld bytes.", theData.length);
    }

    return [FileSignature sha256StringFromData:subData];
}

@end
