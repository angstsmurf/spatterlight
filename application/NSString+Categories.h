//
//  NSString+Categories.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-03-26.
//
//

#import <Foundation/Foundation.h>

@interface NSString (Categories)

@property (readonly, copy) NSString *stringByDecodingXMLEntities;

+ (NSString *)stringWithSummaryOf:(NSArray *)games;
- (NSString *)signatureFromFile;
- (NSString *)scrubInvalidCharacters;

@end
