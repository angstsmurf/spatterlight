//
//  FileSignature.m
//  Spatterlight
//
//  Created by Administrator on 2025-06-10.
//

#import <CommonCrypto/CommonDigest.h>

#import "FileSignature.h"

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
    // CC_SHA256 performs digest calculation and places the result in the caller-supplied buffer for digest (resultCString).
    CC_SHA256(data.bytes, length, resultCString);

    return [NSString stringWithFormat:
                @"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            resultCString[0], resultCString[1], resultCString[2], resultCString[3],
            resultCString[4], resultCString[5], resultCString[6], resultCString[7],
            resultCString[8], resultCString[9], resultCString[10], resultCString[11],
            resultCString[12], resultCString[13], resultCString[14], resultCString[15],
            resultCString[16], resultCString[17], resultCString[18], resultCString[19],
            resultCString[20], resultCString[21], resultCString[22], resultCString[23],
            resultCString[24], resultCString[25], resultCString[26], resultCString[27],
            resultCString[28], resultCString[29], resultCString[30], resultCString[31]
    ];
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
            NSInteger execLength = 0;
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
                    if (nextChunkStart > execStart) {
                        execLength = execStart - nextChunkStart;
                        break;
                    }
                }
            }

            if (execStart > 0) {
                if (execLength == 0) {
                    execLength = (NSInteger)theData.length - execStart;
                }
                NSRange subRange = NSMakeRange((NSUInteger)execStart, (NSUInteger)execLength);
                subRange = NSIntersectionRange(NSMakeRange(0, theData.length), subRange);

                subData = [theData subdataWithRange:subRange];
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
