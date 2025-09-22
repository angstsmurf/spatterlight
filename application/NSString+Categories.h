//
//  NSString+Categories.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-03-26.
//
//

#import <Foundation/Foundation.h>

@class Game;

@interface NSString (Categories)

@property (readonly, copy) NSString *stringByDecodingXMLEntities;

//! A short summary of the games in the array, usually the title of the first and then the total count of games
+ (NSString *)stringWithSummaryOfGames:(NSArray<Game*> *)games;
//! A SHA256 signature calculated from file data
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *signatureFromFile;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *scrubInvalidCharacters;
//! Returns an \c NSArray of range values representing the lines in the string, including leading and trailing spaces (but strips leading and trailing blank lines)
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray<NSValue *> *lineRanges;
//! Returns an \c NSArray of range values representing the lines in the string, including leading and trailing spaces (but optionally strips leading and trailing blank lines)
- (NSArray<NSValue*> *)lineRangesKeepEmptyLines:(BOOL)keepEmpty NS_SWIFT_NAME(lineRanges(keepEmptyLines:));

//! Returns \c YES if range only consists of white space or new lines
- (BOOL)rangeIsEmpty:(NSValue *)rangeValue;

//! Converts range value to range and trims newlines and whitespace
- (NSString *)substringWithRangeValue:(NSValue *)value;

//! The old deprecated way to calculate a file signature, used for backward compatibility
- (NSString *)oldSignatureFromFile;

@end
