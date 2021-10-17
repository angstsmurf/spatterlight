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

+ (NSString *)stringWithSummaryOf:(NSArray<Game*> *)games;
- (NSString *)signatureFromFile;
- (NSString *)scrubInvalidCharacters;
//! Returns an \c NSArray of range values representing the lines in the string, including leading and trailing spaces (but strips leading and trailing blank lines)
- (NSArray<NSValue*> *)lineRanges;
//! Returns an \c NSArray of range values representing the lines in the string, including leading and trailing spaces (but optionally strips leading and trailing blank lines)
- (NSArray<NSValue*> *)lineRangesKeepEmptyLines:(BOOL)keepEmpty;

//! Returns \c YES if range only consists of white space or new lines
- (BOOL)rangeIsEmpty:(NSValue *)rangeValue;

//! Converts range value to range and trims newlines and whitespace
- (NSString *)substringWithRangeValue:(NSValue *)value;

@end
