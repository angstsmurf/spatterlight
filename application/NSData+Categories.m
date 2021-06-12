//
//  NSData+Categories.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-24.
//

#import <CommonCrypto/CommonDigest.h>

#import "NSBitmapImageRep+retro.h"

#import "NSData+Categories.h"

@implementation NSData (MD5)

- (NSString *)md5String {
    unsigned char resultCString[16];
    CC_MD5(self.bytes, (unsigned int)self.length, resultCString);
    
    return [NSString stringWithFormat:
            @"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            resultCString[0], resultCString[1], resultCString[2], resultCString[3],
            resultCString[4], resultCString[5], resultCString[6], resultCString[7],
            resultCString[8], resultCString[9], resultCString[10], resultCString[11],
            resultCString[12], resultCString[13], resultCString[14], resultCString[15]
            ];
}

- (BOOL)isPlaceHolderImage {
    return [self.md5String isEqualToString:@"1855A829466C301CD8113475E8E643BC"];
}


#pragma mark Retro image data

+ (nullable NSData *)imageDataFromRetroURL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromURL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
    return data;
}
+ (nullable NSData *)imageDataFromNeoURL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromNeoURL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
    return data;
}
+ (nullable NSData *)imageDataFromMG1URL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromMG1URL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
    return data;
}

@end
