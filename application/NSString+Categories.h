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
// Returns an NSArray of range values representing the lines in the string, including leading and trailing spaces (but optionally strips leading and trailing blank lines)
- (NSArray *)lineRanges;
- (NSArray *)lineRangesKeepEmptyLines:(BOOL)keepEmpty;

// Returns yes if range only consists of white space or new lines
- (BOOL)rangeIsEmpty:(NSValue *)rangeValue;

// Converts range value to range and trims newlines and whitespace
- (NSString *)substringWithRangeValue:(NSValue *)value;

@end
