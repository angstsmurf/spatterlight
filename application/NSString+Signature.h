//
//  NSString+Signature.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-03-26.
//
//

#import <Foundation/Foundation.h>

@interface NSString (Signature)

- (NSString *)signatureFromFile;
- (NSString *)scrubInvalidCharacters;


@end
